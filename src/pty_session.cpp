// src/pty_session.cpp

#include "pty_session.hpp"

#if defined(__APPLE__)
#include <util.h>
#elif defined(__linux__)
#include <pty.h>
#else
#error PtySession currently supports only macOS and Linux
#endif

#include <cerrno>
#include <csignal>
#include <cstring>
#include <pwd.h>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace webtty {
namespace {

[[nodiscard]]
std::string login_shell() {
   const passwd* const account = ::getpwuid(::getuid());

   if (account == nullptr || account->pw_shell == nullptr ||
       account->pw_shell[0] == '\0') {
      return "/bin/sh";
   }

   return account->pw_shell;
}

[[nodiscard]]
std::string login_argv_zero(const std::string& shell) {
   const auto separator = shell.find_last_of('/');
   const auto name =
      separator == std::string::npos ? shell : shell.substr(separator + 1);

   return '-' + name;
}

void wait_for_child(pid_t child_pid) noexcept {
   for (;;) {
      const pid_t result = ::waitpid(child_pid, nullptr, 0);

      if (result == child_pid) {
         return;
      }

      if (result == -1 && errno == EINTR) {
         continue;
      }

      return;
   }
}

void terminate_child(pid_t child_pid) noexcept {
   if (child_pid <= 0) {
      return;
   }

   // The closed PTY normally causes the shell to exit. Ensure that cleanup
   // remains deterministic if it does not.
   if (::kill(child_pid, SIGHUP) == -1 && errno == ESRCH) {
      wait_for_child(child_pid);
      return;
   }

   const pid_t result = ::waitpid(child_pid, nullptr, WNOHANG);

   if (result == 0) {
      static_cast<void>(::kill(child_pid, SIGKILL));
      wait_for_child(child_pid);
   }
}

} // namespace

PtySession::PtySession(boost::asio::any_io_executor executor)
   : master_{std::move(executor)} {}

PtySession::~PtySession() { close(); }

void PtySession::start() {
   if (is_open() || child_pid_ > 0) {
      throw std::logic_error{"PTY session is already running"};
   }

   // Resolve and allocate these strings before fork(). The child should do
   // virtually nothing except exec() after the process becomes multithreaded.
   const std::string shell = login_shell();
   const std::string argv_zero = login_argv_zero(shell);

   int master_fd{-1};
   const pid_t child_pid = ::forkpty(&master_fd, nullptr, nullptr, nullptr);

   if (child_pid == -1) {
      throw std::system_error{errno, std::generic_category(), "forkpty"};
   }

   if (child_pid == 0) {
      ::execl(shell.c_str(), argv_zero.c_str(), static_cast<char*>(nullptr));

      constexpr char message[] = "webtty: unable to execute login shell\n";
      static_cast<void>(::write(STDERR_FILENO, message, sizeof(message) - 1));

      ::_exit(127);
   }

   child_pid_ = child_pid;

   try {
      master_.assign(master_fd);
   } catch (...) {
      static_cast<void>(::close(master_fd));
      terminate_child(child_pid_);
      child_pid_ = -1;
      throw;
   }
}

void PtySession::close() noexcept {
   boost::system::error_code error;

   if (master_.is_open()) {
      master_.cancel(error);
      master_.close(error);
   }

   if (child_pid_ > 0) {
      terminate_child(child_pid_);
      child_pid_ = -1;
   }
}

bool PtySession::is_open() const noexcept { return master_.is_open(); }

boost::asio::posix::stream_descriptor& PtySession::stream() noexcept {
   return master_;
}

} // namespace webtty

