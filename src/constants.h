#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <string>
#include <chrono>

using Seconds = std::chrono::seconds;

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
