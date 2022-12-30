
#pragma once

#include <grpcpp/completion_queue.h>

#include <chrono>

namespace sgrpc::detail {
inline gpr_timespec duration_to_grp_timespec(std::chrono::nanoseconds delta) {
  gpr_timespec timeout_gpr;
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(delta);
  const auto nanos = delta - seconds;
  timeout_gpr.clock_type = GPR_CLOCK_MONOTONIC;
  timeout_gpr.tv_sec = seconds.count();
  timeout_gpr.tv_nsec = nanos.count();
  return timeout_gpr;
}
} // namespace sgrpc::detail
