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

public:
    TcpRelaySession(boost::asio::executor ex, std::shared_ptr<UpstreamPool> upstreamPool) :
            downstream_socket_(ex),
            upstream_socket_(ex),
            resolver_(ex),
            upstreamPool(std::move(upstreamPool)) {
        std::cout << "TcpRelaySession create" << std::endl;
    }

    boost::asio::ip::tcp::socket &downstream_socket() {
        // Client socket
        return downstream_socket_;
    }

    boost::asio::ip::tcp::socket &upstream_socket() {
        // Remote server socket
        return upstream_socket_;
    }


    void start() {
        std::cout << "TcpRelaySession start()" << std::endl;
        try_connect_upstream();
    }

private:

    void try_connect_upstream() {
        auto s = upstreamPool->getServerBasedOnAddress();
        std::cout << "TcpRelaySession try_connect_upstream()"
                  << " " << s->host << ":" << s->port << std::endl;
        do_resolve(s->host, s->port);
    }

    void
    do_resolve(const std::string &upstream_host, unsigned short upstream_port) {

        // Look up the domain name
        resolver_.async_resolve(
                upstream_host,
                std::to_string(upstream_port),
                [this, self = shared_from_this()](
                        const boost::system::error_code &error,
                        boost::asio::ip::tcp::resolver::results_type results) {
                    if (error) {
                        std::cerr << "do_resolve error:" << error.message() << "\n";
                        // try next
                        try_connect_upstream();
                    }

                    std::cout << "TcpRelaySession do_resolve()"
                              << " " << results->endpoint().address() << ":" << results->endpoint().port()
                              << std::endl;

                    do_connect_upstream(results);
                });

    }

    void
    do_connect_upstream(boost::asio::ip::tcp::resolver::results_type results) {

        // Attempt connection to remote server (upstream side)
        upstream_socket_.async_connect(
                results->endpoint(),
                [this, self = shared_from_this()](const boost::system::error_code &error) {
                    if (!error) {

                        std::cout << "TcpRelaySession do_connect_upstream()" << std::endl;

                        // Setup async read from remote server (upstream)
                        do_upstream_read();

                        // Setup async read from client (downstream)
                        do_downstream_read();

                    } else {
                        std::cerr << "do_connect_upstream error:" << error.message() << "\n";
                        // try next
                        try_connect_upstream();
                    }
                }
        );

    }



    /*
       Section A: Remote Server --> Proxy --> Client
       Process data recieved from remote sever then send to client.
    */

    // Async read from remote server
    void do_upstream_read() {
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

    // Now send data to client
    void do_downstream_write(const size_t &bytes_transferred) {
        boost::asio::async_write(
                downstream_socket_,
                boost::asio::buffer(upstream_data_, bytes_transferred),
                [this, self = shared_from_this(), bytes_transferred](
                        const boost::system::error_code &error,
                        std::size_t bytes_transferred_) {
                    if (!error || bytes_transferred_ != bytes_transferred) {
                        // Write to client complete
                        // read more again
                        do_upstream_read();
                    } else {
                        close(error);
                    }
                });
    }
    // *** End Of Section A ***


    /*
       Section B: Client --> Proxy --> Remove Server
       Process data recieved from client then write to remove server.
    */

    // Async read from client
    void do_downstream_read() {
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

    // Now send data to remote server
    void do_upstream_write(const size_t &bytes_transferred) {
        boost::asio::async_write(
                upstream_socket_,
                boost::asio::buffer(downstream_data_, bytes_transferred),
                [this, self = shared_from_this(), bytes_transferred](
                        const boost::system::error_code &error,
                        std::size_t bytes_transferred_) {
                    if (!error || bytes_transferred_ != bytes_transferred) {
                        // Write to remote server complete
                        // read more again
                        do_downstream_read();
                    } else {
                        close(error);
                    }
                });
    }
    // *** End Of Section B ***

    void close(boost::system::error_code error = {}) {
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
    }

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

    void start() {
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

    void stop() {
        boost::system::error_code ec;
        socket_acceptor.cancel(ec);
    }

private:
    void async_accept() {
        auto session = std::make_shared<TcpRelaySession>(ex, upstreamPool);
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

                            session->start();
                        }
                    }
                    std::cout << "async_accept next." << "\n";
                    async_accept();
                });
    }

};


#endif //SOCKS5BALANCERASIO_TCPRELAYSERVER_H
