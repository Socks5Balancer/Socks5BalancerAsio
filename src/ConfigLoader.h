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

#include <string>
#include <fstream>
#include <vector>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

class ConfigLoader {
public:
    struct Upstream {
        std::string host;
        uint16_t port;
        std::string name;
    };
    struct Config {
        std::vector<Upstream> upstream;

        std::string listenHost;
        uint16_t listenPort;

        std::string testRemoteHost;
        uint16_t testRemotePort;

        std::string upstreamSelectRule;
    };

    Config config;

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

        c.upstreamSelectRule = upstreamSelectRule;

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
