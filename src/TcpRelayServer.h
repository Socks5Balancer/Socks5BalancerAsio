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

#ifndef SOCKS5BALANCERASIO_TCPRELAYSERVER_H
#define SOCKS5BALANCERASIO_TCPRELAYSERVER_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <memory>

// code template from https://github.com/ArashPartow/proxy/blob/master/tcpproxy_server.cpp
class TcpRelaySession : public std::enable_shared_from_this<TcpRelaySession> {

    boost::asio::ip::tcp::socket downstream_socket_;
    boost::asio::ip::tcp::socket upstream_socket_;

    enum {
        max_data_length = 8192
    }; //8KB
    unsigned char downstream_data_[max_data_length];
    unsigned char upstream_data_[max_data_length];

public:
    TcpRelaySession(boost::asio::executor ex) :
            downstream_socket_(ex),
            upstream_socket_(ex) {}

    boost::asio::ip::tcp::socket &downstream_socket() {
        // Client socket
        return downstream_socket_;
    }

    boost::asio::ip::tcp::socket &upstream_socket() {
        // Remote server socket
        return upstream_socket_;
    }


    void start(const std::string &upstream_host, unsigned short upstream_port) {
        try_connect_upstream(upstream_host, upstream_port);
    }

private:

    void try_connect_upstream(const std::string &upstream_host, unsigned short upstream_port) {
        // Attempt connection to remote server (upstream side)
        upstream_socket_.async_connect(
                boost::asio::ip::tcp::endpoint(
                        boost::asio::ip::address::from_string(upstream_host),
                        upstream_port),
                [this, self = shared_from_this()](const boost::system::error_code &error) {
                    if (!error) {

                        // Setup async read from remote server (upstream)
                        do_upstream_read();

                        // Setup async read from client (downstream)
                        do_downstream_read();

                    } else {
                        // TODO retry
                        close();
                    }
                }
        );
    }

    /*
       Section A: Remote Server --> Proxy --> Client
       Process data recieved from remote sever then send to client.
    */

    // Async read from remote server
    void do_upstream_read() {
        upstream_socket_.async_read_some(
                boost::asio::buffer(upstream_data_, max_data_length),
                [this, self = shared_from_this()](const boost::system::error_code &error,
                                                  const size_t &bytes_transferred) {
                    if (!error) {
                        // Read from remote server complete
                        // write it to server
                        do_downstream_write(bytes_transferred);
                    } else {
                        close();
                    }
                });
    }

    // Now send data to client
    void do_downstream_write(const size_t &bytes_transferred) {
        boost::asio::async_write(
                downstream_socket_,
                boost::asio::buffer(upstream_data_, bytes_transferred),
                [this, self = shared_from_this(), bytes_transferred](
                        const boost::system::error_code &error,
                        std::size_t bytes_transferred_) {
                    if (!error || bytes_transferred_ != bytes_transferred) {
                        // Write to client complete
                        // read more again
                        do_upstream_read();
                    } else {
                        close();
                    }
                });
    }
    // *** End Of Section A ***


    /*
       Section B: Client --> Proxy --> Remove Server
       Process data recieved from client then write to remove server.
    */

    // Async read from client
    void do_downstream_read() {
        downstream_socket_.async_read_some(
                boost::asio::buffer(downstream_data_, max_data_length),
                [this, self = shared_from_this()](const boost::system::error_code &error,
                                                  const size_t &bytes_transferred) {
                    if (!error) {
                        // Read from client complete
                        // write it to server
                        do_upstream_write(bytes_transferred);
                    } else {
                        close();
                    }
                });
    }

    // Now send data to remote server
    void do_upstream_write(const size_t &bytes_transferred) {
        boost::asio::async_write(
                upstream_socket_,
                boost::asio::buffer(downstream_data_, bytes_transferred),
                [this, self = shared_from_this(), bytes_transferred](
                        const boost::system::error_code &error,
                        std::size_t bytes_transferred_) {
                    if (!error || bytes_transferred_ != bytes_transferred) {
                        // Write to remote server complete
                        // read more again
                        do_downstream_read();
                    } else {
                        close();
                    }
                });
    }
    // *** End Of Section B ***

    void close() {
        // boost::mutex::scoped_lock lock(mutex_);

        if (downstream_socket_.is_open()) {
            downstream_socket_.close();
        }

        if (upstream_socket_.is_open()) {
            upstream_socket_.close();
        }
    }

};

class TcpRelayServer : public std::enable_shared_from_this<TcpRelayServer> {
public:
    TcpRelayServer(boost::asio::executor ex) {}
};


#endif //SOCKS5BALANCERASIO_TCPRELAYSERVER_H
