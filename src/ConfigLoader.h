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

#ifndef SOCKS5BALANCERASIO_CONFIGLOADER_H
#define SOCKS5BALANCERASIO_CONFIGLOADER_H

#ifdef MSVC
#pragma once
#endif

#include <string>
#include <fstream>
#include <vector>
#include <iostream>
#include <memory>
#include <optional>
#include <chrono>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using ConfigTimeDuration = std::chrono::milliseconds;

struct Upstream {
    std::string host;
    uint16_t port;
    std::string name;
};

enum class RuleEnum {
    loop,
    random,
    one_by_one,
    change_by_time,
};

struct Config {
    std::vector<Upstream> upstream;

    std::string listenHost;
    uint16_t listenPort;

    std::string testRemoteHost;
    uint16_t testRemotePort;

    RuleEnum upstreamSelectRule;

    ConfigTimeDuration serverChangeTime;
};

RuleEnum string2RuleEnum(std::string s);

std::string ruleEnum2string(RuleEnum r);

class ConfigLoader : public std::enable_shared_from_this<ConfigLoader> {
public:
    Config config;

    void print() {
        std::cout << "config.listenHost:" << config.listenHost << "\n";
        std::cout << "config.listenPort:" << config.listenPort << "\n";
        std::cout << "config.testRemoteHost:" << config.testRemoteHost << "\n";
        std::cout << "config.testRemotePort:" << config.testRemotePort << "\n";
        std::cout << "config.upstreamSelectRule:" << ruleEnum2string(config.upstreamSelectRule) << "\n";
        std::cout << "config.serverChangeTime:" << config.serverChangeTime.count() << "\n";
        for (size_t i = 0; i != config.upstream.size(); ++i) {
            const auto &it = config.upstream[i];
            std::cout << "config.upstream [" << i << "]:\n";
            std::cout << "\t" << "upstream.name:" << it.name << "\n";
            std::cout << "\t" << "upstream.host:" << it.host << "\n";
            std::cout << "\t" << "upstream.port:" << it.port << "\n";
        }
    }

    void
    load(const std::string &filename) {
        boost::property_tree::ptree tree;
        boost::property_tree::read_json(filename, tree);
        parse_json(tree);
    }

    void parse_json(const boost::property_tree::ptree &tree) {
        Config c{};

        auto listenHost = tree.get("listenHost", std::string{"127.0.0.1"});
        auto listenPort = tree.get<uint16_t>("listenPort", static_cast<uint16_t>(5000));

        c.listenHost = listenHost;
        c.listenPort = listenPort;

        auto testRemoteHost = tree.get("testRemoteHost", std::string{"www.google.com"});
        auto testRemotePort = tree.get<uint16_t>("testRemotePort", static_cast<uint16_t>(443));

        c.testRemoteHost = testRemoteHost;
        c.testRemotePort = testRemotePort;

        auto upstreamSelectRule = tree.get("upstreamSelectRule", std::string{"random"});
        c.upstreamSelectRule = string2RuleEnum(upstreamSelectRule);

        auto serverChangeTime = tree.get("serverChangeTime", static_cast<long long>(60 * 1000));
        c.serverChangeTime = ConfigTimeDuration{serverChangeTime};

        if (tree.get_child_optional("upstream")) {
            auto upstream = tree.get_child("upstream");
            for (auto &item: upstream) {
                auto &pts = item.second;
                Upstream u;
                u.host = pts.get("host", std::string{"127.0.0.1"});
                u.port = pts.get("port", uint16_t{});
                u.name = pts.get("name", std::string{});
                c.upstream.push_back(u);
            }
        }

        config = c;
    }
};


#endif //SOCKS5BALANCERASIO_CONFIGLOADER_H
