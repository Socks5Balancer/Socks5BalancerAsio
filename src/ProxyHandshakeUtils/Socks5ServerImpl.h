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

#ifndef SOCKS5BALANCERASIO_SOCKS5SERVERIMPL_H
#define SOCKS5BALANCERASIO_SOCKS5SERVERIMPL_H

#ifdef MSVC
#pragma once
#endif

#include <boost/asio/streambuf.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <iostream>
#include <string>
#include <functional>
#include <utility>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>

class ProxyHandshakeAuth;

// Socks5 proxy protocol server
class Socks5ServerImpl : public std::enable_shared_from_this<Socks5ServerImpl> {
public:
    std::weak_ptr<ProxyHandshakeAuth> parents;

public:
    Socks5ServerImpl(const std::shared_ptr<ProxyHandshakeAuth>& parents_) : parents(parents_) {}
};


#endif //SOCKS5BALANCERASIO_SOCKS5SERVERIMPL_H
