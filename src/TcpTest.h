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

#ifndef SOCKS5BALANCERASIO_TCPTEST_H
#define SOCKS5BALANCERASIO_TCPTEST_H

#ifdef MSVC
#pragma once
#endif

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <utility>
#include <iostream>
#include <string>
#include <sstream>
#include <list>
#include <vector>
#include <functional>


class TcpTestSession : public std::enable_shared_from_this<TcpTestSession> {
    boost::asio::ip::tcp::resolver resolver_;
    boost::beast::tcp_stream stream_;

    const std::string socks5Host;
    const std::string socks5Port;

    bool _isComplete = false;

public:
    TcpTestSession(boost::asio::executor executor,
                   const std::string &socks5Host,
                   const std::string &socks5Port
    ) :
            resolver_(executor),
            stream_(executor),
            socks5Host(socks5Host),
            socks5Port(socks5Port) {
//        std::cout << "TcpTestSession :" << socks5Host << ":" << socks5Port << std::endl;
    }

    ~TcpTestSession() {
//        std::cout << "~TcpTestSession()" << std::endl;
    }

    bool isComplete();

    struct CallbackContainer {
        std::function<void()> successfulCallback;
        std::function<void(std::string reason)> failedCallback;
    };
    std::unique_ptr<CallbackContainer> callback;

    void run(std::function<void()> onOk, std::function<void(std::string reason)> onErr);

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
    do_shutdown(bool isOn = false);

};

class TcpTest : public std::enable_shared_from_this<TcpTest> {
    boost::asio::executor executor;
    std::list<std::shared_ptr<TcpTestSession>> sessions;

    std::shared_ptr<boost::asio::steady_timer> cleanTimer;
public:
    TcpTest(boost::asio::executor ex) :
            executor(ex) {
    }

    std::shared_ptr<TcpTestSession> createTest(
            const std::string socks5Host,
            const std::string socks5Port
    );

private:
    void do_cleanTimer();
};


#endif //SOCKS5BALANCERASIO_TCPTEST_H
