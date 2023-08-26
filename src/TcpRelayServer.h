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

#ifndef SOCKS5BALANCERASIO_TCPRELAYSERVER_H
#define SOCKS5BALANCERASIO_TCPRELAYSERVER_H

#ifdef MSVC
#pragma once
#endif

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <string>
#include <utility>
#include <list>
#include <map>
#include <atomic>
#include <mutex>
#include "UpstreamPool.h"
#include "ConnectionTracker.h"
#include "AuthClientManager.h"

#include "TcpRelayStatisticsInfo.h"
#include "./log/Log.h"

class TcpRelaySession;

class TcpRelayServer : public std::enable_shared_from_this<TcpRelayServer> {

    boost::asio::any_io_executor ex;
    std::shared_ptr<ConfigLoader> configLoader;
    std::shared_ptr<UpstreamPool> upstreamPool;
    std::shared_ptr<AuthClientManager> authClientManager;
    std::list<boost::asio::ip::tcp::acceptor> socket_acceptors;

    std::list<std::weak_ptr<TcpRelaySession>> sessions;
    std::shared_ptr<TcpRelayStatisticsInfo> statisticsInfo;

    std::shared_ptr<boost::asio::steady_timer> cleanTimer;
    std::shared_ptr<boost::asio::steady_timer> speedCalcTimer;
public:
    TcpRelayServer(
            boost::asio::any_io_executor ex,
            std::shared_ptr<ConfigLoader> configLoader,
            std::shared_ptr<UpstreamPool> upstreamPool,
            std::shared_ptr<AuthClientManager> authClientManager
    ) : ex(ex),
        configLoader(std::move(configLoader)),
        upstreamPool(std::move(upstreamPool)),
        authClientManager(std::move(authClientManager)),
        statisticsInfo(std::make_shared<TcpRelayStatisticsInfo>()) {
    }

    void start();

    void stop();

private:
    void async_accept(boost::asio::ip::tcp::acceptor &sa);

    void removeExpiredSession();

public:
    void closeAllSession();

    std::shared_ptr<TcpRelayStatisticsInfo> getStatisticsInfo();

    std::shared_ptr<AuthClientManager> getAuthClientManager();

    std::shared_ptr<UpstreamPool> getUpstreamPool();

    void do_cleanTimer();

    void do_speedCalcTimer();

};


#endif //SOCKS5BALANCERASIO_TCPRELAYSERVER_H
