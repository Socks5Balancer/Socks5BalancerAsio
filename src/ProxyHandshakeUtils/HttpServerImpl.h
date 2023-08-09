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

#ifndef SOCKS5BALANCERASIO_HTTPSERVERIMPL_H
#define SOCKS5BALANCERASIO_HTTPSERVERIMPL_H

#ifdef MSVC
#pragma once
#endif

#include <boost/asio/streambuf.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <iostream>
#include <string>
#include <functional>
#include <utility>

#include <regex>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/beast.hpp>

class ProxyHandshakeAuth;

// http proxy protocol server
class HttpServerImpl : public std::enable_shared_from_this<HttpServerImpl> {
public:
    std::weak_ptr<ProxyHandshakeAuth> parents;

public:
    HttpServerImpl(const std::shared_ptr<ProxyHandshakeAuth> &parents_) : parents(parents_) {}

public:

    void do_analysis_client_first_http_header();

    void do_read_client_first_http_header();

    // https://developer.mozilla.org/en-US/docs/Web/HTTP/Authentication#the_general_http_authentication_framework
    void do_send_407();

    void do_ready_to_send_Connection_Established(const std::shared_ptr<decltype(parents)::element_type> &ptr);

    void do_send_Connection_Established();

    void to_send_last_ok_package();

    struct ParsedURI {
        std::string protocol;
        std::string domain;  // only domain must be present
        std::string port;
        std::string resource;
        std::string query;   // everything after '?', possibly nothing
    };

    ParsedURI parseURI(const std::string &url);

    bool checkHeaderAuthString(
            const std::string_view &base64Part,
            const std::shared_ptr<decltype(parents)::element_type> &ptr);

public:
    void fail(boost::system::error_code ec, const std::string &what) {
        std::string r;
        {
            std::stringstream ss;
            ss << what << ": [" << ec.message() << "] . ";
            r = ss.str();
        }
        std::cerr << r << std::endl;

        do_whenError(ec);
    }

    void do_whenError(boost::system::error_code error);

    void badParentPtr() {
        // parents lost, simple ignore it, and stop run
    }

};


#endif //SOCKS5BALANCERASIO_HTTPSERVERIMPL_H
