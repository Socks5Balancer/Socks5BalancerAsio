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

#include <sstream>

#include "./log/Log.h"
#include "./base64.h"

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
    if ("force_only_one" == s) {
        return RuleEnum::force_only_one;
    }
    if ("inherit" == s) {
        return RuleEnum::inherit;
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
        case RuleEnum::force_only_one:
            return "force_only_one";
        case RuleEnum::inherit:
            return "inherit";
        default:
            return "random";
    }
}

std::vector<std::string> RuleEnumList{
        "loop",
        "random",
        "one_by_one",
        "change_by_time",
        "force_only_one",
        "inherit",
};

void ConfigLoader::print() {
    std::stringstream ss;

    ss << "config.listenHost:" << config.listenHost << "\n";
    ss << "config.listenPort:" << config.listenPort << "\n";
    ss << "config.testRemoteHost:" << config.testRemoteHost << "\n";
    ss << "config.testRemotePort:" << config.testRemotePort << "\n";
    ss << "config.stateServerHost:" << config.stateServerHost << "\n";
    ss << "config.stateServerPort:" << config.stateServerPort << "\n";
    ss << "config.upstreamSelectRule:" << ruleEnum2string(config.upstreamSelectRule) << "\n";

    ss << "config.retryTimes:" << config.retryTimes << "\n";

    ss << "config.disableConnectTest:" << config.disableConnectTest << "\n";
    ss << "config.disableConnectionTracker:" << config.disableConnectionTracker << "\n";
    ss << "config.traditionTcpRelay:" << config.traditionTcpRelay << "\n";

    ss << "config.disableSocks4:" << config.disableSocks4 << "\n";

    ss << "config.serverChangeTime:" << config.serverChangeTime.count() << "\n";

    ss << "config.connectTimeout:" << config.connectTimeout.count() << "\n";

    ss << "config.sleepTime:" << config.sleepTime.count() << "\n";

    ss << "config.threadNum:" << config.threadNum << "\n";

    ss << "config.tcpCheckPeriod:" << config.tcpCheckPeriod.count() << "\n";
    ss << "config.tcpCheckStart:" << config.tcpCheckStart.count() << "\n";

    ss << "config.connectCheckPeriod:" << config.connectCheckPeriod.count() << "\n";
    ss << "config.connectCheckStart:" << config.connectCheckStart.count() << "\n";

    ss << "config.additionCheckPeriod:" << config.additionCheckPeriod.count() << "\n";

    for (size_t i = 0; i != config.upstream.size(); ++i) {
        const auto &it = config.upstream[i];
        ss << "config.upstream [" << i << "]:\n";
        ss << "\t" << "upstream.name:" << it.name << "\n";
        ss << "\t" << "upstream.host:" << it.host << "\n";
        ss << "\t" << "upstream.port:" << it.port << "\n";
        ss << "\t" << "upstream.authUser:" << it.authUser << "\n";
        ss << "\t" << "upstream.authPwd:" << it.authPwd << "\n";
        ss << "\t" << "upstream.disable:" << it.disable << "\n";
    }

    for (size_t i = 0; i != config.multiListen.size(); ++i) {
        const auto &it = config.multiListen[i];
        ss << "config.multiListen [" << i << "]:\n";
        ss << "\t" << "multiListen.host:" << it.host << "\n";
        ss << "\t" << "multiListen.port:" << it.port << "\n";
    }

    if (!config.embedWebServerConfig.enable) {
        ss << "config.embedWebServerConfig.enable : false .\n";
    } else {
        auto &ew = config.embedWebServerConfig;
        ss << "config.embedWebServerConfig.enable : true :\n";
        ss << "\t" << "embedWebServerConfig.host:" << ew.host << "\n";
        ss << "\t" << "embedWebServerConfig.port:" << ew.port << "\n";
        ss << "\t" << "embedWebServerConfig.root_path:" << ew.root_path << "\n";
        ss << "\t" << "embedWebServerConfig.index_file_of_root:" << ew.index_file_of_root << "\n";
        ss << "\t" << "embedWebServerConfig.backendHost:" << ew.backendHost << "\n";
        ss << "\t" << "embedWebServerConfig.backendPort:" << ew.backendPort << "\n";
        ss << "\t" << "embedWebServerConfig.backend_json_string:" << ew.backend_json_string << "\n";
    }


    for (size_t i = 0; i != config.authClientInfo.size(); ++i) {
        const auto &it = config.authClientInfo[i];
        ss << "config.AuthClientInfo [" << i << "]:\n";
        ss << "\t" << "AuthClientInfo.user:" << it.user << "\n";
        ss << "\t" << "AuthClientInfo.pwd:" << it.pwd << "\n";
        ss << "\t" << "AuthClientInfo.base64AuthString: " << it.base64AuthString << "\n";
    }
    if (config.authClientInfo.empty()) {
        ss << "config.authClientInfo.empty.\n";
    }

    ss << std::endl;

    BOOST_LOG_S5B(info) << ss.str();

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

    auto threadNum = tree.get("threadNum", static_cast<size_t>(0));
    c.threadNum = threadNum;

    auto retryTimes = tree.get("retryTimes", static_cast<size_t>(3));
    c.retryTimes = retryTimes;

#ifndef FORCE_disableConnectTest
    auto disableConnectTest = tree.get("disableConnectTest", false);
    c.disableConnectTest = disableConnectTest;
#else
    c.disableConnectTest = true;
#endif // FORCE_disableConnectTest
#ifndef FORCE_disableConnectionTracker
    auto disableConnectionTracker = tree.get("disableConnectionTracker", false);
    c.disableConnectionTracker = disableConnectionTracker;
#else
    c.disableConnectionTracker = true;
#endif // FORCE_disableConnectionTracker
#ifndef FORCE_traditionTcpRelay
    auto traditionTcpRelay = tree.get("traditionTcpRelay", false);
    c.traditionTcpRelay = traditionTcpRelay;
#else
    c.traditionTcpRelay = true;
#endif // FORCE_traditionTcpRelay

    auto disableSocks4 = tree.get("disableSocks4", true);
    c.disableSocks4 = disableSocks4;

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
            u.authUser = pts.get("authUser", std::string{});
            u.authPwd = pts.get("authPwd", std::string{});
            u.disable = pts.get("disable", false);
            u.slowImpl = pts.get("slowImpl", false);
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

    if (tree.get_child_optional("AuthClientInfo")) {
        auto multiListen = tree.get_child("AuthClientInfo");
        for (auto &item: multiListen) {
            auto &pts = item.second;
            AuthClientInfo u;
            u.user = pts.get("user", std::string{});
            u.pwd = pts.get("pwd", std::string{});
            u.base64AuthString = base64_encode_string(u.user + std::string{":"} + u.pwd);
            if (!u.user.empty()) {
                // username must not empty
                c.authClientInfo.push_back(u);
            }
        }
    }

    c.embedWebServerConfig = {};
    c.embedWebServerConfig.enable = false;
    c.embedWebServerConfig.host = "127.0.0.1";
    c.embedWebServerConfig.port = 5002;
    c.embedWebServerConfig.backendHost = "";
    c.embedWebServerConfig.backendPort = 0;
    c.embedWebServerConfig.root_path = "./html/";
    c.embedWebServerConfig.index_file_of_root = "stateBootstrap.html";
    c.embedWebServerConfig.allowFileExtList = "htm html js json jpg jpeg png bmp gif ico svg css";
#ifndef DISABLE_EmbedWebServer
    if (tree.get_child_optional("EmbedWebServerConfig")) {
        auto pts = tree.get_child("EmbedWebServerConfig");
        auto &embedWebServerConfig = c.embedWebServerConfig;
        embedWebServerConfig.enable = pts.get("enable", false);
        if (embedWebServerConfig.enable) {
            embedWebServerConfig.host = pts.get("host", embedWebServerConfig.host);
            embedWebServerConfig.port = pts.get("port", embedWebServerConfig.port);
            embedWebServerConfig.backendHost = pts.get("backendHost", embedWebServerConfig.backendHost);
            embedWebServerConfig.backendPort = pts.get("backendPort", embedWebServerConfig.backendPort);
            embedWebServerConfig.root_path = pts.get("root_path", embedWebServerConfig.root_path);
            embedWebServerConfig.index_file_of_root = pts.get("index_file_of_root",
                                                              embedWebServerConfig.index_file_of_root);
            embedWebServerConfig.allowFileExtList = pts.get("allowFileExtList", embedWebServerConfig.allowFileExtList);

        }
    }
#endif // DISABLE_EmbedWebServer


    c.embedWebServerConfig.backend_json_string = (
            boost::format{R"({"host":"%1%","port":%2%})"}
            % (!c.embedWebServerConfig.backendHost.empty()
               ? c.embedWebServerConfig.backendHost : "")
            % (c.embedWebServerConfig.backendPort != 0
               ? c.embedWebServerConfig.backendPort : c.stateServerPort)
    ).str();


    config = c;
}
