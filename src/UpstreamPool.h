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

#include <boost/asio.hpp>
#include <string>
#include <deque>
#include <memory>

struct UpstreamServer {
    std::string host;
    uint16_t port;
    std::string name;
    int index;
};

class UpstreamPool: public std::enable_shared_from_this<UpstreamPool> {
    std::deque<UpstreamServer> pool;
public:
    UpstreamPool(boost::asio::executor ex) {}
};


#endif //SOCKS5BALANCERASIO_UPSTREAMPOOL_H
