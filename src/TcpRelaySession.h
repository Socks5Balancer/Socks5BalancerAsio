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

#ifndef SOCKS5BALANCERASIO_TCPRELAYSESSION_H
#define SOCKS5BALANCERASIO_TCPRELAYSESSION_H

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
#include "DelayCollection.h"
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
    // client send "ip"
    std::string clientEndpointAddrString;
    // client send "ip:port"
    std::string clientEndpointAddrPortString;
    boost::asio::ip::tcp::endpoint listenEndpoint;
    // listen "ip:port"
    std::string listenEndpointAddrString;
    // proxy target "<ip/domain>[:port]"
    // the client want to connect to target though proxy
    std::string targetEndpointAddrString;

    enum {
        max_data_length = 8192
    }; //8KB
    std::array<unsigned char, max_data_length> downstream_data_;
    std::array<unsigned char, max_data_length> upstream_data_;
//    unsigned char downstream_data_[max_data_length];
//    unsigned char upstream_data_[max_data_length];

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

    std::atomic_int64_t firstDelayTimestamp{-1};
    std::atomic_bool firstDelayUpEnd{false};
    std::atomic_bool firstDelayDownEnd{false};

public:

    std::shared_ptr<AuthClientManager::AuthUser> authUser{};

    const size_t relayId;

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
    );

    ~TcpRelaySession() {
        BOOST_LOG_S5B_ID(relayId, trace) << "~TcpRelaySession()";
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

    std::string getClientEndpointAddrPortString() {
        return clientEndpointAddrPortString;
    }

    std::string getListenEndpointAddrString() {
        return listenEndpointAddrString;
    }

    std::string getTargetEndpointAddrString() {
        return targetEndpointAddrString;
    }

    std::pair<std::string, uint16_t> getTargetEndpointAddr() {
        auto p = firstPackAnalyzer;
        if (p) {
            return {p->host, p->port};
        } else {
            return {};
        }
    }

    void start();

    void addUp2Statistics(size_t bytes_transferred_);

    void addDown2Statistics(size_t bytes_transferred_);

    std::shared_ptr<ConnectionTracker> getConnectionTracker();

private:

    void try_connect_upstream();

    void do_resolve(const std::string &upstream_host, unsigned short upstream_port);

    void do_connect_upstream(boost::asio::ip::tcp::resolver::results_type results);



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


#endif //SOCKS5BALANCERASIO_TCPRELAYSESSION_H
