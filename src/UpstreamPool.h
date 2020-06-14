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
#include "ConfigLoader.h"

struct UpstreamServer : public std::enable_shared_from_this<UpstreamServer> {
    std::string host;
    uint16_t port;
    std::string name;
    int index;

    std::string print() {
        std::stringstream ss;
        ss << "["
           << "index:" << index << ", "
           << "name:" << name << ", "
           << "host:" << host << ", "
           << "port:" << port << ", "
           << "]";
        return ss.str();
    }
};

using UpstreamServerRef = std::shared_ptr<UpstreamServer>;
using UpstreamServerRefRef = std::optional<std::shared_ptr<UpstreamServer>>;

class UpstreamPool : public std::enable_shared_from_this<UpstreamPool> {
    std::deque<UpstreamServerRef> _pool;
    size_t lastUseUpstreamIndex = 0;

    std::shared_ptr<ConfigLoader> _configLoader;

    std::default_random_engine randomGenerator;

    using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
    TimePoint lastChangeUpstreamTime;

public:
    UpstreamPool(boost::asio::executor ex) {}

    const std::deque<UpstreamServerRef> &pool() {
        return _pool;
    }

    void setConfig(std::shared_ptr<ConfigLoader> configLoader) {
        _configLoader = std::move(configLoader);
        const auto &c = _configLoader->config.upstream;
        _pool.clear();
        for (size_t i = 0; i != c.size(); ++i) {
            auto &r = c[i];
            UpstreamServerRef u = std::make_shared<UpstreamServer>();
            u->index = i;
            u->name = r.name;
            u->host = r.host;
            u->port = r.port;
            _pool.push_back(u);
        }
    }

    void forceSetLastUseUpstreamIndex(int i) {
        if (i >= 0 && i < _pool.size()) {
            lastUseUpstreamIndex = i;
        }
    }

    size_t getLastUseUpstreamIndex() {
        return lastUseUpstreamIndex;
    }

protected:
    bool checkServer(const UpstreamServerRef &u) const {
        // TODO impl
        return u && true;
    }

    auto getNextServer() -> UpstreamServerRef {
        const auto _lastUseUpstreamIndex = lastUseUpstreamIndex;
        while (true) {
            ++lastUseUpstreamIndex;
            if (lastUseUpstreamIndex >= _pool.size()) {
                lastUseUpstreamIndex = 0;
            }
            if (checkServer(_pool[lastUseUpstreamIndex])) {
                return _pool[lastUseUpstreamIndex]->shared_from_this();
            }
            if (_lastUseUpstreamIndex == lastUseUpstreamIndex) {
                // cannot find
                return UpstreamServerRef{};
            }
        }
    }

    auto tryGetLastServer() -> UpstreamServerRef {
        const auto _lastUseUpstreamIndex = lastUseUpstreamIndex;
        while (true) {
            if (lastUseUpstreamIndex >= _pool.size()) {
                lastUseUpstreamIndex = 0;
            }
            if (checkServer(_pool[lastUseUpstreamIndex])) {
                return _pool[lastUseUpstreamIndex]->shared_from_this();
            }
            ++lastUseUpstreamIndex;
            if (lastUseUpstreamIndex >= _pool.size()) {
                lastUseUpstreamIndex = 0;
            }
            if (_lastUseUpstreamIndex == lastUseUpstreamIndex) {
                // cannot find
                return UpstreamServerRef{};
            }
        }
    }

    auto filterValidServer() -> std::vector<UpstreamServerRef> {
        std::vector<UpstreamServerRef> r;
        for (auto &a:_pool) {
            if (checkServer(a)) {
                r.emplace_back(a->shared_from_this());
            }
        }
        return r;
    }

public:
    auto getServerBasedOnAddress() -> UpstreamServerRef {
        const auto upstreamSelectRule = _configLoader->config.upstreamSelectRule;

        UpstreamServerRef s{};
        switch (upstreamSelectRule) {
            case RuleEnum::loop:
                s = getNextServer();
                std::cout << "getServerBasedOnAddress:" << (s ? s->print() : "nullptr") << "\n";
                return s;
            case RuleEnum::one_by_one:
                s = tryGetLastServer();
                std::cout << "getServerBasedOnAddress:" << (s ? s->print() : "nullptr") << "\n";
                return s;
            case RuleEnum::change_by_time: {
                TimePoint t;
                const auto &d = _configLoader->config.serverChangeTime;
                if ((t - lastChangeUpstreamTime) > d) {
                    s = getNextServer();
                    lastChangeUpstreamTime = TimePoint{};
                } else {
                    s = tryGetLastServer();
                }
                std::cout << "getServerBasedOnAddress:" << (s ? s->print() : "nullptr") << "\n";
                return s;
            }
            case RuleEnum::random:
            default: {
                auto rs = filterValidServer();
                if (!rs.empty()) {
                    std::uniform_int_distribution<size_t> distribution(0, rs.size() - 1);
                    size_t i = distribution(randomGenerator);
                    s = rs[i];
                } else {
                    s.reset();
                }
                std::cout << "getServerBasedOnAddress:" << (s ? s->print() : "nullptr") << "\n";
                return s;
            }
        }
    }
};


#endif //SOCKS5BALANCERASIO_UPSTREAMPOOL_H
