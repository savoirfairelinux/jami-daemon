/*
Copyright (c) 2014 Savoir-Faire Linux Inc.

Author : Adrien Béraud <adrien.beraud@savoirfairelinux.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#pragma once

#include "securedht.h"

#include <thread>
#include <random>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <exception>

#include <unistd.h> // close(fd)

namespace dht {

class DhtRunner {

public:
    typedef std::function<void(Dht::Status, Dht::Status)> StatusCallback;

    DhtRunner() {}
    virtual ~DhtRunner() {
        join();
    }

    void get (InfoHash hash, Dht::GetCallback vcb, Dht::DoneCallback dcb=nullptr, Value::Filter f = Value::AllFilter())
    {
        std::unique_lock<std::mutex> lck(storage_mtx);
        dht_gets.emplace_back(hash, vcb, dcb, f);
        cv.notify_all();
    }

    void put (InfoHash hash, const Value& value, Dht::DoneCallback cb=nullptr)
    {
        std::unique_lock<std::mutex> lck(storage_mtx);
        dht_puts.emplace_back(hash, value, cb);
        cv.notify_all();
    }

    void putEncrypted (InfoHash hash, InfoHash to, const Value& value, Dht::DoneCallback cb=nullptr)
    {
        std::unique_lock<std::mutex> lck(storage_mtx);
        dht_eputs.emplace_back(hash, to, value, cb);
        cv.notify_all();
    }

    void bootstrap(const std::vector<sockaddr_storage>& nodes)
    {
        std::unique_lock<std::mutex> lck(storage_mtx);
        bootstrap_ips.insert(bootstrap_ips.end(), nodes.begin(), nodes.end());
        cv.notify_all();
    }

    void bootstrap(const std::vector<Dht::NodeExport>& nodes)
    {
        std::unique_lock<std::mutex> lck(storage_mtx);
        bootstrap_nodes.insert(bootstrap_nodes.end(), nodes.begin(), nodes.end());
        cv.notify_all();
    }

    void dumpTables() const
    {
        std::unique_lock<std::mutex> lck(dht_mtx);
        dht->dumpTables();
    }

    InfoHash getId() const {
        if (!dht)
            return {};
        return dht->getId();
    }

    std::vector<Dht::NodeExport> getNodes() const {
        std::unique_lock<std::mutex> lck(dht_mtx);
        if (!dht)
            return {};
        return dht->getNodes();
    }

    bool isRunning() const {
        return running;
    }

    /**
     * If threaded is false, loop() must be called periodically.
     */
    void run(in_port_t port, const crypto::Identity identity, StatusCallback cb, bool threaded = false);

    void loop() {
        std::unique_lock<std::mutex> lck(dht_mtx);
        loop_();
    }

    void join();

private:

    void doRun(in_port_t port, const crypto::Identity identity);
    void loop_();

    std::unique_ptr<SecureDht> dht {};
    mutable std::mutex dht_mtx {};
    std::thread dht_thread {};
    std::condition_variable cv {};

    std::thread rcv_thread {};
    std::mutex sock_mtx {};
    std::vector<std::pair<Blob, sockaddr_storage>> rcv {};
    std::atomic<time_t> tosleep {0};

    // IPC temporary storage
    std::vector<std::tuple<InfoHash, Dht::GetCallback, Dht::DoneCallback, Value::Filter>> dht_gets {};
    std::vector<std::tuple<InfoHash, Value, Dht::DoneCallback>> dht_puts {};
    std::vector<std::tuple<InfoHash, InfoHash, Value, Dht::DoneCallback>> dht_eputs {};
    std::vector<sockaddr_storage> bootstrap_ips {};
    std::vector<Dht::NodeExport> bootstrap_nodes {};
    std::mutex storage_mtx {};

    std::atomic<bool> running {false};

    Dht::Status status4 {Dht::Status::Disconnected},
                status6 {Dht::Status::Disconnected};
    StatusCallback statusCb {nullptr};
};

}
