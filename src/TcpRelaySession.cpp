/**
 * Socks5BalancerAsio : A Simple TCP Socket Balancer for balance Multi Socks5 Proxy Backend Powered by Boost.Asio
 * Copyright (C) <2020>  <Jeremie>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "TcpRelaySession.h"

#include <boost/lexical_cast.hpp>

#include "SessionRelayId.h"

TcpRelaySession::TcpRelaySession(
        boost::asio::any_io_executor ex,
        std::shared_ptr<UpstreamPool> upstreamPool,
        std::weak_ptr<TcpRelayStatisticsInfo> statisticsInfo,
        std::shared_ptr<ConfigLoader> configLoader,
        std::shared_ptr<AuthClientManager> authClientManager,
        size_t retryLimit,
        bool traditionTcpRelay,
        bool disableConnectionTracker
) :
        ex(ex),
        downstream_socket_(ex),
        upstream_socket_(ex),
        resolver_(ex),
        upstreamPool(std::move(upstreamPool)),
        authClientManager(std::move(authClientManager)),
        statisticsInfo(std::move(statisticsInfo)),
        retryLimit(retryLimit),
        traditionTcpRelay(traditionTcpRelay),
        disableConnectionTracker(disableConnectionTracker),
        configLoader(std::move(configLoader)),
        relayId(SessionRelayId::getNextRelayId()) {
//        std::cout << "TcpRelaySession create" << std::endl;
}

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
    clientEndpointAddrPortString = clientEndpoint.address().to_string() + ":" +
                                   boost::lexical_cast<std::string>(clientEndpoint.port());
    listenEndpoint = downstream_socket().local_endpoint();
    listenEndpointAddrString = listenEndpoint.address().to_string() + ":" +
                               boost::lexical_cast<std::string>(listenEndpoint.port());

    BOOST_LOG_S5B_ID(relayId, trace) << "TcpRelaySession start()";
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
                    firstPackAnalyzer->getConnectType(),
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
                BOOST_LOG_S5B_ID(relayId, trace)
                    << "TcpRelaySession::try_connect_upstream() get by client getServerByHint";
                s = upstreamPool->getServerByHint(ic->rule, ic->lastUseUpstreamIndex, relayId);
            }
            if (!s) {
                // try get by listen
                auto il = pSI->getInfoListen(clientEndpointAddrString);
                if (il && il->rule != RuleEnum::inherit) {
                    BOOST_LOG_S5B_ID(relayId, trace)
                        << "TcpRelaySession::try_connect_upstream() get by listen getServerByHint";
                    s = upstreamPool->getServerByHint(il->rule, il->lastUseUpstreamIndex, relayId);
                }
            }
        }
        if (!s) {
            // fallback to get by global rule
            BOOST_LOG_S5B_ID(relayId, trace)
                << "TcpRelaySession::try_connect_upstream() get by global rule getServerByHint";
            s = upstreamPool->getServerGlobal(relayId);
        }
        if (s) {
            BOOST_LOG_S5B_ID(relayId, trace)
                << "TcpRelaySession try_connect_upstream()"
                << " " << s->host << ":" << s->port;
            {
                std::lock_guard<std::mutex> g{nowServerMtx};
                nowServer = s;
            }
            do_resolve(s->host, s->port);
        } else {
            BOOST_LOG_S5B_ID(relayId, error) << "try_connect_upstream error:" << "No Valid Server.";
        }
    } else {
        BOOST_LOG_S5B_ID(relayId, error) << "try_connect_upstream error:" << "(retryCount > retryLimit)";
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
                        BOOST_LOG_S5B_ID(relayId, error) << "do_resolve error:" << error.message();
                        nowServer->isOffline = true;
                        // try next
                        try_connect_upstream();
                    }
                } else {

                    BOOST_LOG_S5B_ID(relayId, trace) << "TcpRelaySession do_resolve()"
                                                     << " " << results->endpoint().address() << ":"
                                                     << results->endpoint().port();

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

                    BOOST_LOG_S5B_ID(relayId, trace) << "TcpRelaySession do_connect_upstream()";

                    refAdded = true;
                    ++nowServer->connectCount;
                    nowServer->updateOnlineTime();
                    auto pSI = statisticsInfo.lock();
                    if (pSI) {
                        pSI->addSession(nowServer->index, shared_from_this());
                        pSI->addSessionClient(shared_from_this());
                        pSI->addSessionListen(shared_from_this());
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
                        auto whenComplete = [self = shared_from_this()]() {
                            // start relay
                            if (auto ptr = self) {
                                BOOST_LOG_S5B_ID(ptr->relayId, trace)
                                    << "firstPackAnalyzer whenComplete start relay";

                                // impl: insert protocol analysis on here
                                auto ct = ptr->getConnectionTracker();

                                ptr->targetEndpointAddrString =
                                        ptr->firstPackAnalyzer->host + ":" +
                                        boost::lexical_cast<std::string>(ptr->firstPackAnalyzer->port);
                                auto pSI = ptr->statisticsInfo.lock();
                                if (pSI) {
                                    pSI->updateSessionInfo(ptr);
                                }

                                // Setup async read from remote server (upstream)
                                ptr->do_upstream_read();

                                // Setup async read from client (downstream)
                                ptr->do_downstream_read();
                            } else {
                                BOOST_LOG_S5B_ID(ptr->relayId, error)
                                    << "firstPackAnalyzer whenComplete ptr lock failed.";
                            }
                        };
                        auto whenError = [self = shared_from_this()](boost::system::error_code error) {
                            if (auto ptr = self) {
                                BOOST_LOG_S5B_ID(ptr->relayId, trace)
                                    << "TcpRelaySession whenError call close(error) error:"
                                    << error.what();
                                ptr->close(error);
                            } else {
                                BOOST_LOG_S5B_ID(ptr->relayId, error)
                                    << "firstPackAnalyzer whenError failed. what:"
                                    << error.what();
                            }
                        };

                        // impl : insert first pack analyzer on there
                        if (!firstPackAnalyzer) {
#ifdef Need_ProxyHandshakeAuth
                            firstPackAnalyzer = std::make_shared<ProxyHandshakeAuth>(
                                    shared_from_this(),
                                    downstream_socket_,
                                    upstream_socket_,
                                    configLoader->shared_from_this(),
                                    authClientManager->shared_from_this(),
                                    nowServer->shared_from_this(),
                                    std::move(whenComplete),
                                    std::move(whenError)
                            );
#else
                            firstPackAnalyzer = std::make_shared<FirstPackAnalyzer>(
                                    shared_from_this(),
                                    downstream_socket_,
                                    upstream_socket_,
                                    std::move(whenComplete),
                                    std::move(whenError)
                            );
#endif // Need_ProxyHandshakeAuth
                            firstPackAnalyzer->start();
                        } else {
                            BOOST_LOG_S5B_ID(relayId, trace)
                                << "TcpRelaySession do_connect_upstream call close(error) error:"
                                << error.what();
                            // wrong
                            close(error);
                        }
                    }

                } else {
                    if (error != boost::asio::error::operation_aborted) {
                        BOOST_LOG_S5B_ID(relayId, error) << "do_connect_upstream error:" << error.message();
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
                    BOOST_LOG_S5B_ID(relayId, trace)
                        << "TcpRelaySession do_upstream_read call close(error) error:"
                        << error.what();
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
                    BOOST_LOG_S5B_ID(relayId, trace)
                        << "TcpRelaySession do_downstream_write call close(error) error:"
                        << error.what();
                    close(error);
                }
            });
}

void TcpRelaySession::do_downstream_read() {
    downstream_socket_.async_read_some(
            boost::asio::buffer(downstream_data_.data(), downstream_data_.size()),
            [this, self = shared_from_this()](
                    const boost::system::error_code &error,
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
                    BOOST_LOG_S5B_ID(relayId, trace)
                        << "TcpRelaySession do_downstream_read call close(error) error:"
                        << error.what();
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
                    BOOST_LOG_S5B_ID(relayId, trace)
                        << "TcpRelaySession do_upstream_write call close(error) error:"
                        << error.what();
                    close(error);
                }
            });
}

void TcpRelaySession::close(boost::system::error_code error) {
    BOOST_LOG_S5B_ID(relayId, trace) << "TcpRelaySession::close error:" << error.what();
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
            BOOST_LOG_S5B_ID(relayId, trace) << "TcpRelaySession::close --nowServer->connectCount";
            --nowServer->connectCount;
            auto pSI = statisticsInfo.lock();
            if (pSI) {
                BOOST_LOG_S5B_ID(relayId, trace) << "TcpRelaySession::close --connectCountSub";
                pSI->connectCountSub(nowServer->index);
                pSI->connectCountSubClient(clientEndpointAddrString);
                pSI->connectCountSubListen(listenEndpointAddrString);
            }
        }
    }
}

void TcpRelaySession::forceClose() {
    boost::asio::post(ex, [this, self = shared_from_this()]() {
        BOOST_LOG_S5B_ID(relayId, trace) << "TcpRelaySession forceClose call close(error)";
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



