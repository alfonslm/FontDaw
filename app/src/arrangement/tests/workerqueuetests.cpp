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

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include "internal/workerqueue.h"

using namespace mu::arrangement;

TEST(Arrangement_WorkerQueueTests, RunsEveryJob)
{
    WorkerQueue queue(2);
    std::atomic<int> sum = 0;

    for (int i = 1; i <= 50; ++i) {
        queue.post([&sum, i]() { sum += i; });
    }
    queue.waitUntilIdle();

    EXPECT_EQ(sum.load(), 50 * 51 / 2);
}

TEST(Arrangement_WorkerQueueTests, NeverRunsMoreJobsAtOnceThanItHasThreads)
{
    WorkerQueue queue(2);
    std::atomic<int> running = 0;
    std::atomic<int> most = 0;

    for (int i = 0; i < 12; ++i) {
        queue.post([&running, &most]() {
            const int now = ++running;
            int seen = most.load();
            while (now > seen && !most.compare_exchange_weak(seen, now)) {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            --running;
        });
    }
    queue.waitUntilIdle();

    EXPECT_EQ(running.load(), 0);
    EXPECT_GE(most.load(), 1);
    EXPECT_LE(most.load(), 2);
}

TEST(Arrangement_WorkerQueueTests, CancelDropsWhatHasNotStartedAndWaitsForWhatHas)
{
    WorkerQueue queue(1);
    std::atomic<bool> firstStarted = false;
    std::atomic<bool> release = false;
    std::atomic<bool> firstDone = false;
    std::atomic<int> laterRan = 0;

    queue.post([&]() {
        firstStarted = true;
        while (!release) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        firstDone = true;
    });
    for (int i = 0; i < 5; ++i) {
        queue.post([&laterRan]() { ++laterRan; });
    }

    while (!firstStarted) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::thread releaser([&release]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        release = true;
    });

    queue.cancelAndWait();

    // The running job was waited for; the waiting ones never ran
    EXPECT_TRUE(firstDone.load());
    EXPECT_EQ(laterRan.load(), 0);
    releaser.join();

    // The queue still works afterwards
    queue.post([&laterRan]() { ++laterRan; });
    queue.waitUntilIdle();
    EXPECT_EQ(laterRan.load(), 1);
}
