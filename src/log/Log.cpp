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

#include "Log.h"

#include <boost/core/null_deleter.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/utility/setup/formatter_parser.hpp>

// https://www.boost.org/doc/libs/1_81_0/doc/html/stacktrace/getting_started.html
#include <cstdlib>       // std::abort
#include <exception>     // std::set_terminate
#include <iostream>      // std::cerr
//#include <boost/stacktrace.hpp>

// https://zh.cppreference.com/w/cpp/thread/call_once
// https://www.modernescpp.com/index.php/thread-safe-initialization-of-a-singleton
#include <mutex>
#include <memory>

#include <openssl/opensslv.h>
#include <openssl/crypto.h>

#include "../VERSION/ProgramVersion.h"
#include "../VERSION/CodeVersion.h"

void my_terminate_handler() {
    try {
//        std::cerr << "stacktrace : \n" << boost::stacktrace::stacktrace();
    } catch (...) {}
    std::abort();
}

namespace s5ba_log {
    thread_local std::string threadName;

    // https://zhuanlan.zhihu.com/p/389736260
    boost::log::sources::severity_logger<severity_level> slg;

    // https://stackoverflow.com/questions/23137637/linker-error-while-linking-boost-log-tutorial-undefined-references

    // https://stackoverflow.com/questions/17930553/in-boost-log-how-do-i-format-a-custom-severity-level-using-a-format-string
    const char *severity_level_str[severity_level::MAX] = {
            "trace",
            "debug",
            "info",
            "info_VSERION",
            "warning",
            "error",
            "fatal"
    };

    template<typename CharT, typename TraitsT>
    std::basic_ostream<CharT, TraitsT> &
    operator<<(std::basic_ostream<CharT, TraitsT> &strm, severity_level lvl) {
        const char *str = severity_level_str[lvl];
        if (lvl < severity_level::MAX && lvl >= 0)
            strm << str;
        else
            strm << static_cast< int >(lvl);
        return strm;
    }

    class thread_name_impl :
            public boost::log::attribute::impl {
    public:
        boost::log::attribute_value get_value() override {
            return boost::log::attributes::make_attribute_value(
                    s5ba_log::threadName.empty() ? std::string("no name") : s5ba_log::threadName);
        }

        using value_type = std::string;
    };

    class thread_name :
            public boost::log::attribute {
    public:
        thread_name() : boost::log::attribute(new thread_name_impl()) {
        }

        explicit thread_name(boost::log::attributes::cast_source const &source)
                : boost::log::attribute(source.as<thread_name_impl>()) {
        }

        using value_type = thread_name_impl::value_type;

    };

    BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", severity_level)

    void init_logging() {

//        std::set_terminate(&my_terminate_handler);

        // https://stackoverflow.com/questions/15853981/boost-log-2-0-empty-severity-level-in-logs
        boost::log::register_simple_formatter_factory<severity_level, char>("Severity");

        boost::shared_ptr<boost::log::core> core = boost::log::core::get();

        typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend> sink_t;
        boost::shared_ptr<sink_t> sink(new sink_t());
        sink->locked_backend()->add_stream(boost::shared_ptr<std::ostream>(&std::cout, boost::null_deleter()));
        sink->set_formatter(
                boost::log::expressions::stream
                        // << "["
                        // << std::setw(5)
                        // << boost::log::expressions::attr<unsigned int>("LineID")
                        // << "]"
                        << "["
                        << boost::log::expressions::format_date_time<boost::posix_time::ptime>(
                                "TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                        << "]"
                        << "[G:"
                        << CodeVersion_GIT_REV_F
                        << "]"
                        // << "["
                        // << boost::log::expressions::attr<boost::log::attributes::current_process_id::value_type>(
                        //         "ProcessID")
                        // << "]"
                        // << "["
                        // << boost::log::expressions::attr<boost::log::attributes::current_process_name::value_type>(
                        //         "ProcessName")
                        // << "]"
                        << "["
                        << boost::log::expressions::attr<boost::log::attributes::current_thread_id::value_type>(
                                "ThreadID")
                        << "]"
                        << "["
                        << std::setw(6)
                        << boost::log::expressions::attr<thread_name::value_type>("ThreadName")
                        << "]"
                        //                        << "[" << boost::log::trivial::severity << "] "
                        << "[" << boost::log::expressions::attr<severity_level>("Severity") << "] "
                        << boost::log::expressions::smessage);
        core->add_sink(sink);

        // https://www.boost.org/doc/libs/1_81_0/libs/log/doc/html/log/detailed/attributes.html
        // core->add_global_attribute("LineID", boost::log::attributes::counter<size_t>(1));
        core->add_global_attribute("ThreadName", thread_name());
        core->add_global_attribute("ThreadID", boost::log::attributes::current_thread_id());
        core->add_global_attribute("TimeStamp", boost::log::attributes::local_clock());
        // core->add_global_attribute("ProcessID", boost::log::attributes::current_process_id());
        // core->add_global_attribute("ProcessName", boost::log::attributes::current_process_name());

        // https://stackoverflow.com/questions/69967084/how-to-set-the-severity-level-of-boost-log-library
        // core->get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
        // core->get()->set_filter(severity >= severity_level::error);
        // core->get()->set_filter(severity != severity_level::error);
        // core->get()->set_filter(severity != severity_level::trace);

//        core->get()->set_filter(
//                true
//                #ifndef DEBUG_log_trace
//                && severity != severity_level::trace
//                #endif // DEBUG_log_trace
//                #ifndef DEBUG_log_debug
//                && severity != severity_level::debug
//                #endif // DEBUG_log_debug
//        );

        BOOST_LOG_S5B(info_VSERION) << versionInfo();

        BOOST_LOG_S5B(trace) << "BOOST_LOG_S5B(trace)";
        BOOST_LOG_S5B(debug) << "BOOST_LOG_S5B(debug)";
        BOOST_LOG_S5B(info) << "BOOST_LOG_S5B(info)";
        BOOST_LOG_S5B(info_VSERION) << "BOOST_LOG_S5B(info_VSERION)";
        BOOST_LOG_S5B(warning) << "BOOST_LOG_S5B(warning)";
        BOOST_LOG_S5B(error) << "BOOST_LOG_S5B(error)";
        BOOST_LOG_S5B(fatal) << "BOOST_LOG_S5B(fatal)";

    }

    std::once_flag versionInfo_OnceFlag{};
    std::unique_ptr<std::string> versionInfoString{};

    void versionInfoStringInit() {
        std::stringstream ss;
        ss << "Socks5BalancerAsio"
           << "\n   ProgramVersion " << ProgramVersion
           << "\n   CodeVersion_GIT_REV " << CodeVersion_GIT_REV
           << "\n   CodeVersion_GIT_REV_F " << CodeVersion_GIT_REV_F
           << "\n   CodeVersion_GIT_TAG " << CodeVersion_GIT_TAG
           << "\n   CodeVersion_GIT_BRANCH " << CodeVersion_GIT_BRANCH
           << "\n   Boost " << BOOST_LIB_VERSION
           << "\n   OpenSSL Version " << OpenSSL_version(OPENSSL_VERSION);
        if (OpenSSL_version_num() != OPENSSL_VERSION_NUMBER) {
            ss << "\n   OpenSSL Compile-time Version " << OPENSSL_VERSION_TEXT;
        }
        ss << "\n   OpenSSL Build Flags " << OpenSSL_version(OPENSSL_CFLAGS)
           << "\n   BUILD_DATETIME " << CodeVersion_BUILD_DATETIME
           #ifdef Need_ProxyHandshakeAuth
           << "\n   Need_ProxyHandshakeAuth ON"
           #else
           << "\n   Need_ProxyHandshakeAuth OFF"
           #endif // Need_ProxyHandshakeAuth
           << "\n ------------------------------------------------------------------------- "
           << "\nSocks5BalancerAsio  Copyright (C) <2020>  <Jeremie>"
           << "\n  This program comes with ABSOLUTELY NO WARRANTY; "
           << "\n  This is free software, and you are welcome to redistribute it"
           << "\n  under certain conditions; "
           << "\n       GNU GENERAL PUBLIC LICENSE , Version 3 "
           << "\n ---------- Socks5BalancerAsio  Copyright (C) <2020>  <Jeremie> ---------- ";
        versionInfoString = std::make_unique<decltype(versionInfoString)::element_type>(ss.str());
    }

    std::string versionInfo() {
        std::call_once(versionInfo_OnceFlag, &versionInfoStringInit);
        return *versionInfoString;
    }

} // s5ba_log
