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

#include <iostream>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/lexical_cast.hpp>
#include <memory>
#include <exception>
#include "./log/Log.h"
#include "ConfigLoader.h"
#include "AuthClientManager.h"
#include "TcpTest.h"
#include "ConnectTestHttps.h"
#include "UpstreamPool.h"
#include "TcpRelayServer.h"
#include "StateMonitorServer.h"
#include "EmbedWebServer.h"

#ifdef USE_BOOST_THEAD

#include <boost/thread/thread.hpp>

#endif // USE_BOOST_THEAD

#ifndef DEFAULT_CONFIG
#define DEFAULT_CONFIG R"(config.json)"
#endif // DEFAULT_CONFIG

int main(int argc, const char *argv[]) {

    s5ba_log::threadName = "main";

    s5ba_log::init_logging();

    std::string config_file;
    boost::program_options::options_description desc("options");
    desc.add_options()
            ("config,c", boost::program_options::value<std::string>(&config_file)->
                    default_value(DEFAULT_CONFIG)->
                    value_name("CONFIG"), "specify config file")
            ("help,h", "print help message")
            ("version,v", "print version and build info");
    boost::program_options::positional_options_description pd;
    pd.add("config", 1);
    boost::program_options::variables_map vMap;
    boost::program_options::store(
            boost::program_options::command_line_parser(argc, argv)
                    .options(desc)
                    .positional(pd)
                    .run(), vMap);
    boost::program_options::notify(vMap);
    if (vMap.count("help")) {
        BOOST_LOG_S5B(info_VSERION) << "usage: " << argv[0] << " [[-c] CONFIG]" << "\n";

        BOOST_LOG_S5B(info_VSERION) << "    Socks5BalancerAsio  Copyright (C) <2020>  <Jeremie>\n"
                                    << "    This program comes with ABSOLUTELY NO WARRANTY; \n"
                                    << "    This is free software, and you are welcome to redistribute it\n"
                                    << "    under certain conditions; \n"
                                    << "         GNU GENERAL PUBLIC LICENSE , Version 3 "
                                    << "\n";

        BOOST_LOG_S5B(info_VSERION) << desc << std::endl;
        return 0;
    }
    if (vMap.count("version")) {
        BOOST_LOG_S5B(info_VSERION) << s5ba_log::versionInfo();
        return 0;
    }

    BOOST_LOG_S5B(info) << "config_file: " << config_file << std::endl;

    try {
        boost::asio::io_context ioc;
        boost::asio::any_io_executor ex = boost::asio::make_strand(ioc);

        auto configLoader = std::make_shared<ConfigLoader>();
        configLoader->load(config_file);
        configLoader->print();

        auto tcpTest = std::make_shared<TcpTest>(ex);
        auto connectTestHttps = std::make_shared<ConnectTestHttps>(ex);

        auto authClientManager = std::make_shared<AuthClientManager>(configLoader->shared_from_this());

        auto upstreamPool = std::make_shared<UpstreamPool>(ex, tcpTest, connectTestHttps);
        upstreamPool->setConfig(configLoader);

        auto tcpRelay = std::make_shared<TcpRelayServer>(ex, configLoader, upstreamPool, authClientManager);
        auto stateMonitor = std::make_shared<StateMonitorServer>(
                boost::asio::make_strand(ioc), configLoader, upstreamPool, tcpRelay);

        tcpRelay->start();

        upstreamPool->startCheckTimer();

        stateMonitor->start();

        std::shared_ptr<EmbedWebServer> embedWebServer;
#ifndef DISABLE_EmbedWebServer
        if (configLoader->config.embedWebServerConfig.enable) {
            boost::asio::ip::tcp::endpoint endpoint{
                    boost::asio::ip::make_address(configLoader->config.embedWebServerConfig.host),
                    configLoader->config.embedWebServerConfig.port
            };
            const auto &embedWebServerConfig = configLoader->config.embedWebServerConfig;
            embedWebServer = std::make_shared<EmbedWebServer>(
                    ioc,
                    endpoint,
                    std::make_shared<std::string>(embedWebServerConfig.root_path),
                    std::make_shared<std::string>(embedWebServerConfig.index_file_of_root),
                    std::make_shared<std::string>(embedWebServerConfig.backend_json_string),
                    std::make_shared<std::string>(embedWebServerConfig.allowFileExtList)
            );
            embedWebServer->start();
        }
#endif // DISABLE_EmbedWebServer

        boost::asio::signal_set sig(ioc);
        sig.add(SIGINT);
        sig.add(SIGTERM);
//#ifndef _WIN32
//        sig.add(SIGHUP);
//            sig.add(SIGUSR1);
//#endif // _WIN32
        sig.async_wait([&](const boost::system::error_code error, int signum) {
            if (error) {
                return;
            }
            BOOST_LOG_S5B(warning) << "got signal: " << signum;
            switch (signum) {
                case SIGINT:
                case SIGTERM: {
                    tcpRelay->stop();
                    upstreamPool->stop();
                    tcpTest->stop();
                    connectTestHttps->stop();
                    ioc.stop();
                }
                    break;
//#ifndef _WIN32
//                    case SIGHUP:
//                        restart = true;
//                        service.stop();
//                        break;
//                    case SIGUSR1:
//                        service.reload_cert();
//                        signal_async_wait(sig, service, restart);
//                        break;
//#endif // _WIN32
            }
        });


#ifdef USE_BOOST_THEAD
        size_t processor_count = boost::thread::hardware_concurrency();
        BOOST_LOG_S5B(info) << "processor_count:" << processor_count;
        BOOST_LOG_S5B(info) << "config.threadNum:" << configLoader->config.threadNum;
        processor_count = std::min(processor_count, configLoader->config.threadNum);
        BOOST_LOG_S5B(info) << "processor_count:" << processor_count;
        if (processor_count > 2) {
            boost::thread_group tg;
            for (unsigned i = 0; i < processor_count - 1; ++i) {
                tg.create_thread([&ioc, &tg, i = i]() {
                    try {
                        s5ba_log::threadName = std::string{"Thread-"} + boost::lexical_cast<std::string>(i);
                        ioc.run();
                    } catch (int) {
                        tg.interrupt_all();
                        BOOST_LOG_S5B(error) << "catch (int) exception";
                        return -1;
                    } catch (const std::exception &e) {
                        tg.interrupt_all();
                        BOOST_LOG_S5B(error) << "catch std::exception: " << e.what();
                        return -1;
                    } catch (...) {
                        tg.interrupt_all();
                        BOOST_LOG_S5B(error) << "catch (...) exception";
                        return -1;
                    }
                    return 0;
                });
            }
            tg.join_all();
        } else {
            ioc.run();
        }
#else // USE_BOOST_THEAD
        ioc.run();
#endif // USE_BOOST_THEAD

    } catch (int) {
        BOOST_LOG_S5B(error) << "catch (int) exception";
        return -1;
    }
    catch (const std::exception &e) {
        BOOST_LOG_S5B(error) << "catch std::exception: " << e.what();
        return -1;
    } catch (...) {
        BOOST_LOG_S5B(error) << "catch (...) exception";
        return -1;
    }

    return 0;
}
