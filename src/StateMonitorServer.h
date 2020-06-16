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

#ifndef SOCKS5BALANCERASIO_STATEMONITORSERVER_H
#define SOCKS5BALANCERASIO_STATEMONITORSERVER_H

#ifdef MSVC
#pragma once
#endif

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/property_tree/ptree.hpp>
#include <memory>
#include <string>
#include <sstream>
#include "ConfigLoader.h"
#include "UpstreamPool.h"

// https://www.boost.org/doc/libs/1_73_0/libs/beast/example/http/server/small/http_server_small.cpp

class HttpConnectSession : public std::enable_shared_from_this<HttpConnectSession> {
    std::shared_ptr<ConfigLoader> configLoader;
    std::shared_ptr<UpstreamPool> upstreamPool;

    UpstreamTimePoint startTime;
public:
    HttpConnectSession(boost::asio::ip::tcp::socket socket,
                       std::shared_ptr<ConfigLoader> configLoader,
                       std::shared_ptr<UpstreamPool> upstreamPool)
            : configLoader(configLoader),
              upstreamPool(upstreamPool),
              socket_(std::move(socket)),
              startTime(UpstreamTimePointNow()) {}

    void start() {
        read_request();
        check_deadline();
    }

protected:
    std::string createJsonString();

protected:
    // The socket for the currently connected client.
    boost::asio::ip::tcp::socket socket_;

    // The buffer for performing reads.
    boost::beast::flat_buffer buffer_{8192};

    // The request message.
    boost::beast::http::request<boost::beast::http::dynamic_body> request_;

    // The response message.
    boost::beast::http::response<boost::beast::http::dynamic_body> response_;

    // The timer for putting a deadline on connection processing.
    boost::asio::steady_timer deadline_{socket_.get_executor(), std::chrono::seconds(60)};

protected:
    // Asynchronously receive a complete request message.
    void read_request();

    // Determine what needs to be done with the request message.
    void process_request();

    // Construct a response message based on the program state.
    void create_response();

    // Asynchronously transmit the response message.
    void write_response();

    // Check whether we have spent enough time on this connection.
    void check_deadline();
};

class StateMonitorServer : public std::enable_shared_from_this<StateMonitorServer> {
    boost::asio::executor ex;
    std::shared_ptr<ConfigLoader> configLoader;
    std::shared_ptr<UpstreamPool> upstreamPool;

public:
    StateMonitorServer(
            boost::asio::executor ex,
            std::shared_ptr<ConfigLoader> configLoader,
            std::shared_ptr<UpstreamPool> upstreamPool
    ) :
            ex(ex),
            configLoader(configLoader),
            upstreamPool(upstreamPool),
            address(boost::asio::ip::make_address(configLoader->config.stateServerHost)),
            port(configLoader->config.stateServerPort),
            acceptor(ex, {address, port}),
            socket(ex) {
    }

protected:
    const boost::asio::ip::address address;
    unsigned short port;
    boost::asio::ip::tcp::acceptor acceptor;
    boost::asio::ip::tcp::socket socket;

public:
    void start() {
        std::cout << "StateMonitorServer start on:" << address.to_string() << ":" << port << std::endl;
        http_server();
    }

private:
    // "Loop" forever accepting new connections.
    void http_server();
};


#endif //SOCKS5BALANCERASIO_STATEMONITORSERVER_H
