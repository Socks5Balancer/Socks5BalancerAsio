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

#ifndef SOCKS5BALANCERASIO_PROXYHANDSHAKEAUTH_H
#define SOCKS5BALANCERASIO_PROXYHANDSHAKEAUTH_H

#ifdef MSVC
#pragma once
#endif

#include <boost/assert.hpp>

#include <boost/asio/streambuf.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <iostream>
#include <string>
#include <functional>
#include <utility>
#include "ConnectType.h"

#include "ConfigLoader.h"
#include "UpstreamPool.h"
#include "AuthClientManager.h"

#include <regex>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>

#include "./ProxyHandshakeUtils/HttpServerImpl.h"
#include "./ProxyHandshakeUtils/HttpClientImpl.h"
#include "./ProxyHandshakeUtils/Socks5ServerImpl.h"
#include "./ProxyHandshakeUtils/Socks5ClientImpl.h"


class TcpRelaySession;

class ProxyHandshakeAuth : public std::enable_shared_from_this<ProxyHandshakeAuth> {
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

    std::shared_ptr<ConfigLoader> configLoader;
    std::shared_ptr<AuthClientManager> authClientManager;
    UpstreamServerRef nowServer;

    std::function<void()> whenComplete;
    std::function<void(boost::system::error_code error)> whenError;

    int beforeCompleteUp = 1;
    int beforeCompleteDown = 1;

    // Useless
    ConnectType connectType = ConnectType::unknown;

    std::string host{};
    uint16_t port{0};
public:
    std::shared_ptr<HttpClientImpl> util_HttpClientImpl_;
    std::shared_ptr<HttpServerImpl> util_HttpServerImpl_;
    std::shared_ptr<Socks5ClientImpl> util_Socks5ClientImpl_;
    std::shared_ptr<Socks5ServerImpl> util_Socks5ServerImpl_;

public:

    ProxyHandshakeAuth(
            std::weak_ptr<TcpRelaySession> tcpRelaySession,
            boost::asio::ip::tcp::socket &downstream_socket_,
            boost::asio::ip::tcp::socket &upstream_socket_,
            std::shared_ptr<ConfigLoader> configLoader,
            std::shared_ptr<AuthClientManager> authClientManager,
            UpstreamServerRef nowServer,
            std::function<void()> whenComplete,
            std::function<void(boost::system::error_code error)> whenError
    ) :
            tcpRelaySession(std::move(tcpRelaySession)),
            downstream_socket_(downstream_socket_),
            upstream_socket_(upstream_socket_),
            configLoader(std::move(configLoader)),
            authClientManager(std::move(authClientManager)),
            nowServer(std::move(nowServer)),
            whenComplete(std::move(whenComplete)),
            whenError(std::move(whenError)) {}

    void start() {
        util_HttpClientImpl_ = std::make_shared<decltype(util_HttpClientImpl_)::element_type>(shared_from_this());
        util_HttpServerImpl_ = std::make_shared<decltype(util_HttpServerImpl_)::element_type>(shared_from_this());
        util_Socks5ClientImpl_ = std::make_shared<decltype(util_Socks5ClientImpl_)::element_type>(shared_from_this());
        util_Socks5ServerImpl_ = std::make_shared<decltype(util_Socks5ServerImpl_)::element_type>(shared_from_this());

        // we must keep strong ptr when running, until call the whenComplete,
        // to keep parent TcpRelaySession but dont make circle ref
        auto ptr = tcpRelaySession.lock();
        if (ptr) {
            do_read_client_first_3_byte();
            // debug
//        do_prepare_whenComplete();
        } else {
            badParentPtr();
        }
    }

private:

    void do_read_client_first_3_byte();

public:

    void do_whenUpReady() {
        // do send last ok package to client in downside
        util_HttpServerImpl_->to_send_last_ok_package();
        //
        do_whenCompleteUp();
    }

    void do_whenCompleteUp() {
        --beforeCompleteUp;
        if (beforeCompleteUp < 0) {
            // never go there
            BOOST_ASSERT_MSG(!(beforeCompleteUp < 0), "do_whenCompleteUp (beforeCompleteUp < 0)");
        }
        check_whenComplete();
    }

    void do_whenDownReady() {
        // downside ready for send last ok package
        if (beforeCompleteDown == 0 && beforeCompleteUp > 0) {
            // start upside handshake
            util_Socks5ClientImpl_->start();
        } else {
            // never go there
            BOOST_ASSERT_MSG(false, "do_whenDownReady");
        }
    }

    void do_whenCompleteDown() {
        --beforeCompleteDown;
        if (beforeCompleteDown < 0) {
            // never go there
            BOOST_ASSERT_MSG(!(beforeCompleteDown < 0), "do_whenCompleteDown (beforeCompleteDown < 0)");
        }
        check_whenComplete();
    }

    void do_whenError(boost::system::error_code error) {
        whenError(error);
    }

private:
    void check_whenComplete() {
        if (0 == beforeCompleteUp && 0 == beforeCompleteDown) {
            auto ptr = tcpRelaySession.lock();
            if (ptr) {
                // all ok
                whenComplete();
            }
        }
    }

public:
    void fail(boost::system::error_code ec, const std::string &what) {
        std::string r;
        {
            std::stringstream ss;
            ss << what << ": [" << ec.message() << "] . ";
            r = ss.str();
        }
        std::cerr << r << std::endl;

        do_whenError(ec);
    }

    void badParentPtr() {
        if (!(0 == beforeCompleteUp && 0 == beforeCompleteDown)) {
            do_whenError({});
        }
    }

};


#endif //SOCKS5BALANCERASIO_PROXYHANDSHAKEAUTH_H
