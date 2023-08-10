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

#include "HttpServerImpl.h"

#include <string_view>

#include <boost/algorithm/string.hpp>

#include "../ProxyHandshakeAuth.h"
#include "../base64.h"


// from https://github.com/boostorg/beast/issues/787#issuecomment-376259849
HttpServerImpl::ParsedURI HttpServerImpl::parseURI(const std::string &url) {
    ParsedURI result;
    auto value_or = [](const std::string &value, std::string &&deflt) -> std::string {
        return (value.empty() ? deflt : value);
    };
    // Note: only "http", "https", "ws", and "wss" protocols are supported
    static const std::regex PARSE_URL{R"((([httpsw]{2,5})://)?([^/ :]+)(:(\d+))?(/([^ ?]+)?)?/?\??([^/ ]+\=[^/ ]+)?)",
                                      std::regex_constants::ECMAScript | std::regex_constants::icase};
    std::smatch match;
    if (std::regex_match(url, match, PARSE_URL) && match.size() == 9) {
        result.protocol = value_or(boost::algorithm::to_lower_copy(std::string(match[2])), "http");
        result.domain = match[3];
        const bool is_sequre_protocol = (result.protocol == "https" || result.protocol == "wss");
        result.port = value_or(match[5], (is_sequre_protocol) ? "443" : "80");
        result.resource = value_or(match[6], "/");
        result.query = match[8];
        if (result.domain.empty()) {
            fail({}, "result.domain.empty()");
        }
    } else {
        fail({}, "regex_match fail");
    }
    return result;
}

bool HttpServerImpl::checkHeaderAuthString(
        const std::string_view &base64Part,
        const std::shared_ptr<decltype(parents)::element_type> &ptr
) {
    if (ptr->authClientManager->checkAuth_Base64AuthString(base64Part)) {
        BOOST_LOG_S5B(trace) << "checkHeaderAuthString Base64AuthString ok: " << base64Part;
        return true;
    } else {
        auto rr = base64_decode_string(base64Part);
        auto indexSplitFlag = rr.find(':');
        if (indexSplitFlag == decltype(rr)::npos || indexSplitFlag < 1) {
            // bad format
            BOOST_LOG_S5B(warning) << "checkHeaderAuthString bad format: " << base64Part << " rr: " << rr;
            return false;
        }
        std::string_view user{rr.data(), indexSplitFlag};
        std::string_view pwd{rr.data() + indexSplitFlag + 1, rr.length() - indexSplitFlag - 1};
        if (ptr->authClientManager->checkAuth(user, pwd)) {
            BOOST_LOG_S5B(trace) << "checkHeaderAuthString ok: " << user << " : " << pwd;
            return true;
        } else {
            BOOST_LOG_S5B(warning) << "checkHeaderAuthString wrong: " << user << " : " << pwd;
            return false;
        }
    }
}

void HttpServerImpl::do_whenError(boost::system::error_code error) {
    auto ptr = parents.lock();
    if (ptr) {
        ptr->do_whenError(error);
    }
}

void HttpServerImpl::do_analysis_client_first_http_header() {
    // do_downstream_read
    auto ptr = parents.lock();
    if (ptr) {
        auto d = ptr->downstream_buf_.data();
        size_t len = d.size();
        auto data = reinterpret_cast<const char *>(d.data());
        const boost::string_view s(data, len);
        auto it = s.find("\r\n\r\n");
        if (it != std::string::npos) {
            // find
            BOOST_LOG_S5B(trace) << "do_analysis_client_first_http_header find!";
            try {
                boost::beast::http::parser<true, boost::beast::http::buffer_body> headerParser;
                headerParser.skip(true);
                boost::system::error_code ec;
                headerParser.put(boost::asio::buffer(s), ec);
                if (ec) {
                    BOOST_LOG_S5B(error) << "do_analysis_client_first_http_header headerParser ec:" << ec.message();
                    fail(ec, "do_analysis_client_first_http_header headerParser");
                    return;
                }
                auto h = headerParser.get();

                // check client auth
                if (ptr->authClientManager->needAuth()) {
                    auto hPA = h.count(boost::beast::http::field::proxy_authorization);
                    auto hA = h.count(boost::beast::http::field::authorization);
                    BOOST_LOG_S5B(trace) << "===== proxy_authorization N:" << hPA;
                    BOOST_LOG_S5B(trace) << "===== authorization N:" << hA;
                    if (hPA == 0 && hA == 0) {
                        // no auth
                        BOOST_LOG_S5B(warning) << "do_analysis_client_first_http_header no auth.";
                        // goto 407 to let user retry
                        do_send_407();
                        return;
                    } else {
                        if (hPA > 0) {
                            auto pa = h.at(boost::beast::http::field::proxy_authorization);
                            BOOST_LOG_S5B(trace) << "===== proxy_authorization:" << pa;
                            auto startCheck = boost::starts_with(pa, std::string{"Basic "});
                            BOOST_LOG_S5B(trace) << "===== proxy_authorization startCheck:" << startCheck;

                            const std::string_view base64Part(pa.begin() + 6, pa.size() - 6);
                            BOOST_LOG_S5B(trace) << "===== base64Part:" << base64Part;
                            if (!checkHeaderAuthString(base64Part, ptr)) {
                                // goto 407 to let user retry
                                do_send_407();
                                return;
                            }
                        } else if (hA > 0) {
                            auto pa = h.at(boost::beast::http::field::authorization);
                            BOOST_LOG_S5B(trace) << "===== authorization:" << pa;
                            auto startCheck = boost::starts_with(pa, std::string{"Basic "});
                            BOOST_LOG_S5B(trace) << "===== authorization startCheck:" << startCheck;

                            const std::string_view base64Part(pa.begin() + 6, pa.size() - 6);
                            BOOST_LOG_S5B(trace) << "===== base64Part:" << base64Part;
                            if (!checkHeaderAuthString(base64Part, ptr)) {
                                // goto 407 to let user retry
                                do_send_407();
                                return;
                            }
                        }
                    }
                }

                auto target = h.base().target();
                BOOST_LOG_S5B(trace) << "target:" << target;
                auto uri = parseURI(std::string(target));

                ptr->host = uri.domain;
                ptr->port = boost::lexical_cast<uint16_t>(uri.port);
                BOOST_LOG_S5B(trace) << "host:" << ptr->host;
                BOOST_LOG_S5B(trace) << "port:" << ptr->port;

                if (h.method() == boost::beast::http::verb::connect) {
                    // is "connect"
                    BOOST_LOG_S5B(trace) << "do_analysis_client_first_http_header is connect trim";

//                    ptr->connectType = ConnectType::httpConnect;

                    // remove connect request
                    ptr->downstream_buf_.consume(it + 4);

                    // ready to send Connection Established
                    do_ready_to_send_last_ok_package(ptr);
                    return;
                }

                {
                    // TODO debug  ???  seems never go there
                    BOOST_ASSERT(false);

//                    ptr->connectType = ConnectType::httpOther;

                    // remove connect request
                    ptr->downstream_buf_.consume(it + 4);

                    // ready to send Connection Established
                    do_ready_to_send_last_ok_package(ptr);
                }

                return;
            } catch (const std::exception &e) {
                std::stringstream ss;
                ss << "do_analysis_client_first_http_header (const std::exception &e):" << e.what();
                fail({}, ss.str());
                return;
            }
        } else {
            // not find
            BOOST_LOG_S5B(trace) << "do_analysis_client_first_http_header not find";
            do_read_client_first_http_header();
        }
    } else {
        badParentPtr();
    }
}

void HttpServerImpl::do_read_client_first_http_header() {
    // do_downstream_read
    auto ptr = parents.lock();
    if (ptr) {
        boost::asio::async_read_until(
                ptr->downstream_socket_,
                ptr->downstream_buf_,
                std::string("\r\n\r\n"),
                [this, self = shared_from_this(), ptr](
                        const boost::system::error_code &error,
                        const size_t &bytes_transferred) {
                    boost::ignore_unused(bytes_transferred);
                    if (!error) {
                        BOOST_LOG_S5B(trace) << "do_read_client_first_http_header";
                        do_analysis_client_first_http_header();
                    } else {
                        do_whenError(error);
                    }
                });
    } else {
        badParentPtr();
    }
}

void HttpServerImpl::do_send_407() {
    // do_downstream_write
    auto ptr = parents.lock();
    if (ptr) {
        auto data_send = std::make_shared<std::string>(
                "HTTP/1.1 407 Proxy Authentication Required\r\nProxy-Authenticate: Basic realm=\"Access to internal site\"\r\n\r\n"
        );

        boost::asio::async_write(
                ptr->downstream_socket_,
                boost::asio::buffer(*data_send),
                [this, self = shared_from_this(), data_send, ptr](
                        const boost::system::error_code &ec,
                        std::size_t bytes_transferred_) {
                    if (ec) {
                        return fail(ec, "do_send_407");
                    }
                    if (bytes_transferred_ != data_send->size()) {
                        std::stringstream ss;
                        ss << "do_send_407 with bytes_transferred_:"
                           << bytes_transferred_ << " but data_send->size():" << data_send->size();
                        return fail(ec, ss.str());
                    }

                    // read next request
                    do_read_client_first_http_header();
                }
        );


    } else {
        badParentPtr();
    }
}

void HttpServerImpl::do_send_Connection_Established() {
    // do_downstream_write
    auto ptr = parents.lock();
    if (ptr) {
        auto data_send = std::make_shared<std::string>(
                "HTTP/1.1 200 Connection Established\r\n\r\n"
        );

        boost::asio::async_write(
                ptr->downstream_socket_,
                boost::asio::buffer(*data_send),
                [this, self = shared_from_this(), data_send, ptr](
                        const boost::system::error_code &ec,
                        std::size_t bytes_transferred_) {
                    if (ec) {
                        return fail(ec, "do_send_Connection_Established");
                    }
                    if (bytes_transferred_ != data_send->size()) {
                        std::stringstream ss;
                        ss << "do_send_Connection_Established with bytes_transferred_:"
                           << bytes_transferred_ << " but data_send->size():" << data_send->size();
                        return fail(ec, ss.str());
                    }

                    // BOOST_LOG_S5B(trace) << "do_send_Connection_Established()";

                    ptr->do_whenDownEnd();
                }
        );


    } else {
        badParentPtr();
    }
}

void HttpServerImpl::do_send_Connection_Failed() {
    // do_downstream_write
    auto ptr = parents.lock();
    if (ptr) {
        auto data_send = std::make_shared<std::string>(
                "HTTP/1.1 503 Service Unavailable\r\n\r\n"
        );

        boost::asio::async_write(
                ptr->downstream_socket_,
                boost::asio::buffer(*data_send),
                [this, self = shared_from_this(), data_send, ptr](
                        const boost::system::error_code &ec,
                        std::size_t bytes_transferred_) {
                    if (ec) {
                        return fail(ec, "do_send_Connection_Failed");
                    }
                    if (bytes_transferred_ != data_send->size()) {
                        std::stringstream ss;
                        ss << "do_send_Connection_Failed with bytes_transferred_:"
                           << bytes_transferred_ << " but data_send->size():" << data_send->size();
                        return fail(ec, ss.str());
                    }

                    fail({}, "do_send_Connection_Failed 503 end.");
                    return;
                }
        );


    } else {
        badParentPtr();
    }
}

void HttpServerImpl::do_ready_to_send_last_ok_package(
        const std::shared_ptr<decltype(parents)::element_type> &ptr) {
    ptr->do_whenDownReady();
}

void HttpServerImpl::to_send_last_ok_package() {
    do_send_Connection_Established();
}

void HttpServerImpl::to_send_last_error_package() {
    do_send_Connection_Failed();
}

