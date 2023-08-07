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

#include "FirstPackAnalyzer.h"
#include "TcpRelayServer.h"
#include <regex>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>

#include "./ProxyHandshakeUtils/HttpServerImpl.h"
#include "./ProxyHandshakeUtils/HttpClientImpl.h"
#include "./ProxyHandshakeUtils/Socks5ServerImpl.h"
#include "./ProxyHandshakeUtils/Socks5ClientImpl.h"


// from https://github.com/boostorg/beast/issues/787#issuecomment-376259849
ParsedURI FirstPackAnalyzer::parseURI(const std::string &url) {
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


void FirstPackAnalyzer::start() {
    // we must keep strong ptr when running, until call the whenComplete,
    // to keep parent TcpRelaySession but dont make circle ref
    auto ptr = tcpRelaySession.lock();
    if (ptr) {
        do_read_client_first_3_byte();
        // debug
//        do_prepare_whenComplete();
    } else {
        badParentPtr();
    }
}

void FirstPackAnalyzer::do_prepare_whenComplete() {
    auto ptr = tcpRelaySession.lock();
    if (ptr) {
        // impl: insert protocol analysis start on here
        auto ct = ptr->getConnectionTracker();
        if (ct) {
            ct->relayGotoDown(upstream_buf_);
            ct->relayGotoUp(downstream_buf_);
        }

        // send remain data
        do_prepare_complete_downstream_write();
        do_prepare_complete_upstream_write();
    } else {
        badParentPtr();
    }
}

void FirstPackAnalyzer::do_whenComplete() {
    --beforeComplete;
    if (0 == beforeComplete) {
        auto ptr = tcpRelaySession.lock();
        if (ptr) {
            // all ok
            whenComplete();
        }
    }
}

void FirstPackAnalyzer::do_whenError(boost::system::error_code error) {
    whenError(error);
}

void FirstPackAnalyzer::do_prepare_complete_downstream_write() {
    auto ptr = tcpRelaySession.lock();
    if (ptr) {
        boost::asio::async_write(
                downstream_socket_,
                upstream_buf_,
                [this, self = shared_from_this(), ptr](
                        const boost::system::error_code &error,
                        std::size_t bytes_transferred_) {
                    if (!error) {
                        ptr->addDown2Statistics(bytes_transferred_);

                        do_whenComplete();
                    } else {
                        do_whenError(error);
                    }
                });
    } else {
        badParentPtr();
    }
}

void FirstPackAnalyzer::do_prepare_complete_upstream_write() {
    auto ptr = tcpRelaySession.lock();
    if (ptr) {
        boost::asio::async_write(
                upstream_socket_,
                downstream_buf_,
                [this, self = shared_from_this(), ptr](
                        const boost::system::error_code &error,
                        std::size_t bytes_transferred_) {
                    if (!error) {
                        ptr->addUp2Statistics(bytes_transferred_);

                        do_whenComplete();
                    } else {
                        do_whenError(error);
                    }
                });
    } else {
        badParentPtr();
    }
}

void FirstPackAnalyzer::do_read_client_first_3_byte() {
    // do_downstream_read
    auto ptr = tcpRelaySession.lock();
    if (ptr) {
        boost::asio::async_read(
                downstream_socket_,
                downstream_buf_,
                boost::asio::transfer_at_least(3),
                [this, self = shared_from_this(), ptr](
                        const boost::system::error_code &error,
                        const size_t &bytes_transferred) {
                    if (!error && bytes_transferred >= 3 && downstream_buf_.size() >= 3) {
                        // check first 3 byte

                        auto d = reinterpret_cast<const unsigned char *>(downstream_buf_.data().data());
                        if (d[0] == 0x05 &&
                            d[1] == 0x01 &&
                            d[2] == 0x00) {
                            // is socks5
                            std::cout << "is socks5" << std::endl;
                            connectType = ConnectType::socks5;
                            // relay normal
                            do_prepare_whenComplete();
                        } else if ((d[0] == 'C' || d[0] == 'c') &&
                                   (d[1] == 'O' || d[1] == 'o') &&
                                   (d[2] == 'N' || d[2] == 'n')) {
                            // is http Connect
                            std::cout << "is http Connect" << std::endl;
                            // analysis target server and create socks5 handshake
                            connectType = ConnectType::httpConnect;
                            do_analysis_client_first_http_header();
                        } else {
                            // is other protocol
                            std::cout << "is other protocol" << std::endl;
                            // analysis target server and create socks5 handshake
                            switch (d[0]) {
                                case 'P':
                                case 'p':
                                    // POST, PUT, PATCH
                                    std::cout << "is POST, PUT, PATCH" << std::endl;
                                    connectType = ConnectType::httpOther;
                                    break;
                                case 'G':
                                case 'g':
                                    std::cout << "is GET" << std::endl;
                                    // GET
                                    connectType = ConnectType::httpOther;
                                    break;
                                case 'H':
                                case 'h':
                                    std::cout << "is HEAD" << std::endl;
                                    // HEAD
                                    connectType = ConnectType::httpOther;
                                    break;
                                case 'D':
                                case 'd':
                                    std::cout << "is DELETE" << std::endl;
                                    // DELETE
                                    connectType = ConnectType::httpOther;
                                    break;
                                case 'O':
                                case 'o':
                                    std::cout << "is OPTIONS" << std::endl;
                                    // OPTIONS
                                    connectType = ConnectType::httpOther;
                                    break;
                                case 'T':
                                case 't':
                                    std::cout << "is TRACE" << std::endl;
                                    // TRACE
                                    connectType = ConnectType::httpOther;
                                    break;
                                default:
                                    std::cout << "is default..." << std::endl;
                                    connectType = ConnectType::unknown;
                                    break;
                            }
                            // debug
                            do_analysis_client_first_http_header();
                        }

                    } else {
                        do_whenError(error);
                    }
                });
    } else {
        badParentPtr();
    }
}

void FirstPackAnalyzer::do_read_client_first_http_header() {
    // do_downstream_read
    auto ptr = tcpRelaySession.lock();
    if (ptr) {
        boost::asio::async_read_until(
                downstream_socket_,
                downstream_buf_,
                std::string("\r\n\r\n"),
                [this, self = shared_from_this(), ptr](
                        const boost::system::error_code &error,
                        const size_t &bytes_transferred) {
                    boost::ignore_unused(bytes_transferred);
                    if (!error) {
                        std::cout << "do_read_client_first_http_header" << std::endl;
                        do_analysis_client_first_http_header();
                    } else {
                        do_whenError(error);
                    }
                });
    } else {
        badParentPtr();
    }
}

void FirstPackAnalyzer::do_analysis_client_first_http_header() {
    // do_downstream_read
    auto ptr = tcpRelaySession.lock();
    if (ptr) {
        auto d = downstream_buf_.data();
        size_t len = d.size();
        auto data = reinterpret_cast<const char *>(d.data());
        const std::string_view s(data, len);
        auto it = s.find("\r\n\r\n");
        if (it != std::string::npos) {
            // find
            std::cout << "do_analysis_client_first_http_header find!" << std::endl;
            try {
                boost::beast::http::parser<true, boost::beast::http::buffer_body> headerParser;
                headerParser.skip(true);
                boost::system::error_code ec;
                headerParser.put(boost::asio::buffer(s), ec);
                if (ec) {
                    std::cout << "do_analysis_client_first_http_header headerParser ec:" << ec.message() << std::endl;
                    fail(ec, "do_analysis_client_first_http_header headerParser");
                    return;
                }
                auto h = headerParser.get();

                auto target = h.base().target();
                std::cout << "target:" << target << std::endl;
                auto uri = parseURI(std::string(target));

                host = uri.domain;
                port = boost::lexical_cast<uint16_t>(uri.port);
                std::cout << "host:" << host << std::endl;
                std::cout << "port:" << port << std::endl;

                if (h.method() == boost::beast::http::verb::connect) {
                    // is connect
                    std::cout << "do_analysis_client_first_http_header is connect trim" << std::endl;

                    connectType = ConnectType::httpConnect;

                    // remove connect request
                    downstream_buf_.consume(it + 4);

                    // send Connection Established
                    do_send_Connection_Established();
                    return;
                }

                // send socks5 handshake
                do_socks5_handshake_write();

                return;
            } catch (const std::exception &e) {
                std::stringstream ss;
                ss << "do_analysis_client_first_http_header (const std::exception &e):" << e.what() << std::endl;
                fail({}, ss.str());
                return;
            }
        } else {
            // not find
            std::cout << "do_analysis_client_first_http_header not find" << std::endl;
            do_read_client_first_http_header();
        }
    } else {
        badParentPtr();
    }
}

void FirstPackAnalyzer::do_send_Connection_Established() {
    // do_downstream_write
    auto ptr = tcpRelaySession.lock();
    if (ptr) {
        auto data_send = std::make_shared<std::string>(
                "HTTP/1.1 200 Connection Established\r\n\r\n"
        );

        boost::asio::async_write(
                downstream_socket_,
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

                    // std::cout << "do_send_Connection_Established()" << std::endl;

                    do_socks5_handshake_write();
                }
        );


    } else {
        badParentPtr();
    }
}

void FirstPackAnalyzer::do_socks5_handshake_write() {
    // do_upstream_write
    auto ptr = tcpRelaySession.lock();
    if (ptr) {

        // send socks5 client handshake
        // +----+----------+----------+
        // |VER | NMETHODS | METHODS  |
        // +----+----------+----------+
        // | 1  |    1     | 1 to 255 |
        // +----+----------+----------+
        auto data_send = std::make_shared<std::string>(
                "\x05\x01\x00", 3
        );

        boost::asio::async_write(
                upstream_socket_,
                boost::asio::buffer(*data_send),
                [this, self = shared_from_this(), data_send, ptr](
                        const boost::system::error_code &ec,
                        std::size_t bytes_transferred_) {
                    if (ec) {
                        return fail(ec, "socks5_handshake_write");
                    }
                    if (bytes_transferred_ != data_send->size()) {
                        std::stringstream ss;
                        ss << "socks5_handshake_write with bytes_transferred_:"
                           << bytes_transferred_ << " but data_send->size():" << data_send->size();
                        return fail(ec, ss.str());
                    }

                    // std::cout << "do_socks5_handshake_write()" << std::endl;

                    do_socks5_handshake_read();
                }
        );
    } else {
        badParentPtr();
    }
}

void FirstPackAnalyzer::do_socks5_handshake_read() {
    // do_upstream_read
    auto ptr = tcpRelaySession.lock();
    if (ptr) {

        const size_t MAX_LENGTH = 8196;
        auto socks5_read_buf = std::make_shared<std::vector<uint8_t>>(MAX_LENGTH);

        // Make the connection on the IP address we get from a lookup
        upstream_socket_.async_read_some(
                boost::asio::buffer(*socks5_read_buf, MAX_LENGTH),
                boost::beast::bind_front_handler(
                        [this, self = shared_from_this(), socks5_read_buf, ptr](
                                boost::beast::error_code ec,
                                const size_t &bytes_transferred) {
                            if (ec) {
                                return fail(ec, "socks5_handshake_read");
                            }

                            // check server response
                            //  +----+--------+
                            //  |VER | METHOD |
                            //  +----+--------+
                            //  | 1  |   1    |
                            //  +----+--------+
                            if (bytes_transferred < 2
                                || socks5_read_buf->at(0) != 5
                                || socks5_read_buf->at(1) != 0x00) {
                                return fail(ec, "socks5_handshake_read (bytes_transferred < 2)");
                            }

                            // std::cout << "do_socks5_handshake_read()" << std::endl;
                            do_socks5_connect_write();
                        }));
    } else {
        badParentPtr();
    }
}

void FirstPackAnalyzer::do_socks5_connect_write() {
    // do_upstream_write
    auto ptr = tcpRelaySession.lock();
    if (ptr) {

        // analysis targetHost and targetPort
        // targetHost,
        // targetPort,
        boost::beast::error_code ec;
        auto addr = boost::asio::ip::make_address(host, ec);

        // send socks5 client connect
        // +----+-----+-------+------+----------+----------+
        // |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
        // +----+-----+-------+------+----------+----------+
        // | 1  |  1  | X'00' |  1   | Variable |    2     |
        // +----+-----+-------+------+----------+----------+
        auto data_send = std::make_shared<std::vector<uint8_t>>();
        data_send->insert(data_send->end(), {0x05, 0x01, 0x00});
        if (ec) {
            // is a domain name
            data_send->push_back(0x03); // ATYP
            if (host.size() > 253) {
                return fail(ec, "socks5_connect_write (targetHost.size() > 253)");
            }
            data_send->push_back(static_cast<uint8_t>(host.size()));
            data_send->insert(data_send->end(), host.begin(), host.end());
        } else if (addr.is_v4()) {
            data_send->push_back(0x01); // ATYP
            auto v4 = addr.to_v4().to_bytes();
            data_send->insert(data_send->end(), v4.begin(), v4.end());
        } else if (addr.is_v6()) {
            data_send->push_back(0x04); // ATYP
            auto v6 = addr.to_v6().to_bytes();
            data_send->insert(data_send->end(), v6.begin(), v6.end());
        }
        // port
        data_send->push_back(static_cast<uint8_t>(port >> 8));
        data_send->push_back(static_cast<uint8_t>(port & 0xff));

        boost::asio::async_write(
                upstream_socket_,
                boost::asio::buffer(*data_send),
                boost::beast::bind_front_handler(
                        [this, self = shared_from_this(), data_send, ptr](
                                const boost::system::error_code &ec,
                                std::size_t bytes_transferred_) {
                            if (ec) {
                                return fail(ec, "socks5_connect_write");
                            }
                            if (bytes_transferred_ != data_send->size()) {
                                std::stringstream ss;
                                ss << "socks5_connect_write with bytes_transferred_:"
                                   << bytes_transferred_ << " but data_send->size():" << data_send->size();
                                return fail(ec, ss.str());
                            }

                            // std::cout << "do_socks5_connect_write()" << std::endl;
                            do_socks5_connect_read();
                        })
        );
    } else {
        badParentPtr();
    }
}

void FirstPackAnalyzer::do_socks5_connect_read() {
    // do_upstream_read
    auto ptr = tcpRelaySession.lock();
    if (ptr) {

        const size_t MAX_LENGTH = 8196;
        auto socks5_read_buf = std::make_shared<std::vector<uint8_t>>(MAX_LENGTH);

        // Make the connection on the IP address we get from a lookup
        upstream_socket_.async_read_some(
                boost::asio::buffer(*socks5_read_buf, MAX_LENGTH),
                boost::beast::bind_front_handler(
                        [this, self = shared_from_this(), socks5_read_buf, ptr](
                                boost::beast::error_code ec,
                                const size_t &bytes_transferred) {
                            if (ec) {
                                return fail(ec, "socks5_connect_read");
                            }

                            // check server response
                            // +----+-----+-------+------+----------+----------+
                            // |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
                            // +----+-----+-------+------+----------+----------+
                            // | 1  |  1  | X'00' |  1   | Variable |    2     |
                            // +----+-----+-------+------+----------+----------+
                            if (bytes_transferred < 6
                                || socks5_read_buf->at(0) != 5
                                || socks5_read_buf->at(1) != 0x00
                                || socks5_read_buf->at(2) != 0x00
                                || (
                                        socks5_read_buf->at(3) != 0x01 &&
                                        socks5_read_buf->at(3) != 0x03 &&
                                        socks5_read_buf->at(3) != 0x04
                                )
                                    ) {
//                                std::stringstream ss;
//                                ss << "socks5_connect_read (bytes_transferred < 6)"
//                                   << " the socks5_read_buf:" << std::hex
//                                   << socks5_read_buf->at(0)
//                                   << socks5_read_buf->at(1)
//                                   << socks5_read_buf->at(2)
//                                   << socks5_read_buf->at(3)
//                                        ;
//                                return fail(ec, ss.str());
                                return fail(ec, "FirstPackAnalyzer socks5_connect_read (bytes_transferred < 6)");
                            }
                            if (socks5_read_buf->at(3) == 0x03
                                && bytes_transferred != (socks5_read_buf->at(4) + 4 + 1 + 2)
                                    ) {
                                return fail(ec,
                                            "FirstPackAnalyzer socks5_connect_read (socks5_read_buf->at(3) == 0x03)");
                            }
                            if (socks5_read_buf->at(3) == 0x01
                                && bytes_transferred != (4 + 4 + 2)
                                    ) {
                                return fail(ec,
                                            "FirstPackAnalyzer socks5_connect_read (socks5_read_buf->at(3) == 0x01)");
                            }
                            if (socks5_read_buf->at(3) == 0x04
                                && bytes_transferred != (4 + 16 + 2)
                                    ) {
                                return fail(ec,
                                            "FirstPackAnalyzer socks5_connect_read (socks5_read_buf->at(3) == 0x04)");
                            }

                            // std::cout << "do_socks5_connect_read()" << std::endl;
                            // socks5 handshake now complete
                            do_prepare_whenComplete();
                        }));

    } else {
        badParentPtr();
    }
}

void FirstPackAnalyzer::fail(boost::system::error_code ec, const std::string &what) {
    std::string r;
    {
        std::stringstream ss;
        ss << what << ": [" << ec.message() << "] . ";
        r = ss.str();
    }
    std::cerr << r << std::endl;

    do_whenError(ec);
}

void FirstPackAnalyzer::badParentPtr() {
    if (0 != beforeComplete) {
        do_whenError({});
    }
}
