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
#include "UpstreamPool.h"

// code template from https://github.com/ArashPartow/proxy/blob/master/tcpproxy_server.cpp
class TcpRelaySession : public std::enable_shared_from_this<TcpRelaySession> {

    boost::asio::ip::tcp::socket downstream_socket_;
    boost::asio::ip::tcp::socket upstream_socket_;
    boost::asio::ip::tcp::resolver resolver_;
    std::shared_ptr<UpstreamPool> upstreamPool;

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
    TcpRelaySession(boost::asio::executor ex, std::shared_ptr<UpstreamPool> upstreamPool, size_t retryLimit) :
            downstream_socket_(ex),
            upstream_socket_(ex),
            resolver_(ex),
            upstreamPool(std::move(upstreamPool)),
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

};

class TcpRelayServer : public std::enable_shared_from_this<TcpRelayServer> {

    boost::asio::executor ex;
    std::shared_ptr<ConfigLoader> configLoader;
    std::shared_ptr<UpstreamPool> upstreamPool;
    boost::asio::ip::tcp::acceptor socket_acceptor;

public:
    TcpRelayServer(
            boost::asio::executor ex,
            std::shared_ptr<ConfigLoader> configLoader,
            std::shared_ptr<UpstreamPool> upstreamPool
    ) : ex(ex),
        configLoader(std::move(configLoader)),
        upstreamPool(std::move(upstreamPool)),
        socket_acceptor(ex) {
    }

    void start();

    void stop();

private:
    void async_accept();

};


#endif //SOCKS5BALANCERASIO_TCPRELAYSERVER_H
