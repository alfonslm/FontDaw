/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * FontDaw
 * The notation-first DAW
 *
 * Copyright (C) 2026 FontDaw contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace mu::arrangement {
//! A few worker threads that run jobs in the order they were posted (FD-030): opening a
//! project with many recordings no longer starts one thread and one whole-file read per file.
//! Threads start on demand and stay until the queue is destroyed.
class WorkerQueue
{
public:
    explicit WorkerQueue(size_t maxThreads = 2);
    ~WorkerQueue();

    WorkerQueue(const WorkerQueue&) = delete;
    WorkerQueue& operator=(const WorkerQueue&) = delete;

    void post(std::function<void()> job);

    //! Drops the jobs that have not started and waits until the running ones are done.
    //! After it returns no job of this queue is touching the disk.
    void cancelAndWait();

    //! Waits until every posted job has run.
    void waitUntilIdle();

private:
    void run();

    const size_t m_maxThreads;
    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::condition_variable m_idle;
    std::deque<std::function<void()> > m_jobs;
    std::vector<std::thread> m_threads;
    size_t m_running = 0;
    bool m_stop = false;
};
}
