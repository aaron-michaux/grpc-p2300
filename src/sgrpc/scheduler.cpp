
#include "scheduler.hpp"

#include "detail/context_impl.hpp"

namespace sgrpc {

[[nodiscard]] constexpr bool Scheduler::operator==(const Scheduler& other) const noexcept {
  return &context_ == &other.context_;
}

bool Scheduler::running_in_this_thread() const noexcept {
  return context_.is_running_in_thread(std::this_thread::get_id());
}

}; // namespace sgrpc
