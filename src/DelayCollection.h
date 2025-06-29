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

#ifndef SOCKS5BALANCERASIO_DELAYCOLLECTION_H
#define SOCKS5BALANCERASIO_DELAYCOLLECTION_H

#ifdef MSVC
#pragma once
#endif

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <deque>
#include <map>
#include <atomic>
#include <mutex>
#include <chrono>
#include <limits>
#include "boost/circular_buffer.hpp"

#include "./CMakeParamsConfig/CMakeParams.h"
#include "./log/Log.h"

namespace DelayCollection {
    using TimeMs = std::chrono::milliseconds;
    constexpr TimeMs TimeMsInvalid{-1};
    using TimePoint = std::chrono::steady_clock::time_point;
    using TimePointClock = std::chrono::system_clock::time_point;

    inline TimePoint nowTimePoint() {
        return std::chrono::steady_clock::now();
    }

    inline TimePointClock nowTimePointClock() {
        return std::chrono::system_clock::now();
    }

    class TimeHistory {
    public:

        struct DelayInfo {
            TimeMs delay;
            TimePointClock timeClock;

            auto operator<=>(const DelayInfo &o) const {
                if ((timeClock <=> o.timeClock) != std::strong_ordering::equal) {
                    return timeClock <=> o.timeClock;
                } else if ((delay <=> o.delay) != std::strong_ordering::equal) {
                    return delay <=> o.delay;
                }
                return std::strong_ordering::equal;
            }

            explicit DelayInfo(TimeMs delay) : delay(delay), timeClock(nowTimePointClock()) {}

            DelayInfo &operator=(const DelayInfo &o) = default;

            DelayInfo &operator=(DelayInfo &&o) = default;

            DelayInfo(const DelayInfo &o) = default;

            DelayInfo(DelayInfo &o) = default;

            DelayInfo(DelayInfo &&o) = default;

            ~DelayInfo() = default;
        };

    private:

        std::recursive_mutex mtx;
        
        std::unique_ptr<boost::circular_buffer<DelayInfo>> q;

        size_t maxSize = 8192;
        // size_t maxSize = std::numeric_limits<decltype(maxSize)>::max() / 2;

        void trim() {
            std::lock_guard lg{mtx};
            if (q->size() > maxSize && !q->empty()) {
                // remove front
                size_t needRemove = q->size() - maxSize;
                if (needRemove == 1) [[likely]]
                {
                    // only remove first, we use the impl of `deque` to speed up the remove speed
                    // often into this way, so we mark it use c++20 `[[likely]]`
                    q->pop_front();
                } else {
                    BOOST_LOG_S5B(warning) << "TimeHistory::trim() re-create,"
                                           << " needRemove:" << needRemove
                                           << " maxSize:" << maxSize
                                           << " q.size:" << q->size();
                    // we need remove more than 1 , this only happened when maxSize changed
                    // we copy and re-create it, this will wast much time
                    auto m = std::make_unique<boost::circular_buffer<DelayInfo>>(maxSize);
                    std::copy(std::next(q->begin(), needRemove), q->end(), m->begin());
                    q = std::move(m);
                }
            }
        }

    public:

        TimeHistory() {
            std::lock_guard lg{mtx};
#ifdef Limit_TimeHistory_Memory_Use
            maxSize = CMParams_Limit_TimeHistory_Number;
#endif // Limit_TimeHistory_Memory_Use
            q = std::make_unique<boost::circular_buffer<DelayInfo>>(maxSize);
        }

        void addDelayInfo(TimeMs delay) {
            std::lock_guard lg{mtx};
            q->push_back(DelayInfo{delay});
            trim();
        }

        [[nodiscard]] std::deque<DelayInfo> history() {
            std::lock_guard lg{mtx};
            // deep copy
            return std::move(std::deque<DelayInfo>{q->begin(), q->end()});
        }

        void setMaxSize(size_t m) {
            std::lock_guard lg{mtx};
            m = std::max<size_t>(m, 1);
            maxSize = m;
            BOOST_LOG_S5B(warning) << "TimeHistory::setMaxSize " << m;
            trim();
        }

        [[nodiscard]] decltype(maxSize) getMaxSize() {
            return maxSize;
        }

        void removeBefore(TimePointClock time) {
            std::lock_guard lg{mtx};
            while (!q->empty()) {
                if (q->front().timeClock < time) {
                    q->pop_front();
                } else {
                    break;
                }
            }
        }

        void clean() {
            std::lock_guard lg{mtx};
            q->clear();
        }
    };

    class DelayCollect : public std::enable_shared_from_this<DelayCollect> {
    private:

        TimeMs lastTcpPing{TimeMsInvalid};
        TimeMs lastHttpPing{TimeMsInvalid};
        TimeMs lastRelayFirstDelay{TimeMsInvalid};

        TimeHistory historyTcpPing;
        TimeHistory historyHttpPing;
        TimeHistory historyRelayFirstDelay;

        bool traditionTcpRelay;

    public:

        explicit DelayCollect(bool traditionTcpRelay) : traditionTcpRelay(traditionTcpRelay) {}

    public:

        std::deque<TimeHistory::DelayInfo> getHistoryTcpPing() {
            return std::move(historyTcpPing.history());
        }

        std::deque<TimeHistory::DelayInfo> getHistoryHttpPing() {
            return std::move(historyHttpPing.history());
        }

        std::deque<TimeHistory::DelayInfo> getHistoryRelayFirstDelay() {
            return std::move(historyRelayFirstDelay.history());
        }

        void setMaxSizeTcpPing(size_t m) {
            historyTcpPing.setMaxSize(m);
        }

        void setMaxSizeHttpPing(size_t m) {
            historyHttpPing.setMaxSize(m);
        }

        void setMaxSizeFirstDelay(size_t m) {
            historyRelayFirstDelay.setMaxSize(m);
        }

        size_t getMaxSizeTcpPing() {
            return historyTcpPing.getMaxSize();
        }

        size_t getMaxSizeHttpPing() {
            return historyHttpPing.getMaxSize();
        }

        size_t getMaxSizeFirstDelay() {
            return historyRelayFirstDelay.getMaxSize();
        }

        void removeBeforeTcpPing(TimePointClock time) {
            historyTcpPing.removeBefore(time);
        }

        void removeBeforeHttpPing(TimePointClock time) {
            historyHttpPing.removeBefore(time);
        }

        void removeBeforeFirstDelay(TimePointClock time) {
            historyRelayFirstDelay.removeBefore(time);
        }

        void cleanTcpPing() {
            historyTcpPing.clean();
            lastTcpPing = TimeMsInvalid;
        }

        void cleanHttpPing() {
            historyHttpPing.clean();
            lastHttpPing = TimeMsInvalid;
        }

        void cleanFirstDelay() {
            historyRelayFirstDelay.clean();
            lastRelayFirstDelay = TimeMsInvalid;
        }

    public:

        void pushTcpPing(TimeMs t) {
            lastTcpPing = t;
            if (traditionTcpRelay) {
                return;
            }
            historyTcpPing.addDelayInfo(t);
        }

        void pushHttpPing(TimeMs t) {
            lastHttpPing = t;
            if (traditionTcpRelay) {
                return;
            }
            historyHttpPing.addDelayInfo(t);
        }

        void pushRelayFirstDelay(TimeMs t) {
            lastRelayFirstDelay = t;
            if (traditionTcpRelay) {
                return;
            }
            historyRelayFirstDelay.addDelayInfo(t);
        }

    };

}


#endif //SOCKS5BALANCERASIO_DELAYCOLLECTION_H
