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

#ifndef SOCKS5BALANCERASIO_SOCKS4SERVERIMPL_H
#define SOCKS5BALANCERASIO_SOCKS4SERVERIMPL_H

#ifdef MSVC
#pragma once
#endif

#include <boost/asio/streambuf.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <iostream>
#include <sstream>
#include <string>
#include <functional>
#include <utility>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>

#include "../log/Log.h"

class ProxyHandshakeAuth;


// Socks4 proxy protocol server
// https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/example/cpp11/socks4/socks4.hpp
// https://www.openssh.com/txt/socks4.protocol
// https://zh.wikipedia.org/wiki/SOCKS
class Socks4ServerImpl : public std::enable_shared_from_this<Socks4ServerImpl> {
public:
    std::weak_ptr<ProxyHandshakeAuth> parents;

    bool udpEnabled = false;

    const size_t relayId;
public:
    Socks4ServerImpl(const std::shared_ptr<ProxyHandshakeAuth> &parents_);

    ~Socks4ServerImpl() {
        BOOST_LOG_S5B_ID(relayId, trace) << "~Socks4ServerImpl()";
    }

public:

    void do_analysis_client_first_socks4_header();

    void do_ready_to_send_last_ok_package(const std::shared_ptr<decltype(parents)::element_type> &ptr);

    void do_handshake_client_end_error(uint8_t errorType = 91);

    void do_handshake_client_end();

    void to_send_last_ok_package();

    void to_send_last_error_package();

public:
    void fail(boost::system::error_code ec, const std::string &what);

    void do_whenError(boost::system::error_code error);

    void badParentPtr() {
        // parents lost, simple ignore it, and stop run
    }

};


#endif //SOCKS5BALANCERASIO_SOCKS4SERVERIMPL_H
