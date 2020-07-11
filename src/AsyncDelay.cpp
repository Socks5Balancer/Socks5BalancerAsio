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

#include "AsyncDelay.h"

#include <iostream>

void asyncDelay(std::chrono::milliseconds delayTime, boost::asio::executor executor, std::function<void()> callback) {

    auto timer = std::make_shared<boost::asio::steady_timer>(executor, delayTime);

    std::cout << "asyncDelay:" << delayTime.count() << std::endl;

    timer->async_wait([timer, callback](const boost::system::error_code &e) {
        if (e) {
            return;
        }

        callback();
    });

}
