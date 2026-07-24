#ifndef STRUCTS_H
#define STRUCTS_H

#include <unordered_map>
#include <algorithm>
#include <format>
#include <sstream>

#include "constants.h"

struct HttpObject
{
  //
  // Not the best solution, but just to save overall time dealing with this.
  //
  // E.g. 'GET / HTTP/1.1' for request or 'HTTP/1.1 200 OK' for response.
  //
  Tuple3S FirstLine;

  std::unordered_map<std::string, std::string> Headers;

  std::string ReadHeader(const std::string& key) const
  {
    std::string keyCopy = key;

    std::transform(
      keyCopy.begin(),
      keyCopy.end(),
      keyCopy.begin(),
      [](unsigned char c)
      {
        return std::tolower(c);
      }
    );

    std::string value;

    auto it = Headers.find(keyCopy);
    if (it != Headers.end())
    {
      value = it->second;
    }

    return value;
  }

  std::string ToString()
  {
    std::stringstream ss;

    ss << std::format("Method  / Version: '{}'\n",   std::get<0>(FirstLine))
       << std::format("Path    / Status : '{}'\n",   std::get<1>(FirstLine))
       << std::format("Version / Message: '{}'\n\n", std::get<2>(FirstLine))
       << "Headers:\n\n";

    size_t n = 1;
    for (auto& kvp : Headers)
    {
      ss << std::format("{}. '{}' = '{}'\n", n++, kvp.first, kvp.second);
    }

    return ss.str();
  }
};

#endif
