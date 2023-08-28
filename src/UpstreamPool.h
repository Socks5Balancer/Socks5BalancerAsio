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

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/steady_timer.hpp>
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
#include <chrono>
#include "ConfigLoader.h"
#include "TcpTest.h"
#include "ConnectTestHttps.h"
#include "DelayCollection.h"

#include "./log/Log.h"

using UpstreamTimePoint = std::chrono::time_point<std::chrono::system_clock>;

UpstreamTimePoint UpstreamTimePointNow();

std::string printUpstreamTimePoint(UpstreamTimePoint p);

struct UpstreamServer : public std::enable_shared_from_this<UpstreamServer> {
    std::string host;
    uint16_t port;
    std::string name;
    size_t index;

    std::string authUser;
    std::string authPwd;

    std::optional<UpstreamTimePoint> lastOnlineTime;
    std::optional<UpstreamTimePoint> lastConnectTime;
    bool lastConnectFailed = true;
    std::string lastConnectCheckResult;
    bool isOffline = true;
    std::atomic_size_t connectCount{0};
    bool isManualDisable = false;
    bool disable = false;
    bool slowImpl = false;

    std::chrono::milliseconds lastOnlinePing{-1};
    std::chrono::milliseconds lastConnectPing{-1};

    bool traditionTcpRelay;
    std::shared_ptr<DelayCollection::DelayCollect> delayCollect;

    UpstreamServer(
            size_t index,
            std::string name,
            std::string host,
            uint16_t port,
            std::string authUser,
            std::string authPwd,
            bool disable,
            bool slowImpl,
            bool traditionTcpRelay
    ) :
            host(std::move(host)),
            port(port),
            name(std::move(name)),
            index(index),
            authUser(std::move(authUser)),
            authPwd(std::move(authPwd)),
            isManualDisable(disable),
            disable(disable),
            slowImpl(slowImpl),
            traditionTcpRelay(traditionTcpRelay),
            delayCollect(std::make_shared<DelayCollection::DelayCollect>(traditionTcpRelay)) {}

    std::string print();

    void updateOnlineTime();
};

using UpstreamServerRef = std::shared_ptr<UpstreamServer>;

class UpstreamPool : public std::enable_shared_from_this<UpstreamPool> {
    boost::asio::any_io_executor ex;

    std::deque<UpstreamServerRef> _pool;
    size_t lastUseUpstreamIndex = 0;

    std::shared_ptr<ConfigLoader> _configLoader;

    std::default_random_engine randomGenerator;

    UpstreamTimePoint lastChangeUpstreamTime;

    UpstreamTimePoint lastConnectComeTime;

    std::shared_ptr<TcpTest> tcpTest;
    std::shared_ptr<ConnectTestHttps> connectTestHttps;

public:
    UpstreamPool(boost::asio::any_io_executor ex,
                 std::shared_ptr<TcpTest> tcpTest,
                 std::shared_ptr<ConnectTestHttps> connectTestHttps);

    const std::deque<UpstreamServerRef> &pool();

    void setConfig(std::shared_ptr<ConfigLoader> configLoader);

    void forceSetLastUseUpstreamIndex(size_t i);

    size_t getLastUseUpstreamIndex();

    bool checkServer(const UpstreamServerRef &u) const;

    void updateLastConnectComeTime();

    UpstreamTimePoint getLastConnectComeTime();

protected:

    auto getNextServer(size_t &_lastUseUpstreamIndex) -> UpstreamServerRef;

    auto tryGetLastServer(size_t &_lastUseUpstreamIndex) -> UpstreamServerRef;

    auto filterValidServer() -> std::vector<UpstreamServerRef>;

public:
    auto getServerGlobal(const size_t &relayId) -> UpstreamServerRef;

    auto getServerByHint(
            const RuleEnum &_upstreamSelectRule,
            size_t &_lastUseUpstreamIndex,
            const size_t &relayId,
            bool dontFallbackToGlobal = false
    ) -> UpstreamServerRef;

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

    void stop();

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
