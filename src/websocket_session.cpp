// src/websocket_session.cpp

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket.hpp>

#include <iostream>
#include <utility>

#include "websocket_session.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;

namespace webtty {

WebSocketSession::WebSocketSession(TlsStream&& stream)
   : stream_{std::move(stream)} {}

void WebSocketSession::run(http::request<http::string_body> request) {
   websocket::stream<TlsStream> websocket{std::move(stream_)};

   beast::error_code error;
   websocket.accept(request, error);

   if (error) {
      std::cerr << "WebSocket accept failed: " << error.message() << '\n';
      return;
   }

   beast::flat_buffer buffer;

   for (;;) {
      websocket.read(buffer, error);

      if (error == websocket::error::closed) {
         return;
      }

      if (error) {
         std::cerr << "WebSocket read failed: " << error.message() << '\n';
         return;
      }

      websocket.text(websocket.got_text());
      websocket.write(buffer.data(), error);

      if (error) {
         std::cerr << "WebSocket write failed: " << error.message() << '\n';
         return;
      }

      buffer.consume(buffer.size());
   }
}

} // namespace webtty

