// pool_simple.hpp - Simple std::thread pool with shared MPSC queue (CORRECTED).
//
// Baseline implementation: N worker threads + 1 std::mutex + 1 std::condition_variable
// + 1 std::deque<Task> + counter `tasks_in_flight_` (under same mutex). NO work
// stealing — workers pop from shared back. Tasks are std::function<void()>.
//
// Correctness invariants (verified by smoke test):
//   1. queue_ + tasks_in_flight_ + stop_ modified ONLY under mutex_.
//   2. cv_ wait predicate: stop_ || !queue_.empty().
//   3. idle_ wait predicate: queue_.empty() && tasks_in_flight_ == 0.
//   4. Task execution happens OUTSIDE mutex_ (don't hold lock during user code).
//   5. ~ThreadPool() sets stop_=true, notify_all, joins all workers.
//
// Educational value: shows the cost of contention on a single shared queue at
// scale (16 workers hammering 1 mutex). For small workloads (<64 tasks) the lock
// overhead is amortized; for 1000s of tasks it becomes the bottleneck.
//
// NOT a production recommendation — included for comparison only.

#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace pool_simple {

class ThreadPool {
public:
	explicit ThreadPool(std::size_t numThreads) : stop_(false), tasks_in_flight_(0)
	{
		workers_.reserve(numThreads);
		for (std::size_t i = 0; i < numThreads; ++i) {
			workers_.emplace_back([this] { WorkerLoop(); });
		}
	}

	~ThreadPool() {
        {
            std::lock_guard lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }

	ThreadPool(const ThreadPool &) = delete;
	ThreadPool &operator=(const ThreadPool &) = delete;

	template <typename F>
    void Submit(F&& f) {
        {
            std::lock_guard lock(mutex_);
            queue_.emplace_back(std::forward<F>(f));
            ++tasks_in_flight_;
        }
        cv_.notify_one();
    }

	void WaitIdle()
	{
		std::unique_lock lock(mutex_);
		idle_.wait(lock, [this] {
			return queue_.empty() && tasks_in_flight_ == 0;
		});
	}

	std::size_t NumThreads() const noexcept { return workers_.size(); }

private:
	void WorkerLoop()
	{
		while (true) {
			std::function<void()> task;
			{
				std::unique_lock lock(mutex_);
				cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
				if (stop_ && queue_.empty())
					return;
				task = std::move(queue_.front());
				queue_.pop_front();
			}
			// Execute outside mutex — don't hold lock during user code.
			task();
			{
				std::lock_guard lock(mutex_);
				--tasks_in_flight_;
				if (queue_.empty() && tasks_in_flight_ == 0) {
					idle_.notify_all();
				}
			}
		}
	}

	std::vector<std::thread> workers_;
    std::deque<std::function<void()>> queue_;  // protected by mutex_
    int tasks_in_flight_;                       // protected by mutex_
    bool stop_;                                 // protected by mutex_
    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable idle_;
};

} // namespace pool_simple
