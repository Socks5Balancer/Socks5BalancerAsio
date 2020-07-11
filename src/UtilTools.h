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

#ifndef SOCKS5BALANCERASIO_UTILTOOLS_H
#define SOCKS5BALANCERASIO_UTILTOOLS_H


#include <random>


std::default_random_engine &getUtilsRandomGenerator();


/**
 * get a random num from [start, end]
 *
 * be careful : the number will hit the end .
 *
 *
 * <code>
 *
 * std::vector<int> arr;
 *
 * size_t n = getRandom<size_t>(0, arr.size() - 1);
 *
 * arr.at(n);
 *
 * </code>
 *
 * @tparam T        the type of num
 * @param start     the start
 * @param end       the end
 * @return          the rand num
 */
template<typename T>
T getRandom(T start, T end) {
    std::uniform_int_distribution<T> distribution(start, end);
    T i = distribution(getUtilsRandomGenerator());
    return i;
}


#endif //SOCKS5BALANCERASIO_UTILTOOLS_H
