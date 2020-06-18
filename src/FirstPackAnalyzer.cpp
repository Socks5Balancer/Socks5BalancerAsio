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

void FirstPackAnalyzer::start() {
    // we must keep strong ptr when running, until call the whenComplete,
    // to keep parent TcpRelaySession but dont make circle ref
    auto ptr = tcpRelaySession.lock();
    if (ptr) {
        // debug
        do_prepare_whenComplete();
    }
}

void FirstPackAnalyzer::do_prepare_whenComplete() {
    auto ptr = tcpRelaySession.lock();
    if (ptr) {
        // send remain data
        do_prepare_complete_downstream_write();
        do_prepare_complete_upstream_write();
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
    auto ptr = tcpRelaySession.lock();
    if (ptr) {
        whenError(error);
    }
}

void FirstPackAnalyzer::do_prepare_complete_downstream_write() {
    auto ptr = tcpRelaySession.lock();
    if (ptr) {
        boost::asio::async_write(
                downstream_socket_,
                downstream_buf_,
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
    }
}

void FirstPackAnalyzer::do_prepare_complete_upstream_write() {
    auto ptr = tcpRelaySession.lock();
    if (ptr) {
        boost::asio::async_write(
                upstream_socket_,
                upstream_buf_,
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
    }
}
