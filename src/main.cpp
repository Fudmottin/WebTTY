// src/main.cpp

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "https_server.hpp"

#ifndef WEBTTY_DEFAULT_WWW_ROOT
#error WEBTTY_DEFAULT_WWW_ROOT must be defined by the build system
#endif

namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

namespace {

struct Configuration {
   std::filesystem::path certificate{"certs/server-cert.pem"};
   std::filesystem::path private_key{"certs/server-key.pem"};
   std::filesystem::path document_root{WEBTTY_DEFAULT_WWW_ROOT};
};

[[nodiscard]]
Configuration parse_arguments(int argc, char* argv[]) {
   Configuration configuration;

   if (argc > 1) {
      configuration.certificate = argv[1];
   }

   if (argc > 2) {
      configuration.private_key = argv[2];
   }

   if (argc > 3) {
      configuration.document_root = argv[3];
   }

   if (argc > 4) {
      throw std::runtime_error{
         "usage: webtty [certificate.pem] [private-key.pem] [www-root]"};
   }

   return configuration;
}

void run_server(webtty::HttpsServer& server, asio::io_context& io_context) {
   auto work_guard = asio::make_work_guard(io_context);

   // Unlike the old detached jthread, this jthread has meaningful ownership:
   // it is joined automatically after the io_context is stopped.
   std::jthread io_thread{[&io_context] { io_context.run(); }};

   try {
      server.run();
   } catch (...) {
      work_guard.reset();
      io_context.stop();
      throw;
   }

   work_guard.reset();
   io_context.stop();
}

} // namespace

int main(int argc, char* argv[]) try {
   const auto configuration = parse_arguments(argc, argv);

   asio::io_context io_context{1};
   ssl::context tls_context{ssl::context::tls_server};

   tls_context.set_options(ssl::context::default_workarounds |
                           ssl::context::no_sslv2 | ssl::context::no_sslv3 |
                           ssl::context::single_dh_use);

   tls_context.use_certificate_chain_file(configuration.certificate.string());

   tls_context.use_private_key_file(configuration.private_key.string(),
                                    ssl::context::pem);

   const tcp::endpoint endpoint{asio::ip::make_address("127.0.0.1"), 8443};

   webtty::HttpsServer server{io_context, tls_context, endpoint,
                              configuration.document_root};

   run_server(server, io_context);
} catch (const std::exception& exception) {
   std::cerr << "webtty: " << exception.what() << '\n';
   return 1;
}

