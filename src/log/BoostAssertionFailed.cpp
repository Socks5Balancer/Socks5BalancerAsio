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

#include "./Log.h"
#include <boost/assert.hpp>
//#include <boost/stacktrace.hpp>

namespace boost {
    void assertion_failed(char const *expr, char const *function, char const *file, long line) {
        BOOST_LOG_S5B(fatal)
            << "assertion_failed : [" << expr << "]"
            << " on function [" << function << "]"
            << " on file [" << file << "]"
            << " at line [" << line << "]";
//            << " stacktrace:\n" << boost::stacktrace::stacktrace();
        std::abort();
    }

    void assertion_failed_msg(char const *expr, char const *msg, char const *function, char const *file, long line) {
        BOOST_LOG_S5B(fatal)
            << "assertion_failed_msg : [" << expr << "]"
            << " msg [" << msg << "]"
            << " on function [" << function << "]"
            << " on file [" << file << "]"
            << " at line [" << line << "]";
//            << " stacktrace:\n" << boost::stacktrace::stacktrace();
        std::abort();
    }
}

