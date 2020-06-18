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

#ifndef SOCKS5BALANCERASIO_FIRSTPACKANALYZER_H
#define SOCKS5BALANCERASIO_FIRSTPACKANALYZER_H

#ifdef MSVC
#pragma once
#endif

#include <boost/asio.hpp>
#include <memory>
#include <iostream>
#include <string>
#include <functional>
#include <utility>

// see https://imququ.com/post/web-proxy.html

class TcpRelaySession;

struct ParsedURI {
    std::string protocol;
    std::string domain;  // only domain must be present
    std::string port;
    std::string resource;
    std::string query;   // everything after '?', possibly nothing
};

class FirstPackAnalyzer : public std::enable_shared_from_this<FirstPackAnalyzer> {
public:
    std::weak_ptr<TcpRelaySession> tcpRelaySession;

    // Client --> Proxy --> Remove Server
    boost::asio::streambuf downstream_buf_;
    // Remote Server --> Proxy --> Client
    boost::asio::streambuf upstream_buf_;

    // Client
    boost::asio::ip::tcp::socket &downstream_socket_;
    // Remote Server
    boost::asio::ip::tcp::socket &upstream_socket_;

    std::function<void()> whenComplete;
    std::function<void(boost::system::error_code error)> whenError;

    size_t beforeComplete = 2;

    bool isConnect = false;

    std::string host;
    uint16_t port;
public:
    FirstPackAnalyzer(
            std::weak_ptr<TcpRelaySession> tcpRelaySession,
            boost::asio::ip::tcp::socket &downstream_socket_,
            boost::asio::ip::tcp::socket &upstream_socket_,
            std::function<void()> whenComplete,
            std::function<void(boost::system::error_code error)> whenError
    ) :
            tcpRelaySession(std::move(tcpRelaySession)),
            downstream_socket_(downstream_socket_),
            upstream_socket_(upstream_socket_),
            whenComplete(std::move(whenComplete)),
            whenError(std::move(whenError)) {}

    void start();

    ParsedURI parseURI(const std::string &url);

private:

    void do_read_client_first_3_byte();

    void do_read_client_first_http_header();

    void do_analysis_client_first_http_header();

    void do_prepare_whenComplete();

    void do_whenComplete();

    void do_whenError(boost::system::error_code error);

    void do_prepare_complete_downstream_write();

    void do_prepare_complete_upstream_write();

private:
    void do_socks5_handshake_write();

    void do_socks5_handshake_read();

    void do_socks5_connect_write();

    void do_socks5_connect_read();

    void do_send_Connection_Established();

private:
    void fail(boost::system::error_code ec, const std::string &what);
};


#endif //SOCKS5BALANCERASIO_FIRSTPACKANALYZER_H
