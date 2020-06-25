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

#ifndef SOCKS5BALANCERASIO_CONNECTIONTRACKER_H
#define SOCKS5BALANCERASIO_CONNECTIONTRACKER_H

#ifdef MSVC
#pragma once
#endif

#include <boost/asio.hpp>
#include <memory>
#include <iostream>
#include <string>
#include <functional>
#include <utility>
#include "ConnectType.h"

class TcpRelaySession;

class ConnectionTracker : public std::enable_shared_from_this<ConnectionTracker> {
    std::weak_ptr<TcpRelaySession> tcpRelaySession;

    // Client --> Proxy --> Remove Server
    boost::asio::streambuf up_data_;
    // Remote Server --> Proxy --> Client
    boost::asio::streambuf down_data_;

    ConnectType connectType;

    bool isEnd = false;
public:

    ConnectionTracker() = delete;

    ConnectionTracker(std::weak_ptr<TcpRelaySession> tcpRelaySession,
                      ConnectType connectType = ConnectType::unknown)
            : tcpRelaySession(tcpRelaySession),
              connectType(connectType) {}

    // the data Client --> Proxy --> Remove Server
    void relayGotoUp(const boost::asio::streambuf &buf);

    template<typename PodType, std::size_t N>
    void relayGotoUp(const PodType (&data)[N], std::size_t max_size_in_bytes);

    // the data Remote Server --> Proxy --> Client
    void relayGotoDown(const boost::asio::streambuf &buf);

    template<typename PodType, std::size_t N>
    void relayGotoDown(const PodType (&data)[N], std::size_t max_size_in_bytes);

    bool isComplete() {
        return isEnd;
    }

private:
    void up_data_Update();

    void down_data_Update();

    void cleanUp();

};

// the data Client --> Proxy --> Remove Server
template<typename PodType, std::size_t N>
void ConnectionTracker::relayGotoUp(const PodType (&data)[N], std::size_t max_size_in_bytes) {
    if (isEnd) {
        return;
    }
    size_t s = (N * sizeof(PodType) < max_size_in_bytes
                ? N * sizeof(PodType) : max_size_in_bytes);
    boost::asio::streambuf::mutable_buffers_type bs = up_data_.prepare(s);
    auto it = boost::asio::buffers_begin(bs);
    it = std::copy_n(reinterpret_cast<const unsigned char *>(data), s, it);
    up_data_.commit(s);

    up_data_Update();
}

// the data Remote Server --> Proxy --> Client
template<typename PodType, std::size_t N>
void ConnectionTracker::relayGotoDown(const PodType (&data)[N], std::size_t max_size_in_bytes) {
    if (isEnd) {
        return;
    }
    size_t s = (N * sizeof(PodType) < max_size_in_bytes
                ? N * sizeof(PodType) : max_size_in_bytes);
    boost::asio::streambuf::mutable_buffers_type bs = down_data_.prepare(s);
    auto it = boost::asio::buffers_begin(bs);
    it = std::copy_n(reinterpret_cast<const unsigned char *>(data), s, it);
    down_data_.commit(s);

    down_data_Update();
}


#endif //SOCKS5BALANCERASIO_CONNECTIONTRACKER_H
