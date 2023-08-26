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

#ifndef SOCKS5BALANCERASIO_CONNECTTESTHTTPS_H
#define SOCKS5BALANCERASIO_CONNECTTESTHTTPS_H

#ifdef MSVC
#pragma once
#endif

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <memory>
#include <utility>
#include <iostream>
#include <string>
#include <sstream>
#include <list>
#include <vector>
#include <functional>
#include <openssl/opensslv.h>

#include "AsyncDelay.h"

#ifdef _WIN32

#include <wincrypt.h>
#include <tchar.h>

#endif // _WIN32

class ConnectTestHttpsSession : public std::enable_shared_from_this<ConnectTestHttpsSession> {
    boost::asio::any_io_executor executor;

    boost::asio::ip::tcp::resolver resolver_;
    boost::beast::ssl_stream<boost::beast::tcp_stream> stream_;
    boost::beast::flat_buffer buffer_; // (Must persist between reads)
    boost::beast::http::request<boost::beast::http::empty_body> req_;
    boost::beast::http::response<boost::beast::http::string_body> res_;


    const std::string targetHost;
    uint16_t targetPort;
    const std::string targetPath;
    int httpVersion;
    const std::string socks5Host;
    const std::string socks5Port;
    const std::string socks5AuthUser;
    const std::string socks5AuthPwd;
    bool slowImpl;

    std::chrono::milliseconds delayTime;

    enum {
        MAX_LENGTH = 8192
    };

    bool _isComplete = false;

    std::chrono::milliseconds timePing{-1};
    std::chrono::time_point<std::chrono::steady_clock> startTime{std::chrono::steady_clock::now()};

public:
    ConnectTestHttpsSession(
            boost::asio::any_io_executor executor,
            const std::shared_ptr<boost::asio::ssl::context> &ssl_context,
            const std::string &targetHost,
            uint16_t targetPort,
            const std::string &targetPath,
            int httpVersion,
            const std::string &socks5Host,
            const std::string &socks5Port,
            const std::string &socks5AuthUser,
            const std::string &socks5AuthPwd,
            bool slowImpl,
            std::chrono::milliseconds delayTime = std::chrono::milliseconds{0}
    );

    ~ConnectTestHttpsSession() {
//        std::cout << "~ConnectTestHttpsSession()" << std::endl;
    }

    bool isComplete();

    using SuccessfulInfo = boost::beast::http::response<boost::beast::http::string_body>;

    struct CallbackContainer {
        std::function<void(std::chrono::milliseconds ping, SuccessfulInfo info)> successfulCallback;
        std::function<void(std::string reason)> failedCallback;
    };
    std::unique_ptr<CallbackContainer> callback;

    void run(std::function<void(std::chrono::milliseconds ping, SuccessfulInfo info)> onOk,
             std::function<void(std::string reason)> onErr);

    // to avoid circle ref
    void release();

    void stop();

private:

    void fail(boost::system::error_code ec, const std::string &what);

    void allOk();

    void do_resolve();

    void do_tcp_connect(const boost::asio::ip::tcp::resolver::results_type &results);

    void do_socks5_handshake_write();

    void do_socks5_handshake_read();

    void do_socks5_auth_write();

    void do_socks5_auth_read();

    void do_socks5_connect_write();

    void do_socks5_connect_read();

    void do_ssl_handshake();

    void do_write();

    void do_read();

    void do_shutdown(bool isOn = false);

};

class ConnectTestHttps : public std::enable_shared_from_this<ConnectTestHttps> {
    boost::asio::any_io_executor executor;
    std::shared_ptr<boost::asio::ssl::context> ssl_context;
    bool need_verify_ssl = true;
    std::list<std::shared_ptr<ConnectTestHttpsSession>> sessions;

    std::shared_ptr<boost::asio::steady_timer> cleanTimer;
public:
    ConnectTestHttps(boost::asio::any_io_executor ex);

    std::shared_ptr<ConnectTestHttpsSession> createTest(
            const std::string &socks5Host,
            const std::string &socks5Port,
            const std::string &socks5AuthUser,
            const std::string &socks5AuthPwd,
            bool slowImpl,
            const std::string &targetHost,
            uint16_t targetPort,
            const std::string &targetPath,
            int httpVersion = 11,
            std::chrono::milliseconds maxRandomDelay = std::chrono::milliseconds{0}
    );

    void stop();

private:
    void do_cleanTimer();
};


#endif //SOCKS5BALANCERASIO_CONNECTTESTHTTPS_H
