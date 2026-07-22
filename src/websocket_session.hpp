// src/websocket_session.hpp

#pragma once

#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/ssl.hpp>

namespace webtty {

class WebSocketSession {
 public:
   using TlsStream = boost::beast::ssl_stream<boost::beast::tcp_stream>;

   explicit WebSocketSession(TlsStream&& stream);

   void
   run(boost::beast::http::request<boost::beast::http::string_body> request);

 private:
   TlsStream stream_;
};

} // namespace webtty

