// src/https_server.cpp

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "https_server.hpp"
#include "websocket_session.hpp"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

namespace webtty {
namespace {

using TlsStream = WebSocketSession::TlsStream;

[[nodiscard]]
std::string_view mime_type(const std::filesystem::path& path) {
   const auto extension = path.extension().string();

   if (extension == ".html") {
      return "text/html; charset=utf-8";
   }

   if (extension == ".css") {
      return "text/css; charset=utf-8";
   }

   if (extension == ".js") {
      return "text/javascript; charset=utf-8";
   }

   return "application/octet-stream";
}

void write_bad_request(TlsStream& stream,
                       const http::request<http::string_body>& request,
                       std::string message) {
   http::response<http::string_body> response{http::status::bad_request,
                                              request.version()};

   response.set(http::field::server, "WebTTY");
   response.set(http::field::content_type, "text/plain; charset=utf-8");
   response.keep_alive(false);
   response.body() = std::move(message);
   response.prepare_payload();

   beast::error_code error;
   http::write(stream, response, error);
}

void write_not_found(TlsStream& stream,
                     const http::request<http::string_body>& request) {
   http::response<http::string_body> response{http::status::not_found,
                                              request.version()};

   response.set(http::field::server, "WebTTY");
   response.set(http::field::content_type, "text/plain; charset=utf-8");
   response.keep_alive(false);
   response.body() = "Resource not found.\n";
   response.prepare_payload();

   beast::error_code error;
   http::write(stream, response, error);
}

void serve_file(TlsStream& stream,
                const http::request<http::string_body>& request,
                const std::filesystem::path& document_root) {
   if (request.method() != http::verb::get &&
       request.method() != http::verb::head) {
      write_bad_request(stream, request, "Only GET and HEAD are supported.\n");
      return;
   }

   const std::string target{request.target()};

   if (target.empty() || target.front() != '/' ||
       target.find("..") != std::string::npos) {
      write_bad_request(stream, request, "Invalid request target.\n");
      return;
   }

   const auto relative_path = target == "/"
                                 ? std::filesystem::path{"index.html"}
                                 : std::filesystem::path{target.substr(1)};

   const auto path = document_root / relative_path;

   beast::error_code error;
   http::file_body::value_type body;
   body.open(path.c_str(), beast::file_mode::scan, error);

   if (error == beast::errc::no_such_file_or_directory) {
      write_not_found(stream, request);
      return;
   }

   if (error) {
      std::cerr << "Unable to open " << path << ": " << error.message() << '\n';
      write_not_found(stream, request);
      return;
   }

   const auto size = body.size();

   if (request.method() == http::verb::head) {
      http::response<http::empty_body> response{http::status::ok,
                                                request.version()};

      response.set(http::field::server, "WebTTY");
      response.set(http::field::content_type, mime_type(path));
      response.content_length(size);
      response.keep_alive(false);

      http::write(stream, response, error);
      return;
   }

   http::response<http::file_body> response{std::piecewise_construct,
                                            std::make_tuple(std::move(body)),
                                            std::make_tuple(http::status::ok,
                                                            request.version())};

   response.set(http::field::server, "WebTTY");
   response.set(http::field::content_type, mime_type(path));
   response.content_length(size);
   response.keep_alive(false);

   http::write(stream, response, error);
}

void handle_connection(tcp::socket socket, ssl::context& tls_context,
                       const std::filesystem::path& document_root) {
   beast::error_code error;
   TlsStream stream{std::move(socket), tls_context};

   stream.handshake(ssl::stream_base::server, error);

   if (error) {
      std::cerr << "TLS handshake failed: " << error.message() << '\n';
      return;
   }

   beast::flat_buffer buffer;
   http::request<http::string_body> request;

   http::read(stream, buffer, request, error);

   if (error) {
      std::cerr << "HTTP read failed: " << error.message() << '\n';
      return;
   }

   if (websocket::is_upgrade(request)) {
      if (request.target() != "/terminal") {
         write_not_found(stream, request);
         return;
      }

      WebSocketSession session{std::move(stream)};
      session.run(std::move(request));
      return;
   }

   serve_file(stream, request, document_root);

   stream.shutdown(error);

   // Browsers commonly close first after receiving a response.
   if (error == asio::error::eof || error == ssl::error::stream_truncated) {
      error = {};
   }

   if (error) {
      std::cerr << "TLS shutdown failed: " << error.message() << '\n';
   }
}

} // namespace

HttpsServer::HttpsServer(asio::io_context& io_context,
                         ssl::context& tls_context, tcp::endpoint endpoint,
                         std::filesystem::path document_root)
   : io_context_{io_context}
   , tls_context_{tls_context}
   , endpoint_{std::move(endpoint)}
   , document_root_{std::move(document_root)} {}

void HttpsServer::run() {
   tcp::acceptor acceptor{io_context_};

   acceptor.open(endpoint_.protocol());
   acceptor.set_option(asio::socket_base::reuse_address{true});
   acceptor.bind(endpoint_);
   acceptor.listen(asio::socket_base::max_listen_connections);

   std::cout << "Serving " << document_root_ << '\n'
             << "Listening on https://" << endpoint_ << '\n';

   for (;;) {
      tcp::socket socket{io_context_};
      acceptor.accept(socket);

      std::jthread connection{[this, socket = std::move(socket)]() mutable {
         handle_connection(std::move(socket), tls_context_, document_root_);
      }};

      connection.detach();
   }
}

} // namespace webtty

