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
#include "TcpRelayServer.h"
#include "TcpRelaySession.h"

void ProxyHandshakeAuth::do_read_client_first_3_byte() {
    // do_downstream_read
    auto ptr = tcpRelaySession;
    if (ptr) {
        boost::asio::async_read(
                downstream_socket_,
                downstream_buf_,
                boost::asio::transfer_at_least(3),
                [this, self = shared_from_this(), ptr](
                        const boost::system::error_code &error,
                        const size_t &bytes_transferred) {
                    if (error) {
                        fail(error, "ProxyHandshakeAuth::do_read_client_first_3_byte() (error)");
                        return;
                    }
                    if (!error && bytes_transferred >= 3 && downstream_buf_.size() >= 3) {
                        // check first 3 byte

                        auto d = reinterpret_cast<const unsigned char *>(downstream_buf_.data().data());
                        if (d[0] == 0x05 &&
                            (d[1] == 0x02 || d[1] == 0x01) &&
                            (d[2] == 0x02 || d[2] == 0x00)) {
                            if (downstream_buf_.size() != (2 + (uint8_t) d[1])) {
                                BOOST_LOG_S5B_ID(relayId, error)
                                    << "ProxyHandshakeAuth::do_read_client_first_3_byte()"
                                    << " (downstream_buf_.size() != (2 + (uint8_t) d[1]))";
                                fail(error,
                                     std::string{"ProxyHandshakeAuth::do_read_client_first_3_byte()"} +
                                     std::string{" (downstream_buf_.size() != (2 + (uint8_t) d[1]))"}
                                );
                                return;
                            }
                            // is socks5
                            BOOST_LOG_S5B_ID(relayId, trace) << "is socks5";
                            connectType = ConnectType::socks5;
                            // do socks5 handshake with client (downside)
                            upsideConnectType = UpsideConnectType::socks5;
                            BOOST_ASSERT(!util_Socks5ServerImpl_);
                            util_Socks5ServerImpl_ = std::make_shared<decltype(util_Socks5ServerImpl_)::element_type>(
                                    shared_from_this()
                            );
                            BOOST_ASSERT(util_Socks5ServerImpl_);
                            util_Socks5ServerImpl_->do_analysis_client_first_socks5_header();
                        } else if (d[0] == 0x04 &&
                                   (d[1] == 0x02 || d[1] == 0x01)) {
                            if (configLoader->config.disableSocks4) {
                                BOOST_LOG_S5B_ID(relayId, warning)
                                    << "ProxyHandshakeAuth::do_read_client_first_3_byte()"
                                    << " seems like a socks4/socks4a income, but socks4 is disabled. denny!";
                                fail(error,
                                     std::string{"ProxyHandshakeAuth::do_read_client_first_3_byte()"} +
                                     std::string{" seems like a socks4/socks4a income, but socks4 is disabled. denny!"}
                                );
                                return;
                            }
                            auto data = downstream_buf_.data();
                            size_t len = data.size();
                            auto dd = reinterpret_cast<const unsigned char *>(data.data());
                            bool pLenOk = len >= (1 + 1 + 2 + 4 /*+ pUserIdLen*/ + 1);
                            bool pEndWithNull = dd[len] == '\x00';
                            if (!pLenOk || !pEndWithNull) {
                                BOOST_LOG_S5B_ID(relayId, error)
                                    << "ProxyHandshakeAuth::do_read_client_first_3_byte()"
                                    << " (!pLenOk || !pEndWithNull)";
                                BOOST_LOG_S5B_ID(relayId, trace)
                                    << "ProxyHandshakeAuth::do_read_client_first_3_byte()"
                                    << " pLenOk:" << pLenOk
                                    << " pEndWithNull:" << pEndWithNull
                                    << " len:" << len
                                    << " lenNeed:" << (1 + 1 + 2 + 4 /*+ pUserIdLen*/ + 1);
                                fail(error,
                                     std::string{"ProxyHandshakeAuth::do_read_client_first_3_byte()"} +
                                     std::string{" (!pLenOk || !pEndWithNull)"}
                                );
                                return;
                            }
                            // size_t pUserIdLen = len - (1 + 1 + 2 + 4 + 1);
                            // is socks4
                            BOOST_LOG_S5B_ID(relayId, trace) << "is socks4";
                            connectType = ConnectType::socks4;
                            // do socks5 handshake with server (upside)
                            upsideConnectType = UpsideConnectType::socks5;
                            // do socks4 handshake with client (downside)
                            BOOST_ASSERT(!util_Socks4ServerImpl_);
                            util_Socks4ServerImpl_ = std::make_shared<decltype(util_Socks4ServerImpl_)::element_type>(
                                    shared_from_this()
                            );
                            BOOST_ASSERT(util_Socks4ServerImpl_);
                            util_Socks4ServerImpl_->do_analysis_client_first_socks4_header();
                        } else if ((d[0] == 'C' || d[0] == 'c') &&
                                   (d[1] == 'O' || d[1] == 'o') &&
                                   (d[2] == 'N' || d[2] == 'n')) {
                            // is http Connect
                            BOOST_LOG_S5B_ID(relayId, trace) << "is http Connect";
                            // analysis downside target server and create upside handshake
                            upsideConnectType = UpsideConnectType::socks5;
                            // do http handshake with client (downside)
                            connectType = ConnectType::httpConnect;
                            BOOST_ASSERT(!util_HttpServerImpl_);
                            util_HttpServerImpl_ = std::make_shared<decltype(util_HttpServerImpl_)::element_type>(
                                    shared_from_this()
                            );
                            BOOST_ASSERT(util_HttpServerImpl_);
                            util_HttpServerImpl_->do_analysis_client_first_http_header();
                        } else {
                            // is other protocol
                            BOOST_LOG_S5B_ID(relayId, trace) << "is other protocol";
                            // analysis target server and create socks5 handshake
                            switch (d[0]) {
                                case 'P':
                                case 'p':
                                    // POST, PUT, PATCH
                                    BOOST_LOG_S5B_ID(relayId, trace) << "is POST, PUT, PATCH";
                                    connectType = ConnectType::httpOther;
                                    break;
                                case 'G':
                                case 'g':
                                    BOOST_LOG_S5B_ID(relayId, trace) << "is GET";
                                    // GET
                                    connectType = ConnectType::httpOther;
                                    break;
                                case 'H':
                                case 'h':
                                    BOOST_LOG_S5B_ID(relayId, trace) << "is HEAD";
                                    // HEAD
                                    connectType = ConnectType::httpOther;
                                    break;
                                case 'D':
                                case 'd':
                                    BOOST_LOG_S5B_ID(relayId, trace) << "is DELETE";
                                    // DELETE
                                    connectType = ConnectType::httpOther;
                                    break;
                                case 'O':
                                case 'o':
                                    BOOST_LOG_S5B_ID(relayId, trace) << "is OPTIONS";
                                    // OPTIONS
                                    connectType = ConnectType::httpOther;
                                    break;
                                case 'T':
                                case 't':
                                    BOOST_LOG_S5B_ID(relayId, trace) << "is TRACE";
                                    // TRACE
                                    connectType = ConnectType::httpOther;
                                    break;
                                default:
                                    BOOST_LOG_S5B_ID(relayId, trace) << "is default...";
                                    connectType = ConnectType::unknown;
                                    fail({}, "ConnectType::unknown");
                                    return;
                            }
                            // debug
                            upsideConnectType = UpsideConnectType::socks5;
                            // do http handshake with client (downside)
                            //connectType = ConnectType::httpConnect;
                            BOOST_ASSERT(!util_HttpServerImpl_);
                            util_HttpServerImpl_ = std::make_shared<decltype(util_HttpServerImpl_)::element_type>(
                                    shared_from_this()
                            );
                            BOOST_ASSERT(util_HttpServerImpl_);
                            util_HttpServerImpl_->do_analysis_client_first_http_header();
                        }

                    } else {
                        BOOST_LOG_S5B_ID(relayId, error)
                            << "ProxyHandshakeAuth::do_read_client_first_3_byte():"
                            << " error:" << error.what()
                            << " bytes_transferred:" << bytes_transferred
                            << " downstream_buf_.size():" << downstream_buf_.size();
                        fail(error, "!(!error && bytes_transferred >= 3 && downstream_buf_.size() >= 3)");
                        return;
                    }
                });
    } else {
        badParentPtr();
    }
}

void ProxyHandshakeAuth::start() {
//    util_HttpClientImpl_ = std::make_shared<decltype(util_HttpClientImpl_)::element_type>(shared_from_this());
//    util_HttpServerImpl_ = std::make_shared<decltype(util_HttpServerImpl_)::element_type>(shared_from_this());
//    util_Socks5ClientImpl_ = std::make_shared<decltype(util_Socks5ClientImpl_)::element_type>(shared_from_this());
//    util_Socks5ServerImpl_ = std::make_shared<decltype(util_Socks5ServerImpl_)::element_type>(shared_from_this());
//    util_Socks4ServerImpl_ = std::make_shared<decltype(util_Socks4ServerImpl_)::element_type>(shared_from_this());

    auto ptr = tcpRelaySession;
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
    BOOST_LOG_S5B_ID(relayId, trace) << "ProxyHandshakeAuth::do_whenUpReady()";
    // do send last ok package to client in downside
    switch (connectType) {
        case ConnectType::socks5:
            BOOST_ASSERT(util_Socks5ServerImpl_);
            util_Socks5ServerImpl_->to_send_last_ok_package();
            break;
        case ConnectType::socks4:
            BOOST_ASSERT(util_Socks4ServerImpl_);
            util_Socks4ServerImpl_->to_send_last_ok_package();
            break;
        case ConnectType::httpConnect:
        case ConnectType::httpOther:
            BOOST_ASSERT(util_HttpServerImpl_);
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
    BOOST_LOG_S5B_ID(relayId, trace) << "ProxyHandshakeAuth::do_whenUpReadyError()";
    // do send last error package to client in downside
    switch (connectType) {
        case ConnectType::socks5:
            BOOST_ASSERT(util_Socks5ServerImpl_);
            util_Socks5ServerImpl_->to_send_last_error_package();
            break;
        case ConnectType::socks4:
            BOOST_ASSERT(util_Socks4ServerImpl_);
            util_Socks4ServerImpl_->to_send_last_error_package();
            break;
        case ConnectType::httpConnect:
        case ConnectType::httpOther:
            BOOST_ASSERT(util_HttpServerImpl_);
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
    BOOST_LOG_S5B_ID(relayId, trace) << "ProxyHandshakeAuth::do_whenDownReady() start upside handshake";
    BOOST_ASSERT(!util_Socks5ClientImpl_);
    util_Socks5ClientImpl_ = std::make_shared<decltype(util_Socks5ClientImpl_)::element_type>(shared_from_this());
    BOOST_ASSERT(util_Socks5ClientImpl_);
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
    BOOST_LOG_S5B_ID(relayId, trace) << "ProxyHandshakeAuth::do_whenDownEnd() error:" << error;
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
    BOOST_LOG_S5B_ID(relayId, trace) << "ProxyHandshakeAuth::do_whenCompleteUp()";
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
    BOOST_LOG_S5B_ID(relayId, trace) << "ProxyHandshakeAuth::do_whenCompleteDown()";
    check_whenComplete();
}

void ProxyHandshakeAuth::check_whenComplete() {
    BOOST_LOG_S5B_ID(relayId, trace) << "ProxyHandshakeAuth::check_whenComplete() check";
    if (completeDownError || completeUpError) {
        BOOST_LOG_S5B_ID(relayId, warning) << "check_whenComplete (completeDownError || completeUpError)";
        fail({}, "check_whenComplete (completeDownError || completeUpError)");
        return;
    } else if (completeDown && completeUp) {
        auto ptr = tcpRelaySession;
        if (ptr) {
            // all ok
            BOOST_LOG_S5B_ID(relayId, trace) << "ProxyHandshakeAuth::check_whenComplete() all ok";
            do_whenComplete();
        }
    }
}

bool ProxyHandshakeAuth::upside_support_udp_mode() {
    switch (upsideConnectType) {
        case UpsideConnectType::socks5:
            return true;
            break;
        case UpsideConnectType::socks4:
            return false;
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

bool ProxyHandshakeAuth::upside_in_udp_mode() {
    switch (upsideConnectType) {
        case UpsideConnectType::socks5:
            BOOST_ASSERT(util_Socks5ClientImpl_);
            return util_Socks5ClientImpl_->udpEnabled;
            break;
        case UpsideConnectType::socks4:
            return false;
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
            BOOST_ASSERT(util_Socks5ServerImpl_);
            return util_Socks5ServerImpl_->udpEnabled;
            break;
        case ConnectType::socks4:
            BOOST_ASSERT(util_Socks4ServerImpl_);
            return util_Socks4ServerImpl_->udpEnabled;
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

void ProxyHandshakeAuth::fail(boost::system::error_code ec, const std::string &what) {
    std::string r;
    {
        std::stringstream ss;
        ss << what << ": [" << ec.message() << "] . ";
        r = ss.str();
    }
    BOOST_LOG_S5B_ID(relayId, error) << r;

    do_whenError(ec);
}

ProxyHandshakeAuth::ProxyHandshakeAuth(
        std::shared_ptr<TcpRelaySession> tcpRelaySession_,
        boost::asio::ip::tcp::socket &downstream_socket_,
        boost::asio::ip::tcp::socket &upstream_socket_,
        std::shared_ptr<ConfigLoader> configLoader,
        std::shared_ptr<AuthClientManager> authClientManager,
        UpstreamServerRef nowServer, std::function<void()> whenComplete,
        std::function<void(boost::system::error_code)> whenError
) :
        tcpRelaySession(std::move(tcpRelaySession_)),
        downstream_socket_(downstream_socket_),
        upstream_socket_(upstream_socket_),
        configLoader(std::move(configLoader)),
        authClientManager(std::move(authClientManager)),
        nowServer(std::move(nowServer)),
        whenComplete(std::move(whenComplete)),
        whenError(std::move(whenError)),
        relayId(tcpRelaySession->relayId) {}
