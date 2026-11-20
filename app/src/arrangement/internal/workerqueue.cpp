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

#include "workerqueue.h"

using namespace mu::arrangement;

WorkerQueue::WorkerQueue(size_t maxThreads)
    : m_maxThreads(maxThreads > 0 ? maxThreads : 1)
{
}

WorkerQueue::~WorkerQueue()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_jobs.clear();
        m_stop = true;
    }
    m_wake.notify_all();
    for (std::thread& t : m_threads) {
        t.join();
    }
}

void WorkerQueue::post(std::function<void()> job)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_jobs.push_back(std::move(job));
        // One more thread when every existing one is busy
        if (m_threads.size() < m_maxThreads && m_running + m_jobs.size() > m_threads.size()) {
            m_threads.emplace_back([this]() { run(); });
        }
    }
    m_wake.notify_one();
}

void WorkerQueue::cancelAndWait()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_jobs.clear();
    m_idle.wait(lock, [this]() { return m_running == 0; });
}

void WorkerQueue::waitUntilIdle()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_idle.wait(lock, [this]() { return m_running == 0 && m_jobs.empty(); });
}

void WorkerQueue::run()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    for (;;) {
        m_wake.wait(lock, [this]() { return m_stop || !m_jobs.empty(); });
        if (m_stop) {
            return;
        }

        std::function<void()> job = std::move(m_jobs.front());
        m_jobs.pop_front();
        ++m_running;

        lock.unlock();
        job();
        job = nullptr; // destroy what it captured outside the lock
        lock.lock();

        --m_running;
        if (m_running == 0) {
            m_idle.notify_all(); // waiters check the queue themselves
        }
    }
}
