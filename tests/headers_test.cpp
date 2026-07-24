#include <gtest/gtest.h>
#include <print>

#include "headers.h"
#include "constants.h"

TEST(iterHeaders, Empty)
{
  {
    const std::string example;

    iterHeaders(
      example,
      [](std::string_view headerKey, std::string_view headerValue)
      {
        ASSERT_TRUE(false);
      }
    );
  }
  // ---------------------------------------------------------------------------
  {
    const std::string example = "\r\n";

    iterHeaders(
      example,
      [](std::string_view headerKey, std::string_view headerValue)
      {
        ASSERT_TRUE(false);
      }
    );
  }
  // ---------------------------------------------------------------------------
  {
    const std::string example = "\r\n\r\n";

    iterHeaders(
      example,
      [](std::string_view headerKey, std::string_view headerValue)
      {
        ASSERT_TRUE(false);
      }
    );
  }
  // ---------------------------------------------------------------------------
  {
    const std::string example = "\v\t    ";

    iterHeaders(
      example,
      [](std::string_view headerKey, std::string_view headerValue)
      {
        ASSERT_TRUE(false);
      }
    );
  }
}

// =============================================================================

TEST(iterHeaders, SkipRequestLine)
{
  const std::string_view example =
    "GET HTTP://127.0.0.1:8000/ HTTP/1.1\r\n"
    "Server: BaseHTTP/0.6 Python/3.13.3\r\n"
    "Date: Fri, 24 Jul 2026 11:10:49 GMT\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: 4096\r\n"
    "\r\n";

  std::vector<PairSS> expected =
  {
      { "server", "BaseHTTP/0.6 Python/3.13.3"  }
    , { "date", "Fri, 24 Jul 2026 11:10:49 GMT" }
    , { "content-type", "text/html"             }
    , { "content-length", "4096"                }
  };

  size_t vecCounter = 0;

  iterHeaders(
    example,
    [&vecCounter, &expected]
    (std::string_view headerKey, std::string_view headerValue)
    {
      EXPECT_EQ(expected[vecCounter].first, headerKey);
      EXPECT_EQ(expected[vecCounter].second, headerValue);
      vecCounter++;
    }
  );
}

// =============================================================================

TEST(iterHeaders, SingleHeader)
{
  {
    const std::string_view example =
      "GET HTTP://127.0.0.1:8000/ HTTP/1.1\r\n"
      "\r\n";

    iterHeaders(
      example,
      []
      (std::string_view headerKey, std::string_view headerValue)
      {
        ASSERT_TRUE(false);
      }
    );
  }
  // ---------------------------------------------------------------------------
  {
    const std::string_view example =
      "GET HTTP://127.0.0.1:8000/ HTTP/1.1\r\n"
      "Server: BaseHTTP/0.6 Python/3.13.3\r\n"
      "\r\n";

    iterHeaders(
      example,
      []
      (std::string_view headerKey, std::string_view headerValue)
      {
        EXPECT_EQ("server", headerKey);
        EXPECT_EQ("BaseHTTP/0.6 Python/3.13.3", headerValue);
      }
    );
  }
}

// =============================================================================

TEST(iterHeaders, MultipleHeaders)
{
  const std::string_view example =
    "GET HTTP://127.0.0.1:8000/ HTTP/1.1\r\n"
    "Server: BaseHTTP/0.6 Python/3.13.3\r\n"
    "Date: Fri, 24 Jul 2026 11:10:49 GMT\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: 4096\r\n"
    "\r\n";

  std::vector<PairSS> expected =
  {
      { "server", "BaseHTTP/0.6 Python/3.13.3"  }
    , { "date", "Fri, 24 Jul 2026 11:10:49 GMT" }
    , { "content-type", "text/html"             }
    , { "content-length", "4096"                }
  };

  size_t vecCounter = 0;

  iterHeaders(
    example,
    [&vecCounter, &expected]
    (std::string_view headerKey, std::string_view headerValue)
    {
      EXPECT_EQ(expected[vecCounter].first, headerKey);
      EXPECT_EQ(expected[vecCounter].second, headerValue);
      vecCounter++;
    }
  );
}

// =============================================================================

TEST(iterHeaders, MultipleSameHeaders)
{
  const std::string_view example =
    "GET HTTP://127.0.0.1:8000/ HTTP/1.1\r\n"
    "Server: BaseHTTP/0.6 Python/3.13.3\r\n"
    "Server: BaseHTTP/0.6 Python/3.13.3\r\n"
    "Server: BaseHTTP/0.6 Python/3.13.3\r\n"
    "Server: BaseHTTP/0.6 Python/3.13.3\r\n"
    "Date: Fri, 24 Jul 2026 11:10:49 GMT\r\n"
    "Date: Fri, 24 Jul 2026 11:10:49 GMT\r\n"
    "Content-Type: text/html\r\n"
    "Content-Type: text/html\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: 4096\r\n"
    "Content-Length: 4096\r\n"
    "Content-Length: 4096\r\n"
    "Content-Length: 4096\r\n"
    "\r\n";

  std::vector<PairSS> expected =
  {
      { "server", "BaseHTTP/0.6 Python/3.13.3"  }
    , { "date", "Fri, 24 Jul 2026 11:10:49 GMT" }
    , { "content-type", "text/html"             }
    , { "content-length", "4096"                }
  };

  size_t vecCounter = 0;

  iterHeaders(
    example,
    [&vecCounter, &expected]
    (std::string_view headerKey, std::string_view headerValue)
    {
      EXPECT_EQ(expected[vecCounter].first, headerKey);
      EXPECT_EQ(expected[vecCounter].second, headerValue);
      vecCounter++;
    }
  );
}

// =============================================================================

TEST(findHostPort, Simple)
{
  const std::string_view example =
    "GET HTTP://127.0.0.1:8000/ HTTP/1.1\r\n"
    "User-Agent: curl/7.29.0\r\n"
    "Host: 127.0.0.1:8000\r\n"
    "Accept: */*\r\n"
    "Proxy-Connection: Keep-Alive\r\n"
    "\r\n";

  PairSS res = findHostPort(example);

  EXPECT_EQ("host", res.first);
  EXPECT_EQ("127.0.0.1:8000", res.second);
}

// =============================================================================

TEST(findHostPort, NoHost)
{
  const std::string_view example =
    "GET HTTP://127.0.0.1:8000/ HTTP/1.1\r\n"
    "User-Agent: curl/7.29.0\r\n"
    "Accept: */*\r\n"
    "Proxy-Connection: Keep-Alive\r\n"
    "\r\n";

  PairSS res = findHostPort(example);

  EXPECT_TRUE(res.first.empty());
  EXPECT_TRUE(res.second.empty());
}

// =============================================================================

TEST(findContentLength, Simple)
{
  const std::string_view example =
    "HTTP/1.0 200 OK\r\n"
    "Server: BaseHTTP/0.6 Python/3.13.3\r\n"
    "Date: Fri, 24 Jul 2026 13:07:45 GMT\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: 4096\r\n"
    "\r\n";

  std::optional<size_t> len = findContentLength(example);

  ASSERT_TRUE(len.has_value());
  EXPECT_EQ(4096, len.value());
}

// =============================================================================

TEST(findContentLength, NoContentLength)
{
  const std::string_view example =
    "HTTP/1.1 200 OK\r\n"
    "Date: Fri, 24 Jul 2026 13:05:40 GMT\r\n"
    "Content-Type: text/html\r\n"
    "Transfer-Encoding: chunked\r\n"
    "Connection: close\r\n"
    "Server: cloudflare\r\n"
    "Last-Modified: Tue, 21 Jul 2026 07:16:00 GMT\r\n"
    "Allow: GET, HEAD\r\n"
    "Accept-Ranges: bytes\r\n"
    "Age: 9072\r\n"
    "cf-cache-status: HIT\r\n"
    "CF-RAY: a20323465a329dc7-DME\r\n"
    "\r\n";

  std::optional<size_t> len = findContentLength(example);

  ASSERT_FALSE(len.has_value());
}
