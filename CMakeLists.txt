cmake_minimum_required(VERSION 3.20)

project(
   WebTTY
   VERSION 0.1.0
   DESCRIPTION "A small browser-based terminal emulator"
   LANGUAGES CXX
)

option(WEBTTY_BUILD_TESTS "Build WebTTY tests" ON)

find_package(Boost 1.90 CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_executable(
   webtty
   src/main.cpp
   src/https_server.cpp
   src/websocket_session.cpp
   src/pty_session.cpp
)

target_compile_features(webtty PRIVATE cxx_std_20)

target_include_directories(
   webtty
   PRIVATE
      src
)

target_link_libraries(
   webtty
   PRIVATE
      Boost::headers
      OpenSSL::SSL
      OpenSSL::Crypto
      Threads::Threads
)

# On Linux, forkpty() is provided by libutil.
# macOS provides it through the system libraries.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
   find_library(UTIL_LIBRARY util REQUIRED)

   target_link_libraries(
      webtty
      PRIVATE
         "${UTIL_LIBRARY}"
   )
endif()

target_compile_definitions(
   webtty
   PRIVATE
      WEBTTY_DEFAULT_WWW_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/www"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
   target_compile_options(
      webtty
      PRIVATE
         -Wall
         -Wextra
         -Wpedantic
         -Wconversion
         -Wsign-conversion
   )
endif()

if(WEBTTY_BUILD_TESTS)
   enable_testing()
   add_subdirectory(tests)
endif()

