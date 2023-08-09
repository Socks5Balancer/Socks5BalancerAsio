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

#ifndef SOCKS5BALANCERASIO_BASE64_H
#define SOCKS5BALANCERASIO_BASE64_H

#ifdef MSVC
#pragma once
#endif

#include <string>
#include <vector>

// https://stackoverflow.com/questions/7053538/how-do-i-encode-a-string-to-base64-using-only-boost

#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>

template<typename T>
std::string base64_encode(const T &binary) {
    using namespace boost::archive::iterators;
    using It = base64_from_binary<transform_width<typename T::const_iterator, 6, 8>>;
    auto base64 = std::string(It(binary.begin()), It(binary.end()));
    // Add padding.
    return base64.append((3 - binary.size() % 3) % 3, '=');
}

inline std::string base64_encode_string(const std::string &text) {
    return base64_encode<std::string>(text);
}

inline std::string base64_encode_vector(const std::vector<unsigned char> &binary) {
    return base64_encode<std::vector<unsigned char>>(binary);
}

template<typename T>
T base64_decode(const std::string &base64) {
    using namespace boost::archive::iterators;
    using It = transform_width<binary_from_base64<std::string::const_iterator>, 8, 6>;
    auto binary = T(It(base64.begin()), It(base64.end()));
    // Remove padding.
    auto length = base64.size();
    if (binary.size() > 2 && base64[length - 1] == '=' && base64[length - 2] == '=') {
        binary.erase(binary.end() - 2, binary.end());
    } else if (binary.size() > 1 && base64[length - 1] == '=') {
        binary.erase(binary.end() - 1, binary.end());
    }
    return binary;
}


inline std::vector<unsigned char> base64_decode_vector(const std::string &base64) {
    return base64_decode<std::vector<unsigned char>>(base64);
}

inline std::string base64_decode_string(const std::string &base64) {
    return base64_decode<std::string>(base64);
}

#endif //SOCKS5BALANCERASIO_BASE64_H
