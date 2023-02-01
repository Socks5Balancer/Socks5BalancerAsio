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

#include "EmbedWebServer.h"

#include <filesystem>
#include <boost/algorithm/string.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/version.hpp>

// Return a reasonable mime type based on the extension of a file.
boost::beast::string_view
mime_type(boost::beast::string_view path) {
    using boost::beast::iequals;
    auto const ext = [&path] {
        auto const pos = path.rfind(".");
        if (pos == boost::beast::string_view::npos)
            return boost::beast::string_view{};
        return path.substr(pos);
    }();
    if (iequals(ext, ".htm")) return "text/html";
    if (iequals(ext, ".html")) return "text/html";
    if (iequals(ext, ".php")) return "text/html";
    if (iequals(ext, ".css")) return "text/css";
    if (iequals(ext, ".txt")) return "text/plain";
    if (iequals(ext, ".js")) return "application/javascript";
    if (iequals(ext, ".json")) return "application/json";
    if (iequals(ext, ".xml")) return "application/xml";
    if (iequals(ext, ".swf")) return "application/x-shockwave-flash";
    if (iequals(ext, ".flv")) return "video/x-flv";
    if (iequals(ext, ".png")) return "image/png";
    if (iequals(ext, ".jpe")) return "image/jpeg";
    if (iequals(ext, ".jpeg")) return "image/jpeg";
    if (iequals(ext, ".jpg")) return "image/jpeg";
    if (iequals(ext, ".gif")) return "image/gif";
    if (iequals(ext, ".bmp")) return "image/bmp";
    if (iequals(ext, ".ico")) return "image/vnd.microsoft.icon";
    if (iequals(ext, ".tiff")) return "image/tiff";
    if (iequals(ext, ".tif")) return "image/tiff";
    if (iequals(ext, ".svg")) return "image/svg+xml";
    if (iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string
path_cat(
        boost::beast::string_view base,
        boost::beast::string_view path) {
    if (base.empty())
        return std::string(path);
    std::string result(base);
#ifdef BOOST_MSVC
    char constexpr path_separator = '\\';
    if (result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
    for (auto &c : result)
        if (c == '/')
            c = path_separator;
#else
    char constexpr path_separator = '/';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
#endif
    return result;
}

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template<
        class Body,
        class Allocator=std::allocator<char>,
        class Send
>
void
handle_request(
        boost::beast::string_view doc_root,
        boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>> &&req,
        Send &&send,
        std::shared_ptr<std::string const> index_file_of_root,
        const std::vector<std::string> allowFileExtList) {
    // Returns a bad request response
    auto const bad_request =
            [&req](boost::beast::string_view why) {
                boost::beast::http::response<boost::beast::http::string_body> res{
                        boost::beast::http::status::bad_request,
                        req.version()};
                res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
                res.set(boost::beast::http::field::content_type, "text/html");
                res.keep_alive(req.keep_alive());
                res.body() = std::string(why);
                res.prepare_payload();
                return res;
            };

    // Returns a not found response
    auto const not_found =
            [&req](boost::beast::string_view target) {
                boost::beast::http::response<boost::beast::http::string_body> res{
                        boost::beast::http::status::not_found,
                        req.version()};
                res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
                res.set(boost::beast::http::field::content_type, "text/html");
                res.keep_alive(req.keep_alive());
                res.body() = "The resource '" + std::string(target) + "' was not found.";
                res.prepare_payload();
                return res;
            };

    // Returns a server error response
    auto const server_error =
            [&req](boost::beast::string_view what) {
                boost::beast::http::response<boost::beast::http::string_body> res{
                        boost::beast::http::status::internal_server_error,
                        req.version()};
                res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
                res.set(boost::beast::http::field::content_type, "text/html");
                res.keep_alive(req.keep_alive());
                res.body() = "An error occurred: '" + std::string(what) + "'";
                res.prepare_payload();
                return res;
            };

    // Make sure we can handle the method
    if (req.method() != boost::beast::http::verb::get &&
        req.method() != boost::beast::http::verb::head)
        return send(bad_request("Unknown HTTP-method"));

    // Request path must be absolute and not contain "..".
    if (req.target().empty() ||
        req.target()[0] != '/' ||
        req.target().find("..") != boost::beast::string_view::npos)
        return send(bad_request("Illegal request-target"));

    // ------------------------ path check --------------------

    std::string reqString = std::string{req.target()};
    if (reqString.find("?") != std::string::npos) {
        reqString = reqString.substr(0, reqString.find("?"));
    }
    // Build the path to the requested file
    std::string path = path_cat(doc_root, reqString);
    if (reqString.back() == '/')
        path.append(*index_file_of_root);

    // ------------------------ path check --------------------
    try {
        std::filesystem::path root_path{std::string{doc_root}};
        std::error_code errorCode;
        root_path = std::filesystem::canonical(root_path, errorCode);
        if (errorCode) {
            return send(bad_request("Illegal request-target 2.1"));
        }

        std::filesystem::path file_path{path};
        errorCode.clear();
        file_path = std::filesystem::canonical(file_path, errorCode);
        if (errorCode) {
            return send(bad_request("Illegal request-target 2.2"));
        }

        // from https://stackoverflow.com/a/51436012/3548568
        std::string relCheckString = std::filesystem::relative(file_path, root_path).generic_string();
        if (relCheckString.find("..") != std::string::npos) {
            return send(bad_request("Illegal request-target 2"));
        }

        errorCode.clear();
        if (std::filesystem::is_symlink(std::filesystem::symlink_status(file_path, errorCode))) {
            return send(bad_request("Illegal request-target 3"));
        }
        if (errorCode) {
            return send(bad_request("Illegal request-target 2.3"));
        }

        auto ext = file_path.extension().string();
        if (ext.front() == '.') {
            ext.erase(ext.begin());
        }
        bool isAllow = false;
        for (const auto &a:allowFileExtList) {
            if (ext == a) {
                isAllow = true;
                break;
            }
        }
        if (!isAllow) {
            return send(bad_request("Illegal request-target 2.4"));
        }
    } catch (const std::exception &e) {
        boost::ignore_unused(e);
        return send(bad_request("Illegal request-target 4"));
    }
    // ------------------------ path check --------------------

    // Attempt to open the file
    boost::beast::error_code ec;
    boost::beast::http::file_body::value_type body;
    body.open(path.c_str(), boost::beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    if (ec == boost::beast::errc::no_such_file_or_directory)
        return send(not_found(req.target()));

    // Handle an unknown error
    if (ec)
        return send(server_error(ec.message()));

    // Cache the size since we need it after the move
    auto const size = body.size();

    // Respond to HEAD request
    if (req.method() == boost::beast::http::verb::head) {
        boost::beast::http::response<boost::beast::http::empty_body> res{
                boost::beast::http::status::ok,
                req.version()};
        res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(boost::beast::http::field::content_type, mime_type(path));
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
    }

    // Respond to GET request
    boost::beast::http::response<boost::beast::http::file_body> res{
            std::piecewise_construct,
            std::make_tuple(std::move(body)),
            std::make_tuple(boost::beast::http::status::ok, req.version())};
    res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(boost::beast::http::field::content_type, mime_type(path));
    res.content_length(size);
    res.keep_alive(req.keep_alive());
    return send(std::move(res));
}


//------------------------------------------------------------------------------


// Report a failure
void
fail(boost::beast::error_code ec, char const *what) {
    std::cerr << what << ": " << ec.message() << "\n";
}

//------------------------------------------------------------------------------

template<bool isRequest, class Body, class Fields>
void EmbedWebServerSession::send_lambda::operator()(boost::beast::http::message<isRequest, Body, Fields> &&msg) const {
    // The lifetime of the message has to extend
    // for the duration of the async operation so
    // we use a shared_ptr to manage it.
    auto sp = std::make_shared<
            boost::beast::http::message<isRequest, Body, Fields>>(std::move(msg));

    // Store a type-erased version of the shared
    // pointer in the class to keep it alive.
    self_.res_ = sp;

    // Write the response
    boost::beast::http::async_write(
            self_.stream_,
            *sp,
            boost::beast::bind_front_handler(
                    &EmbedWebServerSession::on_write,
                    self_.shared_from_this(),
                    sp->need_eof()));
}

//------------------------------------------------------------------------------

void EmbedWebServerSession::run() {
    // We need to be executing within a strand to perform async operations
    // on the I/O objects in this session. Although not strictly necessary
    // for single-threaded contexts, this example code is written to be
    // thread-safe by default.
    boost::asio::dispatch(stream_.get_executor(),
                          boost::beast::bind_front_handler(
                                  &EmbedWebServerSession::do_read,
                                  shared_from_this()));
}

void EmbedWebServerSession::do_read() {
    // Make the request empty before reading,
    // otherwise the operation behavior is undefined.
    req_ = {};

    // Set the timeout.
    stream_.expires_after(std::chrono::seconds(30));

    // Read a request
    boost::beast::http::async_read(stream_, buffer_, req_,
                                   boost::beast::bind_front_handler(
                                           &EmbedWebServerSession::on_read,
                                           shared_from_this()));
}

void EmbedWebServerSession::on_read(boost::beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    // This means they closed the connection
    if (ec == boost::beast::http::error::end_of_stream)
        return do_close();

    if (ec)
        return fail(ec, "read");


    std::cout << "req_.target():" << req_.target() << std::endl;
    if (req_.method() == boost::beast::http::verb::get) {
        // answer backend json
        if (boost::beast::string_view{req_.target()} == std::string{"/backend"}) {
            boost::beast::http::response<boost::beast::http::string_body> res{
                    boost::beast::http::status::ok,
                    req_.version()};
            res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(boost::beast::http::field::content_type, "text/json");
            res.keep_alive(req_.keep_alive());
            res.body() = std::string(*backend_json_string);
            res.prepare_payload();
            return lambda_(std::move(res));
        }
    }

    // Send the response
    handle_request(*doc_root_, std::move(req_), lambda_, index_file_of_root, allowFileExtList);
}

void EmbedWebServerSession::on_write(bool close, boost::beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec)
        return fail(ec, "write");

    if (close) {
        // This means we should close the connection, usually because
        // the response indicated the "Connection: close" semantic.
        return do_close();
    }

    // We're done with the response so delete it
    res_ = nullptr;

    // Read another request
    do_read();
}

void EmbedWebServerSession::do_close() {
    // Send a TCP shutdown
    boost::beast::error_code ec;
    stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
}

EmbedWebServer::EmbedWebServer(boost::asio::io_context &ioc, boost::asio::ip::tcp::endpoint endpoint,
                               const std::shared_ptr<const std::string> &doc_root,
                               std::shared_ptr<std::string const> const &index_file_of_root,
                               std::shared_ptr<std::string const> const &backend_json_string,
                               std::shared_ptr<std::string const> const &_allowFileExtList)
        : ioc_(ioc),
          acceptor_(boost::asio::make_strand(ioc)),
          doc_root_(doc_root),
          index_file_of_root(index_file_of_root),
          backend_json_string(backend_json_string) {

    if (_allowFileExtList) {
        std::vector<std::string> extList;
        boost::split(extList, *_allowFileExtList, boost::is_any_of(" "));
        allowFileExtList = extList;
    }

    boost::beast::error_code ec;

    // Open the acceptor
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
        fail(ec, "open");
        return;
    }

    // Allow address reuse
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec) {
        fail(ec, "set_option");
        return;
    }

    // Bind to the server address
    acceptor_.bind(endpoint, ec);
    if (ec) {
        fail(ec, "bind");
        return;
    }

    // Start listening for connections
    acceptor_.listen(
            boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
        fail(ec, "listen");
        return;
    }
}

void EmbedWebServer::do_accept() {
    // The new connection gets its own strand
    acceptor_.async_accept(
            boost::asio::make_strand(ioc_),
            boost::beast::bind_front_handler(
                    &EmbedWebServer::on_accept,
                    shared_from_this()));
}

void EmbedWebServer::on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket) {
    if (ec) {
        fail(ec, "accept");
    } else {
        // Create the session and run it
        std::make_shared<EmbedWebServerSession>(
                std::move(socket),
                doc_root_,
                index_file_of_root,
                backend_json_string,
                allowFileExtList
        )->run();
    }

    // Accept another connection
    do_accept();
}
