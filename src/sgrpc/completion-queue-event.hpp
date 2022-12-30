
#pragma once

namespace sgrpc {

class CompletionQueueEvent {
  virtual ~CompletionQueueEvent() = default;
  virtual void run(bool is_ok) = 0;
};

} // namespace sgrpc
