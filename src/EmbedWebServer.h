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

#ifndef SOCKS5BALANCERASIO_EMBEDWEBSERVER_H
#define SOCKS5BALANCERASIO_EMBEDWEBSERVER_H

#ifdef MSVC
#pragma once
#endif

// https://www.boost.org/doc/libs/develop/libs/beast/example/http/server/async/http_server_async.cpp

#include <boost/asio/executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/property_tree/ptree.hpp>
#include <memory>
#include <string>
#include <iostream>
#include <sstream>

// Handles an HTTP server connection
class EmbedWebServerSession : public std::enable_shared_from_this<EmbedWebServerSession> {
    // This is the C++11 equivalent of a generic lambda.
    // The function object is used to send an HTTP message.
    struct send_lambda {
        EmbedWebServerSession &self_;

        explicit
        send_lambda(EmbedWebServerSession &self)
                : self_(self) {
        }

        template<bool isRequest, class Body, class Fields>
        void
        operator()(boost::beast::http::message<isRequest, Body, Fields> &&msg) const;
    };

    boost::beast::tcp_stream stream_;
    boost::beast::flat_buffer buffer_;
    std::shared_ptr<std::string const> doc_root_;
    std::shared_ptr<std::string const> index_file_of_root;
    std::shared_ptr<std::string const> backend_json_string;
    std::vector<std::string> allowFileExtList;
    boost::beast::http::request<boost::beast::http::string_body> req_;
    std::shared_ptr<void> res_;
    send_lambda lambda_;

public:
    // Take ownership of the stream
    EmbedWebServerSession(
            boost::asio::ip::tcp::socket &&socket,
            std::shared_ptr<std::string const> const &doc_root,
            std::shared_ptr<std::string const> const &index_file_of_root,
            std::shared_ptr<std::string const> const &backend_json_string,
            std::vector<std::string> const &allowFileExtList)
            : stream_(std::move(socket)),
              doc_root_(doc_root),
              index_file_of_root(index_file_of_root),
              backend_json_string(backend_json_string),
              allowFileExtList(allowFileExtList),
              lambda_(*this) {
    }

    // Start the asynchronous operation
    void
    run();

    void
    do_read();

    void
    on_read(
            boost::beast::error_code ec,
            std::size_t bytes_transferred);

    void
    on_write(
            bool close,
            boost::beast::error_code ec,
            std::size_t bytes_transferred);

    void
    do_close();
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class EmbedWebServer : public std::enable_shared_from_this<EmbedWebServer> {
    boost::asio::io_context &ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<std::string const> doc_root_;
    std::shared_ptr<std::string const> index_file_of_root;
    std::shared_ptr<std::string const> backend_json_string;
    std::vector<std::string> allowFileExtList;

public:
    EmbedWebServer(
            boost::asio::io_context &ioc,
            boost::asio::ip::tcp::endpoint endpoint,
            std::shared_ptr<std::string const> const &doc_root,
            std::shared_ptr<std::string const> const &index_file_of_root,
            std::shared_ptr<std::string const> const &backend_json_string,
            std::shared_ptr<std::string const> const &allowFileExtList
    );

    // Start accepting incoming connections
    void
    start() {
        do_accept();
    }

private:
    void
    do_accept();

    void
    on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);
};


#endif //SOCKS5BALANCERASIO_EMBEDWEBSERVER_H
