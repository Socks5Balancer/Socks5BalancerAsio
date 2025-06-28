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

#include "Socks4ServerImpl.h"
#include "../ProxyHandshakeAuth.h"
#include "../TcpRelaySession.h"
#include <algorithm>
#include <vector>

void Socks4ServerImpl::do_whenError(boost::system::error_code error) {
    auto ptr = parents.lock();
    if (ptr) {
        ptr->do_whenError(error);
    }
}


void Socks4ServerImpl::do_analysis_client_first_socks4_header() {
    // do_downstream_read
    auto ptr = parents.lock();
    if (ptr) {
        auto data = ptr->downstream_buf_.data();
        size_t len = data.size();
        auto d = reinterpret_cast<const unsigned char *>(data.data());
        bool pLenOk = len >= (1 + 1 + 2 + 4 /*+ pUserIdLen*/ + 1);
        // pUserIdLen maybe 0 , if no UserId
        bool pEndWithNull = d[len] == '\x00';
        if (!pLenOk) {
            // never go there
            fail({}, "do_analysis_client_first_socks4_header (!pLenOk), never go there");
            return;
        }
        // NOTE: maybe we will face a bad or slow impl client , witch split a package into two or more piece
        //      so, we will not receive all the data in the package
        //      that is a bad impl, now , only the Golang impl have this issue
        //      BUT, in this place , we cannot detect is the client slow OR the client invalid
        if (!pEndWithNull) {
            // never go there
            fail({}, "do_analysis_client_first_socks4_header (!pEndWithNull), never go there");
            return;
        }
        // https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/example/cpp11/socks4/socks4.hpp
        // https://www.openssh.com/txt/socks4.protocol
        // https://zh.wikipedia.org/wiki/SOCKS
        //
        //                        0.0.0.x if socks4a                          | socks4a only
        //   +----+----+----+----+----+----+----+----+----+----+....+----+----+----+....+----+
        //   | VN | CD | DSTPORT |      DSTIP        | USERID       |NULL| HOSTNAME     |NULL|
        //   +----+----+----+----+----+----+----+----+----+----+....+----+----+----+....+----+
        //     1    1      2              4             variable      1    variable       1
        //
        //   USERID maybe len=0 if no USERID.
        //
        std::vector<int> nullByteIndex;
        for (int i = 8; i < len; ++i) {
            if (d[i] == 0) {
                nullByteIndex.emplace_back(i);
            }
        }
        if (d[4] == 0 &&
            d[5] == 0 &&
            d[6] == 0 &&
            d[7] != 0) {
            // this is socks4a
            if (nullByteIndex.size() != 2) {
                // more or less \x00
                BOOST_LOG_S5B_ID(relayId, error)
                    << "do_analysis_client_first_socks4_header (nullByteIndex.size() != 2) invalid socks4a package";
                fail({}, "do_analysis_client_first_socks4_header (nullByteIndex.size() != 2) invalid socks4a package");
                return;
            }
            if (nullByteIndex[1] - nullByteIndex[0] < 2) {
                // invalid HOSTNAME
                BOOST_LOG_S5B_ID(relayId, error)
                    << "do_analysis_client_first_socks4_header (nullByteIndex[1] - nullByteIndex[0] < 2) invalid HOSTNAME";
                fail({}, "do_analysis_client_first_socks4_header (nullByteIndex[1] - nullByteIndex[0] < 2)");
                return;
            }
            // get HOSTNAME
            BOOST_LOG_S5B_ID(relayId, trace) << "do_analysis_client_first_socks4_header this is socks4a";

            ptr->host = std::string{
                    d + nullByteIndex[0] + 1,
                    d + nullByteIndex[1]
            };
            BOOST_LOG_S5B_ID(relayId, trace)
                << "do_analysis_client_first_socks4_header ptr->host:[" << ptr->host << "]";
        } else {
            // this is socks4
            if (nullByteIndex.size() != 1) {
                // more or less \x00
                BOOST_LOG_S5B_ID(relayId, error)
                    << "do_analysis_client_first_socks4_header (nullByteIndex.size() != 1) invalid socks4 package";
                fail({}, "do_analysis_client_first_socks4_header (nullByteIndex.size() != 1) invalid socks4 package");
                return;
            }
            BOOST_LOG_S5B_ID(relayId, trace) << "do_analysis_client_first_socks4_header this is socks4";
            // get IP
            auto addr = reinterpret_cast<const unsigned char *>(d + 4);
            boost::asio::ip::address_v4::bytes_type ipV4Byte;
            // Copy the address into our array
            std::copy(addr, addr + ipV4Byte.size(), ipV4Byte.data());
            // Finally, initialize.
            boost::asio::ip::address_v4 ipv4(ipV4Byte);
            ptr->host = ipv4.to_string();
            BOOST_LOG_S5B_ID(relayId, trace)
                << "do_analysis_client_first_socks4_header ptr->host:[" << ptr->host << "]";
        }
        // socks4 only support ipv4
        ptr->port = (
                d[2] << 8
                |
                d[3]
        );
        if (ptr->authClientManager->needAuth()) {
            // need auth
            if (nullByteIndex[0] <= 8) {
                // the len(USERID)==0, USERID not exist
                BOOST_LOG_S5B_ID(relayId, error)
                    << "do_analysis_client_first_socks4_header need auth but (nullByteIndex[1] <= 8), "
                    << " need auth but no USERID";
                do_handshake_client_end_error(92);
                return;
            } else {
                // get and check username
                auto username = std::string{
                        d + 8,
                        d + nullByteIndex[0]
                };
                BOOST_LOG_S5B_ID(relayId, trace) << "do_analysis_client_first_socks4_header auth username:" << username;

                auto au = ptr->authClientManager->checkAuthUserOnly(username);
                if (au) {
                    BOOST_LOG_S5B_ID(relayId, trace)
                        << "do_auth_client_read auth ok :[" << username << "]";
                    ptr->tcpRelaySession->authUser = au;
                    // ok
                } else {
                    BOOST_LOG_S5B_ID(relayId, trace)
                        << "do_auth_client_read auth error :[" << username << "]";
                    do_handshake_client_end_error(92);
                    return;
                }
            }
        }
        BOOST_LOG_S5B_ID(relayId, trace)
            << "do_analysis_client_first_socks4_header ptr->port:[" << ptr->port << "]";
        switch (d[1]) {
            case 0x01:
                // CONNECT
                BOOST_LOG_S5B_ID(relayId, trace) << "do_analysis_client_first_socks4_header CONNECT";
                ptr->proxyRelayMode = ProxyRelayMode::connect;
                break;
            case 0x02:
                // BIND
                // we not impl this, never
                BOOST_LOG_S5B_ID(relayId, trace) << "do_analysis_client_first_socks4_header BIND";

                ptr->downstream_buf_.consume(len);
                BOOST_ASSERT(ptr->downstream_buf_.size() == 0);

                do_handshake_client_end_error(91);
                ptr->proxyRelayMode = ProxyRelayMode::bind;
                break;
            default:
                // never go there
                return fail(
                        {},
                        "do_analysis_client_first_socks4_header switch (d[1]), never go there");
                break;
        }

        ptr->downstream_buf_.consume(len);
        BOOST_ASSERT(ptr->downstream_buf_.size() == 0);
        BOOST_LOG_S5B_ID(relayId, trace)
            << "do_analysis_client_first_socks4_header call do_ready_to_send_last_ok_package";
        do_ready_to_send_last_ok_package(ptr);

    } else {
        badParentPtr();
    }
}

void Socks4ServerImpl::do_ready_to_send_last_ok_package(
        const std::shared_ptr<decltype(parents)::element_type> &ptr
) {
    ptr->do_whenDownReady();
}


void Socks4ServerImpl::do_handshake_client_end_error(uint8_t errorType) {
    BOOST_LOG_S5B_ID(relayId, warning) << "do_handshake_client_end_error errorType:" << errorType;

    // do_downstream_write
    auto ptr = parents.lock();
    if (ptr) {
        //
        //    +----+----+----+----+----+----+----+----+
        //    | VN | CD | DSTPORT |      DSTIP        |
        //    +----+----+----+----+----+----+----+----+
        //      1    1      2              4
        // VN is the version of the reply code and should be 0. CD is the result code with one of the following values:
        //
        //	90: request granted
        //	91: request rejected or failed
        //	92: request rejected because SOCKS server cannot connect to indented on the client
        //

        switch (errorType) {
            case 91:
            case 92:
                break;
            default:
                // never go there
                BOOST_ASSERT_MSG(false, "do_handshake_client_end_error switch (errorType) default, never go there");
                return fail({}, "do_handshake_client_end_error switch (errorType) default, never go there");
                break;
        }

        auto data_send = std::make_shared<std::string>(
                "\x00\x01\x00\x00\x00\x00\x00\x00", 8
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


void Socks4ServerImpl::do_handshake_client_end() {

    // do_downstream_write
    auto ptr = parents.lock();
    if (ptr) {
        //
        //    +----+----+----+----+----+----+----+----+
        //    | VN | CD | DSTPORT |      DSTIP        |
        //    +----+----+----+----+----+----+----+----+
        //      1    1      2              4
        // VN is the version of the reply code and should be 0. CD is the result code with one of the following values:
        //
        //	90: request granted
        //	91: request rejected or failed
        //	92: request rejected because SOCKS server cannot connect to indented on the client
        //

        // we never in BIND mode , and the protocol not defined UDP and IPv6
        // so, simple ignore it
        auto data_send = std::make_shared<std::string>(
                "\x00\x01\x00\x00\x00\x00\x00\x00", 8
        );

        data_send->at(1) = static_cast<char>(90);

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

                    ptr->do_whenDownEnd(false);
                }
        );

    } else {
        badParentPtr();
    }

}


void Socks4ServerImpl::to_send_last_ok_package() {
    do_handshake_client_end();
}

void Socks4ServerImpl::to_send_last_error_package() {
    BOOST_LOG_S5B_ID(relayId, warning) << "to_send_last_error_package";
    do_handshake_client_end_error(91);
}

Socks4ServerImpl::Socks4ServerImpl(const std::shared_ptr<ProxyHandshakeAuth> &parents_)
        : parents(parents_), relayId(parents_->relayId) {}

void Socks4ServerImpl::fail(boost::system::error_code ec, const std::string &what) {
    std::string r;
    {
        std::stringstream ss;
        ss << what << ": [" << ec.message() << "] . ";
        r = ss.str();
    }
    BOOST_LOG_S5B_ID(relayId, error) << "Socks4ServerImpl::fail " << r;

    do_whenError(ec);
}

