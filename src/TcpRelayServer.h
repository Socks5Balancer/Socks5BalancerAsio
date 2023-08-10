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

#ifndef SOCKS5BALANCERASIO_TCPRELAYSERVER_H
#define SOCKS5BALANCERASIO_TCPRELAYSERVER_H

#ifdef MSVC
#pragma once
#endif

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <string>
#include <utility>
#include <list>
#include <map>
#include <atomic>
#include <mutex>
#include "UpstreamPool.h"
#include "ConnectionTracker.h"
#include "AuthClientManager.h"
#ifdef Need_ProxyHandshakeAuth
#include "ProxyHandshakeAuth.h"
#else
#include "FirstPackAnalyzer.h"
#endif // Need_ProxyHandshakeAuth
#include "TcpRelayStatisticsInfo.h"
#include "./log/Log.h"

// code template from https://github.com/ArashPartow/proxy/blob/master/tcpproxy_server.cpp
class TcpRelaySession : public std::enable_shared_from_this<TcpRelaySession> {

    boost::asio::any_io_executor ex;

    boost::asio::ip::tcp::socket downstream_socket_;
    boost::asio::ip::tcp::socket upstream_socket_;
    boost::asio::ip::tcp::resolver resolver_;
    std::shared_ptr<UpstreamPool> upstreamPool;
    std::shared_ptr<AuthClientManager> authClientManager;

    std::weak_ptr<TcpRelayStatisticsInfo> statisticsInfo;

    boost::asio::ip::tcp::endpoint clientEndpoint;
    std::string clientEndpointAddrString;
    boost::asio::ip::tcp::endpoint listenEndpoint;
    std::string listenEndpointAddrString;

    enum {
        max_data_length = 8192
    }; //8KB
    unsigned char downstream_data_[max_data_length];
    unsigned char upstream_data_[max_data_length];

#ifdef Need_ProxyHandshakeAuth
    std::shared_ptr<ProxyHandshakeAuth> firstPackAnalyzer;
#else
    std::shared_ptr<FirstPackAnalyzer> firstPackAnalyzer;
#endif // Need_ProxyHandshakeAuth
    std::shared_ptr<ConnectionTracker> connectionTracker;

    std::mutex nowServerMtx;
    std::atomic_bool refAdded{false};
    UpstreamServerRef nowServer;

    size_t retryCount = 0;
    const size_t retryLimit;

    bool traditionTcpRelay;
    bool disableConnectionTracker;

    bool isDeCont = false;

    std::shared_ptr<ConfigLoader> configLoader;
public:
    TcpRelaySession(
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
            statisticsInfo(std::move(statisticsInfo)),
            configLoader(std::move(configLoader)),
            authClientManager(std::move(authClientManager)),
            retryLimit(retryLimit),
            traditionTcpRelay(traditionTcpRelay),
            disableConnectionTracker(disableConnectionTracker) {
//        std::cout << "TcpRelaySession create" << std::endl;
    }

    ~TcpRelaySession() {
//        std::cout << "~TcpRelaySession()" << std::endl;
        close();
    }

    boost::asio::ip::tcp::socket &downstream_socket();

    boost::asio::ip::tcp::socket &upstream_socket();

    UpstreamServerRef getNowServer() {
        std::lock_guard<std::mutex> g{nowServerMtx};
        return nowServer;
    }

    std::string getClientEndpointAddrString() {
        return clientEndpointAddrString;
    }

    std::string getListenEndpointAddrString() {
        return listenEndpointAddrString;
    }

    void start();

    void addUp2Statistics(size_t bytes_transferred_);

    void addDown2Statistics(size_t bytes_transferred_);

    std::shared_ptr<ConnectionTracker> getConnectionTracker();

private:

    void try_connect_upstream();

    void
    do_resolve(const std::string &upstream_host, unsigned short upstream_port);

    void
    do_connect_upstream(boost::asio::ip::tcp::resolver::results_type results);



    /*
       Section A: Remote Server --> Proxy --> Client
       Process data recieved from remote sever then send to client.
    */

    // Async read from remote server
    void do_upstream_read();

    // Now send data to client
    void do_downstream_write(const size_t &bytes_transferred);
    // *** End Of Section A ***


    /*
       Section B: Client --> Proxy --> Remove Server
       Process data recieved from client then write to remove server.
    */

    // Async read from client
    void do_downstream_read();

    // Now send data to remote server
    void do_upstream_write(const size_t &bytes_transferred);
    // *** End Of Section B ***

    void close(boost::system::error_code error = {});

public:
    void forceClose();

    void stop();
};

class TcpRelayServer : public std::enable_shared_from_this<TcpRelayServer> {

    boost::asio::any_io_executor ex;
    std::shared_ptr<ConfigLoader> configLoader;
    std::shared_ptr<UpstreamPool> upstreamPool;
    std::shared_ptr<AuthClientManager> authClientManager;
    std::list<boost::asio::ip::tcp::acceptor> socket_acceptors;

    std::list<std::weak_ptr<TcpRelaySession>> sessions;
    std::shared_ptr<TcpRelayStatisticsInfo> statisticsInfo;

    std::shared_ptr<boost::asio::steady_timer> cleanTimer;
    std::shared_ptr<boost::asio::steady_timer> speedCalcTimer;
public:
    TcpRelayServer(
            boost::asio::any_io_executor ex,
            std::shared_ptr<ConfigLoader> configLoader,
            std::shared_ptr<UpstreamPool> upstreamPool,
            std::shared_ptr<AuthClientManager> authClientManager
    ) : ex(ex),
        configLoader(std::move(configLoader)),
        upstreamPool(std::move(upstreamPool)),
        authClientManager(std::move(authClientManager)),
        statisticsInfo(std::make_shared<TcpRelayStatisticsInfo>()) {
    }

    void start();

    void stop();

private:
    void async_accept(boost::asio::ip::tcp::acceptor &sa);

    void removeExpiredSession();

public:
    void closeAllSession();

    std::shared_ptr<TcpRelayStatisticsInfo> getStatisticsInfo();

    void do_cleanTimer();

    void do_speedCalcTimer();

};


#endif //SOCKS5BALANCERASIO_TCPRELAYSERVER_H
