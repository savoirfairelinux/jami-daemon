/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "../../../bin/nodejs/signal_dispatch.h"

#include <iostream>
#include <thread>
#include <vector>

int
main()
{
    std::queue<std::function<void()>> signals;
    std::mutex mutex;
    std::vector<int> delivered;
    bool callbackCanEnqueue = false;

    signals.emplace([&] {
        std::thread probe([&] {
            callbackCanEnqueue = mutex.try_lock();
            if (callbackCanEnqueue)
                mutex.unlock();
        });
        probe.join();
        if (callbackCanEnqueue) {
            std::lock_guard lock(mutex);
            signals.emplace([&] { delivered.push_back(3); });
        }
        delivered.push_back(1);
    });
    signals.emplace([&] { delivered.push_back(2); });

    libjami::nodejs::drainPendingSignals(signals, mutex);
    if (!callbackCanEnqueue || delivered != std::vector<int> {1, 2, 3} || !signals.empty()) {
        std::cerr << "Signal callbacks must run unlocked and preserve delivery order\n";
        return 1;
    }
}
