#include "headers.h"
#include "constants.h"
#include "utils.h"

#include <ranges>
#include <string_view>
#include <print>
#include <algorithm>
#include <iostream>

using namespace std::string_view_literals;

using Callback = std::function<void(std::string_view, std::string_view)>;

// =============================================================================

template <typename F>
bool IsFunctionValid(const F& fn)
{
  //
  // http://www.cplusplus.com/reference/functional/function/target_type/
  //
  // Return value
  // The type_info object that corresponds to the type of the target,
  // or typeid(void) if the object is an empty function.
  //
  return (fn.target_type() != typeid(void));
}

// =============================================================================

bool ParseHeader(const std::string& headerLine, PairSS& out)
{
  //
  // According to RFC 7230:
  // "Each header field consists of a case-insensitive field name
  // followed by a colon (":"), optional leading whitespace, the
  // field value, and optional trailing whitespace."
  //
  size_t pos = headerLine.find(":");
  if (not headerLine.empty() and pos != std::string::npos)
  {
    std::string key = headerLine.substr(0, pos);

    std::string value = headerLine.substr(pos + 1);
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

    //
    // HTTP header keys are case-insensitive.
    //
    std::transform(
      key.begin(),
      key.end(),
      key.begin(),
      [](unsigned char c)
      {
        return std::tolower(c);
      }
    );

    out = { key, value };
  }
  else
  {
    return false;
  }

  return true;
}

// =============================================================================

void iterHeaders(std::string_view req, Callback&& callback)
{
  if (req.empty())
  {
    return;
  }

  std::unordered_map<std::string_view, std::string_view> headersReadSoFar;

  std::vector<std::string> headersData;

  constexpr std::string_view lineDelimiter = "\r\n";

  std::ranges::split_view<std::string_view, std::string_view>
  splitted = std::views::split(req, lineDelimiter);

  for (const auto& line : splitted)
  {
    std::string lineStr(line.begin(), line.end());
    headersData.push_back(lineStr);
  }

  bool firstLine = true;

  for (auto& line : headersData)
  {
    //
    // 'GET / HTTP/1.1' or whatever.
    //
    if (firstLine)
    {
      firstLine = false;
      continue;
    }

    //
    // HTTP headers.
    //
    PairSS header;
    if (ParseHeader(line, header))
    {
      const std::string& key   = header.first;
      const std::string& value = header.second;

      auto it = headersReadSoFar.find(key);
      if (it == headersReadSoFar.end())
      {
        headersReadSoFar[key] = value;

        if (IsFunctionValid(callback))
        {
          callback(key, value);
        }
      }
    }
  }
}

// =============================================================================

PairSS findHostPort(std::string_view req)
{
  PairSS res;

  std::string copy(req.begin(), req.end());

  std::transform(
    copy.begin(),
    copy.end(),
    copy.begin(),
    [](unsigned char c)
    {
      return std::tolower(c);
    }
  );

  size_t pos = copy.find("host:");
  if (pos != std::string::npos)
  {
    size_t delimPos = copy.find("\r\n", pos);
    if (delimPos != std::string::npos)
    {
      std::string header = copy.substr(pos, (delimPos - pos));
      PairSS pair;
      if (ParseHeader(header, pair))
      {
        res = pair;
      }
    }
  }

  return res;
}

// =============================================================================

std::optional<size_t> findContentLength(std::string_view rsp)
{
  size_t contentLength = 0;

  std::string copy(rsp.begin(), rsp.end());

  std::transform(
    copy.begin(),
    copy.end(),
    copy.begin(),
    [](unsigned char c)
    {
      return std::tolower(c);
    }
  );

  size_t pos = copy.find("content-length:");
  if (pos != std::string::npos)
  {
    size_t delimPos = copy.find("\r\n", pos);
    if (delimPos != std::string::npos)
    {
      std::string header = copy.substr(pos, (delimPos - pos));
      PairSS res;
      if (ParseHeader(header, res))
      {
        auto conv = Str2Int<size_t>(res.second);
        if (not conv)
        {
          std::cerr << std::format("Failed to convert Content-Length! '{}'",
                                  conv.error());
          return std::nullopt;
        }

        contentLength = *conv;
      }
    }
  }
  else
  {
    return std::nullopt;
  }

  return contentLength;
}
