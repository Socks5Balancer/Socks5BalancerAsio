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

#include "ConnectionTracker.h"

#include <boost/asio.hpp>

// the data Client --> Proxy --> Remove Server
void ConnectionTracker::relayGotoUp(const boost::asio::streambuf &buf) {
    if (isEnd) {
        return;
    }
    auto b = buf.data();
    size_t s = b.size();
    // https://www.boost.org/doc/libs/1_73_0/doc/html/boost_asio/reference/streambuf.html
    // https://stackoverflow.com/questions/33852492/how-can-i-prepare-char-arrays-to-boostasiostreambufmutable-buffers-type
    boost::asio::streambuf::mutable_buffers_type bs = up_data_.prepare(s);
    auto it = boost::asio::buffers_begin(bs);
    it = std::copy_n(reinterpret_cast<const unsigned char *>(b.data()), s, it);
    up_data_.commit(s);

    up_data_Update();
}

// the data Remote Server --> Proxy --> Client
void ConnectionTracker::relayGotoDown(const boost::asio::streambuf &buf) {
    if (isEnd) {
        return;
    }
    auto b = buf.data();
    size_t s = b.size();
    // https://www.boost.org/doc/libs/1_73_0/doc/html/boost_asio/reference/streambuf.html
    // https://stackoverflow.com/questions/33852492/how-can-i-prepare-char-arrays-to-boostasiostreambufmutable-buffers-type
    boost::asio::streambuf::mutable_buffers_type bs = down_data_.prepare(s);
    auto it = boost::asio::buffers_begin(bs);
    it = std::copy_n(reinterpret_cast<const unsigned char *>(b.data()), s, it);
    down_data_.commit(s);

    down_data_Update();
}

void ConnectionTracker::up_data_Update() {
    // TODO do analysis on there


    // debug
    cleanUp();
}

void ConnectionTracker::down_data_Update() {
    // TODO do analysis on there


    // debug
    cleanUp();
}

void ConnectionTracker::cleanUp() {
    isEnd = true;
    down_data_.consume(down_data_.size());
    up_data_.consume(up_data_.size());
}
