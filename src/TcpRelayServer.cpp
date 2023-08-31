
#include "TcpRelayServer.h"
#include "TcpRelaySession.h"

#include <boost/lexical_cast.hpp>

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
            BOOST_LOG_S5B(trace) << "listening on: " << local_endpoint.address() << ":" << local_endpoint.port();

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
            boost::asio::make_strand(ioc),
            upstreamPool,
            statisticsInfo->weak_from_this(),
            configLoader->shared_from_this(),
            authClientManager->shared_from_this(),
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
                    BOOST_LOG_S5B(error) << "async_accept error: operation_aborted";
                    return;
                }
                if (!error) {
                    BOOST_LOG_S5B_ID(session->relayId, trace) << "async_accept accept.";
                    boost::system::error_code ec;
                    auto clientEndpoint = session->downstream_socket().remote_endpoint(ec);
                    auto listenEndpoint = session->downstream_socket().local_endpoint();
                    if (!ec) {
                        BOOST_LOG_S5B_ID(session->relayId, trace)
                            << "incoming connection from : "
                            << clientEndpoint.address() << ":" << clientEndpoint.port()
                            << "  on : "
                            << listenEndpoint.address() << ":" << listenEndpoint.port();

                        upstreamPool->updateLastConnectComeTime();
                        session->start();
                    }
                }
                BOOST_LOG_S5B_ID(session->relayId, trace) << "TcpRelayServer::async_accept() async_accept next.";
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

std::shared_ptr<AuthClientManager> TcpRelayServer::getAuthClientManager() {
    return authClientManager;
}

std::shared_ptr<UpstreamPool> TcpRelayServer::getUpstreamPool() {
    return upstreamPool;
}

void TcpRelayServer::do_cleanTimer() {
    auto c = [this, self = shared_from_this(), cleanTimer = this->cleanTimer]
            (const boost::system::error_code &e) {
        if (e) {
            return;
        }
//        BOOST_LOG_S5B(trace) << "do_cleanTimer()";

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
//        BOOST_LOG_S5B(trace) << "do_speedCalcTimer()";

        statisticsInfo->calcByteAll();

        speedCalcTimer->expires_at(speedCalcTimer->expiry() + std::chrono::seconds{1});
        do_speedCalcTimer();
    };
    speedCalcTimer->async_wait(c);
}
