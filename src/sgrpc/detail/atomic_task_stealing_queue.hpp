
#pragma once

#include <atomic>
#include <deque>
#include <iterator>
#include <memory>
#include <mutex>
#include <vector>

#include <cassert>

namespace sgrpc::detail {

/**
 * @private
 * @brief A lockfree deque-based queue, with an optimistic try_push and
 *        try_pop interface
 */
template <typename T> class OptimisiticLockingQueue final {
public:
  using value_type = T;

  /**
   * @brief Attempts to `std::move` the start of the queue into `x`
   * Pop fails if the queue is empty, or `try_lock` fails to acquire a
   * lock
   *
   * THREAD_SAFE
   *
   * @return `true` if, and only if, the pop is successful
   *
   * Preconditions:
   * + `x` is not executable.
   *
   * Postconditions:
   * + `x` is an executable thunk if, and only if, `true` is returned.
   */
  bool try_pop(value_type& x) {
    std::unique_lock lock{padlock_, std::try_to_lock};
    if (!lock || queue_.empty())
      return false;
    try {
      x = std::move(queue_.front());
      queue_.pop_front();
    } catch (...) {
      // FATAL("exception copying thunk. Please make all thunks noexcept "
      //       "copyable and noexcept movable");
      return false;
    }
    return true;
  }

  /**
   * @brief Attempts to push `f` into the queue.
   * Push fails if the queue is full, or `try_lock` fails to acquire a
   * lock
   *
   * THREAD_SAFE
   *
   * @return `true` if, and only if, the push is successful
   *
   * Preconditions:
   * + `f` is an executable thunk
   *
   * Preconditions:
   * + `f` has been moved from if, and only if, `true` is returned
   */
  bool try_push(value_type&& f) {
    std::unique_lock lock{padlock_, std::try_to_lock};
    if (!lock)
      return false;
    queue_.emplace_back(std::move(f));
    return true;
  }

  std::deque<value_type> eject() {
    std::lock_guard lock{padlock_};
    std::deque<value_type> result;
    std::swap(result, queue_);
    return result;
  }

private:
  std::deque<value_type> queue_;
  std::mutex padlock_;
};

} // namespace sgrpc::detail

namespace sgrpc {

/**
 * @private
 * @brief A blocking queue, with notifications for pushes and pops
 */
template <typename T> class AtomicTaskStealingQueue final {
public:
  using value_type = T;

private:
  std::vector<detail::OptimisiticLockingQueue<value_type>> queues_;
  unsigned n_queues_ = 0;

  std::atomic<unsigned> push_index_ = 0; //!< next queue to push to
  std::atomic<unsigned> pop_index_ = 0;  //!< next queue to pop from

  std::atomic<unsigned> in_push_ = false; //!< number of concurrent push operations
  std::atomic<bool> is_done_ = false;

public:
  /**
   * Roughly FIFO thread-safe "optimistic lockfree" task-stealing queue
   *
   * Exceptions:
   * + `std::bad_alloc` if allocation fails.
   */
  explicit AtomicTaskStealingQueue(unsigned number_queues)
      : queues_{number_queues}, n_queues_{number_queues}, is_done_{false} {}

  /**
   * @brief Signals that no more elements should be pushed
   *
   * THREAD SAFE
   */
  void signal_done() { is_done_.store(true, std::memory_order_release); }

  /**
   * @brief Drains the underlying queues, returning all the thunks.
   *
   * If done hasn't been signalled, then drain will signal it.
   *
   * THREAD SAFE
   */
  std::deque<value_type> stop_and_eject() {
    signal_done();
    std::atomic_signal_fence(std::memory_order_acq_rel); // Forbid reordering
    std::deque<value_type> thunks;
    do {
      for (auto i = 0u; i != n_queues_; ++i) {
        auto queue_thunks = queues_[i].eject();
        thunks.insert(std::end(thunks), std::make_move_iterator(std::begin(queue_thunks)),
                      std::make_move_iterator(std::end(queue_thunks)));
      }
    } while (in_push_.load(std::memory_order_acquire) > 0);
    return thunks;
  }

  bool done_is_signalled() const noexcept { return is_done_.load(std::memory_order_acquire); }

  /**
   * @brief Push `thunk` onto the queue. If the queue is full, then
   *        pop the start of the queue before pushing, and execute
   *        the popped thunk.
   *
   * THREAD SAFE
   *
   * Non-blocking push provides an alternative mechanism to slow
   * down over-active producers of thunks. Normally, a task queue
   * would be required to block the producer, or allocate more
   * memory for storage -- until memory is exhausted. Backing storage is
   * fixed size for a non-blocking push, and, instead of blocking the
   * producer, we free up storage by executing one of the pending thunks.
   *
   * This means that some thunks will be executed outside of the
   * thread-pool.
   */
  bool push(value_type&& thunk) {
    assert(thunk);
    in_push_.fetch_add(1, std::memory_order_acq_rel);
    std::atomic_signal_fence(std::memory_order_acq_rel); // Forbid reordering
    bool is_done = done_is_signalled();
    if (!is_done) {
      value_type out_thunk;

      for (bool did_push = false; !did_push;) {
        const auto offset = push_index_.fetch_add(1, std::memory_order_relaxed);
        for (auto i = 0u; i != n_queues_ && !did_push; ++i) {
          assert(!out_thunk);
          if (queues_[(offset + i) % n_queues_].try_push(std::move(thunk)))
            did_push = true;
        }
      }
    }
    in_push_.fetch_sub(1, std::memory_order_acq_rel);
    return !is_done; // Always succeeds
  }

  /**
   * @brief Attempt to pop an element; non-blocking.
   * @return `true` if, and only if, an element is popped.
   *
   * THREAD SAFE
   */
  bool try_pop(value_type& thunk) {
    const auto offset = pop_index_.fetch_add(1, std::memory_order_relaxed);
    for (auto i = 0u; i != n_queues_; ++i) {
      auto& queue = queues_[(offset + i) % n_queues_];
      if (queue.try_pop(thunk))
        return true;
    }
    return false;
  }
};

} // namespace sgrpc
