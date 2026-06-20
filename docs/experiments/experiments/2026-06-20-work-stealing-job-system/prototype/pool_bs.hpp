// pool_bs.hpp - BS::thread_pool v5.0.0 wrapper (work stealing, header-only, MIT).
//
// BS::thread_pool = Barak Shoshany's work-stealing thread pool. Internally uses
// a per-worker task queue with global fallback for stealing. Submit via
// `pool.submit_task(...)` and `pool.wait_for_tasks()` (or `wait_for_tasks_dur`).
//
// Reference: https://github.com/bshoshany/thread-pool
// License: MIT (Barak Shoshany)
// Version pinned: v5.0.0 (2024-12-20)

#pragma once

#include "BS_thread_pool.hpp"

namespace pool_bs {

using ThreadPool = BS::thread_pool<>;

} // namespace pool_bs
