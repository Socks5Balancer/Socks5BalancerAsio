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
#include <boost/asio/executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/parsers.hpp>
#include <memory>
#include <exception>
#include "ConfigLoader.h"
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
        std::cout << "usage: " << argv[0] << " [[-c] CONFIG]" << "\n" << std::endl;

        std::cout << "    Socks5BalancerAsio  Copyright (C) <2020>  <Jeremie>\n"
                  << "    This program comes with ABSOLUTELY NO WARRANTY; \n"
                  << "    This is free software, and you are welcome to redistribute it\n"
                  << "    under certain conditions; \n"
                  << "         GNU GENERAL PUBLIC LICENSE , Version 3 "
                  << "\n" << std::endl;

        std::cout << desc << std::endl;
        return 0;
    }
    if (vMap.count("version")) {
        std::cout << std::string("Boost ") + BOOST_LIB_VERSION + ", " + OpenSSL_version(OPENSSL_VERSION) << std::endl;
        std::cout << "OpenSSL Information:" << "\n";
        if (OpenSSL_version_num() != OPENSSL_VERSION_NUMBER) {
            std::cout << std::string("\tCompile-time Version: ") + OPENSSL_VERSION_TEXT << "\n";
        }
        std::cout << std::string("\tBuild Flags: ") + OpenSSL_version(OPENSSL_CFLAGS) << "\n";
        std::cout << std::endl;
        return 0;
    }

    std::cout << "config_file: " << config_file << std::endl;

    try {
        boost::asio::io_context ioc;
        boost::asio::executor ex = boost::asio::make_strand(ioc);

        auto configLoader = std::make_shared<ConfigLoader>();
        configLoader->load(config_file);
        configLoader->print();

        auto tcpTest = std::make_shared<TcpTest>(ex);
        auto connectTestHttps = std::make_shared<ConnectTestHttps>(ex);

        auto upstreamPool = std::make_shared<UpstreamPool>(ex, tcpTest, connectTestHttps);
        upstreamPool->setConfig(configLoader);

        auto tcpRelay = std::make_shared<TcpRelayServer>(ex, configLoader, upstreamPool);
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
            std::cerr << "got signal: " << signum << std::endl;
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
        std::cout << "processor_count:" << processor_count << std::endl;
        std::cout << "config.threadNum:" << configLoader->config.threadNum << std::endl;
        processor_count = std::min(processor_count, configLoader->config.threadNum);
        std::cout << "processor_count:" << processor_count << std::endl;
        if (processor_count > 2) {
            boost::thread_group tg;
            for (unsigned i = 0; i < processor_count - 1; ++i) {
                tg.create_thread([&ioc, &tg]() {
                    try {
                        ioc.run();
                    } catch (int) {
                        tg.interrupt_all();
                        std::cerr << "catch (int) exception" << "\n";
                        return -1;
                    }
                    catch (const std::exception &e) {
                        tg.interrupt_all();
                        std::cerr << "catch std::exception: " << e.what() << "\n";
                        return -1;
                    } catch (...) {
                        tg.interrupt_all();
                        std::cerr << "catch (...) exception" << "\n";
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
        std::cerr << "catch (int) exception" << "\n";
        return -1;
    }
    catch (const std::exception &e) {
        std::cerr << "catch std::exception: " << e.what() << "\n";
        return -1;
    } catch (...) {
        std::cerr << "catch (...) exception" << "\n";
        return -1;
    }

    return 0;
}
