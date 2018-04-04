#include <algorithm>
#include <iostream>
#include <iterator>
#include <string_view>
#include <string>
#include <sstream>

#include <portable_concurrency/future>

#include <asio/io_context.hpp>
#include <asio/read_until.hpp>
#include <asio/streambuf.hpp>
#include <asio/write.hpp>

#include <asio/ip/address.hpp>
#include <asio/ip/basic_resolver.hpp>
#include <asio/ip/tcp.hpp>

#include "adapters.h"

using namespace std::literals;

int main() try {
  asio::io_context ctx;
  asio::ip::tcp::socket socket{ctx};
  constexpr std::string_view httpbin_host = "httpbin.org"sv;
  constexpr std::string_view request = "HEAD / HTTP/1.1\r\nHost: httpbin.org\r\n\r\n"sv;
  asio::streambuf response;

  asio::ip::tcp::resolver resolver{ctx};
  pc::future<size_t> response_sz = resolver.async_resolve(httpbin_host, "80", use_future)
    .next([&socket, httpbin_host](asio::ip::tcp::resolver::results_type resolved_results) {
      if (resolved_results.empty())
        throw std::runtime_error{"No addresses resolved for host: " + std::string{httpbin_host}};
      return socket.async_connect(resolved_results.begin()->endpoint(), use_future);
    }).next([&socket, request] {
      return asio::async_write(socket, asio::buffer(request), use_future);
    }).next([&socket, request, &response](size_t written) {
      if (written != request.size())
        throw std::runtime_error{"failed to send request"};
      return asio::async_read_until(socket, response, "\r\n\r\n"sv, use_future);
    });
  ctx.run();

  std::cout << "Response:\n";
  std::copy_n(std::istreambuf_iterator<char>{&response}, response_sz.get(), std::ostream_iterator<char>{std::cout});
  return EXIT_SUCCESS;
} catch (const std::exception& error) {
  std::cerr << "Error: " << error.what() << '\n';
  return  EXIT_FAILURE;
}
