// src/pty_session.hpp

#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

#include <sys/types.h>

namespace webtty {

class PtySession {
 public:
   explicit PtySession(boost::asio::any_io_executor executor);
   ~PtySession();

   PtySession(const PtySession&) = delete;
   PtySession& operator=(const PtySession&) = delete;
   PtySession(PtySession&&) = delete;
   PtySession& operator=(PtySession&&) = delete;

   void start();
   void close() noexcept;

   [[nodiscard]]
   bool is_open() const noexcept;

   [[nodiscard]]
   boost::asio::posix::stream_descriptor& stream() noexcept;

 private:
   boost::asio::posix::stream_descriptor master_;
   pid_t child_pid_{-1};
};

} // namespace webtty

