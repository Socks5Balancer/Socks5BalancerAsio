
#include "TcpRelayServer.h"

boost::asio::ip::tcp::socket &TcpRelaySession::downstream_socket() {
    // Client socket
    return downstream_socket_;
}

boost::asio::ip::tcp::socket &TcpRelaySession::upstream_socket() {
    // Remote server socket
    return upstream_socket_;
}

void TcpRelaySession::start() {
    boost::system::error_code ec;
    endpoint = downstream_socket().remote_endpoint(ec);
    clientEndpointAddrString = endpoint.address().to_string();

    std::cout << "TcpRelaySession start()" << std::endl;
    try_connect_upstream();
}

void TcpRelaySession::try_connect_upstream() {
    if (retryCount <= retryLimit) {
        ++retryCount;
        auto s = upstreamPool->getServerBasedOnAddress();
        if (s) {
            std::cout << "TcpRelaySession try_connect_upstream()"
                      << " " << s->host << ":" << s->port << std::endl;
            nowServer = s;
            do_resolve(s->host, s->port);
        } else {
            std::cerr << "try_connect_upstream error:" << "No Valid Server." << "\n";
        }
    } else {
        std::cerr << "try_connect_upstream error:" << "(retryCount > retryLimit)" << "\n";
    }
}

void TcpRelaySession::do_resolve(const std::string &upstream_host, unsigned short upstream_port) {

    // Look up the domain name
    resolver_.async_resolve(
            upstream_host,
            std::to_string(upstream_port),
            [this, self = shared_from_this()](
                    const boost::system::error_code &error,
                    boost::asio::ip::tcp::resolver::results_type results) {
                if (error) {
                    std::cerr << "do_resolve error:" << error.message() << "\n";
                    nowServer->isOffline = true;
                    // try next
                    try_connect_upstream();
                }

                std::cout << "TcpRelaySession do_resolve()"
                          << " " << results->endpoint().address() << ":" << results->endpoint().port()
                          << std::endl;

                do_connect_upstream(results);
            });

}

void TcpRelaySession::do_connect_upstream(boost::asio::ip::tcp::resolver::results_type results) {

    // Attempt connection to remote server (upstream side)
    upstream_socket_.async_connect(
            results->endpoint(),
            [this, self = shared_from_this()](const boost::system::error_code &error) {
                if (!error) {

                    std::cout << "TcpRelaySession do_connect_upstream()" << std::endl;

                    ++nowServer->connectCount;
                    nowServer->updateOnlineTime();
                    auto pSI = statisticsInfo.lock();
                    if (pSI) {
                        pSI->addSession(nowServer->index, weak_from_this());
                        pSI->addSession(clientEndpointAddrString, weak_from_this());
                        pSI->lastConnectServerIndex.store(nowServer->index);
                        pSI->connectCountAdd(nowServer->index);
                        pSI->connectCountAdd(clientEndpointAddrString);
                    }

                    // Setup async read from remote server (upstream)
                    do_upstream_read();

                    // Setup async read from client (downstream)
                    do_downstream_read();

                } else {
                    std::cerr << "do_connect_upstream error:" << error.message() << "\n";
                    nowServer->isOffline = true;
                    // try next
                    try_connect_upstream();
                }
            }
    );

}

void TcpRelaySession::do_upstream_read() {
    upstream_socket_.async_read_some(
            boost::asio::buffer(upstream_data_, max_data_length),
            [this, self = shared_from_this()](const boost::system::error_code &error,
                                              const size_t &bytes_transferred) {
                if (!error) {
                    // Read from remote server complete
                    // write it to server
                    do_downstream_write(bytes_transferred);
                } else {
                    close(error);
                }
            });
}

void TcpRelaySession::do_downstream_write(const size_t &bytes_transferred) {
    boost::asio::async_write(
            downstream_socket_,
            boost::asio::buffer(upstream_data_, bytes_transferred),
            [this, self = shared_from_this(), bytes_transferred](
                    const boost::system::error_code &error,
                    std::size_t bytes_transferred_) {
                if (!error || bytes_transferred_ != bytes_transferred) {
                    auto pSI = statisticsInfo.lock();
                    if (pSI) {
                        pSI->addByteDown(nowServer->index, bytes_transferred_);
                        pSI->addByteDown(clientEndpointAddrString, bytes_transferred_);
                    }
                    // Write to client complete
                    // read more again
                    do_upstream_read();
                } else {
                    close(error);
                }
            });
}

void TcpRelaySession::do_downstream_read() {
    downstream_socket_.async_read_some(
            boost::asio::buffer(downstream_data_, max_data_length),
            [this, self = shared_from_this()](const boost::system::error_code &error,
                                              const size_t &bytes_transferred) {
                if (!error) {
                    // Read from client complete
                    // write it to server
                    do_upstream_write(bytes_transferred);
                } else {
                    close(error);
                }
            });
}

void TcpRelaySession::do_upstream_write(const size_t &bytes_transferred) {
    boost::asio::async_write(
            upstream_socket_,
            boost::asio::buffer(downstream_data_, bytes_transferred),
            [this, self = shared_from_this(), bytes_transferred](
                    const boost::system::error_code &error,
                    std::size_t bytes_transferred_) {
                if (!error || bytes_transferred_ != bytes_transferred) {
                    auto pSI = statisticsInfo.lock();
                    if (pSI) {
                        pSI->addByteUp(nowServer->index, bytes_transferred_);
                        pSI->addByteUp(clientEndpointAddrString, bytes_transferred_);
                    }
                    // Write to remote server complete
                    // read more again
                    do_downstream_read();
                } else {
                    close(error);
                }
            });
}

void TcpRelaySession::close(boost::system::error_code error) {
    if (error == boost::asio::error::eof) {
        // Rationale:
        // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
        error = {};
    }

    if (downstream_socket_.is_open()) {
        downstream_socket_.close();
    }

    if (upstream_socket_.is_open()) {
        upstream_socket_.close();
    }

    if (!isDeCont) {
        isDeCont = true;
        if (nowServer) {
            --nowServer->connectCount;
            auto pSI = statisticsInfo.lock();
            if (pSI) {
                pSI->connectCountSub(nowServer->index);
                pSI->connectCountSub(clientEndpointAddrString);
            }
        }
    }
}

void TcpRelaySession::forceClose() {
    boost::asio::post(ex, [this, self = shared_from_this()]() {
        close();
    });
}

void TcpRelayServer::start() {
    if (!cleanTimer) {
        cleanTimer = std::make_shared<boost::asio::steady_timer>(ex, std::chrono::seconds{5});
        do_cleanTimer();
    }
    if (!speedCalcTimer) {
        speedCalcTimer = std::make_shared<boost::asio::steady_timer>(ex, std::chrono::seconds{1});
        do_speedCalcTimer();
    }

    boost::asio::ip::tcp::resolver resolver(ex);
    boost::asio::ip::tcp::endpoint listen_endpoint =
            *resolver.resolve(configLoader->config.listenHost,
                              std::to_string(configLoader->config.listenPort)).begin();
    socket_acceptor.open(listen_endpoint.protocol());
    socket_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

    socket_acceptor.bind(listen_endpoint);
    socket_acceptor.listen();

    auto local_endpoint = socket_acceptor.local_endpoint();
    std::cout << "listening on: " << local_endpoint.address() << ":" << local_endpoint.port() << std::endl;

    async_accept();
}

void TcpRelayServer::stop() {
    boost::system::error_code ec;
    socket_acceptor.cancel(ec);
}

void TcpRelayServer::async_accept() {
    auto session = std::make_shared<TcpRelaySession>(
            ex,
            upstreamPool,
            statisticsInfo->weak_from_this(),
            configLoader->config.retryTimes
    );
    sessions.push_back(session->weak_from_this());
    socket_acceptor.async_accept(
            session->downstream_socket(),
            [this, session](const boost::system::error_code error) {
                if (error == boost::asio::error::operation_aborted) {
                    // got cancel signal, stop calling myself
                    std::cerr << "async_accept error: operation_aborted" << "\n";
                    return;
                }
                if (!error) {
                    std::cout << "async_accept accept." << "\n";
                    boost::system::error_code ec;
                    auto endpoint = session->downstream_socket().remote_endpoint(ec);
                    if (!ec) {
                        std::cout << "incoming connection from : "
                                  << endpoint.address() << ":" << endpoint.port() << "\n";

                        upstreamPool->updateLastConnectComeTime();
                        session->start();
                    }
                }
                std::cout << "async_accept next." << "\n";
                async_accept();
            });
}

void TcpRelayServer::removeExpiredSession() {
    auto it = sessions.begin();
    while (it != sessions.end()) {
        if ((*it).expired()) {
            it = sessions.erase(it);
        } else {
            ++it;
        }
    }
}

void TcpRelayServer::closeAllSession() {
    auto it = sessions.begin();
    while (it != sessions.end()) {
        auto p = (*it).lock();
        if (p) {
            p->forceClose();
            ++it;
        } else {
            it = sessions.erase(it);
        }
    }
}

std::shared_ptr<TcpRelayStatisticsInfo> TcpRelayServer::getStatisticsInfo() {
    return statisticsInfo;
}

void TcpRelayServer::do_cleanTimer() {
    auto c = [this, self = shared_from_this(), cleanTimer = this->cleanTimer]
            (const boost::system::error_code &e) {
        if (e) {
            return;
        }
        std::cout << "do_cleanTimer()" << std::endl;

        removeExpiredSession();
        statisticsInfo->removeExpiredSessionAll();

        cleanTimer->expires_at(cleanTimer->expiry() + std::chrono::seconds{5});
        do_cleanTimer();
    };
    cleanTimer->async_wait(c);
}

void TcpRelayServer::do_speedCalcTimer() {
    auto c = [this, self = shared_from_this(), speedCalcTimer = this->speedCalcTimer]
            (const boost::system::error_code &e) {
        if (e) {
            return;
        }
//        std::cout << "do_speedCalcTimer()" << std::endl;

        statisticsInfo->calcByteAll();

        speedCalcTimer->expires_at(speedCalcTimer->expiry() + std::chrono::seconds{1});
        do_speedCalcTimer();
    };
    speedCalcTimer->async_wait(c);
}

void TcpRelayStatisticsInfo::Info::removeExpiredSession() {
    auto it = sessions.begin();
    while (it != sessions.end()) {
        if ((*it).expired()) {
            it = sessions.erase(it);
        } else {
            ++it;
        }
    }
}

void TcpRelayStatisticsInfo::Info::closeAllSession() {
    auto it = sessions.begin();
    while (it != sessions.end()) {
        auto p = (*it).lock();
        if (p) {
            p->forceClose();
            ++it;
        } else {
            it = sessions.erase(it);
        }
    }
}

void TcpRelayStatisticsInfo::Info::calcByte() {
    size_t newByteUp = byteUp.load();
    size_t newByteDown = byteDown.load();
    byteUpChange = newByteUp - byteUpLast;
    byteDownChange = newByteDown - byteDownLast;
    byteUpLast = newByteUp;
    byteDownLast = newByteDown;
    if (byteUpChange > byteUpChangeMax) {
        byteUpChangeMax = byteUpChange;
    }
    if (byteDownChange > byteDownChangeMax) {
        byteDownChangeMax = byteDownChange;
    }
}

void TcpRelayStatisticsInfo::Info::connectCountAdd() {
    ++connectCount;
}

void TcpRelayStatisticsInfo::Info::connectCountSub() {
    --connectCount;
}

void TcpRelayStatisticsInfo::addSession(size_t index, std::weak_ptr<TcpRelaySession> s) {
    if (upstreamIndex.find(index) == upstreamIndex.end()) {
        upstreamIndex.try_emplace(index, std::make_shared<Info>());
    }
    upstreamIndex.at(index)->sessions.push_back(s);
}

void TcpRelayStatisticsInfo::addSession(std::string addr, std::weak_ptr<TcpRelaySession> s) {
    if (clientIndex.find(addr) == clientIndex.end()) {
        clientIndex.try_emplace(addr, std::make_shared<Info>());
    }
    clientIndex.at(addr)->sessions.push_back(s);
}

std::shared_ptr<TcpRelayStatisticsInfo::Info> TcpRelayStatisticsInfo::getInfo(size_t index) {
    auto n = upstreamIndex.find(index);
    if (n != upstreamIndex.end()) {
        return n->second;
    } else {
        return {};
    }
}

std::shared_ptr<TcpRelayStatisticsInfo::Info> TcpRelayStatisticsInfo::getInfo(std::string addr) {
    auto n = clientIndex.find(addr);
    if (n != clientIndex.end()) {
        return n->second;
    } else {
        return {};
    }
}

void TcpRelayStatisticsInfo::removeExpiredSession(size_t index) {
    auto p = getInfo(index);
    if (p) {
        p->removeExpiredSession();
    }
}

void TcpRelayStatisticsInfo::removeExpiredSession(std::string addr) {
    auto p = getInfo(addr);
    if (p) {
        p->removeExpiredSession();
    }
}

void TcpRelayStatisticsInfo::addByteUp(size_t index, size_t b) {
    auto p = getInfo(index);
    if (p) {
        p->byteUp += b;
    }
}

void TcpRelayStatisticsInfo::addByteUp(std::string addr, size_t b) {
    auto p = getInfo(addr);
    if (p) {
        p->byteUp += b;
    }
}

void TcpRelayStatisticsInfo::addByteDown(size_t index, size_t b) {
    auto p = getInfo(index);
    if (p) {
        p->byteDown += b;
    }
}

void TcpRelayStatisticsInfo::addByteDown(std::string addr, size_t b) {
    auto p = getInfo(addr);
    if (p) {
        p->byteDown += b;
    }
}

void TcpRelayStatisticsInfo::calcByteAll() {
    for (auto &a: upstreamIndex) {
        a.second->calcByte();
    }
    for (auto &a: clientIndex) {
        a.second->calcByte();
    }
}

void TcpRelayStatisticsInfo::removeExpiredSessionAll() {
    for (auto &a: upstreamIndex) {
        a.second->removeExpiredSession();
    }
    for (auto &a: clientIndex) {
        a.second->removeExpiredSession();
    }
}

void TcpRelayStatisticsInfo::closeAllSession(size_t index) {
    auto p = getInfo(index);
    if (p) {
        p->closeAllSession();
    }
}

void TcpRelayStatisticsInfo::closeAllSession(std::string addr) {
    auto p = getInfo(addr);
    if (p) {
        p->closeAllSession();
    }
}

void TcpRelayStatisticsInfo::connectCountAdd(size_t index) {
    auto p = getInfo(index);
    if (p) {
        p->connectCountAdd();
    }
}

void TcpRelayStatisticsInfo::connectCountAdd(std::string addr) {
    auto p = getInfo(addr);
    if (p) {
        p->connectCountAdd();
    }
}

void TcpRelayStatisticsInfo::connectCountSub(size_t index) {
    auto p = getInfo(index);
    if (p) {
        p->connectCountSub();
    }
}

void TcpRelayStatisticsInfo::connectCountSub(std::string addr) {
    auto p = getInfo(addr);
    if (p) {
        p->connectCountSub();
    }
}

std::map<size_t, std::shared_ptr<TcpRelayStatisticsInfo::Info>> &TcpRelayStatisticsInfo::getUpstreamIndex() {
    return upstreamIndex;
}

std::map<std::string, std::shared_ptr<TcpRelayStatisticsInfo::Info>> &TcpRelayStatisticsInfo::getClientIndex() {
    return clientIndex;
}
