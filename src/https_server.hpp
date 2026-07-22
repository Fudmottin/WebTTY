// src/https_server.hpp

#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

#include <filesystem>

namespace webtty {

class HttpsServer {
 public:
   HttpsServer(boost::asio::io_context& io_context,
               boost::asio::ssl::context& tls_context,
               boost::asio::ip::tcp::endpoint endpoint,
               std::filesystem::path document_root);

   void run();

 private:
   boost::asio::io_context& io_context_;
   boost::asio::ssl::context& tls_context_;
   boost::asio::ip::tcp::endpoint endpoint_;
   std::filesystem::path document_root_;
};

} // namespace webtty

