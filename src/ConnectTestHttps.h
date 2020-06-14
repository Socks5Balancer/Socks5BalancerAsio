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

#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include <utility>
#include <iostream>
#include <string>
#include <sstream>
#include <list>
#include <vector>
#include <functional>
#include <openssl/opensslv.h>

#ifdef _WIN32

#include <wincrypt.h>
#include <tchar.h>

#endif // _WIN32

class ConnectTestHttpsSession : public std::enable_shared_from_this<ConnectTestHttpsSession> {
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

    enum {
        MAX_LENGTH = 8192
    };

    bool _isComplete = false;

public:
    ConnectTestHttpsSession(
            boost::asio::executor executor,
            const std::shared_ptr<boost::asio::ssl::context> &ssl_context,
            const std::string &targetHost,
            int targetPort,
            const std::string &targetPath,
            int httpVersion,
            const std::string &socks5Host,
            const std::string &socks5Port
    );

    ~ConnectTestHttpsSession() {
//        std::cout << "~ConnectTestHttpsSession()" << std::endl;
    }

    bool isComplete();

    using SuccessfulInfo = boost::beast::http::response<boost::beast::http::string_body>;

    struct CallbackContainer {
        std::function<void(SuccessfulInfo info)> successfulCallback;
        std::function<void(std::string reason)> failedCallback;
    };
    std::unique_ptr<CallbackContainer> callback;

    void run(std::function<void(SuccessfulInfo info)> onOk, std::function<void(std::string reason)> onErr);

    // to avoid circle ref
    void release();

private:

    void
    fail(boost::system::error_code ec, const std::string &what);

    void
    allOk();

    void
    do_resolve();

    void
    do_tcp_connect(const boost::asio::ip::tcp::resolver::results_type &results);

    void
    do_socks5_handshake_write();

    void
    do_socks5_handshake_read();

    void
    do_socks5_connect_write();

    void
    do_socks5_connect_read();

    void
    do_ssl_handshake();

    void
    do_write();

    void
    do_read();

    void
    do_shutdown(bool isOn = false);

};

class ConnectTestHttps : public std::enable_shared_from_this<ConnectTestHttps> {
    boost::asio::executor executor;
    std::shared_ptr<boost::asio::ssl::context> ssl_context;
    bool need_verify_ssl = true;
    std::list<std::shared_ptr<ConnectTestHttpsSession>> sessions;

    boost::asio::steady_timer cleanTimer;
public:
    ConnectTestHttps(boost::asio::executor ex);

    std::shared_ptr<ConnectTestHttpsSession> &&createTest(
            const std::string &socks5Host,
            const std::string &socks5Port,
            const std::string &targetHost,
            int targetPort,
            const std::string &targetPath,
            int httpVersion = 11
    );

private:
    void do_cleanTimer();
};


#endif //SOCKS5BALANCERASIO_CONNECTTESTHTTPS_H
