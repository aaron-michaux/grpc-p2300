
#pragma once

namespace sgrpc {

class CompletionQueueEvent {
public:
  virtual ~CompletionQueueEvent() = default;
  virtual void complete(bool is_ok) = 0;
};

} // namespace sgrpc
