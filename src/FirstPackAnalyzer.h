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
#include <string>
#include <functional>
#include <utility>

class TcpRelaySession;

class FirstPackAnalyzer : public std::enable_shared_from_this<FirstPackAnalyzer> {
public:
    std::weak_ptr<TcpRelaySession> tcpRelaySession;

    boost::asio::streambuf downstream_buf_;
    boost::asio::streambuf upstream_buf_;

    boost::asio::ip::tcp::socket &downstream_socket_;
    boost::asio::ip::tcp::socket &upstream_socket_;

    std::function<void()> whenComplete;
    std::function<void(boost::system::error_code error)> whenError;

    size_t beforeComplete = 2;
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

    void do_prepare_whenComplete();

    void do_whenComplete();

    void do_whenError(boost::system::error_code error);

    void do_prepare_complete_downstream_write();

    void do_prepare_complete_upstream_write();

};


#endif //SOCKS5BALANCERASIO_FIRSTPACKANALYZER_H
