// pool_simple.hpp - Simple std::thread pool with shared MPSC queue.
//
// Baseline implementation: N worker threads + 1 std::mutex + 1 std::condition_variable
// + 1 std::deque<Task>. NO work stealing — workers pop from shared back. Tasks are
// std::function<void()> (heap-allocated closure, virtual call).
//
// Educational value: shows the cost of contention on a single shared queue at
// scale (16 workers hammering 1 mutex). For small workloads (<64 tasks) the lock
// overhead is amortized; for 1000s of tasks it becomes the bottleneck.
//
// NOT a production recommendation — included for comparison only.

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace pool_simple {

class ThreadPool {
  public:
	explicit ThreadPool(std::size_t numThreads)
	{
		workers_.reserve(numThreads);
		for (std::size_t i = 0; i < numThreads; ++i) {
			workers_.emplace_back([this] { WorkerLoop(); });
		}
	}

	~ThreadPool()
	{
		{
			std::lock_guard lock(mutex_);
			stop_ = true;
		}
		cv_.notify_all();
		for (auto &t : workers_)
			t.join();
	}

	ThreadPool(const ThreadPool &) = delete;
	ThreadPool &operator=(const ThreadPool &) = delete;

	template <typename F>
	void Submit(F &&f)
	{
		{
			std::lock_guard lock(mutex_);
			queue_.emplace_back(std::forward<F>(f));
		}
		cv_.notify_one();
	}

	void WaitIdle()
	{
		// Naive: wait until queue empty AND all workers idle. Polling is OK for short
		// benchmarks; for production use a generation counter or per-worker event.
		std::unique_lock lock(mutex_);
		idle_.wait(lock, [this] {
			if (!queue_.empty())
				return false;
			for (auto idle : workerIdle_) {
				if (!idle)
					return false;
			}
			return true;
		});
	}

	std::size_t NumThreads() const noexcept { return workers_.size(); }

  private:
	void WorkerLoop()
	{
		workerIdle_.push_back(true);
		std::unique_lock lock(mutex_, std::defer_lock);
		while (true) {
			lock.lock();
			cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
			if (stop_ && queue_.empty()) {
				workerIdle_.back() = true;
				lock.unlock();
				cvIdleCheck();
				return;
			}
			if (queue_.empty()) {
				lock.unlock();
				continue;
			}
			auto task = std::move(queue_.front());
			queue_.pop_front();
			workerIdle_.back() = false;
			lock.unlock();

			task();

			{
				std::lock_guard lock(mutex_);
				workerIdle_.back() = true;
			}
			cvIdleCheck();
		}
	}

	void cvIdleCheck()
	{
		std::lock_guard lock(mutex_);
		idle_.notify_all();
	}

	std::vector<std::thread> workers_;
	std::deque<std::function<void()>> queue_;
	std::vector<bool> workerIdle_; // protected by mutex_
	std::mutex mutex_;
	std::condition_variable cv_;
	std::condition_variable idle_;
	bool stop_ = false;
};

} // namespace pool_simple
