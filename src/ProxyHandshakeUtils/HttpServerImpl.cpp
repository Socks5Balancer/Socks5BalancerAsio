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
#include <boost/url/parse.hpp>

#include "../ProxyHandshakeAuth.h"
#include "../TcpRelaySession.h"
#include "../base64.h"

std::string print_string_as_hex(const std::string &str) {
    std::stringstream ss;
    for (unsigned char c: str) {
        ss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(c) << ' ';
    }
    ss << std::dec;// 恢复为十进制
    return ss.str();
}

HttpServerImpl::ParsedURI HttpServerImpl::ParsedURI::clone() const {
    ParsedURI result;
    result.isOk = isOk;
    result.protocol = std::string(protocol);
    result.domain = std::string(domain);
    result.port = std::string(port);
    result.port_u16t = port_u16t;
    result.resource = std::string(resource);
    result.query = std::string(query);
    result.match = match;// std::smatch is copyable
    result.url = std::string(url);
    return result;
}

void HttpServerImpl::ParsedURI::debug_print(const size_t relayId) const {
    BOOST_LOG_S5B_ID(relayId, trace) << "ParsedURI: isOk:" << isOk;
    BOOST_LOG_S5B_ID(relayId, trace) << "ParsedURI: protocol:" << protocol;
    BOOST_LOG_S5B_ID(relayId, trace) << "ParsedURI: domain:" << domain;
    BOOST_LOG_S5B_ID(relayId, trace) << "ParsedURI: port:" << port;
    BOOST_LOG_S5B_ID(relayId, trace) << "ParsedURI: port_u16t:" << port_u16t;
    BOOST_LOG_S5B_ID(relayId, trace) << "ParsedURI: resource:" << resource;
    BOOST_LOG_S5B_ID(relayId, trace) << "ParsedURI: query:" << query;
    BOOST_LOG_S5B_ID(relayId, trace) << "ParsedURI: url:" << url;
    BOOST_LOG_S5B_ID(relayId, trace) << "ParsedURI: url hex:" << print_string_as_hex(url);
}

HttpServerImpl::ParsedURI HttpServerImpl::parseURIBoost(const std::string &url, const size_t relayId) {
    ParsedURI result;
    try {
        boost::urls::result<boost::urls::url_view> parsed = boost::urls::parse_uri(url);
        if (!parsed.has_value()) {
            result.isOk = false;
            result.failed = false;
            return result;
        }
        const auto &u = parsed.value();
        result.port = u.port();
        result.port_u16t = u.port_number();
        result.protocol = u.scheme();
        result.domain = u.host();
        result.resource = u.path();
        result.query = u.query();
        result.url = url;
        result.isOk = true;
        result.failed = false;
        // Convert protocol to lowercase
        boost::algorithm::to_lower(result.protocol);
        if (result.port.empty()) {
            // Default port for http is 80, https is 443
            if (result.protocol == "http") {
                result.port = "80";
                result.port_u16t = 80;
            } else if (result.protocol == "https") {
                result.port = "443";
                result.port_u16t = 443;
            }
        }
        result.debug_print(relayId);
        return result;
    }
    catch (const std::exception &e) {
        BOOST_LOG_S5B_ID(relayId, trace) << "parseURIBoost (const std::exception &e):" << e.what();
        result.isOk = false;
        result.failed = false;
        result.debug_print(relayId);
        return result;
    }
}

// from https://github.com/boostorg/beast/issues/787#issuecomment-376259849
HttpServerImpl::ParsedURI HttpServerImpl::parseURI(const std::string &url, const size_t relayId) {
    ParsedURI result;
    auto value_or = [](const std::string &value, std::string &&deflt) -> std::string {
        return (value.empty() ? deflt : value);
    };
    // Note: only "http", "https", "ws", and "wss" protocols are supported
    static const std::regex PARSE_URL{
        R"((([httpsw]{2,5})://)?([^/ :]+)(:(\d+))?(/([^ ?]+)?)?/?\??([^/ =]+[^/ ]+)?)",
        std::regex_constants::ECMAScript | std::regex_constants::icase
    };
    std::smatch match;
    result.match = match;
    result.url = url;
    if (std::regex_match(url, match, PARSE_URL) && match.size() == 9) {
        result.protocol = value_or(boost::algorithm::to_lower_copy(std::string(match[2])), "http");
        result.domain = match[3];
        const bool is_secure_protocol = (result.protocol == "https" || result.protocol == "wss");
        result.port = value_or(match[5], (is_secure_protocol) ? "443" : "80");
        result.resource = value_or(match[6], "/");
        result.query = match[8];
        result.isOk = true;
        result.failed = false;
        if (!boost::conversion::try_lexical_convert(result.port, result.port_u16t)) {
            result.port_u16t = 0;
            result.isOk = false;
            result.failed = false;
        }
        if (result.domain.empty()) {
            fail({}, "result.domain.empty()");
            result.isOk = false;
            result.failed = true;
        }
    } else {
        fail({}, "regex_match fail");
        result.isOk = false;
        result.failed = true;
    }
    result.debug_print(relayId);
    return result.clone();
}

std::shared_ptr<AuthClientManager::AuthUser> HttpServerImpl::checkHeaderAuthString(
    const std::string_view &base64Part,
    const std::shared_ptr<decltype(parents)::element_type> &ptr
) {
    std::shared_ptr<AuthClientManager::AuthUser> au = ptr->authClientManager->checkAuth_Base64AuthString(base64Part);
    if (au) {
        BOOST_LOG_S5B_ID(relayId, trace) << "checkHeaderAuthString Base64AuthString ok: " << base64Part;
        return au;
    } else {
        auto rr = base64_decode_string(base64Part);
        auto indexSplitFlag = rr.find(':');
        if (indexSplitFlag == decltype(rr)::npos || indexSplitFlag < 1) {
            // bad format
            BOOST_LOG_S5B_ID(relayId, warning) << "checkHeaderAuthString bad format: " << base64Part << " rr: " << rr;
            return {};
        }
        std::string_view user{rr.data(), indexSplitFlag};
        std::string_view pwd{rr.data() + indexSplitFlag + 1, rr.length() - indexSplitFlag - 1};
        au = ptr->authClientManager->checkAuth(user, pwd);
        if (au) {
            BOOST_LOG_S5B_ID(relayId, trace) << "checkHeaderAuthString ok: " << user << " : " << pwd;
            return au;
        } else {
            BOOST_LOG_S5B_ID(relayId, warning) << "checkHeaderAuthString wrong: " << user << " : " << pwd;
            return {};
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
            BOOST_LOG_S5B_ID(relayId, trace) << "do_analysis_client_first_http_header find!";
            try {
                boost::beast::http::parser<true, boost::beast::http::buffer_body> headerParser;
                headerParser.skip(true);
                boost::system::error_code ec;
                headerParser.put(boost::asio::buffer(s), ec);
                if (ec) {
                    BOOST_LOG_S5B_ID(relayId, error)
                        << "do_analysis_client_first_http_header headerParser ec:"
                        << ec.message();
                    fail(ec, "do_analysis_client_first_http_header headerParser");
                    return;
                }
                auto h = headerParser.get();

                // check client auth
                if (ptr->authClientManager->needAuth()) {
                    auto hPA = h.count(boost::beast::http::field::proxy_authorization);
                    auto hA = h.count(boost::beast::http::field::authorization);
                    BOOST_LOG_S5B_ID(relayId, trace) << "===== proxy_authorization N:" << hPA;
                    BOOST_LOG_S5B_ID(relayId, trace) << "===== authorization N:" << hA;
                    if (hPA == 0 && hA == 0) {
                        // no auth
                        BOOST_LOG_S5B_ID(relayId, warning) << "do_analysis_client_first_http_header no auth.";
                        // goto 407 to let user retry
                        do_send_407();
                        return;
                    } else {
                        if (hPA > 0) {
                            auto pa = h.at(boost::beast::http::field::proxy_authorization);
                            BOOST_LOG_S5B_ID(relayId, trace) << "===== proxy_authorization:" << pa;
                            auto startCheck = boost::starts_with(pa, std::string{"Basic "});
                            // TODO support `basic BASIC BaSIc bAsIC`
                            BOOST_LOG_S5B_ID(relayId, trace) << "===== proxy_authorization startCheck:" << startCheck;

                            const std::string_view base64Part(pa.begin() + 6, pa.size() - 6);
                            BOOST_LOG_S5B_ID(relayId, trace) << "===== base64Part:" << base64Part;
                            auto au = checkHeaderAuthString(base64Part, ptr);
                            if (!au) {
                                // goto 407 to let user retry
                                do_send_407();
                                return;
                            } else {
                                ptr->tcpRelaySession->authUser = au;
                            }
                        } else if (hA > 0) {
                            auto pa = h.at(boost::beast::http::field::authorization);
                            BOOST_LOG_S5B_ID(relayId, trace) << "===== authorization:" << pa;
                            auto startCheck = boost::starts_with(pa, std::string{"Basic "});
                            BOOST_LOG_S5B_ID(relayId, trace) << "===== authorization startCheck:" << startCheck;

                            const std::string_view base64Part(pa.begin() + 6, pa.size() - 6);
                            BOOST_LOG_S5B_ID(relayId, trace) << "===== base64Part:" << base64Part;
                            auto au = checkHeaderAuthString(base64Part, ptr);
                            if (!au) {
                                // goto 407 to let user retry
                                do_send_407();
                                return;
                            } else {
                                ptr->tcpRelaySession->authUser = au;
                            }
                        }
                    }
                }

                if (h.method() == boost::beast::http::verb::connect) {
                    // is "connect"
                    BOOST_LOG_S5B_ID(relayId, trace) << "do_analysis_client_first_http_header is connect trim";


                    // the remote server address and port will follow the CONNECT string , like this: `CONNECT x.yyy.com:443 HTTP/1.1\r\nHost: x.yyy.com\r\n\r\n`
                    auto target = std::string(h.target());
                    BOOST_LOG_S5B_ID(relayId, trace) << "target:[" << target << "]. hex:[" << print_string_as_hex(target) << "].";
                    auto uri = parseURI(target, relayId);

                    BOOST_LOG_S5B_ID(relayId, trace) << "do_analysis_client_first_http_header data:" << s;
                    if (!uri.isOk) {
                        if (!uri.failed) {
                            fail({}, "do_analysis_client_first_http_header parseURI failed.");
                        }
                        return;
                    }

                    ptr->host = uri.domain;
                    ptr->port = uri.port_u16t;
                    BOOST_LOG_S5B_ID(relayId, trace) << "host:" << ptr->host;
                    BOOST_LOG_S5B_ID(relayId, trace) << "port:" << ptr->port;


                    ptr->proxyRelayMode = ProxyRelayMode::connect;

                    // remove connect request
                    ptr->downstream_buf_.consume(it + 4);

                    // ready to send Connection Established
                    do_ready_to_send_last_ok_package(ptr);
                    return;
                }

                // is not connect
                // it will be a http request, like `GET / HTTP/1.1\r\nHost: x.yyy.com\r\n\r\n`
                {
                    // BOOST_ASSERT(false);
                    BOOST_LOG_S5B_ID(relayId, trace) << "do_analysis_client_first_http_header"
                                                       << " (method() != boost::beast::http::verb::connect)"
                                                       << " seems like a normal http request ?";

                    if (h.count(boost::beast::http::field::host) == 0) {
                        // no host , invalid request
                        BOOST_LOG_S5B_ID(relayId, warning) << "do_analysis_client_first_http_header no host field.";
                        fail({}, "do_analysis_client_first_http_header no host field.");
                        return;
                    }

                    auto host = h.at(boost::beast::http::field::host);
                    BOOST_LOG_S5B_ID(relayId, trace) << "target:[" << host << "]. hex:[" << print_string_as_hex(host) << "].";
                    auto uri = parseURI(host, relayId);

                    BOOST_LOG_S5B_ID(relayId, trace) << "do_analysis_client_first_http_header data:" << s;
                    if (!uri.isOk) {
                        if (!uri.failed) {
                            fail({}, "do_analysis_client_first_http_header parseURI failed.");
                        }
                        return;
                    }

                    ptr->host = uri.domain;
                    ptr->port = uri.port_u16t;
                    BOOST_LOG_S5B_ID(relayId, trace) << "host:" << ptr->host;
                    BOOST_LOG_S5B_ID(relayId, trace) << "port:" << ptr->port;


                    ptr->proxyRelayMode = ProxyRelayMode::connect;

					// redirect it as ordinary http request, don't modify the request
                    //// remove connect request
                    //ptr->downstream_buf_.consume(it + 4);

                    // ready to send Connection Established
                    do_ready_to_send_last_ok_package(ptr);
                }

                return;
            }
            catch (const std::exception &e) {
                std::stringstream ss;
                ss << "do_analysis_client_first_http_header (const std::exception &e):" << e.what();
                fail({}, ss.str());
                return;
            }
        } else {
            // not find
            BOOST_LOG_S5B_ID(relayId, trace) << "do_analysis_client_first_http_header not find";
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
                    BOOST_LOG_S5B_ID(relayId, trace) << "do_read_client_first_http_header";
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
        // https://datatracker.ietf.org/doc/html/rfc7617
        // add `charset="UTF-8"` ,
        //      ours username/passwd direct read from json file as raw byte format (std::string == std::base_string<char>),
        //      so in the memory of `AuthClientManager`, the username/passwd is encoded as UTF-8 format,
        //      (because json file is encode with `UTF-8 no BOM` ),
        //      then, we simply tell the client we need UTF-8 encode, after that, all will work well.
        auto data_send = std::make_shared<std::string>(
            "HTTP/1.1 407 Proxy Authentication Required\r\nProxy-Authenticate: Basic realm=\"Access to internal site\", charset=\"UTF-8\"\r\n\r\n"
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

                // BOOST_LOG_S5B_ID(relayId, trace) << "do_send_Connection_Established()";

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

				// TODO ????? maybe leak memory here ? or duplicate free memory ?
                //fail({}, "do_send_Connection_Failed 503 end.");
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

HttpServerImpl::HttpServerImpl(
    const std::shared_ptr<ProxyHandshakeAuth> &parents_
) : parents(parents_), relayId(parents_->relayId) {
}

void HttpServerImpl::fail(boost::system::error_code ec, const std::string &what) {
    std::string r;
    {
        std::stringstream ss;
        ss << what << ": [" << ec.message() << "] . ";
        r = ss.str();
    }
    BOOST_LOG_S5B_ID(relayId, error) << r;

    do_whenError(ec);
}
