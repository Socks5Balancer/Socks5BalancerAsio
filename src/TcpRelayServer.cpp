
#include "TcpRelayServer.h"

boost::asio::ip::tcp::socket &TcpRelaySession::downstream_socket() {
    // Client socket
    return downstream_socket_;
}

boost::asio::ip::tcp::socket &TcpRelaySession::upstream_socket() {
    // Remote server socket
    return upstream_socket_;
}

void TcpRelaySession::start() {
    std::cout << "TcpRelaySession start()" << std::endl;
    try_connect_upstream();
}

void TcpRelaySession::try_connect_upstream() {
    if (retryCount <= retryLimit) {
        ++retryCount;
        auto s = upstreamPool->getServerBasedOnAddress();
        if (s) {
            std::cout << "TcpRelaySession try_connect_upstream()"
                      << " " << s->host << ":" << s->port << std::endl;
            nowServer = s;
            do_resolve(s->host, s->port);
        } else {
            std::cerr << "try_connect_upstream error:" << "No Valid Server." << "\n";
        }
    } else {
        std::cerr << "try_connect_upstream error:" << "(retryCount > retryLimit)" << "\n";
    }
}

void TcpRelaySession::do_resolve(const std::string &upstream_host, unsigned short upstream_port) {

    // Look up the domain name
    resolver_.async_resolve(
            upstream_host,
            std::to_string(upstream_port),
            [this, self = shared_from_this()](
                    const boost::system::error_code &error,
                    boost::asio::ip::tcp::resolver::results_type results) {
                if (error) {
                    std::cerr << "do_resolve error:" << error.message() << "\n";
                    nowServer->isOffline = true;
                    // try next
                    try_connect_upstream();
                }

                std::cout << "TcpRelaySession do_resolve()"
                          << " " << results->endpoint().address() << ":" << results->endpoint().port()
                          << std::endl;

                do_connect_upstream(results);
            });

}

void TcpRelaySession::do_connect_upstream(boost::asio::ip::tcp::resolver::results_type results) {

    // Attempt connection to remote server (upstream side)
    upstream_socket_.async_connect(
            results->endpoint(),
            [this, self = shared_from_this()](const boost::system::error_code &error) {
                if (!error) {

                    std::cout << "TcpRelaySession do_connect_upstream()" << std::endl;

                    ++nowServer->connectCount;
                    nowServer->updateOnlineTime();

                    // Setup async read from remote server (upstream)
                    do_upstream_read();

                    // Setup async read from client (downstream)
                    do_downstream_read();

                } else {
                    std::cerr << "do_connect_upstream error:" << error.message() << "\n";
                    nowServer->isOffline = true;
                    // try next
                    try_connect_upstream();
                }
            }
    );

}

void TcpRelaySession::do_upstream_read() {
    upstream_socket_.async_read_some(
            boost::asio::buffer(upstream_data_, max_data_length),
            [this, self = shared_from_this()](const boost::system::error_code &error,
                                              const size_t &bytes_transferred) {
                if (!error) {
                    // Read from remote server complete
                    // write it to server
                    do_downstream_write(bytes_transferred);
                } else {
                    close(error);
                }
            });
}

void TcpRelaySession::do_downstream_write(const size_t &bytes_transferred) {
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
                    close(error);
                }
            });
}

void TcpRelaySession::do_downstream_read() {
    downstream_socket_.async_read_some(
            boost::asio::buffer(downstream_data_, max_data_length),
            [this, self = shared_from_this()](const boost::system::error_code &error,
                                              const size_t &bytes_transferred) {
                if (!error) {
                    // Read from client complete
                    // write it to server
                    do_upstream_write(bytes_transferred);
                } else {
                    close(error);
                }
            });
}

void TcpRelaySession::do_upstream_write(const size_t &bytes_transferred) {
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
                    close(error);
                }
            });
}

void TcpRelaySession::close(boost::system::error_code error) {
    if (error == boost::asio::error::eof) {
        // Rationale:
        // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
        error = {};
    }

    if (downstream_socket_.is_open()) {
        downstream_socket_.close();
    }

    if (upstream_socket_.is_open()) {
        upstream_socket_.close();
    }

    --nowServer->connectCount;
}

void TcpRelayServer::start() {
    boost::asio::ip::tcp::resolver resolver(ex);
    boost::asio::ip::tcp::endpoint listen_endpoint =
            *resolver.resolve(configLoader->config.listenHost,
                              std::to_string(configLoader->config.listenPort)).begin();
    socket_acceptor.open(listen_endpoint.protocol());
    socket_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

    socket_acceptor.bind(listen_endpoint);
    socket_acceptor.listen();

    auto local_endpoint = socket_acceptor.local_endpoint();
    std::cout << "listening on: " << local_endpoint.address() << ":" << local_endpoint.port() << std::endl;

    async_accept();
}

void TcpRelayServer::stop() {
    boost::system::error_code ec;
    socket_acceptor.cancel(ec);
}

void TcpRelayServer::async_accept() {
    auto session = std::make_shared<TcpRelaySession>(ex, upstreamPool, configLoader->config.retryTimes);
    socket_acceptor.async_accept(
            session->downstream_socket(),
            [this, session](const boost::system::error_code error) {
                if (error == boost::asio::error::operation_aborted) {
                    // got cancel signal, stop calling myself
                    std::cerr << "async_accept error: operation_aborted" << "\n";
                    return;
                }
                if (!error) {
                    std::cout << "async_accept accept." << "\n";
                    boost::system::error_code ec;
                    auto endpoint = session->downstream_socket().remote_endpoint(ec);
                    if (!ec) {
                        std::cout << "incoming connection from : "
                                  << endpoint.address() << ":" << endpoint.port() << "\n";

                        session->start();
                    }
                }
                std::cout << "async_accept next." << "\n";
                async_accept();
            });
}
