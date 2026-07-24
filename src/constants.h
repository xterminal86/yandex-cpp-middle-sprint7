#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <string>
#include <chrono>
#include <tuple>
#include <string>

using Seconds = std::chrono::seconds;
using Tuple3S = std::tuple<std::string, std::string, std::string>;
using PairSS  = std::pair<std::string, std::string>;

namespace Constants
{
  extern const std::string Ruler;
  extern const std::string RulerSND;
  extern const std::string RulerRCV;

  extern const Seconds ConnectionTimeout;
  extern const Seconds ExecutionTimeout;

  constexpr size_t ChunkSizeBytes = 4096;
}

#endif
