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
        config.put("serverChangeTime", c.serverChangeTime.count());
        config.put("connectTimeout", c.connectTimeout.count());
        config.put("tcpCheckPeriod", c.tcpCheckPeriod.count());
        config.put("tcpCheckStart", c.tcpCheckStart.count());
        config.put("connectCheckPeriod", c.connectCheckPeriod.count());
        config.put("connectCheckStart", c.connectCheckStart.count());
        config.put("additionCheckPeriod", c.additionCheckPeriod.count());

        boost::property_tree::ptree pUS;
        for (const auto &a:c.upstream) {
            boost::property_tree::ptree n;
            n.put("name", a.name);
            n.put("host", a.host);
            n.put("port", a.port);
            n.put("disable", a.disable);
            pUS.push_back(std::make_pair("", n));
        }
        config.add_child("upstream", pUS);

        root.add_child("config", config);
    }

    if (upstreamPool) {
        boost::property_tree::ptree pool;

        pool.put("getLastUseUpstreamIndex", upstreamPool->getLastUseUpstreamIndex());

        boost::property_tree::ptree pS;
        for (const auto &a : upstreamPool->pool()) {
            boost::property_tree::ptree n;
            n.put("index", a->index);
            n.put("name", a->name);
            n.put("host", a->host);
            n.put("port", a->port);
            n.put("isOffline", a->isOffline);
            n.put("lastConnectFailed", a->lastConnectFailed);
            n.put("isManualDisable", a->isManualDisable);
            n.put("disable", a->disable);
            n.put("connectCount", a->connectCount.load());
            n.put("lastOnlineTime", (
                    a->lastOnlineTime.has_value() ?
                    printUpstreamTimePoint(a->lastOnlineTime.value()) : "<empty>"));
            n.put("lastConnectTime", (
                    a->lastConnectTime.has_value() ?
                    printUpstreamTimePoint(a->lastConnectTime.value()) : "<empty>"));
            n.put("isWork", upstreamPool->checkServer(a));

            pS.push_back(std::make_pair("", n));
        }
        pool.add_child("upstream", pS);

        root.put("nowTime", printUpstreamTimePoint(UpstreamTimePointNow()));

        root.add_child("pool", pool);
    }

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

    auto origin = request_.at(boost::beast::http::field::origin);
    if (!origin.empty()) {
        response_.set(boost::beast::http::field::access_control_allow_origin, origin);
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

void HttpConnectSession::create_response() {
    if (request_.target() == "/") {
        response_.set(boost::beast::http::field::content_type, "text/json");
        boost::beast::ostream(response_.body())
                << createJsonString() << "\n";
    } else {
        response_.result(boost::beast::http::status::not_found);
        response_.set(boost::beast::http::field::content_type, "text/plain");
        boost::beast::ostream(response_.body()) << "File not found\r\n";
    }
}

void HttpConnectSession::write_response() {
    auto self = shared_from_this();

    response_.set(boost::beast::http::field::content_length, response_.body().size());

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
                            upstreamPool
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
