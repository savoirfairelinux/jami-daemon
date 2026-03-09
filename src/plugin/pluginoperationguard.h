/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace jami {

class PluginOperationGuardState
{
public:
    class Guard
    {
    public:
        explicit Guard(PluginOperationGuardState& state)
            : state_(&state)
        {
            state_->beginOperation();
        }

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

        Guard(Guard&& other) noexcept
            : state_(other.state_)
        {
            other.state_ = nullptr;
        }

        Guard& operator=(Guard&& other) noexcept
        {
            if (this != &other) {
                release();
                state_ = other.state_;
                other.state_ = nullptr;
            }
            return *this;
        }

        ~Guard() { release(); }

    private:
        void release()
        {
            if (state_) {
                state_->endOperation();
                state_ = nullptr;
            }
        }

        PluginOperationGuardState* state_ {};
    };

    Guard acquire() { return Guard(*this); }

    void waitUntilReady(std::unique_lock<std::mutex>& lock)
    {
        cv_.wait(lock, [this] { return !unloading_; });
    }

    void beginUnload(std::unique_lock<std::mutex>& lock)
    {
        unloading_ = true;
        cv_.wait(lock, [this] { return activeOperations_ == 0; });
    }

    void endUnload(std::unique_lock<std::mutex>& lock)
    {
        unloading_ = false;
        lock.unlock();
        cv_.notify_all();
    }

    std::mutex& mutex() const { return mutex_; }

private:
    void beginOperation()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        waitUntilReady(lock);
        ++activeOperations_;
    }

    void endOperation()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (activeOperations_ > 0)
            --activeOperations_;
        if (!unloading_ || activeOperations_ == 0)
            cv_.notify_all();
    }

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::size_t activeOperations_ {};
    bool unloading_ {false};
};

} // namespace jami
