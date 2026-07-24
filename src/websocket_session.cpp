// src/websocket_session.cpp

#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <iostream>
#include <memory>
#include <utility>

#include "websocket_session.hpp"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;

namespace webtty {

WebSocketSession::WebSocketSession(TlsStream&& stream)
   : websocket_{std::move(stream)}
   , pty_{websocket_.get_executor()} {}

void WebSocketSession::run(http::request<http::string_body> request) {
   beast::error_code error;
   websocket_.accept(request, error);

   if (error) {
      std::cerr << "WebSocket accept failed: " << error.message() << '\n';
      return;
   }

   try {
      // The child shell is created only after the WebSocket handshake
      // has completed successfully.
      pty_.start();
   } catch (const std::exception& exception) {
      std::cerr << "PTY start failed: " << exception.what() << '\n';
      stop();
      return;
   }

   // For now, preserve the browser's existing string-based protocol.
   // PTY output is fundamentally bytes; binary frames will be preferable
   // once terminal.js is updated to receive ArrayBuffer data.
   websocket_.text(true);

   const auto self = shared_from_this();

   asio::co_spawn(
      websocket_.get_executor(),
      [self]() -> asio::awaitable<void> { co_await self->websocket_to_pty(); },
      asio::detached);

   asio::co_spawn(
      websocket_.get_executor(),
      [self]() -> asio::awaitable<void> { co_await self->pty_to_websocket(); },
      asio::detached);
}

asio::awaitable<void> WebSocketSession::websocket_to_pty() {
   for (;;) {
      beast::error_code error;

      co_await websocket_.async_read(websocket_buffer_,
                                     asio::redirect_error(asio::use_awaitable,
                                                          error));

      if (error) {
         if (error != websocket::error::closed && !stopped_) {
            std::cerr << "WebSocket read failed: " << error.message() << '\n';
         }

         stop();
         co_return;
      }

      co_await asio::async_write(pty_.stream(), websocket_buffer_.data(),
                                 asio::redirect_error(asio::use_awaitable,
                                                      error));

      websocket_buffer_.consume(websocket_buffer_.size());

      if (error) {
         if (!stopped_) {
            std::cerr << "PTY write failed: " << error.message() << '\n';
         }

         stop();
         co_return;
      }
   }
}

asio::awaitable<void> WebSocketSession::pty_to_websocket() {
   for (;;) {
      beast::error_code error;

      const std::size_t size = co_await pty_.stream().async_read_some(
         asio::buffer(pty_buffer_),
         asio::redirect_error(asio::use_awaitable, error));

      if (error) {
         if (!stopped_) {
            std::cerr << "PTY read failed: " << error.message() << '\n';
         }

         stop();
         co_return;
      }

      co_await websocket_.async_write(asio::buffer(pty_buffer_.data(), size),
                                      asio::redirect_error(asio::use_awaitable,
                                                           error));

      if (error) {
         if (error != websocket::error::closed && !stopped_) {
            std::cerr << "WebSocket write failed: " << error.message() << '\n';
         }

         stop();
         co_return;
      }
   }
}

void WebSocketSession::stop() noexcept {
   if (stopped_) {
      return;
   }

   stopped_ = true;
   pty_.close();

   beast::error_code error;
   auto& socket = beast::get_lowest_layer(websocket_).socket();

   socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);

   socket.close(error);
}

} // namespace webtty

