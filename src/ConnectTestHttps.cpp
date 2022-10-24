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

#include "ConnectTestHttps.h"

#include "UtilTools.h"

#include <boost/beast/version.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>

ConnectTestHttpsSession::ConnectTestHttpsSession(boost::asio::executor executor,
                                                 const std::shared_ptr<boost::asio::ssl::context> &ssl_context,
                                                 const std::string &targetHost, uint16_t targetPort,
                                                 const std::string &targetPath, int httpVersion,
                                                 const std::string &socks5Host, const std::string &socks5Port,
                                                 bool slowImpl,
                                                 std::chrono::milliseconds delayTime) :
        executor(executor),
        resolver_(executor),
        stream_(executor, *ssl_context),
        targetHost(targetHost),
        targetPort(targetPort),
        targetPath(targetPath),
        httpVersion(httpVersion),
        socks5Host(socks5Host),
        socks5Port(socks5Port),
        slowImpl(slowImpl),
        delayTime(delayTime) {
//        std::cout << "ConnectTestHttpsSession :" << socks5Host << ":" << socks5Port << std::endl;

    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(stream_.native_handle(), targetHost.c_str())) {
        boost::beast::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
        std::cerr << ec.message() << "\n";
        return;
    }

    // Set up an HTTP GET request message
    req_.version(httpVersion);
    req_.method(boost::beast::http::verb::get);
    req_.target(targetPath);
    req_.set(boost::beast::http::field::host, targetHost);
    req_.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);

}

bool ConnectTestHttpsSession::isComplete() {
    return _isComplete;
}

void ConnectTestHttpsSession::run(std::function<void(SuccessfulInfo)> onOk, std::function<void(std::string)> onErr) {
    callback = std::make_unique<CallbackContainer>();
    callback->successfulCallback = std::move(onOk);
    callback->failedCallback = std::move(onErr);
    if (delayTime.count() == 0) {
        do_resolve();
    } else {
        asyncDelay(delayTime, executor, [self = shared_from_this(), this]() {
            do_resolve();
        });
    }
}

void ConnectTestHttpsSession::release() {
    callback.reset();
    _isComplete = true;
}

void ConnectTestHttpsSession::stop() {
    boost::system::error_code ec;
    stream_.shutdown(ec);
    resolver_.cancel();
    release();
}

void ConnectTestHttpsSession::fail(boost::system::error_code ec, const std::string &what) {
    std::string r;
    {
        std::stringstream ss;
        ss << what << ": [" << ec.message() << "] . on "
           << "targetHost:" << targetHost << " "
           << "targetPort:" << targetPort << " "
           << "targetPath:" << targetPath << " "
           << "httpVersion:" << httpVersion << " "
           << "socks5Host:" << socks5Host << " "
           << "socks5Port:" << socks5Port << " ";
        r = ss.str();
    }
    std::cerr << r << "\n";
    if (callback && callback->failedCallback) {
        callback->failedCallback(r);
    }
    release();
}

void ConnectTestHttpsSession::allOk() {
//        std::cout << res_ << std::endl;
    if (callback && callback->successfulCallback) {
        callback->successfulCallback(res_);
    }
    release();
}

void ConnectTestHttpsSession::do_resolve() {
//        std::cout << "do_resolve on :" << socks5Host << ":" << socks5Port << std::endl;

    // Look up the domain name
    resolver_.async_resolve(
            socks5Host,
            socks5Port,
            boost::beast::bind_front_handler(
                    [this, self = shared_from_this()](
                            boost::beast::error_code ec,
                            const boost::asio::ip::tcp::resolver::results_type &results) {
                        if (ec) {
                            return fail(ec, "resolve");
                        }

//                            std::cout << "do_resolve on :" << socks5Host << ":" << socks5Port
//                                      << " get results: "
//                                      << results->endpoint().address() << ":" << results->endpoint().port()
//                                      << std::endl;
                        do_tcp_connect(results);
                    }));

}

void ConnectTestHttpsSession::do_tcp_connect(
        const boost::asio::ip::basic_resolver<boost::asio::ip::tcp, boost::asio::executor>::results_type &results) {


    // Set a timeout on the operation
    boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

    // Make the connection on the IP address we get from a lookup
    boost::beast::get_lowest_layer(stream_).async_connect(
            results,
            boost::beast::bind_front_handler(
                    [this, self = shared_from_this(), results](
                            boost::beast::error_code ec,
                            const boost::asio::ip::tcp::resolver::results_type::endpoint_type &) {
                        if (ec) {
                            std::stringstream ss;
                            ss << "do_tcp_connect on :"
                               << results->endpoint().address() << ":" << results->endpoint().port();
                            return fail(ec, ss.str().c_str());
                        }

                        do_socks5_handshake_write();
                    }));

}

void ConnectTestHttpsSession::do_socks5_handshake_write() {

    // send socks5 client handshake
    // +----+----------+----------+
    // |VER | NMETHODS | METHODS  |
    // +----+----------+----------+
    // | 1  |    1     | 1 to 255 |
    // +----+----------+----------+
    auto data_send = std::make_shared<std::string>(
            "\x05\x01\x00", 3
    );

    // Set a timeout on the operation
    boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

    boost::asio::async_write(
            boost::beast::get_lowest_layer(stream_),
            boost::asio::buffer(*data_send),
            boost::beast::bind_front_handler(
                    [this, self = shared_from_this(), data_send](
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

                        do_socks5_handshake_read();
                    })
    );
}

void ConnectTestHttpsSession::do_socks5_handshake_read() {
    auto socks5_read_buf = std::make_shared<std::vector<uint8_t>>(MAX_LENGTH);

    // Set a timeout on the operation
    boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

    // Make the connection on the IP address we get from a lookup
    boost::beast::get_lowest_layer(stream_).async_read_some(
            boost::asio::buffer(*socks5_read_buf, MAX_LENGTH),
            boost::beast::bind_front_handler(
                    [this, self = shared_from_this(), socks5_read_buf](
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
                            do_shutdown();
                            return fail(ec, "socks5_handshake_read (bytes_transferred < 2)");
                        }

                        do_socks5_connect_write();
                    }));
}

void ConnectTestHttpsSession::do_socks5_connect_write() {


    // analysis targetHost and targetPort
    // targetHost,
    // targetPort,
    boost::beast::error_code ec;
    auto addr = boost::asio::ip::make_address(targetHost, ec);

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
        if (targetHost.size() > 253) {
            do_shutdown();
            return fail(ec, "socks5_connect_write (targetHost.size() > 253)");
        }
        data_send->push_back(static_cast<uint8_t>(targetHost.size()));
        data_send->insert(data_send->end(), targetHost.begin(), targetHost.end());
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
    data_send->push_back(static_cast<uint8_t>(targetPort >> 8));
    data_send->push_back(static_cast<uint8_t>(targetPort & 0xff));

    // Set a timeout on the operation
    boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

    boost::asio::async_write(
            boost::beast::get_lowest_layer(stream_),
            boost::asio::buffer(*data_send),
            boost::beast::bind_front_handler(
                    [this, self = shared_from_this(), data_send](
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

                        do_socks5_connect_read();
                    })
    );
}

void ConnectTestHttpsSession::do_socks5_connect_read() {
    auto socks5_read_buf = std::make_shared<std::vector<uint8_t>>(MAX_LENGTH);

    // Set a timeout on the operation
    boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

    // Make the connection on the IP address we get from a lookup
//    boost::beast::get_lowest_layer(stream_).async_read_some(
    boost::asio::async_read(
            boost::beast::get_lowest_layer(stream_),
            boost::asio::buffer(*socks5_read_buf, MAX_LENGTH),
            boost::asio::transfer_at_least(6),
            boost::beast::bind_front_handler(
                    [this, self = shared_from_this(), socks5_read_buf](
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
                            do_shutdown();
                            std::stringstream ss;
                            ss << "ConnectTestHttpsSession socks5_connect_read (bytes_transferred < 6) "
                               << "bytes_transferred:" << bytes_transferred
                               << " the socks5_read_buf: "
                               << int(socks5_read_buf->at(0))
                               << int(socks5_read_buf->at(1))
                               << int(socks5_read_buf->at(2))
                               << int(socks5_read_buf->at(3));
                            return fail(ec, ss.str());
//                            return fail(ec, "socks5_connect_read (bytes_transferred < 6)");
                        }

                        if (slowImpl) {
                            // the server is a bad impl (like Golang), it maybe not send all data in one package, we try to ignore it
                            switch (socks5_read_buf->at(3)) {
                                case 0x03: {
                                    if (bytes_transferred != (socks5_read_buf->at(4) + 4 + 1 + 2)) {
                                        // try to read last data
                                        boost::asio::async_read(
                                                boost::beast::get_lowest_layer(stream_),
                                                boost::asio::buffer(*socks5_read_buf, MAX_LENGTH),
                                                boost::asio::transfer_exactly(
                                                        (socks5_read_buf->at(4) + 4 + 1 + 2) - bytes_transferred
                                                ),
                                                boost::beast::bind_front_handler(
                                                        [this, self = shared_from_this(), socks5_read_buf](
                                                                boost::beast::error_code ec,
                                                                const size_t &bytes_transferred) {
                                                            boost::ignore_unused(bytes_transferred);
                                                            if (ec) {
                                                                return fail(ec, "socks5_connect_read slowImpl 0x03");
                                                            }
                                                            do_ssl_handshake();
                                                        }
                                                )
                                        );
                                    } else {
                                        do_ssl_handshake();
                                    }
                                }
                                    break;
                                case 0x01: {
                                    if (bytes_transferred != (4 + 4 + 2)) {
                                        // try to read last data
                                        boost::asio::async_read(
                                                boost::beast::get_lowest_layer(stream_),
                                                boost::asio::buffer(*socks5_read_buf, MAX_LENGTH),
                                                boost::asio::transfer_exactly(
                                                        (4 + 4 + 2) - bytes_transferred
                                                ),
                                                boost::beast::bind_front_handler(
                                                        [this, self = shared_from_this(), socks5_read_buf](
                                                                boost::beast::error_code ec,
                                                                const size_t &bytes_transferred) {
                                                            boost::ignore_unused(bytes_transferred);
                                                            if (ec) {
                                                                return fail(ec, "socks5_connect_read slowImpl 0x01");
                                                            }
                                                            do_ssl_handshake();
                                                        }
                                                )
                                        );
                                    } else {
                                        do_ssl_handshake();
                                    }
                                }
                                    break;
                                case 0x04:{
                                    if (bytes_transferred != (4 + 16 + 2)) {
                                        // try to read last data
                                        boost::asio::async_read(
                                                boost::beast::get_lowest_layer(stream_),
                                                boost::asio::buffer(*socks5_read_buf, MAX_LENGTH),
                                                boost::asio::transfer_exactly(
                                                        (4 + 16 + 2) - bytes_transferred
                                                ),
                                                boost::beast::bind_front_handler(
                                                        [this, self = shared_from_this(), socks5_read_buf](
                                                                boost::beast::error_code ec,
                                                                const size_t &bytes_transferred) {
                                                            boost::ignore_unused(bytes_transferred);
                                                            if (ec) {
                                                                return fail(ec, "socks5_connect_read slowImpl 0x04");
                                                            }
                                                            do_ssl_handshake();
                                                        }
                                                )
                                        );
                                    } else {
                                        do_ssl_handshake();
                                    }
                                }
                                    break;
                                default:
                                    return fail(ec, "socks5_connect_read slowImpl: never go there");
                                    break;
                            }
                            return;
                        }

                        if (socks5_read_buf->at(3) == 0x03
                            && bytes_transferred != (socks5_read_buf->at(4) + 4 + 1 + 2)
                                ) {
                            do_shutdown();
                            std::stringstream ss;
                            ss
                                    << "ConnectTestHttpsSession socks5_connect_read (socks5_read_buf->at(3) == 0x03) bytes_transferred: "
                                    << int(bytes_transferred) << " must is " << int(socks5_read_buf->at(4) + 4 + 1 + 2)
                                    << "  socks5_read_buf->at(4):" << int(socks5_read_buf->at(4));
                            return fail(ec, ss.str());
//                            return fail(ec, "socks5_connect_read (socks5_read_buf->at(3) == 0x03)");
                        }
                        if (socks5_read_buf->at(3) == 0x01
                            && bytes_transferred != (4 + 4 + 2)
                                ) {
                            do_shutdown();
                            std::stringstream ss;
                            ss
                                    << "ConnectTestHttpsSession socks5_connect_read (socks5_read_buf->at(3) == 0x01) bytes_transferred: "
                                    << int(bytes_transferred) << " must is " << int(4 + 4 + 2);
                            return fail(ec, ss.str());
//                            return fail(ec, "socks5_connect_read (socks5_read_buf->at(3) == 0x01)");
                        }
                        if (socks5_read_buf->at(3) == 0x04
                            && bytes_transferred != (4 + 16 + 2)
                                ) {
                            do_shutdown();
                            std::stringstream ss;
                            ss
                                    << "ConnectTestHttpsSession socks5_connect_read (socks5_read_buf->at(3) == 0x04) bytes_transferred: "
                                    << int(bytes_transferred) << " must is " << int(4 + 16 + 2);
                            return fail(ec, ss.str());
//                            return fail(ec, "socks5_connect_read (socks5_read_buf->at(3) == 0x04)");
                        }

                        // socks5 handshake now complete
                        do_ssl_handshake();
                    }));
}

void ConnectTestHttpsSession::do_ssl_handshake() {
    // Perform the SSL handshake
    stream_.async_handshake(
            boost::asio::ssl::stream_base::client,
            boost::beast::bind_front_handler(
                    [this, self = shared_from_this()](boost::beast::error_code ec) {
                        if (ec) {
                            return fail(ec, "ssl_handshake");
                        }

                        do_write();
                    }));
}

void ConnectTestHttpsSession::do_write() {

    // Set a timeout on the operation
    boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

    // Send the HTTP request to the remote host
    boost::beast::http::async_write(
            stream_, req_,
            boost::beast::bind_front_handler(
                    [this, self = shared_from_this()](
                            boost::beast::error_code ec,
                            std::size_t bytes_transferred) {
                        boost::ignore_unused(bytes_transferred);

                        if (ec) {
                            return fail(ec, "write");
                        }

                        do_read();
                    }));
}

void ConnectTestHttpsSession::do_read() {
    // Receive the HTTP response
    boost::beast::http::async_read(
            stream_, buffer_, res_,
            boost::beast::bind_front_handler(
                    [this, self = shared_from_this()](
                            boost::beast::error_code ec,
                            std::size_t bytes_transferred) {
                        boost::ignore_unused(bytes_transferred);

                        if (ec) {
                            return fail(ec, "read");
                        }

                        // Write the message to standard out
                        // std::cout << res_ << std::endl;

                        do_shutdown(true);
                    }));

}

void ConnectTestHttpsSession::do_shutdown(bool isOn) {

    // Set a timeout on the operation
    boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

    // Gracefully close the stream
    stream_.async_shutdown(
            boost::beast::bind_front_handler(
                    [this, self = shared_from_this(), isOn](boost::beast::error_code ec) {
                        if (ec == boost::asio::error::eof) {
                            // Rationale:
                            // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
                            ec = {};
                        }
                        if (ec) {
                            // https://github.com/boostorg/beast/issues/915
                            if (ec.message() != "stream truncated") {
                                return fail(ec, "shutdown");
                            }
                        }

                        if (isOn) {
                            // If we get here then the connection is closed gracefully
                            allOk();
                        }
                    }));
}

ConnectTestHttps::ConnectTestHttps(boost::asio::executor ex) :
        executor(ex),
        ssl_context(std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23)) {

    if (need_verify_ssl) {
        ssl_context->set_verify_mode(boost::asio::ssl::verify_peer);
        ssl_context->set_default_verify_paths();
#ifdef _WIN32
        HCERTSTORE h_store = CertOpenSystemStore(0, _T("ROOT"));
        if (h_store) {
            X509_STORE *store = SSL_CTX_get_cert_store(ssl_context->native_handle());
            PCCERT_CONTEXT p_context = NULL;
            while ((p_context = CertEnumCertificatesInStore(h_store, p_context))) {
                const unsigned char *encoded_cert = p_context->pbCertEncoded;
                X509 *x509 = d2i_X509(NULL, &encoded_cert, p_context->cbCertEncoded);
                if (x509) {
                    X509_STORE_add_cert(store, x509);
                    X509_free(x509);
                }
            }
            CertCloseStore(h_store, 0);
        }
#endif // _WIN32
    } else {
        ssl_context->set_verify_mode(boost::asio::ssl::verify_none);
    }

}

std::shared_ptr<ConnectTestHttpsSession>
ConnectTestHttps::createTest(const std::string &socks5Host, const std::string &socks5Port,
                             bool slowImpl,
                             const std::string &targetHost, uint16_t targetPort, const std::string &targetPath,
                             int httpVersion,
                             std::chrono::milliseconds maxRandomDelay) {
    if (!cleanTimer) {
        cleanTimer = std::make_shared<boost::asio::steady_timer>(executor, std::chrono::seconds{5});
        do_cleanTimer();
    }
    auto s = std::make_shared<ConnectTestHttpsSession>(
            this->executor,
            this->ssl_context,
            targetHost,
            targetPort,
            targetPath,
            httpVersion,
            socks5Host,
            socks5Port,
            slowImpl,
            std::chrono::milliseconds{
                    maxRandomDelay.count() > 0 ? getRandom<long long int>(0, maxRandomDelay.count()) : 0
            }
    );
    sessions.push_back(s->shared_from_this());
    // sessions.front().lock();
    return s;
}

void ConnectTestHttps::do_cleanTimer() {
    auto c = [this, self = shared_from_this(), cleanTimer = this->cleanTimer]
            (const boost::system::error_code &e) {
        if (e) {
            return;
        }
//        std::cout << "do_cleanTimer()" << std::endl;

        auto it = sessions.begin();
        while (it != sessions.end()) {
            if (!(*it) || (*it)->isComplete()) {
                it = sessions.erase(it);
            } else {
                ++it;
            }
        }

        cleanTimer->expires_at(cleanTimer->expiry() + std::chrono::seconds{5});
        do_cleanTimer();
    };
    cleanTimer->async_wait(c);
}

void ConnectTestHttps::stop() {
    if (cleanTimer) {
        boost::system::error_code ec;
        cleanTimer->cancel(ec);
        cleanTimer.reset();
    }
    for (auto &a: sessions) {
        a->stop();
    }
    {
        auto it = sessions.begin();
        while (it != sessions.end()) {
            if (!(*it) || (*it)->isComplete()) {
                it = sessions.erase(it);
            } else {
                ++it;
            }
        }
    }
}
