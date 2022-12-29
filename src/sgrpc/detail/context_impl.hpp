
#pragma once

namespace sgrpc {
class ExecutionContext;
}

namespace sgrpc::detail {
bool Scheduler::running_in_this_thread(const ExecutionContext& context) noexcept;
}
