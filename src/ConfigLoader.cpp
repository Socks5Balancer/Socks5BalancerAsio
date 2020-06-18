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

#include "ConfigLoader.h"

RuleEnum string2RuleEnum(std::string s) {
    if ("loop" == s) {
        return RuleEnum::loop;
    }
    if ("random" == s) {
        return RuleEnum::random;
    }
    if ("one_by_one" == s) {
        return RuleEnum::one_by_one;
    }
    if ("change_by_time" == s) {
        return RuleEnum::change_by_time;
    }
    return RuleEnum::random;
}

std::string ruleEnum2string(RuleEnum r) {
    switch (r) {
        case RuleEnum::loop:
            return "loop";
        case RuleEnum::random:
            return "random";
        case RuleEnum::one_by_one:
            return "one_by_one";
        case RuleEnum::change_by_time:
            return "change_by_time";
        default:
            return "random";
    }
}

std::vector<std::string> RuleEnumList{
        "loop",
        "random",
        "one_by_one",
        "change_by_time",
};

void ConfigLoader::print() {
    std::cout << "config.listenHost:" << config.listenHost << "\n";
    std::cout << "config.listenPort:" << config.listenPort << "\n";
    std::cout << "config.testRemoteHost:" << config.testRemoteHost << "\n";
    std::cout << "config.testRemotePort:" << config.testRemotePort << "\n";
    std::cout << "config.stateServerHost:" << config.stateServerHost << "\n";
    std::cout << "config.stateServerPort:" << config.stateServerPort << "\n";
    std::cout << "config.upstreamSelectRule:" << ruleEnum2string(config.upstreamSelectRule) << "\n";

    std::cout << "config.retryTimes:" << config.retryTimes << "\n";

    std::cout << "config.serverChangeTime:" << config.serverChangeTime.count() << "\n";

    std::cout << "config.connectTimeout:" << config.connectTimeout.count() << "\n";

    std::cout << "config.sleepTime:" << config.sleepTime.count() << "\n";

    std::cout << "config.tcpCheckPeriod:" << config.tcpCheckPeriod.count() << "\n";
    std::cout << "config.tcpCheckStart:" << config.tcpCheckStart.count() << "\n";

    std::cout << "config.connectCheckPeriod:" << config.connectCheckPeriod.count() << "\n";
    std::cout << "config.connectCheckStart:" << config.connectCheckStart.count() << "\n";

    std::cout << "config.additionCheckPeriod:" << config.additionCheckPeriod.count() << "\n";

    for (size_t i = 0; i != config.upstream.size(); ++i) {
        const auto &it = config.upstream[i];
        std::cout << "config.upstream [" << i << "]:\n";
        std::cout << "\t" << "upstream.name:" << it.name << "\n";
        std::cout << "\t" << "upstream.host:" << it.host << "\n";
        std::cout << "\t" << "upstream.port:" << it.port << "\n";
        std::cout << "\t" << "upstream.disable:" << it.disable << "\n";
    }

    for (size_t i = 0; i != config.multiListen.size(); ++i) {
        const auto &it = config.multiListen[i];
        std::cout << "config.multiListen [" << i << "]:\n";
        std::cout << "\t" << "multiListen.host:" << it.host << "\n";
        std::cout << "\t" << "multiListen.port:" << it.port << "\n";
    }

}

void ConfigLoader::load(const std::string &filename) {
    boost::property_tree::ptree tree;
    boost::property_tree::read_json(filename, tree);
    parse_json(tree);
}

void ConfigLoader::parse_json(const boost::property_tree::ptree &tree) {
    Config c{};

    auto listenHost = tree.get("listenHost", std::string{"127.0.0.1"});
    auto listenPort = tree.get<uint16_t>("listenPort", static_cast<uint16_t>(5000));

    c.listenHost = listenHost;
    c.listenPort = listenPort;

    auto testRemoteHost = tree.get("testRemoteHost", std::string{"www.google.com"});
    auto testRemotePort = tree.get<uint16_t>("testRemotePort", static_cast<uint16_t>(443));

    c.testRemoteHost = testRemoteHost;
    c.testRemotePort = testRemotePort;

    auto stateServerHost = tree.get("stateServerHost", std::string{"127.0.0.1"});
    auto stateServerPort = tree.get<uint16_t>("stateServerPort", static_cast<uint16_t>(5010));

    c.stateServerHost = stateServerHost;
    c.stateServerPort = stateServerPort;

    auto upstreamSelectRule = tree.get("upstreamSelectRule", std::string{"random"});
    c.upstreamSelectRule = string2RuleEnum(upstreamSelectRule);

    auto retryTimes = tree.get("retryTimes", static_cast<size_t>(3));
    c.retryTimes = retryTimes;

    auto serverChangeTime = tree.get("serverChangeTime", static_cast<long long>(60 * 1000));
    c.serverChangeTime = ConfigTimeDuration{serverChangeTime};

    auto connectTimeout = tree.get("connectTimeout", static_cast<long long>(2 * 1000));
    c.connectTimeout = ConfigTimeDuration{connectTimeout};

    auto sleepTime = tree.get("sleepTime", static_cast<long long>(30 * 60 * 1000));
    c.sleepTime = ConfigTimeDuration{sleepTime};

    auto tcpCheckPeriod = tree.get("tcpCheckPeriod", static_cast<long long>(5 * 1000));
    c.tcpCheckPeriod = ConfigTimeDuration{tcpCheckPeriod};
    auto tcpCheckStart = tree.get("tcpCheckStart", static_cast<long long>(1 * 1000));
    c.tcpCheckStart = ConfigTimeDuration{tcpCheckStart};

    auto connectCheckPeriod = tree.get("connectCheckPeriod", static_cast<long long>(5 * 60 * 1000));
    c.connectCheckPeriod = ConfigTimeDuration{connectCheckPeriod};
    auto connectCheckStart = tree.get("connectCheckStart", static_cast<long long>(1 * 1000));
    c.connectCheckStart = ConfigTimeDuration{connectCheckStart};

    auto additionCheckPeriod = tree.get("additionCheckPeriod", static_cast<long long>(10 * 1000));
    c.additionCheckPeriod = ConfigTimeDuration{additionCheckPeriod};

    if (tree.get_child_optional("upstream")) {
        auto upstream = tree.get_child("upstream");
        for (auto &item: upstream) {
            auto &pts = item.second;
            Upstream u;
            u.host = pts.get("host", std::string{"127.0.0.1"});
            u.port = pts.get("port", uint16_t{});
            u.name = pts.get("name", std::string{});
            u.disable = pts.get("disable", false);
            c.upstream.push_back(u);
        }
    }

    if (tree.get_child_optional("multiListen")) {
        auto multiListen = tree.get_child("multiListen");
        for (auto &item: multiListen) {
            auto &pts = item.second;
            MultiListen u;
            u.host = pts.get("host", std::string{"127.0.0.1"});
            u.port = pts.get("port", uint16_t{});
            c.multiListen.push_back(u);
        }
    }

    config = c;
}
