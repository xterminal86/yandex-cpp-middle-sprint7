#include "utils.h"
#include "constants.h"

#include <iostream>
#include <print>

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

//
// NOTE: search for whitespaces only (i.e. ' ').
//
std::string Trim(const std::string& in)
{
  auto _ltrim = [](const std::string& s)
  {
    size_t start = s.find_first_not_of(' ');
    return (start == std::string::npos) ? "" : s.substr(start);
  };

  auto _rtrim = [](const std::string& s)
  {
    size_t end = s.find_last_not_of(' ');
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
  };

  return _rtrim(_ltrim(in));
}

// =============================================================================

std::optional<Tuple3S> ExtractMethod(const std::string& line)
{
  Tuple3S res;

  std::vector<std::string> parts = StringSplit(line, ' ');
  if (parts.size() >= 3)
  {
    std::get<0>(res) = Trim(parts[0]);
    std::get<1>(res) = Trim(parts[1]);

    //
    // E.g. 'HTTP/1.1 302 Moved temporarily' in response.
    //
    std::stringstream rest;
    for (size_t i = 2; i < parts.size(); i++)
    {
      if (i != 2)
      {
        rest << " ";
      }

      rest << parts[i];
    }

    std::get<2>(res) = rest.str();
  }
  else
  {
    std::cerr << "[ERR] Cannot parse '<METHOD> <PATH> <VERSION>'!\n";
    return std::nullopt;
  }

  return res;
}

// =============================================================================

bool ParseHeader(const std::string& line, HttpObject& obj)
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

    obj.Headers[key] = value;
  }
  else
  {
    return false;
  }

  return true;
}

// =============================================================================

std::optional<HttpObject> StringToHttpObject(std::string& rcv)
{
  HttpObject obj;

  bool parseOk = true;

  //
  // Cannot use const here for some fucking reason.
  //
  std::string requestDataDelimiter = "\r\n";

  std::vector<std::string> headersData;

  std::ranges::split_view<
    std::ranges::ref_view<std::string>,
    std::ranges::ref_view<std::string>
  > splitted = std::views::split(rcv, requestDataDelimiter);

  for (const auto& line : splitted)
  {
    std::string lineStr(line.begin(), line.end());
    headersData.push_back(lineStr);
  }

  for (size_t i = 0; i < headersData.size(); i++)
  {
    const std::string& line = headersData[i];

    //
    // 'GET / HTTP/1.1' or whatever.
    //
    // RFC says that 1 space character is a delimiter:
    // 'status-line = HTTP-version SP status-code SP reason-phrase CRLF'
    // so theoretically there can be multiple spaces in between.
    //
    if (i == 0)
    {
      std::optional<Tuple3S> method = ExtractMethod(line);
      if (method.has_value())
      {
        obj.FirstLine = *method;
      }
      else
      {
        parseOk = false;
        break;
      }
    }
    else
    {
      if (line.empty())
      {
        //
        // We don't need to parse body in this task.
        //
        break;
      }
      else
      {
        if (not ParseHeader(line, obj))
        {
          parseOk = false;
          break;
        }
      }
    }
  }

  if (not parseOk)
  {
    return std::nullopt;
  }

  return obj;
}
