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

std::optional<HttpObject> StringToHttpObject(std::string& rcv, bool isRequest)
{
  HttpObject obj;

  bool parseOk = true;

  //
  // Cannot use const here for some fucking reason.
  //
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

    //
    // 'GET / HTTP/1.1' or whatever.
    //
    // RFC says that 1 space character is a delimiter:
    // 'status-line = HTTP-version SP status-code SP reason-phrase CRLF'
    // so theoretically there can be multiple spaces in between.
    //
    if (i == 0)
    {
      std::vector<std::string> parts = StringSplit(line, ' ');
      if (parts.size() == 3)
      {
        std::get<0>(obj.FirstLine) = Trim(parts[0]);
        std::get<1>(obj.FirstLine) = Trim(parts[1]);
        std::get<2>(obj.FirstLine) = Trim(parts[2]);
      }
      else
      {
        std::cerr << "Cannot parse '<METHOD> <PATH> <VERSION>'!\n";
        parseOk = false;
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
          obj.Body = line;
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
            std::cerr << std::format("Invalid header format: '{}'!\n", line);
            parseOk = false;
            break;
          }
        }
      }
    }
  }

  if (parseOk)
  {
    std::println("{}", (isRequest ? kRulerSND : kRulerRCV));
    std::println("Parsed {}:\n", (isRequest ? "request" : "response"));
    std::println("{}", obj.ToString());
    std::println("{}", (isRequest ? kRulerSND : kRulerRCV));
  }
  else
  {
    return std::nullopt;
  }

  return obj;
}
