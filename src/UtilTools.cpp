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

#include "UtilTools.h"

#include <memory>

// https://zh.cppreference.com/w/cpp/thread/call_once
// https://www.modernescpp.com/index.php/thread-safe-initialization-of-a-singleton
#include <mutex>


template<typename T>
class SingletonWrapper {
    std::once_flag onceFlag{};

    std::unique_ptr<T> itemPtr;

    void fill() {
        itemPtr = std::make_unique<T>();
    }

public:

    T &get() {
        std::call_once(onceFlag, &SingletonWrapper<T>::fill, this);
        return *itemPtr;
    }

};

//std::once_flag onceFlag_UtilsRandomGenerator{};
//
//std::unique_ptr<std::default_random_engine> UtilsRandomGenerator;
//
//void fillUtilsRandomGenerator() {
//    UtilsRandomGenerator = std::make_unique<decltype(UtilsRandomGenerator)::element_type>();
//}
//
//std::default_random_engine &getUtilsRandomGenerator() {
//    if (!UtilsRandomGenerator) {
//        std::call_once(onceFlag_UtilsRandomGenerator, fillUtilsRandomGenerator);
////        UtilsRandomGenerator = std::make_unique<decltype(UtilsRandomGenerator)::element_type>();
//    }
//    return *UtilsRandomGenerator;
//}

SingletonWrapper<std::default_random_engine> singletonUtilsRandomGenerator;

std::default_random_engine &getUtilsRandomGenerator() {
    return singletonUtilsRandomGenerator.get();
}


