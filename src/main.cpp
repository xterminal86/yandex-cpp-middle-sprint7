#include "headers.h"

#include <boost/asio.hpp>
// io_service is replaced with io_context since boost 1.87
#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <string_view>
#include <iostream>
#include <print>
#include <ranges>

using boost::asio::io_context;
using boost::asio::co_spawn;
using boost::asio::async_read_until;
using boost::asio::awaitable;
using boost::asio::use_awaitable;
using boost::system::error_code;
using boost::asio::buffer;
using boost::asio::dynamic_buffer;
using boost::asio::transfer_at_least;
using boost::asio::ip::tcp;


constexpr std::string_view delimiter = "\r\n\r\n";

struct HttpRequest
{
  std::string Method;
  std::string Path;
  std::string Version;

  std::unordered_map<std::string, std::string> Headers;

  std::string Body;

  std::string ToString()
  {
    std::stringstream ss;

    ss << std::format("Method:  '{}'\n", Method)
       << std::format("Path:    '{}'\n", Path)
       << std::format("Version: '{}'\n", Version)
       << "Headers:\n\n";

    for (auto& kvp : Headers)
    {
      ss << std::format("'{}' = '{}'\n", kvp.first, kvp.second);
    }

    ss << "\n";
    ss << "Body:\n";
    ss << Body;

    return ss.str();
  }
};

awaitable<void> session(tcp::socket client_socket,
                        io_context& io_service)
{
  try
  {
    HttpRequest req;

    std::string clientIp = client_socket.remote_endpoint().address().to_string();
    std::println("{} connected", clientIp);

    std::string rcv;

    error_code ec;

    // Read the HTTP request headers until we find the double CRLF
    size_t bytes = co_await async_read_until(
      client_socket,
      dynamic_buffer(rcv),
      delimiter,
      redirect_error(use_awaitable, ec)
    );

    // Cannot use const here for some fucking reason.
    std::string headersDelimiter = "\r\n";

    std::vector<std::string> postData;

    std::ranges::split_view<
      std::ranges::ref_view<std::string>,
      std::ranges::ref_view<std::string>
    > splitted = std::views::split(rcv, headersDelimiter);

    for (const auto& line : splitted)
    {
      std::string lineStr(line.begin(), line.end());
      postData.push_back(lineStr);
    }

    for (size_t i = 0; i < postData.size(); i++)
    {
      if (i == 0)
      {
        auto spl = std::views::split(postData[i], ' ');
        auto parts = spl | std::views::transform(
          [](auto&& range)
          {
            return std::string(range.begin(), range.end());
          }
        ) | std::ranges::to<std::vector>();

        if (parts.size() == 3)
        {
          req.Method  = parts[0];
          req.Path    = parts[1];
          req.Version = parts[2];
        }
        else
        {
          std::cerr << "Cannot parse METHOD PATH VERSION";
        }
      }
      else
      {
      }
    }

    if (ec)
    {
      std::cerr << std::format("Error reading from client: '{}'", ec.message());
      co_return;
    }

    size_t dp = rcv.find(delimiter);
    std::println("*** {}", dp);

    // Extract the request line (first line of the request)
    std::string request_line =
      rcv.substr(rcv.find(delimiter) + delimiter.length(), 3);
    std::println("\nRequest: '{}'", request_line);

    client_socket.shutdown(tcp::socket::shutdown_both, ec);
    client_socket.close();

    std::println("{} disconnected", clientIp);
  }
  catch (const std::exception& ex)
  {
    std::cerr << std::format(
      "Exception caught during handling of client request: '{}'", ex.what()
    );
  }

  co_return;
}

class Server
{
public:
  Server(io_context& io_service, short port)
    : io_service_(io_service)
    , acceptor_(io_service, tcp::endpoint(tcp::v4(), port))
    , socket_(io_service)
  {
    std::println("Listening on port {}...", port);
    do_accept();
  }

private:
  void do_accept()
  {
    std::println("Waiting for connections...");
    co_spawn(
      io_service_,
      [this]() -> awaitable<void>
      {
        while (true)
        {
          try
          {
            tcp::socket s = co_await acceptor_.async_accept(use_awaitable);
            co_spawn(
              io_service_,
              session(std::move(s), io_service_), boost::asio::detached);
          }
          catch (const std::exception& ex)
          {
            std::cerr << std::format(
              "Exception caught during accept: '{}'", ex.what()
            );
            break;
          }
        }
      },
      boost::asio::detached
    );
  }

  io_context& io_service_;
  tcp::acceptor acceptor_;
  tcp::socket socket_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << std::format("Usage: {} <PORT>\n", argv[0]);
      return 1;
    }

    uint64_t port = std::stoull(argv[1]);
    if (port > 65535)
    {
      std::cerr << "Port range must be [0 ; 65535]\n";
      return 1;
    }

    io_context io_service(1);
    Server server(io_service, port);
    io_service.run();
  }
  catch (const std::exception& e)
  {
    std::cerr << std::format("Exception caught: '{}'\n", e.what());
  }
}
