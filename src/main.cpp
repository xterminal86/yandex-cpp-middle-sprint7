#include <boost/asio.hpp>
// io_service is replaced with io_context since boost 1.87
#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/use_awaitable.hpp>

// For operator || on awaitables.
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <string_view>
#include <iostream>
#include <print>

#include "constants.h"
#include "headers.h"
#include "utils.h"

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

using namespace boost::asio::experimental::awaitable_operators;

constexpr std::string_view DoubleCRLF = "\r\n\r\n";

//
// Need to accept parameters by value because co_spawn immediately returns and
// thus our original variables will go out of scope.
//
awaitable<std::string> DoRequest(HttpObject originalRequest,
                                 std::string host,
                                 uint64_t port,
                                 tcp::socket& clientSocket)
{
  using namespace boost::asio;

  // Get event loop "handler" for this coroutine (i.e. io_context).
  auto executor = co_await this_coro::executor;

  tcp::resolver resolver(executor);
  auto endpoints = co_await resolver.async_resolve(host, std::to_string(port));

  error_code ec;

  tcp::socket socket(executor);

  steady_timer timer(executor);

  //
  // You can test connection timeout by trying to connect to a closed port (e.g.
  // curl -v -x 127.0.0.1:9999 example.com:1234).
  //
  timer.expires_after(Constants::ConnectionTimeout);

  bool connectOk = true;

  timer.async_wait(
    [&](error_code timerEc)
    {
      if (not timerEc)
      {
        socket.cancel();
        connectOk = false;
      }
    }
  );

  co_await socket.async_connect(
    *endpoints.begin(),
    redirect_error(use_awaitable, ec)
  );

  timer.cancel();

  if (ec)
  {
    std::string msg = std::format("Connection error: '{}'!\n", ec.message());
    std::cerr << msg;
    co_await async_write(clientSocket, buffer(msg));
    co_return msg;
  }

  // Send request.
  std::stringstream request;

  //request << "GET / HTTP/1.1\r\n";

  //
  // Forward original request method.
  //
  request << std::get<0>(originalRequest.FirstLine) << " "
          << std::get<1>(originalRequest.FirstLine) << " "
          << std::get<2>(originalRequest.FirstLine)
          << "\r\n"
          << "";

  //
  // Forward all request headers.
  // NOTE: It seems example.com won't respond if you add custom header(s), so
  // test this on stub-server.py
  //
  for (auto& kvp : originalRequest.Headers)
  {
    request << std::format("{}: {}\r\n", kvp.first, kvp.second);
  }

  //
  // Without this it won't connect to anything but stub-server.py
  //
  request << "Connection: close\r\n"
          << "\r\n"
          << "";

  co_await async_write(
    socket,
    buffer(request.str()),
    redirect_error(use_awaitable, ec)
  );

  if (ec)
  {
    std::string msg = std::format("Write error: '{}'!\n", ec.message());
    std::cerr << msg;
    co_await async_write(clientSocket, buffer(msg));
    co_return msg;
  }

  // Read response using a dynamic buffer.
  std::string responseRcv;

  timer.expires_after(Constants::ExecutionTimeout);

  bool execOk = true;

  timer.async_wait(
    [&](error_code timerEc)
    {
      if (not timerEc)
      {
        socket.cancel();
        execOk = false;
      }
    }
  );

  // Read until the server closes the connection.
  while (not ec)
  {
    std::array<char, Constants::ChunkSizeBytes> chunk;
    size_t bytes_read = co_await socket.async_read_some(
      buffer(chunk),
      redirect_error(use_awaitable, ec)
    );

    if (not ec && bytes_read > 0)
    {
      //
      // По хорошему для экономии памяти здесь тогда не надо набирать строку и
      // сразу отсылать ответ, но поскольку учебный проект, то для дебага
      // оставим.
      //
      responseRcv += std::string(chunk.data(), bytes_read);
    }

    if (not execOk)
    {
      responseRcv = "Read error!\n";
      break;
    }
  }

  timer.cancel();

  // The connection was closed, get the full response.
  //std::println("Full response:");
  //std::println("{}", responseRcv);
  //std::println("");

  socket.close();

  // Send response AS IS back to curl or whatever.
  co_await async_write(clientSocket, buffer(responseRcv));

  co_return responseRcv;
}

// =============================================================================

awaitable<void> ProcessRequest(const HttpObject& req,
                               io_context& io_service,
                               tcp::socket& clientSocket)
{
  std::string hostHeader = req.ReadHeader("Host");
  if (hostHeader.empty())
  {
    std::cerr << "No 'Host' HTTP header found!\n";
    co_return;
  }

  std::vector<std::string> parts = StringSplit(hostHeader, ':');

  const std::string& host = parts[0];

  //
  // Default port is assumed by curl if protocol is "standard" (e.g.
  // 'http://www.whatever.com' -> 80, 'https://www.whatever.com' -> 443 and so
  // on), so it won't be in a header. Since we only use HTTP here, assume it's
  // always 80 by default.
  //
  uint64_t port = 80;

  if (parts.size() == 2)
  {
    auto conv = Str2Int<uint64_t>(parts[1]);
    if (not conv)
    {
      std::cerr << conv.error();
      co_return;
    }

    port = *conv;

    //
    // If you use curl, it validates port value automatically, but just in case.
    //
    if (port > 65535)
    {
      std::string err = "Port must be [0; 65535]!\n";
      std::cerr << err;
      co_return;
    }
  }

  std::string response = co_await DoRequest(std::move(req),
                                            host,
                                            port,
                                            clientSocket);
  //std::println("Response raw:\n");
  //std::println("{}", response);
  //std::println("");

  std::optional<HttpObject> resp = StringToHttpObject(response);
  if (resp)
  {
    std::println("{}", Constants::RulerRCV);
    std::println("Parsed response:\n");
    std::println("{}", resp.value().ToString());
    std::println("{}", Constants::RulerRCV);
  }
}

// =============================================================================

awaitable<void> Session(tcp::socket client_socket,
                        io_context& io_service)
{
  try
  {
    std::string clientIp = client_socket.remote_endpoint().address().to_string();
    std::println("{}", Constants::Ruler);
    std::println("{} connected", clientIp);

    std::string rcv;

    error_code ec;

    //
    // co_await will "block" until coroutine is finished.
    //
    // Read the HTTP request headers until we find the double CRLF
    //
    size_t bytes = co_await async_read_until(
      client_socket,
      dynamic_buffer(rcv),
      DoubleCRLF,
      redirect_error(use_awaitable, ec)
    );

    if (ec)
    {
      std::cerr << std::format("Error reading from client: '{}'", ec.message());
      co_return;
    }

    std::optional<HttpObject> req = StringToHttpObject(rcv);
    if (req)
    {
      std::println("{}", Constants::RulerSND);
      std::println("Parsed request:\n");
      std::println("{}", req.value().ToString());
      std::println("{}", Constants::RulerSND);

      co_await ProcessRequest(*req, io_service, client_socket);
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

    //
    // co_spawn "detaches" execution by scheduling coroutine on the event loop
    // (i.e. io_service_ here). So we're scheduling a coroutine that will listen
    // to incoming connections indefinitely. boost::asio::detached means to
    // ignore the result of the coroutine, but in case of unhandled exception
    // std::terminate() will be called.
    //
    co_spawn(
      io_service_,
      [this]() -> awaitable<void>
      {
        while (true)
        {
          try
          {
            //
            // This will "block" until client connects.
            //
            tcp::socket s = co_await acceptor_.async_accept(use_awaitable);

            //
            // Schedule new coroutine to handle new incoming connection.
            //
            co_spawn(
              io_service_,
              Session(std::move(s), io_service_),
              boost::asio::detached
            );
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

// =============================================================================

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << std::format("Usage: {} <PORT>\n", argv[0]);
      return 1;
    }

    uint64_t port = 0;

    auto conv = Str2Int<uint64_t>(argv[1]);
    if (not conv)
    {
      std::cerr << conv.error();
      return 1;
    }

    port = *conv;

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
