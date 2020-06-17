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

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <memory>
#include <string>
#include <utility>
#include <list>
#include <map>
#include <atomic>
#include "UpstreamPool.h"

class TcpRelaySession;

class TcpRelayStatisticsInfo : public std::enable_shared_from_this<TcpRelayStatisticsInfo> {
public:
    struct Info : public std::enable_shared_from_this<Info> {
        std::list<std::weak_ptr<TcpRelaySession>> sessions;

        std::atomic_size_t byteUp = 0;
        std::atomic_size_t byteDown = 0;
        size_t byteUpLast = 0;
        size_t byteDownLast = 0;
        size_t byteUpChange = 0;
        size_t byteDownChange = 0;
        size_t byteUpChangeMax = 0;
        size_t byteDownChangeMax = 0;

        std::atomic_size_t connectCount{0};

        void removeExpiredSession();

        void closeAllSession();

        void calcByte();

        void connectCountAdd();

        void connectCountSub();
    };

private:
    std::map<size_t, std::shared_ptr<Info>> upstreamIndex;
    std::map<std::string, std::shared_ptr<Info>> clientIndex;

public:
    std::atomic_size_t lastConnectServerIndex{0};

public:
    std::map<size_t, std::shared_ptr<Info>> &getUpstreamIndex();

    std::map<std::string, std::shared_ptr<Info>> &getClientIndex();

public:
    void addSession(size_t index, std::weak_ptr<TcpRelaySession> s);

    void addSession(std::string addr, std::weak_ptr<TcpRelaySession> s);

    std::shared_ptr<Info> getInfo(size_t index);

    std::shared_ptr<Info> getInfo(std::string addr);

    void removeExpiredSession(size_t index);

    void removeExpiredSession(std::string addr);

    void addByteUp(size_t index, size_t b);

    void addByteUp(std::string addr, size_t b);

    void addByteDown(size_t index, size_t b);

    void addByteDown(std::string addr, size_t b);

    void connectCountAdd(size_t index);

    void connectCountAdd(std::string addr);

    void connectCountSub(size_t index);

    void connectCountSub(std::string addr);

    void calcByteAll();

    void removeExpiredSessionAll();

    void closeAllSession(size_t index);

    void closeAllSession(std::string addr);

};

// code template from https://github.com/ArashPartow/proxy/blob/master/tcpproxy_server.cpp
class TcpRelaySession : public std::enable_shared_from_this<TcpRelaySession> {

    boost::asio::executor ex;

    boost::asio::ip::tcp::socket downstream_socket_;
    boost::asio::ip::tcp::socket upstream_socket_;
    boost::asio::ip::tcp::resolver resolver_;
    std::shared_ptr<UpstreamPool> upstreamPool;

    std::weak_ptr<TcpRelayStatisticsInfo> statisticsInfo;

    boost::asio::ip::tcp::endpoint endpoint;
    std::string clientEndpointAddrString;

    enum {
        max_data_length = 8192
    }; //8KB
    unsigned char downstream_data_[max_data_length];
    unsigned char upstream_data_[max_data_length];

    UpstreamServerRef nowServer;

    size_t retryCount = 0;
    const size_t retryLimit;

    bool isDeCont = false;
public:
    TcpRelaySession(
            boost::asio::executor ex,
            std::shared_ptr<UpstreamPool> upstreamPool,
            std::weak_ptr<TcpRelayStatisticsInfo> statisticsInfo,
            size_t retryLimit
    ) :
            ex(ex),
            downstream_socket_(ex),
            upstream_socket_(ex),
            resolver_(ex),
            upstreamPool(std::move(upstreamPool)),
            statisticsInfo(statisticsInfo),
            retryLimit(retryLimit) {
        std::cout << "TcpRelaySession create" << std::endl;
    }

    boost::asio::ip::tcp::socket &downstream_socket();

    boost::asio::ip::tcp::socket &upstream_socket();


    void start();

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
};

class TcpRelayServer : public std::enable_shared_from_this<TcpRelayServer> {

    boost::asio::executor ex;
    std::shared_ptr<ConfigLoader> configLoader;
    std::shared_ptr<UpstreamPool> upstreamPool;
    boost::asio::ip::tcp::acceptor socket_acceptor;

    std::list<std::weak_ptr<TcpRelaySession>> sessions;
    std::shared_ptr<TcpRelayStatisticsInfo> statisticsInfo;

    std::shared_ptr<boost::asio::steady_timer> cleanTimer;
    std::shared_ptr<boost::asio::steady_timer> speedCalcTimer;
public:
    TcpRelayServer(
            boost::asio::executor ex,
            std::shared_ptr<ConfigLoader> configLoader,
            std::shared_ptr<UpstreamPool> upstreamPool
    ) : ex(ex),
        configLoader(std::move(configLoader)),
        upstreamPool(std::move(upstreamPool)),
        socket_acceptor(ex),
        statisticsInfo(std::make_shared<TcpRelayStatisticsInfo>()) {
    }

    void start();

    void stop();

private:
    void async_accept();

    void removeExpiredSession();

public:
    void closeAllSession();

    std::shared_ptr<TcpRelayStatisticsInfo> getStatisticsInfo();

    void do_cleanTimer();

    void do_speedCalcTimer();

};


#endif //SOCKS5BALANCERASIO_TCPRELAYSERVER_H
