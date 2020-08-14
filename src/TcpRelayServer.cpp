
#include "TcpRelayServer.h"

#include <boost/lexical_cast.hpp>

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
    clientEndpoint = downstream_socket().remote_endpoint(ec);
    clientEndpointAddrString = clientEndpoint.address().to_string();
    listenEndpoint = downstream_socket().local_endpoint();
    listenEndpointAddrString = listenEndpoint.address().to_string() + ":" +
                               boost::lexical_cast<std::string>(listenEndpoint.port());

    std::cout << "TcpRelaySession start()" << std::endl;
    try_connect_upstream();
}

std::shared_ptr<ConnectionTracker> TcpRelaySession::getConnectionTracker() {
    if (disableConnectionTracker) {
        return {};
    }
    if (!connectionTracker) {
        if (!firstPackAnalyzer) {
            connectionTracker = std::make_shared<ConnectionTracker>(
                    weak_from_this()
            );
        } else {
            // read state from firstPackAnalyzer
            connectionTracker = std::make_shared<ConnectionTracker>(
                    weak_from_this(),
                    firstPackAnalyzer->connectType,
                    firstPackAnalyzer->host,
                    firstPackAnalyzer->port
            );
        }
    }
    return connectionTracker;
}

void TcpRelaySession::try_connect_upstream() {
    if (retryCount <= retryLimit) {
        ++retryCount;
        UpstreamServerRef s{};
        auto pSI = statisticsInfo.lock();
        if (pSI) {
            // try get by client
            auto ic = pSI->getInfoClient(clientEndpointAddrString);
            if (ic && ic->rule != RuleEnum::inherit) {
                s = upstreamPool->getServerByHint(ic->rule, ic->lastUseUpstreamIndex);
            }
            if (!s) {
                // try get by listen
                auto il = pSI->getInfoListen(clientEndpointAddrString);
                if (il && il->rule != RuleEnum::inherit) {
                    s = upstreamPool->getServerByHint(il->rule, il->lastUseUpstreamIndex);
                }
            }
        }
        if (!s) {
            // fallback to get by global rule
            s = upstreamPool->getServerGlobal();
        }
        if (s) {
            std::cout << "TcpRelaySession try_connect_upstream()"
                      << " " << s->host << ":" << s->port << std::endl;
            {
                std::lock_guard<std::mutex> g{nowServerMtx};
                nowServer = s;
            }
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
                    if (error != boost::asio::error::operation_aborted) {
                        std::cerr << "do_resolve error:" << error.message() << "\n";
                        nowServer->isOffline = true;
                        // try next
                        try_connect_upstream();
                    }
                } else {

                    std::cout << "TcpRelaySession do_resolve()"
                              << " " << results->endpoint().address() << ":" << results->endpoint().port()
                              << std::endl;

                    do_connect_upstream(results);
                }
            });

}

void TcpRelaySession::do_connect_upstream(boost::asio::ip::tcp::resolver::results_type results) {

    // Attempt connection to remote server (upstream side)
    upstream_socket_.async_connect(
            results->endpoint(),
            [this, self = shared_from_this()](const boost::system::error_code &error) {
                if (!error) {

                    std::cout << "TcpRelaySession do_connect_upstream()" << std::endl;

                    refAdded = true;
                    ++nowServer->connectCount;
                    nowServer->updateOnlineTime();
                    auto pSI = statisticsInfo.lock();
                    if (pSI) {
                        pSI->addSession(nowServer->index, weak_from_this());
                        pSI->addSessionClient(clientEndpointAddrString, weak_from_this());
                        pSI->addSessionListen(listenEndpointAddrString, weak_from_this());
                        pSI->lastConnectServerIndex.store(nowServer->index);
                        pSI->connectCountAdd(nowServer->index);
                        pSI->connectCountAddClient(clientEndpointAddrString);
                        pSI->connectCountAddListen(listenEndpointAddrString);
                    }

                    if (traditionTcpRelay) {
                        // Setup async read from remote server (upstream)
                        do_upstream_read();

                        // Setup async read from client (downstream)
                        do_downstream_read();
                    } else {
                        // impl : insert first pack analyzer on there
                        if (!firstPackAnalyzer) {
                            firstPackAnalyzer = std::make_shared<FirstPackAnalyzer>(
                                    shared_from_this(),
                                    downstream_socket_,
                                    upstream_socket_,
                                    [self = weak_from_this()]() {
                                        // start relay
                                        if (auto ptr = self.lock()) {
                                            // impl: insert protocol analysis on here
                                            auto ct = ptr->getConnectionTracker();

                                            // Setup async read from remote server (upstream)
                                            ptr->do_upstream_read();

                                            // Setup async read from client (downstream)
                                            ptr->do_downstream_read();
                                        } else {
                                            std::cerr << "firstPackAnalyzer whenComplete ptr lock failed." << std::endl;
                                        }
                                    },
                                    [self = weak_from_this()](boost::system::error_code error) {
                                        if (auto ptr = self.lock()) {
                                            ptr->close(error);
                                        } else {
                                            std::cerr << "firstPackAnalyzer whenError ptr lock failed." << std::endl;
                                        }
                                    }
                            );
                            firstPackAnalyzer->start();
                        } else {
                            // wrong
                            close(error);
                        }
                    }

                } else {
                    if (error != boost::asio::error::operation_aborted) {
                        std::cerr << "do_connect_upstream error:" << error.message() << "\n";
                        nowServer->isOffline = true;
                        // try next
                        try_connect_upstream();
                    }
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
                    auto ct = getConnectionTracker();
                    if (ct) {
                        ct->relayGotoDown(upstream_data_, bytes_transferred);
                    }

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
                    addDown2Statistics(bytes_transferred_);
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
                    auto ct = getConnectionTracker();
                    if (ct) {
                        ct->relayGotoUp(downstream_data_, bytes_transferred);
                    }

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
                    addUp2Statistics(bytes_transferred_);
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
        boost::system::error_code ec;
        downstream_socket_.shutdown(boost::asio::socket_base::shutdown_both, ec);
        ec.clear();
        downstream_socket_.cancel(ec);
        ec.clear();
        downstream_socket_.close(ec);
    }

    if (upstream_socket_.is_open()) {
        boost::system::error_code ec;
        upstream_socket_.shutdown(boost::asio::socket_base::shutdown_both, ec);
        ec.clear();
        upstream_socket_.cancel(ec);
        ec.clear();
        upstream_socket_.close(ec);
    }

    if (!isDeCont) {
        std::lock_guard<std::mutex> g{nowServerMtx};
        if (nowServer && refAdded) {
            isDeCont = true;
            --nowServer->connectCount;
            auto pSI = statisticsInfo.lock();
            if (pSI) {
                pSI->connectCountSub(nowServer->index);
                pSI->connectCountSubClient(clientEndpointAddrString);
                pSI->connectCountSubListen(listenEndpointAddrString);
            }
        }
    }
}

void TcpRelaySession::forceClose() {
    boost::asio::post(ex, [this, self = shared_from_this()]() {
        close();
    });
}

void TcpRelaySession::stop() {
    boost::system::error_code ec;
    upstream_socket_.cancel(ec);
    ec.clear();
    downstream_socket_.cancel(ec);
    ec.clear();
    upstream_socket_.close(ec);
    ec.clear();
    downstream_socket_.close(ec);
    ec.clear();
}

void TcpRelaySession::addUp2Statistics(size_t bytes_transferred_) {
    auto pSI = statisticsInfo.lock();
    if (pSI) {
        pSI->addByteUp(nowServer->index, bytes_transferred_);
        pSI->addByteUpClient(clientEndpointAddrString, bytes_transferred_);
        pSI->addByteUpListen(listenEndpointAddrString, bytes_transferred_);
    }
}

void TcpRelaySession::addDown2Statistics(size_t bytes_transferred_) {
    auto pSI = statisticsInfo.lock();
    if (pSI) {
        pSI->addByteDown(nowServer->index, bytes_transferred_);
        pSI->addByteDownClient(clientEndpointAddrString, bytes_transferred_);
        pSI->addByteDownListen(listenEndpointAddrString, bytes_transferred_);
    }
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
    MultiListen ul;
    ul.host = configLoader->config.listenHost;
    ul.port = configLoader->config.listenPort;

    auto f = [this, self = shared_from_this(), &resolver](MultiListen &ul) {
        auto it = resolver.resolve(ul.host, std::to_string(ul.port)).begin();
        decltype(it) end;
        for (; end != it; ++it) {

            // https://raw.githubusercontent.com/boostcon/2011_presentations/master/wed/IPv6.pdf
            // https://stackoverflow.com/questions/32803756/accepting-ipv4-and-ipv6-connections-on-a-single-port-using-boost-asio

            // maybe will resolve two endpoint from a host,
            // example, we can get '0.0.0.0'(v4) and '::'(v6) from host string '::'

            boost::asio::ip::tcp::endpoint listen_endpoint = *it;
            auto &sa = socket_acceptors.emplace_back(ex);
            sa.open(listen_endpoint.protocol());
            sa.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            sa.bind(listen_endpoint);
            sa.listen();
            auto local_endpoint = sa.local_endpoint();
            std::cout << "listening on: " << local_endpoint.address() << ":" << local_endpoint.port() << std::endl;

        }
    };
    f(ul);
    for (auto &a: configLoader->config.multiListen) {
        f(a);
    }

    for (auto &a: socket_acceptors) {
        async_accept(a);
    }
}

void TcpRelayServer::stop() {
    for (auto &a: socket_acceptors) {
        boost::system::error_code ec;
        a.cancel(ec);
    }
    for (auto &a: sessions) {
        if (auto ptr = a.lock()) {
            ptr->stop();
        }
    }
}

void TcpRelayServer::async_accept(boost::asio::ip::tcp::acceptor &sa) {
    auto session = std::make_shared<TcpRelaySession>(
            ex,
            upstreamPool,
            statisticsInfo->weak_from_this(),
            configLoader->config.retryTimes,
            configLoader->config.traditionTcpRelay,
            configLoader->config.disableConnectionTracker
    );
    sessions.push_back(session->weak_from_this());
    sa.async_accept(
            session->downstream_socket(),
            [this, session, &sa](const boost::system::error_code error) {
                if (error == boost::asio::error::operation_aborted) {
                    // got cancel signal, stop calling myself
                    std::cerr << "async_accept error: operation_aborted" << "\n";
                    return;
                }
                if (!error) {
                    std::cout << "async_accept accept." << "\n";
                    boost::system::error_code ec;
                    auto clientEndpoint = session->downstream_socket().remote_endpoint(ec);
                    auto listenEndpoint = session->downstream_socket().local_endpoint();
                    if (!ec) {
                        std::cout << "incoming connection from : "
                                  << clientEndpoint.address() << ":" << clientEndpoint.port()
                                  << "  on : "
                                  << listenEndpoint.address() << ":" << listenEndpoint.port()
                                  << "\n";

                        upstreamPool->updateLastConnectComeTime();
                        session->start();
                    }
                }
                std::cout << "async_accept next." << "\n";
                async_accept(sa);
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
//        std::cout << "do_cleanTimer()" << std::endl;

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
