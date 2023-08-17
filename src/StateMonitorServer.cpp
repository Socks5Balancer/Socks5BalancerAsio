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

#include "StateMonitorServer.h"
#include "TcpRelaySession.h"

#include <regex>
#include <type_traits>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "SessionRelayId.h"

std::string HttpConnectSession::createJsonString() {
    boost::property_tree::ptree root;

    if (configLoader) {
        boost::property_tree::ptree config;

        const auto &c = configLoader->config;
        config.put("listenHost", c.listenHost);
        config.put("listenPort", c.listenPort);
        config.put("testRemoteHost", c.testRemoteHost);
        config.put("testRemotePort", c.testRemotePort);
        config.put("stateServerHost", c.stateServerHost);
        config.put("stateServerPort", c.stateServerPort);
        config.put("retryTimes", c.retryTimes);
        config.put("disableConnectTest", c.disableConnectTest);
        config.put("traditionTcpRelay", c.traditionTcpRelay);
        config.put("serverChangeTime", c.serverChangeTime.count());
        config.put("connectTimeout", c.connectTimeout.count());
        config.put("sleepTime", c.sleepTime.count());
        config.put("tcpCheckPeriod", c.tcpCheckPeriod.count());
        config.put("tcpCheckStart", c.tcpCheckStart.count());
        config.put("connectCheckPeriod", c.connectCheckPeriod.count());
        config.put("connectCheckStart", c.connectCheckStart.count());
        config.put("additionCheckPeriod", c.additionCheckPeriod.count());

        config.put("relayId", SessionRelayId::readRelayId());
        config.put("relayIdMod", SessionRelayId::relayIdMod());

        boost::property_tree::ptree pUS;
        for (const auto &a: c.upstream) {
            boost::property_tree::ptree n;
            n.put("name", a.name);
            n.put("host", a.host);
            n.put("port", a.port);
            n.put("disable", a.disable);
            pUS.push_back(std::make_pair("", n));
        }
        config.add_child("upstream", pUS);

        const auto &ews = c.embedWebServerConfig;
        boost::property_tree::ptree pEWS;
        {
            pEWS.put("enable", ews.enable);
            pEWS.put("host", ews.host);
            pEWS.put("port", ews.port);
            pEWS.put("backendHost", ews.backendHost);
            pEWS.put("backendPort", ews.backendPort);
            pEWS.put("root_path", ews.root_path);
            pEWS.put("index_file_of_root", ews.index_file_of_root);
            pEWS.put("backend_json_string", ews.backend_json_string);
        }
        config.add_child("EmbedWebServerConfig", pEWS);

        root.add_child("config", config);

        root.put("nowRule", ruleEnum2string(configLoader->config.upstreamSelectRule));
    }

    if (upstreamPool) {
        boost::property_tree::ptree pool;

        pool.put("getLastUseUpstreamIndex", upstreamPool->getLastUseUpstreamIndex());
        pool.put("lastConnectComeTime", printUpstreamTimePoint(upstreamPool->getLastConnectComeTime()));
        pool.put("lastConnectComeTimeAgo", std::chrono::duration_cast<std::chrono::milliseconds>(
                UpstreamTimePointNow() - upstreamPool->getLastConnectComeTime()
        ).count());

        auto pT = tcpRelayServer.lock();
        decltype(std::declval<decltype(pT)::element_type>().getStatisticsInfo()) info;
        if (pT) {
            info = pT->getStatisticsInfo();
        }

        boost::property_tree::ptree pS;
        for (const auto &a: upstreamPool->pool()) {
            boost::property_tree::ptree n;
            n.put("index", a->index);
            n.put("name", a->name);
            n.put("host", a->host);
            n.put("port", a->port);
            n.put("isOffline", a->isOffline);
            n.put("lastConnectFailed", a->lastConnectFailed);
            n.put("isManualDisable", a->isManualDisable);
            n.put("disable", a->disable);
            n.put("lastConnectCheckResult", a->lastConnectCheckResult);
            n.put("connectCount", a->connectCount.load());
            n.put("lastOnlineTime", (
                    a->lastOnlineTime.has_value() ?
                    printUpstreamTimePoint(a->lastOnlineTime.value()) : "<empty>"));
            n.put("lastConnectTime", (
                    a->lastConnectTime.has_value() ?
                    printUpstreamTimePoint(a->lastConnectTime.value()) : "<empty>"));
            n.put("isWork", upstreamPool->checkServer(a));

            if (info) {
                auto iInfo = info->getInfo(a->index);
                if (iInfo) {
                    n.put("byteDownChange", iInfo->byteDownChange);
                    n.put("byteUpChange", iInfo->byteUpChange);
                    n.put("byteDownLast", iInfo->byteDownLast);
                    n.put("byteUpLast", iInfo->byteUpLast);
                    n.put("byteUpChangeMax", iInfo->byteUpChangeMax);
                    n.put("byteDownChangeMax", iInfo->byteDownChangeMax);
                    n.put("sessionsCount", iInfo->calcSessionsNumber());
                    n.put("connectCount2", iInfo->connectCount.load());
                    n.put("byteInfo", true);
                } else {
                    n.put("byteDownChange", 0ll);
                    n.put("byteUpChange", 0ll);
                    n.put("byteDownLast", 0ll);
                    n.put("byteUpLast", 0ll);
                    n.put("byteUpChangeMax", 0ll);
                    n.put("byteDownChangeMax", 0ll);
                    n.put("sessionsCount", 0ll);
                    n.put("connectCount2", 0ll);
                    n.put("byteInfo", false);
                }
            }

            if (!n.get_child_optional("byteDownChange").has_value()) {
                n.put("byteInfo", false);
            }

            pS.push_back(std::make_pair("", n));
        }
        pool.add_child("upstream", pS);

        root.add_child("pool", pool);
    }

    if (!RuleEnumList.empty()) {
        boost::property_tree::ptree rs;
        for (const auto &a: RuleEnumList) {
            boost::property_tree::ptree n;
            n.put("", a);
            rs.push_back(std::make_pair("", n));
        }
        root.add_child("RuleEnumList", rs);
    }

    if (auto pT = tcpRelayServer.lock()) {
        decltype(std::declval<decltype(pT)::element_type>().getStatisticsInfo()) info;
        if (pT) {
            info = pT->getStatisticsInfo();
            root.put("lastConnectServerIndex", info->lastConnectServerIndex);

            boost::property_tree::ptree pS;
            for (const auto &a: info->getUpstreamIndex()) {
                boost::property_tree::ptree n;

                n.put("index", a.first);
                n.put("connectCount", a.second->connectCount.load());
                n.put("sessionsCount", a.second->calcSessionsNumber());
                n.put("byteDownChange", a.second->byteDownChange);
                n.put("byteUpChange", a.second->byteUpChange);
                n.put("byteDownLast", a.second->byteDownLast);
                n.put("byteUpLast", a.second->byteUpLast);
                n.put("byteUpChangeMax", a.second->byteUpChangeMax);
                n.put("byteDownChangeMax", a.second->byteDownChangeMax);
                n.put("rule", ruleEnum2string(a.second->rule));
                n.put("lastUseUpstreamIndex", a.second->lastUseUpstreamIndex);

                pS.push_back(std::make_pair("", n));
            }
            root.add_child("UpstreamIndex", pS);

            boost::property_tree::ptree pC;
            for (const auto &a: info->getClientIndex()) {
                boost::property_tree::ptree n;

                n.put("index", a.first);
                n.put("connectCount", a.second->connectCount.load());
                n.put("sessionsCount", a.second->calcSessionsNumber());
                n.put("byteDownChange", a.second->byteDownChange);
                n.put("byteUpChange", a.second->byteUpChange);
                n.put("byteDownLast", a.second->byteDownLast);
                n.put("byteUpLast", a.second->byteUpLast);
                n.put("byteUpChangeMax", a.second->byteUpChangeMax);
                n.put("byteDownChangeMax", a.second->byteDownChangeMax);
                n.put("rule", ruleEnum2string(a.second->rule));
                n.put("lastUseUpstreamIndex", a.second->lastUseUpstreamIndex);

                pC.push_back(std::make_pair("", n));
            }
            root.add_child("ClientIndex", pC);

            boost::property_tree::ptree pL;
            for (const auto &a: info->getListenIndex()) {
                boost::property_tree::ptree n;

                n.put("index", a.first);
                n.put("connectCount", a.second->connectCount.load());
                n.put("sessionsCount", a.second->calcSessionsNumber());
                n.put("byteDownChange", a.second->byteDownChange);
                n.put("byteUpChange", a.second->byteUpChange);
                n.put("byteDownLast", a.second->byteDownLast);
                n.put("byteUpLast", a.second->byteUpLast);
                n.put("byteUpChangeMax", a.second->byteUpChangeMax);
                n.put("byteDownChangeMax", a.second->byteDownChangeMax);
                n.put("rule", ruleEnum2string(a.second->rule));
                n.put("lastUseUpstreamIndex", a.second->lastUseUpstreamIndex);

                pL.push_back(std::make_pair("", n));
            }
            root.add_child("ListenIndex", pL);

        }
    }
    root.put("startTime", printUpstreamTimePoint(startTime));
    root.put("runTime", std::chrono::duration_cast<std::chrono::milliseconds>(
            UpstreamTimePointNow() - startTime
    ).count());
    root.put("nowTime", printUpstreamTimePoint(UpstreamTimePointNow()));

    std::stringstream ss;
    boost::property_tree::write_json(ss, root);
    return ss.str();
}

void HttpConnectSession::read_request() {
    auto self = shared_from_this();

    boost::beast::http::async_read(
            socket_,
            buffer_,
            request_,
            [self](boost::beast::error_code ec,
                   std::size_t bytes_transferred) {
                boost::ignore_unused(bytes_transferred);
                if (!ec)
                    self->process_request();
            });
}

void HttpConnectSession::process_request() {
    response_.version(request_.version());
    response_.keep_alive(false);

    if (request_.find(boost::beast::http::field::origin) != request_.end()) {
        auto origin = request_.at(boost::beast::http::field::origin);
        if (!origin.empty()) {
            response_.set(boost::beast::http::field::access_control_allow_origin, origin);
        }
    }

    switch (request_.method()) {
        case boost::beast::http::verb::get:
            response_.result(boost::beast::http::status::ok);
            response_.set(boost::beast::http::field::server, "Beast");
            create_response();
            break;

        default:
            // We return responses indicating an error if
            // we do not recognize the request method.
            response_.result(boost::beast::http::status::bad_request);
            response_.set(boost::beast::http::field::content_type, "text/plain");
            boost::beast::ostream(response_.body())
                    << "Invalid request-method '"
                    << std::string(request_.method_string())
                    << "'";
            break;
    }

    write_response();
}

void HttpConnectSession::path_op(HttpConnectSession::QueryPairsType &queryPairs) {
    response_.result(boost::beast::http::status::ok);

    if (!queryPairs.empty()) {
//        // find first not key that not start with '_', otherwise first item
//        auto qIt = std::find_if(
//                queryPairs.begin(), queryPairs.end(),
//                [](const QueryPairsType::value_type &a) {
//                    return a.first.front() != '_';
//                }
//        );
//        if (qIt == queryPairs.end()) {
//            qIt = queryPairs.begin();
//        }
//        HttpConnectSession::QueryPairsType::value_type q = *qIt;

        // find target
        auto targetMode = queryPairs.find("_targetMode");
//        auto targetMode = std::find_if(
//                queryPairs.begin(), queryPairs.end(),
//                [](const QueryPairsType::value_type &t) {
//                    return t.first == "_targetMode";
//                }
//        );
        auto target = queryPairs.find("_target");
//        auto target = std::find_if(
//                queryPairs.begin(), queryPairs.end(),
//                [](const QueryPairsType::value_type &t) {
//                    return t.first == "_target";
//                }
//        );

        try {
            if (queryPairs.count("enable") > 0) {
                auto q = *queryPairs.find("enable");
                auto index = boost::lexical_cast<size_t>(q.second);
                if (index >= 0 && index < upstreamPool->pool().size()) {
                    upstreamPool->pool().at(index)->isManualDisable = false;
                }
            }
            if (queryPairs.count("disable") > 0) {
                auto q = *queryPairs.find("disable");
                auto index = boost::lexical_cast<size_t>(q.second);
                if (index >= 0 && index < upstreamPool->pool().size()) {
                    upstreamPool->pool().at(index)->isManualDisable = true;
                }
            }
            if (queryPairs.count("forceNowUseServer") > 0) {
                auto q = *queryPairs.find("forceNowUseServer");
                auto index = boost::lexical_cast<size_t>(q.second);
                if (index >= 0 && index < upstreamPool->pool().size()) {
                    if (targetMode != queryPairs.end()
                        && target != queryPairs.end()) {
                        if (auto ptr = tcpRelayServer.lock()) {
                            auto t = targetMode->second;
                            if ("client" == t) {
                                if (ptr->getStatisticsInfo()) {
                                    auto info = ptr->getStatisticsInfo()->getInfoClient(target->second);
                                    if (info) {
                                        info->lastUseUpstreamIndex = index;
                                    }
                                }
                            } else if ("listen" == t) {
                                if (ptr->getStatisticsInfo()) {
                                    auto info = ptr->getStatisticsInfo()->getInfoListen(target->second);
                                    if (info) {
                                        info->lastUseUpstreamIndex = index;
                                    }
                                }
                            }
                        }
                    } else {
                        // global
                        upstreamPool->forceSetLastUseUpstreamIndex(index);
                    }
                }
            }
            if (queryPairs.count("forceCheckServer") > 0) {
                auto q = *queryPairs.find("forceCheckServer");
                auto index = boost::lexical_cast<size_t>(q.second);
                if (index >= 0 && index < upstreamPool->pool().size()) {
                    upstreamPool->forceCheckOne(index);
                }
            }
            if (queryPairs.count("enableAllServer") > 0) {
                auto q = *queryPairs.find("enableAllServer");
                auto index = boost::lexical_cast<size_t>(q.second);
                if (index == 1) {
                    for (auto &a: upstreamPool->pool()) {
                        a->isManualDisable = false;
                    }
                }
            }
            if (queryPairs.count("disableAllServer") > 0) {
                auto q = *queryPairs.find("disableAllServer");
                auto index = boost::lexical_cast<size_t>(q.second);
                if (index == 1) {
                    for (auto &a: upstreamPool->pool()) {
                        a->isManualDisable = true;
                    }
                }
            }
            if (queryPairs.count("cleanAllCheckState") > 0) {
                auto q = *queryPairs.find("cleanAllCheckState");
                auto index = boost::lexical_cast<size_t>(q.second);
                if (index == 1) {
                    for (auto &a: upstreamPool->pool()) {
                        a->isOffline = false;
                        a->lastConnectFailed = false;
                        a->lastOnlineTime.reset();
                        a->lastConnectTime.reset();
                    }
                    // recheck
                    upstreamPool->forceCheckNow();
                }
            }
            if (queryPairs.count("forceCheckAllServer") > 0) {
                auto q = *queryPairs.find("forceCheckAllServer");
                auto index = boost::lexical_cast<size_t>(q.second);
                if (index == 1) {
                    upstreamPool->forceCheckNow();
                }
            }
            if (queryPairs.count("endConnectOnServer") > 0) {
                auto q = *queryPairs.find("endConnectOnServer");
                auto index = boost::lexical_cast<size_t>(q.second);
                if (index >= 0 && index < upstreamPool->pool().size()) {
                    auto p = tcpRelayServer.lock();
                    if (p) {
                        p->getStatisticsInfo()->closeAllSession(index);
                    }
                }
            }
            if (queryPairs.count("endAllConnect") > 0) {
                auto q = *queryPairs.find("endAllConnect");
                auto index = boost::lexical_cast<size_t>(q.second);
                if (targetMode != queryPairs.end()
                    && target != queryPairs.end()) {
                    if (index == 0) {
                        if (auto ptr = tcpRelayServer.lock()) {
                            auto t = targetMode->second;
                            if ("client" == t) {
                                if (ptr->getStatisticsInfo()) {
                                    auto info = ptr->getStatisticsInfo()->getInfoClient(target->second);
                                    if (info) {
                                        info->closeAllSession();
                                    }
                                }
                            } else if ("listen" == t) {
                                if (ptr->getStatisticsInfo()) {
                                    auto info = ptr->getStatisticsInfo()->getInfoListen(target->second);
                                    if (info) {
                                        info->closeAllSession();
                                    }
                                }
                            }
                        }
                    }
                    if (index == 2) {
                        auto _upstream = queryPairs.find("_upstream");
                        if (_upstream != queryPairs.end()) {
                            // TODO
                        }
                    }
                } else {
                    // global
                    if (index == 1) {
                        auto p = tcpRelayServer.lock();
                        if (p) {
                            p->closeAllSession();
                        }
                    }
                }
            }
            if (queryPairs.count("newRule") > 0) {
                auto q = *queryPairs.find("newRule");
                std::optional<decltype(RuleEnumList)::value_type> n;
                for (const auto &a: RuleEnumList) {
                    if (a == q.second) {
                        n = a;
                        break;
                    }
                }
                if (n.has_value()) {
                    if (targetMode != queryPairs.end()
                        && target != queryPairs.end()) {
                        if (auto ptr = tcpRelayServer.lock()) {
                            auto t = targetMode->second;
                            if ("client" == t) {
                                if (ptr->getStatisticsInfo()) {
                                    auto info = ptr->getStatisticsInfo()->getInfoClient(target->second);
                                    if (info) {
                                        info->rule = string2RuleEnum(n.value());
                                    }
                                }
                            } else if ("listen" == t) {
                                if (ptr->getStatisticsInfo()) {
                                    auto info = ptr->getStatisticsInfo()->getInfoListen(target->second);
                                    if (info) {
                                        info->rule = string2RuleEnum(n.value());
                                    }
                                }
                            }
                        }
                    } else {
                        // global
                        configLoader->config.upstreamSelectRule = string2RuleEnum(n.value());
                        if (RuleEnum::inherit == configLoader->config.upstreamSelectRule) {
                            configLoader->config.upstreamSelectRule = RuleEnum::random;
                        }
                    }
                } else {
                    response_.result(boost::beast::http::status::bad_request);
                    response_.set(boost::beast::http::field::content_type, "text/plain");
                    boost::beast::ostream(response_.body()) << "newRule not in RuleEnumList" << "\r\n";
                }
            }
        } catch (const boost::bad_lexical_cast &e) {
            std::cout << "boost::bad_lexical_cast:" << e.what() << std::endl;
            response_.result(boost::beast::http::status::bad_request);
            response_.set(boost::beast::http::field::content_type, "text/plain");
            boost::beast::ostream(response_.body()) << "boost::bad_lexical_cast:" << e.what() << "\r\n";
        } catch (const std::exception &e) {
            std::cout << "std::exception:" << e.what() << std::endl;
            response_.result(boost::beast::http::status::bad_request);
            response_.set(boost::beast::http::field::content_type, "text/plain");
            boost::beast::ostream(response_.body()) << "std::exception:" << e.what() << "\r\n";
        }
    }
}

void HttpConnectSession::path_per_info(HttpConnectSession::QueryPairsType &queryPairs) {
    response_.result(boost::beast::http::status::ok);

    if (!queryPairs.empty()) {
        auto targetMode = queryPairs.find("targetMode");
        auto target = queryPairs.find("target");

        try {
            if (queryPairs.end() == targetMode || queryPairs.end() == target) {
                response_.result(boost::beast::http::status::bad_request);
                response_.set(boost::beast::http::field::content_type, "text/plain");
                return;
            }

            if (auto pT = tcpRelayServer.lock()) {

                decltype(std::declval<decltype(pT)::element_type>().getStatisticsInfo()) info;
                if (pT) {
                    info = pT->getStatisticsInfo();

                    bool valid = false;

                    boost::property_tree::ptree root;

                    boost::property_tree::ptree pool;
                    for (const auto &a: upstreamPool->pool()) {
                        boost::property_tree::ptree n;
                        n.put("index", a->index);
                        n.put("name", a->name);
                        n.put("host", a->host);
                        n.put("port", a->port);
                        n.put("isOffline", a->isOffline);
                        n.put("lastConnectFailed", a->lastConnectFailed);
                        n.put("isManualDisable", a->isManualDisable);
                        n.put("disable", a->disable);
                        n.put("lastConnectCheckResult", a->lastConnectCheckResult);
                        n.put("connectCount", a->connectCount.load());
                        n.put("lastOnlineTime", (
                                a->lastOnlineTime.has_value() ?
                                printUpstreamTimePoint(a->lastOnlineTime.value()) : "<empty>"));
                        n.put("lastConnectTime", (
                                a->lastConnectTime.has_value() ?
                                printUpstreamTimePoint(a->lastConnectTime.value()) : "<empty>"));
                        n.put("isWork", upstreamPool->checkServer(a));

                        if (info) {
                            auto iInfo = info->getInfo(a->index);
                            if (iInfo) {
                                n.put("byteDownChange", iInfo->byteDownChange);
                                n.put("byteUpChange", iInfo->byteUpChange);
                                n.put("byteDownLast", iInfo->byteDownLast);
                                n.put("byteUpLast", iInfo->byteUpLast);
                                n.put("byteUpChangeMax", iInfo->byteUpChangeMax);
                                n.put("byteDownChangeMax", iInfo->byteDownChangeMax);
                                n.put("sessionsCount", iInfo->calcSessionsNumber());
                                n.put("connectCount2", iInfo->connectCount.load());
                                n.put("byteInfo", true);
                            } else {
                                n.put("byteDownChange", 0ll);
                                n.put("byteUpChange", 0ll);
                                n.put("byteDownLast", 0ll);
                                n.put("byteUpLast", 0ll);
                                n.put("byteUpChangeMax", 0ll);
                                n.put("byteDownChangeMax", 0ll);
                                n.put("sessionsCount", 0ll);
                                n.put("connectCount2", 0ll);
                                n.put("byteInfo", false);
                            }
                        }

                        if (!n.get_child_optional("byteDownChange").has_value()) {
                            n.put("byteInfo", false);
                        }

                        pool.push_back(std::make_pair("", n));
                    }
                    root.add_child("UpstreamPool", pool);

                    boost::property_tree::ptree pS;
                    for (const auto &a: info->getUpstreamIndex()) {
                        boost::property_tree::ptree n;

                        n.put("index", a.first);
                        n.put("connectCount", a.second->connectCount.load());
                        n.put("sessionsCount", a.second->calcSessionsNumber());
                        n.put("byteDownChange", a.second->byteDownChange);
                        n.put("byteUpChange", a.second->byteUpChange);
                        n.put("byteDownLast", a.second->byteDownLast);
                        n.put("byteUpLast", a.second->byteUpLast);
                        n.put("byteUpChangeMax", a.second->byteUpChangeMax);
                        n.put("byteDownChangeMax", a.second->byteDownChangeMax);
                        n.put("rule", ruleEnum2string(a.second->rule));
                        n.put("lastUseUpstreamIndex", a.second->lastUseUpstreamIndex);

                        pS.push_back(std::make_pair("", n));
                    }
                    root.add_child("UpstreamIndex", pS);


                    if ("client" == targetMode->second) {
                        if (auto in = info->getInfoClient(target->second)) {
                            valid = true;

                            boost::property_tree::ptree pC;
                            {
                                std::lock_guard lg{in->sessionsMtx};
                                for (const auto &wp: in->sessions) {
                                    if (auto a = wp.ptr.lock()) {
                                        boost::property_tree::ptree n;

                                        if (auto s = a->getNowServer()) {
                                            n.put("serverIndex", s->index);
                                        }
                                        n.put("ClientEndpoint", a->getClientEndpointAddrString());
                                        n.put("ListenEnd", a->getListenEndpointAddrString());
                                        n.put("TargetEndpoint", a->getTargetEndpointAddrString());
                                        n.put("TargetHost", wp.host);
                                        n.put("TargetPort", wp.post);
                                        n.put("StartTime", wp.startTime);

                                        pC.push_back(std::make_pair("", n));
                                    }
                                }
                            }
                            root.add_child("ClientIndex", pC);

                            boost::property_tree::ptree baseInfo;
                            baseInfo.put("index", targetMode->second);
                            baseInfo.put("connectCount", in->connectCount.load());
                            baseInfo.put("sessionsCount", in->calcSessionsNumber());
                            baseInfo.put("byteDownChange", in->byteDownChange);
                            baseInfo.put("byteUpChange", in->byteUpChange);
                            baseInfo.put("byteDownLast", in->byteDownLast);
                            baseInfo.put("byteUpLast", in->byteUpLast);
                            baseInfo.put("byteUpChangeMax", in->byteUpChangeMax);
                            baseInfo.put("byteDownChangeMax", in->byteDownChangeMax);
                            baseInfo.put("rule", ruleEnum2string(in->rule));
                            baseInfo.put("lastUseUpstreamIndex", in->lastUseUpstreamIndex);
                            root.add_child("BaseInfo", baseInfo);
                        }
                    }

                    if ("listen" == targetMode->second) {
                        if (auto in = info->getInfoListen(target->second)) {
                            valid = true;

                            boost::property_tree::ptree pL;
                            {
                                std::lock_guard lg{in->sessionsMtx};
                                for (const auto &wp: in->sessions) {
                                    if (auto a = wp.ptr.lock()) {
                                        boost::property_tree::ptree n;

                                        if (auto s = a->getNowServer()) {
                                            n.put("serverIndex", s->index);
                                        }
                                        n.put("ClientEndpoint", a->getClientEndpointAddrString());
                                        n.put("ListenEnd", a->getListenEndpointAddrString());
                                        n.put("TargetEndpoint", a->getTargetEndpointAddrString());
                                        n.put("TargetHost", wp.host);
                                        n.put("TargetPort", wp.post);
                                        n.put("StartTime", wp.startTime);

                                        pL.push_back(std::make_pair("", n));
                                    }
                                }
                            }
                            root.add_child("ListenIndex", pL);

                            boost::property_tree::ptree baseInfo;
                            baseInfo.put("index", targetMode->second);
                            baseInfo.put("connectCount", in->connectCount.load());
                            baseInfo.put("sessionsCount", in->calcSessionsNumber());
                            baseInfo.put("byteDownChange", in->byteDownChange);
                            baseInfo.put("byteUpChange", in->byteUpChange);
                            baseInfo.put("byteDownLast", in->byteDownLast);
                            baseInfo.put("byteUpLast", in->byteUpLast);
                            baseInfo.put("byteUpChangeMax", in->byteUpChangeMax);
                            baseInfo.put("byteDownChangeMax", in->byteDownChangeMax);
                            baseInfo.put("rule", ruleEnum2string(in->rule));
                            baseInfo.put("lastUseUpstreamIndex", in->lastUseUpstreamIndex);
                            root.add_child("BaseInfo", baseInfo);
                        }
                    }

                    if (valid) {
                        std::stringstream ss;
                        boost::property_tree::write_json(ss, root);

                        response_.result(boost::beast::http::status::ok);
                        response_.set(boost::beast::http::field::content_type, "text/json");
                        boost::beast::ostream(response_.body()) << ss.str() << "\n";
                        return;
                    }
                }
            }

            response_.result(boost::beast::http::status::precondition_required);
            response_.set(boost::beast::http::field::content_type, "text/plain");
            boost::beast::ostream(response_.body()) << "target cannot find or not valid" << "\r\n";
            return;

        } catch (const boost::bad_lexical_cast &e) {
            std::cout << "boost::bad_lexical_cast:" << e.what() << std::endl;
            response_.result(boost::beast::http::status::bad_request);
            response_.set(boost::beast::http::field::content_type, "text/plain");
            boost::beast::ostream(response_.body()) << "boost::bad_lexical_cast:" << e.what() << "\r\n";
        } catch (const std::exception &e) {
            std::cout << "std::exception:" << e.what() << std::endl;
            response_.result(boost::beast::http::status::bad_request);
            response_.set(boost::beast::http::field::content_type, "text/plain");
            boost::beast::ostream(response_.body()) << "std::exception:" << e.what() << "\r\n";
        }
    }
}

void HttpConnectSession::create_response() {
    std::cout << "request_.target():" << request_.target() << std::endl;

    if (request_.target() == "/") {
        response_.set(boost::beast::http::field::content_type, "text/json");
        boost::beast::ostream(response_.body())
                << createJsonString() << "\n";
        return;
    }

    // from https://github.com/boostorg/beast/issues/787#issuecomment-376259849
    static const std::regex PARSE_URL{R"((/([^ ?]+)?)?/?\??([^/ ]+\=[^/ ]+)?)",
                                      std::regex_constants::ECMAScript | std::regex_constants::icase};
    std::smatch match;
    auto url = std::string{request_.target()};
    if (std::regex_match(url, match, PARSE_URL) && match.size() == 4) {
        std::string path = match[1];
        std::string query = match[3];

        std::vector<std::string> queryList;
        boost::split(queryList, query, boost::is_any_of("&"));

        HttpConnectSession::QueryPairsType queryPairs;
        for (const auto &a: queryList) {
            std::vector<std::string> p;
            boost::split(p, a, boost::is_any_of("="));
            if (p.size() == 1) {
                queryPairs.emplace(p.at(0), "");
            }
            if (p.size() == 2) {
                queryPairs.emplace(p.at(0), p.at(1));
            }
            if (p.size() > 2) {
                std::stringstream ss;
                for (size_t i = 1; i != p.size(); ++i) {
                    ss << p.at(i);
                }
                queryPairs.emplace(p.at(0), ss.str());
            }
        }

        std::cout << "query:" << query << std::endl;
        std::cout << "queryList:" << "\n";
        for (const auto &a: queryList) {
            std::cout << "\t" << a;
        }
        std::cout << std::endl;
        std::cout << "queryPairs:" << "\n";
        for (const auto &a: queryPairs) {
            std::cout << "\t" << a.first << " = " << a.second;
        }
        std::cout << std::endl;

        if (path == "/op") {
            return path_op(queryPairs);
        }
        if (path == "/clientInfo") {
            return path_per_info(queryPairs);
        }
        if (path == "/listenInfo") {
            return path_per_info(queryPairs);
        }
    }

    response_.result(boost::beast::http::status::not_found);
    response_.set(boost::beast::http::field::content_type, "text/plain");
    boost::beast::ostream(response_.body()) << "File not found\r\n";

}

void HttpConnectSession::write_response() {
    auto self = shared_from_this();

    response_.set(boost::beast::http::field::content_length, std::to_string(response_.body().size()));

    boost::beast::http::async_write(
            socket_,
            response_,
            [self](boost::beast::error_code ec, std::size_t) {
                self->socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
                self->deadline_.cancel();
            });
}

void HttpConnectSession::check_deadline() {
    auto self = shared_from_this();

    deadline_.async_wait(
            [self](boost::beast::error_code ec) {
                if (!ec) {
                    // Close socket to cancel any outstanding operation.
                    self->socket_.close(ec);
                }
            });
}

void StateMonitorServer::http_server() {
    acceptor.async_accept(
            socket,
            [this, self = shared_from_this()](boost::beast::error_code ec) {
                if (!ec) {
                    std::make_shared<HttpConnectSession>(
                            std::move(socket),
                            configLoader,
                            upstreamPool,
                            tcpRelayServer,
                            startTime
                    )->start();
                }
                if (ec && (
                        boost::asio::error::operation_aborted == ec ||
                        boost::asio::error::host_unreachable == ec ||
                        boost::asio::error::address_in_use == ec ||
                        boost::asio::error::address_family_not_supported == ec ||
                        boost::asio::error::host_unreachable == ec ||
                        boost::asio::error::host_unreachable == ec
                )) {
                    std::cerr << "StateMonitorServer http_server async_accept error:"
                              << ec << std::endl;
                    return;
                }
                http_server();
            });
}
