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

#ifndef SOCKS5BALANCERASIO_LOG_H
#define SOCKS5BALANCERASIO_LOG_H

#ifdef MSVC
#pragma once
#endif

#include <string>
#include <boost/log/core.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/log/sources/record_ostream.hpp>

namespace s5ba_log{

    enum severity_level {
        trace,
        debug,
        info,
        info_VSERION,
        warning,
        error,
        fatal,
        MAX
    };

    extern boost::log::sources::severity_logger<severity_level> slg;
    extern const char *severity_level_str[severity_level::MAX];

    extern thread_local std::string threadName;

    // https://stackoverflow.com/questions/60977433/including-thread-name-in-boost-log
    void init_logging();

    std::string versionInfo();

}

#ifndef BOOST_LOG_S5B
#define BOOST_LOG_S5B(lvl) BOOST_LOG_SEV(::s5ba_log::slg, ::s5ba_log::severity_level::lvl )
#endif // BOOST_LOG_S5B

#endif //SOCKS5BALANCERASIO_LOG_H
