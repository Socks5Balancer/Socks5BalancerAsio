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

#include "TcpTest.h"

void TcpTestSession::do_resolve() {
//        std::cout << "do_resolve on :" << socks5Host << ":" << socks5Port << std::endl;

    // Look up the domain name
    resolver_.async_resolve(
            socks5Host,
            socks5Port,
            [this, self = shared_from_this()](
                    boost::system::error_code ec,
                    const boost::asio::ip::tcp::resolver::results_type &results) {
                if (ec) {
                    std::stringstream ss;
                    ss << "do_resolve on :" << socks5Host << ":" << socks5Port;
                    return fail(ec, ss.str().c_str());
                }

//                    std::cout << "do_resolve on :" << socks5Host << ":" << socks5Port
//                              << " get results: "
//                              << results->endpoint().address() << ":" << results->endpoint().port()
//                              << std::endl;
                do_tcp_connect(results);
            });

}

void TcpTestSession::do_tcp_connect(
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

                        do_shutdown(true);
                    }));

}

void TcpTestSession::do_shutdown(bool isOn) {

    stream_.close();

    if (isOn) {
        // If we get here then the connection is closed gracefully
        allOk();
    }

}

void TcpTestSession::allOk() {
    if (callback && callback->successfulCallback) {
        callback->successfulCallback();
    }
    release();
}

void TcpTestSession::fail(boost::system::error_code ec, const std::string &what) {
    std::string r;
    {
        std::stringstream ss;
        ss << what << ": " << ec.message();
        r = ss.str();
    }
    std::cerr << r << "\n";
    if (callback && callback->failedCallback) {
        callback->failedCallback(r);
    }
    release();
}

void TcpTestSession::release() {
    callback.reset();
    _isComplete = true;
}

void TcpTestSession::stop() {
    resolver_.cancel();
    stream_.cancel();
    stream_.close();
    release();
}

void TcpTestSession::run(std::function<void()> onOk, std::function<void(std::string)> onErr) {
    callback = std::make_unique<CallbackContainer>();
    callback->successfulCallback = std::move(onOk);
    callback->failedCallback = std::move(onErr);
    do_resolve();
}

bool TcpTestSession::isComplete() {
    return _isComplete;
}

void TcpTest::do_cleanTimer() {
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

std::shared_ptr<TcpTestSession> TcpTest::createTest(std::string socks5Host, std::string socks5Port) {
    if (!cleanTimer) {
        cleanTimer = std::make_shared<boost::asio::steady_timer>(executor, std::chrono::seconds{5});
        do_cleanTimer();
    }
    auto s = std::make_shared<TcpTestSession>(
            executor,
            socks5Host,
            socks5Port
    );
    sessions.push_back(s->shared_from_this());
    return s;
}


void TcpTest::stop() {
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
