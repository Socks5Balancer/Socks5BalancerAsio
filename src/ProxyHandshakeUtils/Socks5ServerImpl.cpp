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

#include "Socks5ServerImpl.h"
#include "../ProxyHandshakeAuth.h"
#include <algorithm>

void Socks5ServerImpl::do_whenError(boost::system::error_code error) {
    auto ptr = parents.lock();
    if (ptr) {
        ptr->do_whenError(error);
    }
}

void Socks5ServerImpl::do_analysis_client_first_socks5_header() {
    // do_downstream_read
    auto ptr = parents.lock();
    if (ptr) {
        auto data = ptr->downstream_buf_.data();
        size_t len = data.size();
        auto d = reinterpret_cast<const unsigned char *>(data.data());
        if (len < 3) {
            // never go there
            fail({}, "do_analysis_client_first_socks5_header (len < 3), never go there");
            return;
        }
        if (len != (2 + (uint8_t) d[1])) {
            BOOST_LOG_S5B_ID(relayId, error) << "do_analysis_client_first_socks5_header (len != (2 + (uint8_t) d[1]))";
            fail({}, std::string{"do_analysis_client_first_socks5_header (len != (2 + (uint8_t) d[1]))"});
            return;
        }
        // https://datatracker.ietf.org/doc/html/rfc1928
        //  client to server
        //  +-----+----------+----------+
        //  | VER | NMETHODS | METHODS  |
        //  +-----+----------+----------+
        //  |  1  |    1     | 1 to 255 |
        //  +-----+----------+----------+
        //
        if (d[0] == 0x05 &&
            d[1] == 0x01 &&
            d[2] == 0x00) {
            // is socks5 no auth
            BOOST_LOG_S5B_ID(relayId, trace) << "is socks5 no auth";
            ptr->downstream_buf_.consume(3);
            if (ptr->authClientManager->needAuth()) {
                // do socks5 auth with client (downside)
//                do_auth_client_write();
                fail({}, "do_analysis_client_first_socks5_header (ptr->authClientManager->needAuth()) ,no auth");
                return;
            } else {
                // do socks5 handshake with client (downside)
                BOOST_LOG_S5B_ID(relayId, trace) << "do socks5 handshake with client (downside)";
                do_handshake_client_write();
                return;
            }
        } else if (d[0] == 0x05 && d[1] > 0) {
            {
                std::stringstream ss;
                ss << "do_analysis_client_first_socks5_header (d[0] == 0x05 && d[1] > 0):"
                   << " len:" << len
                   << " data:"
                   << std::string{(char *) d, len}
                   << " [*]:";
                for (size_t i = 0; i != len; ++i) {
                    ss << "[" << i << "]" << (int) d[i];
                }
                BOOST_LOG_S5B_ID(relayId, trace) << ss.str();
            }
            // is socks5 with username/passwd auth
            BOOST_LOG_S5B_ID(relayId, trace) << "is socks5 with username/passwd auth";
            std::string authMethodList{reinterpret_cast<const char *>(d) + 2, (uint8_t) d[1]};
            ptr->downstream_buf_.consume((2 + (uint8_t) d[1]));
            BOOST_ASSERT(ptr->downstream_buf_.size() == 0);
            {
                std::stringstream ss;
                ss << "do_analysis_client_first_socks5_header authMethodList:"
                   << " size:" << authMethodList.size()
                   << " data:" << authMethodList
                   << " (authMethodList.find(0x02) != std::string::npos):"
                   << (authMethodList.find(0x02) != std::string::npos)
                   << " [*]:";
                for (size_t i = 0; i != authMethodList.size(); ++i) {
                    ss << "[" << i << "]" << (int) authMethodList[i];
                }
                BOOST_LOG_S5B_ID(relayId, trace) << ss.str();
            }

            if (authMethodList.find(0x02) != std::string::npos) {
                // client support user/pwd auth method
                if (ptr->authClientManager->needAuth()) {
                    // do socks5 auth with client (downside)
                    BOOST_LOG_S5B_ID(relayId, trace) << "do socks5 auth with client (downside)";
                    do_auth_client_write();
                    return;
                } else {
                    // do socks5 handshake with client (downside)
                    BOOST_LOG_S5B_ID(relayId, trace) << "do socks5 handshake with client (downside)";
                    do_handshake_client_write();
                    return;
                }
            } else {
                BOOST_LOG_S5B_ID(relayId, error) << "do_analysis_client_first_socks5_header no support auth";
                do_handshake_client_header_error();
                return;
            }

        } else {
            BOOST_LOG_S5B_ID(relayId, error)
                << "do_analysis_client_first_socks5_header do_handshake_client_header_error";
            do_handshake_client_header_error();
            return;
        }

    } else {
        badParentPtr();
    }
}

void Socks5ServerImpl::do_auth_client_write() {
    // https://datatracker.ietf.org/doc/html/rfc1928
    // server auth request
    //  +----+---------+
    //  |VER | METHOD  |
    //  +----+---------+
    //  | 1  | 1(0x02) |
    //  +----+---------+
    //
    // 0x02 means user/passwd auth

    // do_downstream_write
    auto ptr = parents.lock();
    if (ptr) {

        auto data_send = std::make_shared<std::string>(
                "\x05\x02", 2
        );

        boost::asio::async_write(
                ptr->downstream_socket_,
                boost::asio::buffer(*data_send),
                [this, self = shared_from_this(), data_send, ptr](
                        const boost::system::error_code &ec,
                        std::size_t bytes_transferred_) {
                    if (ec) {
                        return fail(ec, "do_auth_client_write");
                    }
                    if (bytes_transferred_ != data_send->size()) {
                        std::stringstream ss;
                        ss << "do_auth_client_write with bytes_transferred_:"
                           << bytes_transferred_ << " but data_send->size():" << data_send->size();
                        return fail(ec, ss.str());
                    }

                    do_auth_client_read();
                }
        );
    } else {
        badParentPtr();
    }
}

void Socks5ServerImpl::do_auth_client_read() {
    // https://datatracker.ietf.org/doc/html/rfc1929
    // client auth response
    //  +----+------+----------+------+----------+
    //  |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
    //  +----+------+----------+------+----------+
    //  | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
    //  +----+------+----------+------+----------+
    //

    // do_downstream_read
    auto ptr = parents.lock();
    if (ptr) {

        const size_t MAX_LENGTH = 8196;
        auto socks5_read_buf = std::make_shared<std::vector<uint8_t>>(MAX_LENGTH);

        ptr->downstream_socket_.async_read_some(
                boost::asio::buffer(*socks5_read_buf, MAX_LENGTH),
                boost::beast::bind_front_handler(
                        [this, self = shared_from_this(), socks5_read_buf, ptr](
                                boost::beast::error_code ec,
                                const size_t &bytes_transferred) {
                            if (ec) {
                                return fail(ec, "do_auth_client_read");
                            }
                            {
                                std::stringstream ss;
                                ss << "do_auth_client_read get bytes_transferred:"
                                   << bytes_transferred
                                   << " data:"
                                   << std::string{(char *) socks5_read_buf->data(),
                                                  bytes_transferred}
                                   << " [*]:"
                                   << "[0]" << (int) socks5_read_buf->at(0)
                                   << "[1]" << (int) socks5_read_buf->at(1)
                                   << "[2]" << (int) socks5_read_buf->at(2 + socks5_read_buf->at(1));
                                BOOST_LOG_S5B_ID(relayId, trace) << ss.str();
                            }

                            if (bytes_transferred < (2 + 1 + 1 + 1)) {
                                return fail(ec, "do_auth_client_read (bytes_transferred < (2 + 1 + 1 + 1))");
                            }
                            if (bytes_transferred < (
                                    2 + socks5_read_buf->at(1)
                                    + 1
                                    + socks5_read_buf->at(2 + socks5_read_buf->at(1)))) {
                                return fail(ec, "do_auth_client_read (bytes_transferred too short)");
                            }
                            if (bytes_transferred > (
                                    2 + socks5_read_buf->at(1)
                                    + 1
                                    + socks5_read_buf->at(2 + socks5_read_buf->at(1)))) {
                                return fail(ec, "do_auth_client_read (bytes_transferred too long)");
                            }
                            if (socks5_read_buf->at(0) != 0x01) {
                                return fail(ec, "do_auth_client_read (socks5_read_buf->at(0) != 0x01)");
                            }
                            if (socks5_read_buf->at(1) == 0) {
                                return fail(ec, "do_auth_client_read (socks5_read_buf->at(1) == 0)");
                            }
                            if (socks5_read_buf->at(2 + socks5_read_buf->at(1)) == 0) {
                                return fail(
                                        ec,
                                        "do_auth_client_read (socks5_read_buf->at(2 + socks5_read_buf->at(1)) == 0)");
                            }

                            std::string_view user{
                                    (char *) (socks5_read_buf->data() + 2),
                                    socks5_read_buf->at(1)
                            };
                            std::string_view pwd{
                                    (char *) (socks5_read_buf->data() + 2 + socks5_read_buf->at(1) + 1),
                                    socks5_read_buf->at(2 + socks5_read_buf->at(1))
                            };
                            auto c = ptr->authClientManager->checkAuth(user, pwd);
                            if (c) {
                                BOOST_LOG_S5B_ID(relayId, trace)
                                    << "do_auth_client_read auth ok :[" << user << "]:[" << pwd << "]";
                                do_auth_client_ok();
                            } else {
                                BOOST_LOG_S5B_ID(relayId, trace)
                                    << "do_auth_client_read auth error :[" << user << "]:[" << pwd << "]";
                                do_auth_client_error();
                            }
                        }));


    } else {
        badParentPtr();
    }
}

void Socks5ServerImpl::do_auth_client_ok() {
    // https://datatracker.ietf.org/doc/html/rfc1929
    // auth ok
    //   +----+---------+
    //   |VER | STATUS  |
    //   +----+---------+
    //   | 1  | 1(0x00) |
    //   +----+---------+
    //

    // do_downstream_write
    auto ptr = parents.lock();
    if (ptr) {
        auto data_send = std::make_shared<std::string>(
                "\x01\x00", 2
        );

        BOOST_LOG_S5B_ID(relayId, trace) << "do do_handshake_client_read";
        boost::asio::async_write(
                ptr->downstream_socket_,
                boost::asio::buffer(*data_send),
                [this, self = shared_from_this(), data_send, ptr](
                        const boost::system::error_code &ec,
                        std::size_t bytes_transferred_) {
                    if (ec) {
                        return fail(ec, "do_auth_client_ok");
                    }
                    if (bytes_transferred_ != data_send->size()) {
                        std::stringstream ss;
                        ss << "do_auth_client_ok with bytes_transferred_:"
                           << bytes_transferred_ << " but data_send->size():" << data_send->size();
                        return fail(ec, ss.str());
                    }

                    BOOST_LOG_S5B_ID(relayId, trace) << "call do_handshake_client_read";
                    do_handshake_client_read();
                }
        );
    } else {
        badParentPtr();
    }
}

void Socks5ServerImpl::do_auth_client_error() {
    // https://datatracker.ietf.org/doc/html/rfc1929
    // auth error
    //   +----+---------+
    //   |VER | STATUS  |
    //   +----+---------+
    //   | 1  | 1(0xff) |
    //   +----+---------+
    //
    //
    //

    // do_downstream_write
    auto ptr = parents.lock();
    if (ptr) {
        auto data_send = std::make_shared<std::string>(
                "\x01\xff", 2
        );

        boost::asio::async_write(
                ptr->downstream_socket_,
                boost::asio::buffer(*data_send),
                [this, self = shared_from_this(), data_send, ptr](
                        const boost::system::error_code &ec,
                        std::size_t bytes_transferred_) {
                    if (ec) {
                        return fail(ec, "do_auth_client_error");
                    }
                    if (bytes_transferred_ != data_send->size()) {
                        std::stringstream ss;
                        ss << "do_auth_client_error with bytes_transferred_:"
                           << bytes_transferred_ << " but data_send->size():" << data_send->size();
                        return fail(ec, ss.str());
                    }

                    BOOST_LOG_S5B_ID(relayId, warning) << "auth_client_error end.";
                    ptr->do_whenDownEnd(true);
//                    fail({}, "auth_client_error");
                }
        );
    } else {
        badParentPtr();
    }
}

void Socks5ServerImpl::do_handshake_client_write() {
    // https://datatracker.ietf.org/doc/html/rfc1928
    // server auth none
    //  +----+---------+
    //  |VER | METHOD  |
    //  +----+---------+
    //  | 1  | 1(0x02) |
    //  +----+---------+
    //
    // 0x00 means no auth

    // do_downstream_write
    auto ptr = parents.lock();
    if (ptr) {

        auto data_send = std::make_shared<std::string>(
                "\x05\x00", 2
        );

        boost::asio::async_write(
                ptr->downstream_socket_,
                boost::asio::buffer(*data_send),
                [this, self = shared_from_this(), data_send, ptr](
                        const boost::system::error_code &ec,
                        std::size_t bytes_transferred_) {
                    if (ec) {
                        return fail(ec, "do_handshake_client_write");
                    }
                    if (bytes_transferred_ != data_send->size()) {
                        std::stringstream ss;
                        ss << "do_handshake_client_write with bytes_transferred_:"
                           << bytes_transferred_ << " but data_send->size():" << data_send->size();
                        return fail(ec, ss.str());
                    }

                    do_handshake_client_read();
                }
        );
    } else {
        badParentPtr();
    }
}

void Socks5ServerImpl::do_handshake_client_read() {
    // do_downstream_read
    auto ptr = parents.lock();
    if (ptr) {
        // https://datatracker.ietf.org/doc/html/rfc1928
        //
        //  +----+-----+-------+------+----------+----------+
        //  |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
        //  +----+-----+-------+------+----------+----------+
        //  | 1  |  1  | X'00' |  1   | Variable |    2     |
        //  +----+-----+-------+------+----------+----------+
        //

        const size_t MAX_LENGTH = 8196;
        auto socks5_read_buf = std::make_shared<std::vector<uint8_t>>(MAX_LENGTH);

        ptr->downstream_socket_.async_read_some(
                boost::asio::buffer(*socks5_read_buf, MAX_LENGTH),
                boost::beast::bind_front_handler(
                        [this, self = shared_from_this(), socks5_read_buf, ptr](
                                boost::beast::error_code ec,
                                const size_t &bytes_transferred) {
                            if (ec) {
                                return fail(ec, "do_handshake_client_read");
                            }
                            BOOST_LOG_S5B_ID(relayId, trace) << "do_handshake_client_read get bytes_transferred:"
                                                             << bytes_transferred;

                            if (bytes_transferred < (4 + 2)) {
                                return fail(ec, "do_handshake_client_read (bytes_transferred < (4 + 2))");
                            }
                            if (socks5_read_buf->at(0) != 5
                                || (
                                        socks5_read_buf->at(1) != 0x01    // CONNECT
                                        && socks5_read_buf->at(1) != 0x02    // BIND
                                        && socks5_read_buf->at(1) != 0x03    // UDP
                                )
                                || socks5_read_buf->at(2) != 0x00) {
                                return fail(ec, "do_handshake_client_read (invalid)");
                            }
                            if (socks5_read_buf->at(3) == 0x01) {
                                // IPv4
                                if (bytes_transferred < (4 + 4 + 2)) {
                                    return fail(ec, "do_handshake_client_read (invalid IPv4)");
                                }
                                if (bytes_transferred > (4 + 4 + 2)) {
                                    return fail(ec, "do_handshake_client_read (IPv4 too long)");
                                }
                                // https://stackoverflow.com/questions/10220912/quickest-way-to-initialize-asioipaddress-v6
                                // We need an unsigned char* pointer to the IP address
                                auto addr = reinterpret_cast<unsigned char *>(
                                        socks5_read_buf->data() + 4
                                );
                                boost::asio::ip::address_v4::bytes_type ipV4Byte;
                                // Copy the address into our array
                                std::copy(addr, addr + ipV4Byte.size(), ipV4Byte.data());
                                // Finally, initialize.
                                boost::asio::ip::address_v4 ipv4(ipV4Byte);
                                ptr->host = ipv4.to_string();
                                ptr->port = (
                                        socks5_read_buf->at(bytes_transferred - 2) << 8
                                        |
                                        socks5_read_buf->at(bytes_transferred - 1)
                                );
                            } else if (socks5_read_buf->at(3) == 0x03) {
                                // domain
                                if (bytes_transferred < (4 + 1 + socks5_read_buf->at(4) + 2)) {
                                    return fail(ec, "do_handshake_client_read (invalid domain)");
                                }
                                if (bytes_transferred > (4 + 1 + socks5_read_buf->at(4) + 2)) {
                                    return fail(ec, "do_handshake_client_read (domain too long)");
                                }
                                ptr->host = std::string{
                                        socks5_read_buf->begin() + 4 + 1,
                                        socks5_read_buf->begin() + 4 + 1 + socks5_read_buf->at(4)};
                                ptr->port = (
                                        socks5_read_buf->at(bytes_transferred - 2) << 8
                                        |
                                        socks5_read_buf->at(bytes_transferred - 1)
                                );
                            } else if (socks5_read_buf->at(3) == 0x04) {
                                // IPv6
                                if (bytes_transferred < (4 + 16 + 2)) {
                                    return fail(ec, "do_handshake_client_read (invalid IPv6)");
                                }
                                if (bytes_transferred > (4 + 16 + 2)) {
                                    return fail(ec, "do_handshake_client_read (IPv6 too long)");
                                }
                                // https://stackoverflow.com/questions/10220912/quickest-way-to-initialize-asioipaddress-v6
                                // We need an unsigned char* pointer to the IP address
                                auto addr = reinterpret_cast<unsigned char *>(
                                        socks5_read_buf->data() + 4
                                );
                                boost::asio::ip::address_v6::bytes_type ipV6Byte;
                                // Copy the address into our array
                                std::copy(addr, addr + ipV6Byte.size(), ipV6Byte.data());
                                // Finally, initialize.
                                boost::asio::ip::address_v6 ipv6(ipV6Byte);
                                ptr->host = ipv6.to_string();
                                ptr->port = (
                                        socks5_read_buf->at(bytes_transferred - 2) << 8
                                        |
                                        socks5_read_buf->at(bytes_transferred - 1)
                                );
                            } else {
                                // invalid
                                return fail(ec, "do_handshake_client_read (socks5_read_buf->at(3) invalid)");
                            }
                            BOOST_LOG_S5B_ID(relayId, trace) << "do_handshake_client_read after read target:"
                                                             << " host:" << ptr->host
                                                             << " port:" << ptr->port;
                            switch (socks5_read_buf->at(1)) {
                                case 0x01:
                                    // CONNECT
                                    BOOST_LOG_S5B_ID(relayId, trace) << "do_handshake_client_read CONNECT";
                                    ptr->proxyRelayMode = ProxyRelayMode::connect;
                                    break;
                                case 0x02:
                                    // BIND
                                    // we not impl this, never
                                    BOOST_LOG_S5B_ID(relayId, trace) << "do_handshake_client_read BIND";
                                    do_handshake_client_end_error(0x07);
                                    ptr->proxyRelayMode = ProxyRelayMode::bind;
                                    break;
                                case 0x03:
                                    // UDP
                                    if (!ptr->upside_support_udp_mode()) {
                                        // upside not support UDP
                                        BOOST_LOG_S5B_ID(relayId, warning)
                                            << "do_handshake_client_read ( upside not support UDP)";
                                        do_handshake_client_end_error(0x07);
                                        return;
                                    }
//                                    // we not impl this, only NOW
//                                    BOOST_LOG_S5B_ID(relayId, warning) << "do_handshake_client_read UDP not impl";
//                                    do_handshake_client_end_error(0x07);

                                    // set udpEnabled=true if user need UDP
                                    BOOST_LOG_S5B_ID(relayId, trace) << "do_handshake_client_read UDP";
                                    udpEnabled = true;
                                    ptr->proxyRelayMode = ProxyRelayMode::udp;
                                    break;
                                default:
                                    // never go there
                                    return fail(
                                            ec,
                                            "do_handshake_client_read switch (socks5_read_buf->at(1)), never go there");
                                    break;
                            }


                            BOOST_LOG_S5B_ID(relayId, trace)
                                << "do_handshake_client_read call do_ready_to_send_last_ok_package";
                            do_ready_to_send_last_ok_package(ptr);

                        }));


    } else {
        badParentPtr();
    }
}

void Socks5ServerImpl::do_handshake_client_header_error() {
    // do_downstream_write
    auto ptr = parents.lock();
    if (ptr) {
        // https://datatracker.ietf.org/doc/html/rfc1928
        // unsupported auth OR handshake error
        //   +----+------------------+
        //   |VER | STATUS or METHOD |
        //   +----+------------------+
        //   | 1  |     1 (0xff)     |
        //   +----+------------------+
        //
        // 0xff means error
        //

        auto data_send = std::make_shared<std::string>(
                "\x05\xff", 2
        );

        boost::asio::async_write(
                ptr->downstream_socket_,
                boost::asio::buffer(*data_send),
                [this, self = shared_from_this(), data_send, ptr](
                        const boost::system::error_code &ec,
                        std::size_t bytes_transferred_) {
                    if (ec) {
                        return fail(ec, "do_handshake_client_end_error");
                    }
                    if (bytes_transferred_ != data_send->size()) {
                        std::stringstream ss;
                        ss << "do_handshake_client_end_error with bytes_transferred_:"
                           << bytes_transferred_ << " but data_send->size():" << data_send->size();
                        return fail(ec, ss.str());
                    }

                    BOOST_LOG_S5B_ID(relayId, warning) << "do_handshake_client_end_error end.";
                    ptr->do_whenDownEnd(true);
//                    fail({}, "do_handshake_client_end_error end.");
                }
        );
    } else {
        badParentPtr();
    }
}

void Socks5ServerImpl::do_ready_to_send_last_ok_package(
        const std::shared_ptr<decltype(parents)::element_type> &ptr
) {
    ptr->do_whenDownReady();
}

void Socks5ServerImpl::do_handshake_client_end_error(uint8_t errorType) {
    BOOST_LOG_S5B_ID(relayId, warning) << "do_handshake_client_end_error errorType:" << errorType;

    // do_downstream_write
    auto ptr = parents.lock();
    if (ptr) {
        // https://datatracker.ietf.org/doc/html/rfc1928
        //
        // server response, the connect error
        //  +----+---------+-------+---------+---------------+-----------+
        //  |VER |   REP   |  RSV  |  ATYP   |   BND.ADDR    | BND.PORT  |
        //  +----+---------+-------+---------+---------------+-----------+
        //  | 1  | 1(0x01) | X'00' | 1(0x01) | 0x00,00,00,00 |  0x00,00  |
        //  +----+---------+-------+---------+---------------+-----------+
        //
        //
        //   X'00' succeeded
        //   X'01' general SOCKS server failure
        //   X'02' connection not allowed by ruleset
        //   X'03' Network unreachable
        //   X'04' Host unreachable
        //   X'05' Connection refused
        //   X'06' TTL expired
        //   X'07' Command not supported
        //   X'08' Address type not supported
        //   X'09' to X'FF' unassigned
        //
        //
        // fill BND info as 0.0.0.0:0  (0x00,00,00,00, 0x00,00)

        switch (errorType) {
            case 0x01:
            case 0x07:
                break;
            default:
                // never go there
                BOOST_ASSERT_MSG(false, "do_handshake_client_end_error switch (errorType) default, never go there");
                return fail({}, "do_handshake_client_end_error switch (errorType) default, never go there");
                break;
        }

        auto data_send = std::make_shared<std::string>(
                "\x05\x01\x00\x01\x00\x00\x00\x00\x00\x00", 10
        );

        data_send->at(1) = static_cast<char>(errorType);

        boost::asio::async_write(
                ptr->downstream_socket_,
                boost::asio::buffer(*data_send),
                [this, self = shared_from_this(), data_send, ptr](
                        const boost::system::error_code &ec,
                        std::size_t bytes_transferred_) {
                    if (ec) {
                        return fail(ec, "do_handshake_client_end_error");
                    }
                    if (bytes_transferred_ != data_send->size()) {
                        std::stringstream ss;
                        ss << "do_handshake_client_end_error with bytes_transferred_:"
                           << bytes_transferred_ << " but data_send->size():" << data_send->size();
                        return fail(ec, ss.str());
                    }

                    ptr->do_whenDownEnd(true);
                }
        );

    } else {
        badParentPtr();
    }
}

void Socks5ServerImpl::do_handshake_client_end() {

    // do_downstream_write
    auto ptr = parents.lock();
    if (ptr) {
        // https://datatracker.ietf.org/doc/html/rfc1928
        //
        // server response, the connect ok
        //  +----+---------+-------+---------+---------------+-----------+
        //  |VER |   REP   |  RSV  |  ATYP   |   BND.ADDR    | BND.PORT  |
        //  +----+---------+-------+---------+---------------+-----------+
        //  | 1  | 1(0x00) | X'00' | 1(0x01) | 0x00,00,00,00 |  0x00,00  |
        //  +----+---------+-------+---------+---------------+-----------+
        //
        // now we not impl UDP
        //
        // get BND info from upside
        // OR
        // fill BND info as 0.0.0.0:0  (0x00,00,00,00, 0x00,00)



        auto data_send = std::make_shared<std::vector<uint8_t>>();
        data_send->push_back(0x05);
        data_send->push_back(0x00);
        data_send->push_back(0x00);
        // load BND info from upside AND impl UDP
        switch (ptr->bindHost.size()) {
            case 0: {
                // normal connect
                BOOST_ASSERT(ptr->proxyRelayMode == ProxyRelayMode::connect);
                const char *const tc = "\x05\x00\x00\x01\x00\x00\x00\x00\x00\x00";
                //std::string t{tc, 10};
                //  auto data_send = std::make_shared<std::string>(
                //      "\x05\x00\x00\x01\x00\x00\x00\x00\x00\x00", 10
                //  );
                auto tt = std::vector<uint8_t>{tc, tc + 10};
                data_send->swap(tt);
            }
                break;
            case 4: {
                // IP V4
                data_send->push_back(0x01);
                data_send->insert(data_send->end(), ptr->bindHost.begin(), ptr->bindHost.end());
                data_send->insert(data_send->end(), ptr->bindPort.begin(), ptr->bindPort.end());
            }
                break;
            case 16: {
                // IP V6
                data_send->push_back(0x04);
                data_send->insert(data_send->end(), ptr->bindHost.begin(), ptr->bindHost.end());
                data_send->insert(data_send->end(), ptr->bindPort.begin(), ptr->bindPort.end());
            }
                break;
            default: {
                // domain
                data_send->push_back(0x03);
                if (ptr->host.size() > 255) {
                    return fail({}, "do_handshake_client_end (ptr->host.size() > 255)");
                }
                BOOST_ASSERT(ptr->bindHost.size() <= 255);
                data_send->push_back((uint8_t) ptr->bindHost.size());
                data_send->insert(data_send->end(), ptr->bindHost.begin(), ptr->bindHost.end());
                data_send->insert(data_send->end(), ptr->bindPort.begin(), ptr->bindPort.end());
            }
                break;
        }

        BOOST_ASSERT((udpEnabled ? ptr->proxyRelayMode == ProxyRelayMode::udp : true));
        // client request UDP but upside cannot provide
        if (udpEnabled) {
            if (!ptr->upside_in_udp_mode()) {
                BOOST_LOG_S5B_ID(relayId, warning) << "do_handshake_client_end (!ptr->upside_in_udp_mode())";
                do_handshake_client_end_error(0x07);
                return;
            } else {
                // now in UDP mode
                // empty
            }
        } else {
            // empty
        }

        boost::asio::async_write(
                ptr->downstream_socket_,
                boost::asio::buffer(*data_send),
                [this, self = shared_from_this(), data_send, ptr](
                        const boost::system::error_code &ec,
                        std::size_t bytes_transferred_) {
                    if (ec) {
                        return fail(ec, "do_handshake_client_end");
                    }
                    if (bytes_transferred_ != data_send->size()) {
                        std::stringstream ss;
                        ss << "do_handshake_client_end with bytes_transferred_:"
                           << bytes_transferred_ << " but data_send->size():" << data_send->size();
                        return fail(ec, ss.str());
                    }

                    BOOST_LOG_S5B_ID(relayId, trace) << "do_handshake_client_end";
                    ptr->do_whenDownEnd(false);
                }
        );

    } else {
        badParentPtr();
    }
}

void Socks5ServerImpl::to_send_last_ok_package() {
    do_handshake_client_end();
}

void Socks5ServerImpl::to_send_last_error_package() {
    BOOST_LOG_S5B_ID(relayId, warning) << "to_send_last_error_package";
    do_handshake_client_end_error(0x01);
}

Socks5ServerImpl::Socks5ServerImpl(
        const std::shared_ptr<ProxyHandshakeAuth> &parents_
) : parents(parents_), relayId(parents_->relayId) {}

void Socks5ServerImpl::fail(boost::system::error_code ec, const std::string &what) {
    std::string r;
    {
        std::stringstream ss;
        ss << what << ": [" << ec.message() << "] . ";
        r = ss.str();
    }
    BOOST_LOG_S5B_ID(relayId, error) << r;

    do_whenError(ec);
}
