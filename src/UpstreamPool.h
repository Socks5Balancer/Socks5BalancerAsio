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

#ifndef SOCKS5BALANCERASIO_UPSTREAMPOOL_H
#define SOCKS5BALANCERASIO_UPSTREAMPOOL_H

#ifdef MSVC
#pragma once
#endif

#include <boost/asio.hpp>
#include <string>
#include <deque>
#include <memory>
#include <optional>
#include <functional>
#include <sstream>
#include <utility>
#include <random>
#include <chrono>
#include <atomic>
#include "ConfigLoader.h"
#include "TcpTest.h"
#include "ConnectTestHttps.h"

using UpstreamTimePoint = std::chrono::time_point<std::chrono::system_clock>;

UpstreamTimePoint UpstreamTimePointNow();

std::string printUpstreamTimePoint(UpstreamTimePoint p);

struct UpstreamServer : public std::enable_shared_from_this<UpstreamServer> {
    std::string host;
    uint16_t port;
    std::string name;
    size_t index;

    std::optional<UpstreamTimePoint> lastOnlineTime;
    std::optional<UpstreamTimePoint> lastConnectTime;
    bool lastConnectFailed = true;
    std::string lastConnectCheckResult;
    bool isOffline = true;
    std::atomic_size_t connectCount{0};
    bool isManualDisable = false;
    bool disable = false;

    UpstreamServer(
            size_t index,
            std::string name,
            std::string host,
            uint16_t port,
            bool disable
    ) :
            index(index),
            name(name),
            host(host),
            port(port),
            disable(disable),
            isManualDisable(disable) {}

    std::string print();

    void updateOnlineTime();
};

using UpstreamServerRef = std::shared_ptr<UpstreamServer>;

class UpstreamPool : public std::enable_shared_from_this<UpstreamPool> {
    boost::asio::executor ex;

    std::deque<UpstreamServerRef> _pool;
    size_t lastUseUpstreamIndex = 0;

    std::shared_ptr<ConfigLoader> _configLoader;

    std::default_random_engine randomGenerator;

    UpstreamTimePoint lastChangeUpstreamTime;

    UpstreamTimePoint lastConnectComeTime;

    std::shared_ptr<TcpTest> tcpTest;
    std::shared_ptr<ConnectTestHttps> connectTestHttps;

public:
    UpstreamPool(boost::asio::executor ex,
                 std::shared_ptr<TcpTest> tcpTest,
                 std::shared_ptr<ConnectTestHttps> connectTestHttps);

    const std::deque<UpstreamServerRef> &pool();

    void setConfig(std::shared_ptr<ConfigLoader> configLoader);

    void forceSetLastUseUpstreamIndex(int i);

    size_t getLastUseUpstreamIndex();

    bool checkServer(const UpstreamServerRef &u) const;

    void updateLastConnectComeTime();

    UpstreamTimePoint getLastConnectComeTime();

protected:

    auto getNextServer() -> UpstreamServerRef;

    auto tryGetLastServer() -> UpstreamServerRef;

    auto filterValidServer() -> std::vector<UpstreamServerRef>;

public:
    auto getServerBasedOnAddress() -> UpstreamServerRef;


private:
    using CheckerTimerType = boost::asio::steady_timer;
    using CheckerTimerPeriodType = boost::asio::chrono::microseconds;
    std::shared_ptr<CheckerTimerType> tcpCheckerTimer;
    std::shared_ptr<CheckerTimerType> connectCheckerTimer;
    std::weak_ptr<CheckerTimerType> forceCheckerTimer;

    std::shared_ptr<boost::asio::steady_timer> additionTimer;
public:
    void endCheckTimer();

    void startCheckTimer();

    std::string print();

    void forceCheckNow();

    void forceCheckOne(size_t index);

private:
    void do_tcpCheckerTimer();

    void do_tcpCheckerTimer_impl();

    void do_tcpCheckerOne_impl(UpstreamServerRef a);

    void do_connectCheckerTimer();

    void do_connectCheckerTimer_impl();

    void do_connectCheckerOne_impl(UpstreamServerRef a);

    void do_forceCheckNow(std::shared_ptr<CheckerTimerType> _forceCheckerTimer);

private:
    void startAdditionTimer();

    void endAdditionTimer();

    void do_AdditionTimer();

    void do_AdditionTimer_impl();

    std::atomic_bool isAdditionTimerRunning{false};

};


#endif //SOCKS5BALANCERASIO_UPSTREAMPOOL_H
