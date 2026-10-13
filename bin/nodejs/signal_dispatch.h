#pragma once

#include <functional>
#include <mutex>
#include <queue>
#include <utility>

namespace libjami::nodejs {

inline void
drainPendingSignals(std::queue<std::function<void()>>& signals, std::mutex& mutex)
{
    while (true) {
        std::function<void()> callback;
        {
            std::lock_guard lock(mutex);
            if (signals.empty())
                break;
            callback = std::move(signals.front());
            signals.pop();
        }
        callback();
    }
}

} // namespace libjami::nodejs
