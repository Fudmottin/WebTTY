// src/websocket_session.hpp

#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

#include <array>
#include <cstddef>
#include <memory>

#include "pty_session.hpp"

namespace webtty {

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
 public:
   using TlsStream = boost::beast::ssl_stream<boost::beast::tcp_stream>;
   using WebSocket = boost::beast::websocket::stream<TlsStream>;

   explicit WebSocketSession(TlsStream&& stream);

   void
   run(boost::beast::http::request<boost::beast::http::string_body> request);

 private:
   boost::asio::awaitable<void> websocket_to_pty();
   boost::asio::awaitable<void> pty_to_websocket();

   void stop() noexcept;

   WebSocket websocket_;
   PtySession pty_;

   boost::beast::flat_buffer websocket_buffer_;
   std::array<char, 4096> pty_buffer_{};

   bool stopped_{false};
};

} // namespace webtty

