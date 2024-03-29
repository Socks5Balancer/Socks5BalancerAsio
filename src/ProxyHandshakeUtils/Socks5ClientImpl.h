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

#ifndef SOCKS5BALANCERASIO_SOCKS5CLIENTIMPL_H
#define SOCKS5BALANCERASIO_SOCKS5CLIENTIMPL_H

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

// Socks5 proxy protocol client
class Socks5ClientImpl : public std::enable_shared_from_this<Socks5ClientImpl> {
public:
    std::weak_ptr<ProxyHandshakeAuth> parents;

    bool udpEnabled = false;

    const size_t relayId;
public:
    Socks5ClientImpl(const std::shared_ptr<ProxyHandshakeAuth> &parents_);

    ~Socks5ClientImpl() {
        BOOST_LOG_S5B_ID(relayId, trace) << "~Socks5ClientImpl()";
    }

    void start();

public:

    void do_socks5_handshake_write();

    void do_socks5_handshake_read();

    void do_socks5_auth_write();

    void do_socks5_auth_read();

    void do_socks5_connect_write();

    void do_socks5_connect_read();

public:
    void fail(boost::system::error_code ec, const std::string &what);

    void do_whenError(boost::system::error_code error);

    void badParentPtr() {
        // parents lost, simple ignore it, and stop run
    }

};


#endif //SOCKS5BALANCERASIO_SOCKS5CLIENTIMPL_H
