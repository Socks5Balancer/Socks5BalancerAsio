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
                                    break;
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
