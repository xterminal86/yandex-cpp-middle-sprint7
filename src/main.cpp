#include "headers.h"

#include <boost/asio.hpp>
// io_service is replaced with io_context since boost 1.87
#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

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

// =============================================================================

struct HttpRequest
{
  std::string Method;
  std::string Path;
  std::string Version;

  std::unordered_map<std::string, std::string> Headers;

  std::string Body;

  std::string ReadHeader(const std::string& key) const
  {
    std::string copy = key;

    std::transform(
      copy.begin(),
      copy.end(),
      copy.begin(),
      [](unsigned char c)
      {
        return std::tolower(c);
      }
    );

    std::string value;

    if (Headers.contains(copy))
    {
      value = Headers.at(copy);
    }

    return value;
  }

  std::string ToString()
  {
    std::stringstream ss;

    ss << std::format("Method:  '{}'\n", Method)
       << std::format("Path:    '{}'\n", Path)
       << std::format("Version: '{}'\n\n", Version)
       << "Headers:\n\n";

    size_t n = 1;
    for (auto& kvp : Headers)
    {
      ss << std::format("{}. '{}' = '{}'\n", n++, kvp.first, kvp.second);
    }

    ss << "\n";
    ss << "Body:\n";
    ss << Body;

    return ss.str();
  }
};

// =============================================================================

std::vector<std::string> StringSplit(const std::string& str, char delimiter)
{
  auto spl = std::views::split(str, delimiter);
  auto parts = spl | std::views::transform(
    [](auto&& range)
    {
      return std::string(range.begin(), range.end());
    }
  ) | std::ranges::to<std::vector>();

  return parts;
}

// =============================================================================

std::optional<HttpRequest> ParseRequest(std::string& rcv)
{
  HttpRequest req;

  bool requestOk = true;

  // Cannot use const here for some fucking reason.
  std::string reqiestDataDelimiter = "\r\n";

  std::vector<std::string> postData;

  std::ranges::split_view<
    std::ranges::ref_view<std::string>,
    std::ranges::ref_view<std::string>
  > splitted = std::views::split(rcv, reqiestDataDelimiter);

  uint8_t emptyLinesCount = 0;

  for (const auto& line : splitted)
  {
    std::string lineStr(line.begin(), line.end());
    postData.push_back(lineStr);
  }

  for (size_t i = 0; i < postData.size(); i++)
  {
    const std::string& line = postData[i];

    // 'GET / HTTP/1.1' or whatever.
    if (i == 0)
    {
      std::vector<std::string> parts = StringSplit(line, ' ');
      if (parts.size() == 3)
      {
        req.Method  = parts[0];
        req.Path    = parts[1];
        req.Version = parts[2];
      }
      else
      {
        std::cerr << "Cannot parse '<METHOD> <PATH> <VERSION>'!\n";
        requestOk = false;
        break;
      }
    }
    else
    {
      if (line.empty())
      {
        emptyLinesCount++;
      }
      else
      {
        //
        // Blank line before body.
        //
        if (emptyLinesCount >= 1)
        {
          req.Body = line;
        }
        else
        {
          //
          // According to RFC 7230:
          // "Each header field consists of a case-insensitive field name
          // followed by a colon (":"), optional leading whitespace, the
          // field value, and optional trailing whitespace."
          //
          size_t pos = line.find(":");
          if (not line.empty() and pos != std::string::npos)
          {
            std::string key = line.substr(0, pos);

            std::string value = line.substr(pos + 1);
            if (not value.empty())
            {
              //
              // Типа метод двух указателей, привет Алгосам, лол.
              //
              size_t begin = 0;
              size_t end = value.length() - 1;
              while (true)
              {
                if (value[begin] != ' ' and value[end] != ' ')
                {
                  break;
                }

                if (value[begin] == ' ') begin++;
                if (value[end]   == ' ') end--;
              }

              value = std::string(value.begin() + begin,
                                  value.begin() + end + 1);
            }

            // HTTP header keys are case-insensitive.

            std::transform(
              key.begin(),
              key.end(),
              key.begin(),
              [](unsigned char c)
              {
                return std::tolower(c);
              }
            );

            req.Headers[key] = value;
          }
          else
          {
            std::cerr << std::format("Invalid header format: '{}'!\n", line);
            requestOk = false;
            break;
          }
        }
      }
    }
  }

  if (requestOk)
  {
    std::println("---------------");
    std::println("Parsed request:");
    std::println("---------------");
    std::println("{}", req.ToString());
    std::println("---------------");
  }
  else
  {
    return std::nullopt;
  }

  return req;
}

// =============================================================================

awaitable<void> DoRequest(const HttpRequest& originalRequest,
                          const std::string& host,
                          const uint64_t port)
{
  using namespace boost::asio;

  auto executor = co_await this_coro::executor;

  tcp::resolver resolver(executor);
  auto endpoints = co_await resolver.async_resolve(host, std::to_string(port));

  error_code ec;

  tcp::socket socket(executor);

  // Set connection timeout.
  steady_timer timer(executor, std::chrono::seconds(30));

  co_await socket.async_connect(
    *endpoints.begin(),
    redirect_error(use_awaitable, ec)
  );

  timer.cancel();

  if (ec)
  {
    std::cerr << std::format("Connection error: '{}'!\n", ec.message());
    co_return;
  }

  // Send request
  std::string request = "GET / HTTP/1.1\r\n";
  request += std::format("Host: {}\r\n", host);
  request += std::format("User-Agent: {}\r\n",
                         originalRequest.ReadHeader("User-Agent"));
  request += "Connection: close\r\n";
  request += "\r\n";

  co_await async_write(socket, buffer(request));

  // Read response using a dynamic buffer.
  std::string responseRcv;

  // Read until the server closes the connection
  while (not ec)
  {
    std::array<char, 4096> chunk;
    size_t bytes_read = co_await socket.async_read_some(
      buffer(chunk),
      redirect_error(use_awaitable, ec)
    );

    if (not ec && bytes_read > 0)
    {
      responseRcv += std::string(chunk.data(), bytes_read);
    }
  }

  // The connection was closed, get the full response
  std::println("{}", responseRcv);

  socket.close();

  /*
  using namespace boost::asio;
  using namespace boost::beast;

  namespace beast = boost::beast;

  auto executor = co_await this_coro::executor;
  auto resolver = use_awaitable.as_default_on(tcp::resolver(executor));
  auto stream   = use_awaitable.as_default_on(beast::tcp_stream(executor));

  auto const results = co_await resolver.async_resolve(host,
                                                       std::to_string(port));

  // Set connection timeout.
  stream.expires_after(std::chrono::seconds(10));
  co_await stream.async_connect(results);

  // 10 - HTTP/1.0, 11 - HTTP/1.1
  http::request<http::string_body> req(http::verb::get, "/", 11);
  req.set(http::field::host, host);
  req.set(http::field::user_agent, originalRequest.ReadHeader("User-Agent"));

  // Set execution timeout.
  stream.expires_after(std::chrono::seconds(30));
  co_await http::async_write(stream, req);

  // Receive the response
  beast::flat_buffer buffer;
  http::response<http::dynamic_body> res;
  co_await http::async_read(stream, buffer, res);

  std::ostringstream oss;

  oss << res;

  std::string response = oss.str();

  // Print the response.
  std::println("{}", response);

  // Gracefully close the connection
  beast::error_code ec;
  stream.socket().shutdown(tcp::socket::shutdown_both, ec);
  if (ec && ec != beast::errc::not_connected)
  {
    std::cerr << "Failed to shutdown proxy connection!\n";
  }
  */
}

// =============================================================================

void ProcessRequest(const HttpRequest& req, io_context& io_service)
{
  std::vector<std::string> parts = StringSplit(req.Body, ' ');
  if (parts.size() != 2)
  {
    std::cerr << "Request body should be a string '<HOST> <PORT>'!\n";
    return;
  }

  const std::string& host = parts[0];

  uint64_t port = 0;

  try
  {
    port = std::stoull(parts[1]);
  }
  catch (std::exception& ex)
  {
    std::cerr << std::format(
      "Exception caught during request processing: '{}'\n", ex.what()
    );
    return;
  }

  if (port > 65535)
  {
    std::cerr << "Port must be [0; 65535]!\n";
    return;
  }

  co_spawn(
    io_service,
    DoRequest(req, host, port),
    [](std::exception_ptr e)
    {
      if (e)
      {
        try
        {
          std::rethrow_exception(e);
        }
        catch (std::exception& ex)
        {
          std::cerr << std::format(
            "Exception caught during request processing: '{}'!\n", ex.what()
          );
        }
      }
    }
  );
}

// =============================================================================

awaitable<void> session(tcp::socket client_socket,
                        io_context& io_service)
{
  const static std::string ruler(80, '-');

  try
  {
    std::string clientIp = client_socket.remote_endpoint().address().to_string();
    std::println("{}", ruler);
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

    if (ec)
    {
      std::cerr << std::format("Error reading from client: '{}'", ec.message());
      co_return;
    }

    std::optional<HttpRequest> req = ParseRequest(rcv);
    if (req)
    {
      ProcessRequest(*req, io_service);
    }

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
