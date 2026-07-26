#include "constants.h"

namespace Constants
{
  const std::string Ruler(80, '-');
  const std::string RulerSND(80, '>');
  const std::string RulerRCV(80, '<');

  const Seconds ConnectionTimeout = Seconds(5);
  const Seconds ExecutionTimeout  = Seconds(10);
}
