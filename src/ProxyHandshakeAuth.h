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
#include "./ProxyHandshakeUtils/Socks4ServerImpl.h"
#include "./ProxyHandshakeUtils/ProxyRelayMode.h"

#include "./log/Log.h"

class TcpRelaySession;

class ProxyHandshakeAuth : public std::enable_shared_from_this<ProxyHandshakeAuth> {
public:
    // this is a circle ref, to keep parents valid
    // must reset this ref, after complete or error, to make sure circle break
    std::shared_ptr<TcpRelaySession> tcpRelaySession;

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

    ProxyRelayMode proxyRelayMode = ProxyRelayMode::none;
    // bindHost bytes (4,16,N<256)
    std::string bindHost;
    // bindPort bytes (2)
    std::string bindPort;

    const size_t relayId;

private:

    bool readyUp = false;
    bool readyDown = false;
    bool completeUp = false;
    bool completeDown = false;
    bool completeUpError = false;
    bool completeDownError = false;

    ConnectType connectType = ConnectType::unknown;

    enum class UpsideConnectType {
        socks5,
        socks4,
        http,
        none,
    };
    UpsideConnectType upsideConnectType = UpsideConnectType::none;

public:

    std::string host{};
    uint16_t port{0};

private:
    std::shared_ptr<HttpClientImpl> util_HttpClientImpl_;
    std::shared_ptr<HttpServerImpl> util_HttpServerImpl_;
    std::shared_ptr<Socks5ClientImpl> util_Socks5ClientImpl_;
    std::shared_ptr<Socks5ServerImpl> util_Socks5ServerImpl_;
    std::shared_ptr<Socks4ServerImpl> util_Socks4ServerImpl_;

public:

    ProxyHandshakeAuth(
            std::shared_ptr<TcpRelaySession> tcpRelaySession,
            boost::asio::ip::tcp::socket &downstream_socket_,
            boost::asio::ip::tcp::socket &upstream_socket_,
            std::shared_ptr<ConfigLoader> configLoader,
            std::shared_ptr<AuthClientManager> authClientManager,
            UpstreamServerRef nowServer,
            std::function<void()> whenComplete,
            std::function<void(boost::system::error_code error)> whenError
    );

    ~ProxyHandshakeAuth() {
        BOOST_LOG_S5B_ID(relayId, trace) << "~ProxyHandshakeAuth()";
    }

    void start();

private:

    void do_read_client_first_3_byte();

public:

    ConnectType getConnectType() {
        return connectType;
    }

public:

    bool upside_support_udp_mode();

    bool upside_in_udp_mode();

    bool downside_in_udp_mode();

public:

    void do_whenUpReady();

    void do_whenUpReadyError();

    void do_whenDownReady();

    void do_whenUpEnd();

    void do_whenDownEnd(bool error = false);

private:

    void do_whenCompleteUp();

    void do_whenCompleteDown();

public:

    void do_whenComplete() {
        BOOST_LOG_S5B_ID(relayId, trace) << "ProxyHandshakeAuth::do_whenComplete()";
        BOOST_ASSERT(tcpRelaySession);
        BOOST_ASSERT(whenComplete);
        whenComplete();
        tcpRelaySession.reset();
        whenComplete = nullptr;
        whenError = nullptr;
    }

    void do_whenError(boost::system::error_code error) {
        BOOST_LOG_S5B_ID(relayId, trace) << "ProxyHandshakeAuth::do_whenError()";
        BOOST_ASSERT(tcpRelaySession);
        BOOST_ASSERT(whenError);
        whenError(error);
        tcpRelaySession.reset();
        whenComplete = nullptr;
        whenError = nullptr;
    }

private:
    void check_whenComplete();

public:
    void fail(boost::system::error_code ec, const std::string &what);

    void badParentPtr() {
        if (!(completeDown && completeUp)) {
            do_whenError({});
        }
    }

};


#endif //SOCKS5BALANCERASIO_PROXYHANDSHAKEAUTH_H
