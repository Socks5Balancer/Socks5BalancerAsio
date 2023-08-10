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

#include "ProxyHandshakeAuth.h"

void ProxyHandshakeAuth::do_read_client_first_3_byte() {
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
                            (d[1] == 0x02 || d[1] == 0x01 || d[1] == 0x00) &&
                            (d[2] == 0x02 || d[2] == 0x00)) {
                            // is socks5
                            std::cout << "is socks5" << std::endl;
                            connectType = ConnectType::socks5;
                            // do socks5 handshake with client (downside)
                            util_Socks5ServerImpl_->do_analysis_client_first_socks5_header();
                        } else if ((d[0] == 'C' || d[0] == 'c') &&
                                   (d[1] == 'O' || d[1] == 'o') &&
                                   (d[2] == 'N' || d[2] == 'n')) {
                            // is http Connect
                            std::cout << "is http Connect" << std::endl;
                            // analysis downside target server and create upside handshake
                            connectType = ConnectType::httpConnect;
                            // do http handshake with client (downside)
                            util_HttpServerImpl_->do_analysis_client_first_http_header();
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
                                    fail({}, "ConnectType::unknown");
                                    return;
                            }
                            // debug
                            // do http handshake with client (downside)
                            util_HttpServerImpl_->do_analysis_client_first_http_header();
                        }

                    } else {
                        do_whenError(error);
                    }
                });
    } else {
        badParentPtr();
    }
}

void ProxyHandshakeAuth::start() {
    util_HttpClientImpl_ = std::make_shared<decltype(util_HttpClientImpl_)::element_type>(shared_from_this());
    util_HttpServerImpl_ = std::make_shared<decltype(util_HttpServerImpl_)::element_type>(shared_from_this());
    util_Socks5ClientImpl_ = std::make_shared<decltype(util_Socks5ClientImpl_)::element_type>(shared_from_this());
    util_Socks5ServerImpl_ = std::make_shared<decltype(util_Socks5ServerImpl_)::element_type>(shared_from_this());

    auto ptr = tcpRelaySession.lock();
    if (ptr) {
        do_read_client_first_3_byte();
    } else {
        badParentPtr();
    }
}

void ProxyHandshakeAuth::do_whenUpReady() {
    if (readyUp) {
        // never go there
        BOOST_ASSERT_MSG(false, "do_whenUpReady");
        fail({}, "do_whenUpReady , (readyUp), never go there");
        return;
    }
    readyUp = true;
    // do send last ok package to client in downside
    switch (connectType) {
        case ConnectType::socks5:
            util_Socks5ServerImpl_->to_send_last_ok_package();
            break;
        case ConnectType::httpConnect:
        case ConnectType::httpOther:
            util_HttpServerImpl_->to_send_last_ok_package();
            break;
        case ConnectType::unknown:
        default:
            // never go there
            BOOST_ASSERT_MSG(false, "do_whenDownReady");
            fail({}, "do_whenUpReady (connectType invalid), never go there.");
            return;
    }
}

void ProxyHandshakeAuth::do_whenUpReadyError() {
    if (readyUp) {
        // never go there
        BOOST_ASSERT_MSG(false, "do_whenUpReadyError");
        fail({}, "do_whenUpReadyError , (readyUp), never go there");
        return;
    }
    readyUp = true;
    completeUpError = true;
    // do send last error package to client in downside
    switch (connectType) {
        case ConnectType::socks5:
            util_Socks5ServerImpl_->to_send_last_error_package();
            break;
        case ConnectType::httpConnect:
        case ConnectType::httpOther:
            util_HttpServerImpl_->to_send_last_error_package();
            break;
        case ConnectType::unknown:
        default:
            // never go there
            BOOST_ASSERT_MSG(false, "do_whenUpReadyError");
            fail({}, "do_whenUpReadyError (connectType invalid), never go there.");
            return;
    }
}

void ProxyHandshakeAuth::do_whenDownReady() {
    // downside ready for send last ok package
    if (readyDown) {
        // never go there
        BOOST_ASSERT_MSG(false, "do_whenDownReady");
        fail({}, "do_whenDownReady , (readyDown), never go there");
        return;
    }
    if (completeUp) {
        // never go there
        BOOST_ASSERT_MSG(false, "completeUp");
        fail({}, "do_whenDownReady , (completeUp), never go there");
        return;
    }
    readyDown = true;
    // start upside handshake
    util_Socks5ClientImpl_->start();
    // util_HttpClientImpl_->start();
}

void ProxyHandshakeAuth::do_whenUpEnd() {
    do_whenCompleteUp();
}

void ProxyHandshakeAuth::do_whenDownEnd(bool error) {
    if (error) {
        completeDownError = true;
    }
    do_whenCompleteDown();
}

void ProxyHandshakeAuth::do_whenCompleteUp() {
    if (completeUp) {
        // never go there
        BOOST_ASSERT_MSG(!(completeUp), "do_whenCompleteUp (completeUp)");
        fail({}, "do_whenCompleteUp (completeUp), never go there.");
        return;
    }
    completeUp = true;
    check_whenComplete();
}

void ProxyHandshakeAuth::do_whenCompleteDown() {
    if (completeDown) {
        // never go there
        BOOST_ASSERT_MSG(!(completeDown), "do_whenCompleteDown (completeDown)");
        fail({}, "do_whenCompleteDown (completeDown), never go there.");
        return;
    }
    completeDown = true;
    check_whenComplete();
}

void ProxyHandshakeAuth::check_whenComplete() {
    if (completeDown && completeUp) {
        if (completeDownError || completeUpError) {
            fail({}, "check_whenComplete (completeDownError || completeUpError)");
            return;
        } else {
            auto ptr = tcpRelaySession.lock();
            if (ptr) {
                // all ok
                whenComplete();
            }
        }
    }
}

bool ProxyHandshakeAuth::upside_support_udp_mode() {
    switch (connectType) {
        case ConnectType::socks5:
            return true;
            break;
        case ConnectType::httpConnect:
            return false;
            break;
        case ConnectType::httpOther:
            return false;
            break;
        case ConnectType::unknown:
        default:
            return false;
            break;
    }
}

bool ProxyHandshakeAuth::upside_in_udp_mode() {
    switch (upsideConnectType) {
        case UpsideConnectType::socks5:
            return util_Socks5ClientImpl_->udpEnabled;
            break;
        case UpsideConnectType::http:
            return false;
            break;
        case UpsideConnectType::none:
        default:
            return false;
            break;
    }
}

bool ProxyHandshakeAuth::downside_in_udp_mode() {
    switch (connectType) {
        case ConnectType::socks5:
            return util_Socks5ServerImpl_->udpEnabled;
            break;
        case ConnectType::httpConnect:
            return false;
            break;
        case ConnectType::httpOther:
            return false;
            break;
        case ConnectType::unknown:
        default:
            return false;
            break;
    }
}