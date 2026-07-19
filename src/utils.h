#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>
#include <expected>
#include <charconv>
#include <ranges>
#include <concepts>
#include <optional>

#include "structs.h"

std::vector<std::string> StringSplit(const std::string& str, char delimiter);
std::string Trim(const std::string& in);
std::optional<HttpObject> StringToHttpObject(std::string& rcv, bool isRequest);

// =============================================================================

template <typename T>
concept IsInt = std::signed_integral<T>
        and not std::is_same_v<std::remove_cv_t<T>, char>;

template <typename T>
concept IsUInt = std::unsigned_integral<T>;

template <typename T>
requires IsInt<T> or IsUInt<T>
[[nodiscard]]
std::expected<T, std::string> Str2Int(const std::string& input)
{
  T result;

  const char* first = input.data();
  const char* last  = input.data() + input.size();

  auto [ptr, ec] = std::from_chars(first, last, result);

  if (ec == std::errc() and ptr != last)
  {
    return std::unexpected("Failed to convert!");
  }
  else if (ec == std::errc::invalid_argument)
  {
    return std::unexpected("Invalid type!");
  }
  else if (ec == std::errc::result_out_of_range)
  {
    return std::unexpected("Result is out of range!");
  }

  return result;
}

#endif
