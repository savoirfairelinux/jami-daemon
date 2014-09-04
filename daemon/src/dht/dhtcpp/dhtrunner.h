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

namespace Dht {

class DhtRunner {

public:
    typedef std::function<void(Dht::Status, Dht::Status)> StatusCallback;

    DhtRunner() {}
    virtual ~DhtRunner() {
        join();
    }

    void get (InfoHash hash, Dht::GetCallback vcb, Dht::DoneCallback dcb=nullptr)
    {
        {
            std::unique_lock<std::mutex> lck(dht_mtx);
            dht_gets.emplace_back(hash, vcb, dcb);
        }
        cv.notify_all();
    }

    void put (InfoHash hash, const Value& value, Dht::DoneCallback cb=nullptr)
    {
        {
            std::unique_lock<std::mutex> lck(dht_mtx);
            dht_puts.emplace_back(hash, value, cb);
        }
        cv.notify_all();
    }

    void putEncrypted (InfoHash hash, InfoHash to, const Value& value, Dht::DoneCallback cb=nullptr)
    {
        {
            std::unique_lock<std::mutex> lck(dht_mtx);
            dht_eputs.emplace_back(hash, to, value, cb);
        }
        cv.notify_all();
    }

    void bootstrap(const std::vector<sockaddr_storage>& nodes)
    {
        {
            std::unique_lock<std::mutex> lck(dht_mtx);
            bootstrap_ips.insert(bootstrap_ips.end(), nodes.begin(), nodes.end());
        }
        cv.notify_all();
    }

    void bootstrap(const std::vector<Dht::NodeExport>& nodes)
    {
        {
            std::unique_lock<std::mutex> lck(dht_mtx);
            bootstrap_nodes.insert(bootstrap_nodes.end(), nodes.begin(), nodes.end());
        }
        cv.notify_all();
    }

    void run(in_port_t port, const SecureDht::Identity identity, StatusCallback cb, bool threaded = false)
    {
        if (running)
            return;
        if (rcv_thread.joinable())
            rcv_thread.join();
        statusCb = cb;
        running = true;
        doRun(port, identity);
        if (!threaded)
            return;
        dht_thread = std::thread([this]() {
            while (running) {
                loop();
                std::mutex mtx = {};
                {
                    std::unique_lock<std::mutex> lk(mtx);
                    cv.wait_for(lk, std::chrono::seconds( tosleep ), [this]() {
                        if (!running) return true;
                        {
                            std::unique_lock<std::mutex> lck(sock_mtx);
                            if (!rcv.empty()) return true;
                        }
                        {
                            std::unique_lock<std::mutex> lck(dht_mtx);
                            if (!dht_gets.empty() || !dht_puts.empty() || !bootstrap_nodes.empty())
                                return true;
                        }
                        return false;
                    });
                }
            }
        });
    }

    void join() {
        //std::cout << "Dht::join()" << std::endl;
        running = false;
        cv.notify_all();
        if (dht_thread.joinable()) {
            dht_thread.join();
        }
        if (rcv_thread.joinable())
            rcv_thread.join();
    }

    InfoHash getId() const {
        return dht->getId();
    }

    std::vector<Dht::NodeExport> getNodes() {
        std::lock(dht_mtx, sock_mtx);
        return dht->getNodes();
    }

    bool isRunning() const {
        return running;
    }

    void loop()
    {
        int rc;
        time_t tosl;
        {
            std::unique_lock<std::mutex> lck(sock_mtx);
            if (rcv.size()) {
                for (const auto& pck : rcv) {
                    auto& buf = pck.first;
                    auto& from = pck.second;
                    rc = dht->periodic(buf.data(), buf.size()-1, (sockaddr*)&from, from.ss_family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6), &tosl);
                    if (rc < 0 && (rc == EINVAL || rc == EFAULT)) {
                        break;
                    }
                }
                if (rc < 0 && (rc == EINVAL || rc == EFAULT))
                    return;
                rcv.clear();
            } else {
                rc = dht->periodic(nullptr, 0, nullptr, 0, &tosl);
            }
        }
        tosleep = tosl;

        {
            std::unique_lock<std::mutex> lck(dht_mtx);

            for (auto& get : dht_gets) {
                std::cout << "Processing get (" <<  std::get<0>(get) << ")" << std::endl;
                dht->get(std::get<0>(get), std::get<1>(get), std::get<2>(get));
            }
            dht_gets.clear();

            for (auto& put : dht_eputs) {
                auto& id = std::get<0>(put);
                auto& val = std::get<2>(put);
                std::cout << "Processing encrypted put from " << id << " to " << std::get<1>(put) << " -> " << val << std::endl;
                dht->putEncrypted(id, std::get<1>(put), val, std::get<3>(put));
            }
            dht_eputs.clear();

            for (auto& put : dht_puts) {
                auto& id = std::get<0>(put);
                auto& val = std::get<1>(put);
                std::cout << "Processing put " << id << " -> " << val << std::endl;
                if (val.type == Value::Type::Peer)
                    dht->put(id, val, std::get<2>(put));
                else
                    dht->putSigned(id, val, std::get<2>(put));
            }
            dht_puts.clear();

            for (auto& node : bootstrap_nodes)
                dht->insertNode(node);
            bootstrap_nodes.clear();

            for (auto& node : bootstrap_ips) {
                dht->pingNode((sockaddr*)&node, sizeof(node));
                std::this_thread::sleep_for( std::chrono::microseconds(/*rand_delay()*/ 10) );
            }
            bootstrap_ips.clear();
        }

        if (statusCb) {
            Dht::Status nstatus4 = dht->getStatus(AF_INET);
            Dht::Status nstatus6 = dht->getStatus(AF_INET6);
            if (nstatus4 != status4 || nstatus6 != status6) {
                status4 = nstatus4;
                status6 = nstatus6;
                statusCb(status4, status6);
            }
        }
    }

private:

    void doRun(in_port_t port, const SecureDht::Identity identity)
    {
        dht.reset();

        int s = socket(PF_INET, SOCK_DGRAM, 0);
        int s6 = socket(PF_INET6, SOCK_DGRAM, 0);
        if(s >= 0) {
            sockaddr_in sin {
                .sin_family = AF_INET,
                .sin_port = htons(port)
            };
            int rc = bind(s, (sockaddr*)&sin, sizeof(sin));
            if(rc < 0)
                throw DhtException("Can't bind IPv4 socket");
        }
        if(s6 >= 0) {
            int val = 1;
            int rc = setsockopt(s6, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&val, sizeof(val));
            if(rc < 0) {
                perror("setsockopt(IPV6_V6ONLY)");
                return;
            }

            /* BEP-32 mandates that we should bind this socket to one of our
               global IPv6 addresses.  In this simple example, this only
               happens if the user used the -b flag. */
            sockaddr_in6 sin6 {
                .sin6_family = AF_INET6,
                .sin6_port = htons(port)
            };
            rc = bind(s6, (sockaddr*)&sin6, sizeof(sin6));
            if(rc < 0)
                throw DhtException("Can't bind IPv6 socket");
        }

        dht = std::unique_ptr<SecureDht>(new SecureDht {s, s6, identity});

        rcv_thread = std::thread([this,s,s6]() {
            std::mt19937 engine(std::random_device{}());
            auto rand_delay = std::bind(std::uniform_int_distribution<uint32_t>(0, 1000000), engine);
            try {
                while (true) {
                    uint8_t buf[4096 * 64];
                    sockaddr_storage from;
                    socklen_t fromlen;

                    struct timeval tv;
                    fd_set readfds;
                    tv.tv_sec = tosleep / 5;
                    tv.tv_usec = rand_delay();
                    //std::cout << "Dht::rcv_thread loop " << tv.tv_sec << "." << tv.tv_usec << std::endl;

                    FD_ZERO(&readfds);
                    if(s >= 0)
                        FD_SET(s, &readfds);
                    if(s6 >= 0)
                        FD_SET(s6, &readfds);
                    int rc = select(s > s6 ? s + 1 : s6 + 1, &readfds, NULL, NULL, &tv);
                    if(rc < 0) {
                        if(errno != EINTR) {
                            perror("select");
                            std::this_thread::sleep_for( std::chrono::seconds(1) );
                        }
                    }

                    if(!running)
                        break;

                    if(rc > 0) {
                        fromlen = sizeof(from);
                        if(s >= 0 && FD_ISSET(s, &readfds))
                            rc = recvfrom(s, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&from, &fromlen);
                        else if(s6 >= 0 && FD_ISSET(s6, &readfds))
                            rc = recvfrom(s6, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&from, &fromlen);
                        else
                            break;
                        if (rc > 0) {
                            buf[rc] = 0;
                            {
                                std::unique_lock<std::mutex> lck(sock_mtx);
                                rcv.emplace_back(Blob {buf, buf+rc+1}, from);
                            }
                            cv.notify_all();
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error int DHT networking thread: " << e.what() << std::endl;
            }
            if (s >= 0)
                close(s);
            if (s6 >= 0)
                close(s6);
        });
    }

    std::unique_ptr<SecureDht> dht {};

    Dht::Status status4 {Dht::Status::Disconnected},
                status6 {Dht::Status::Disconnected};
    StatusCallback statusCb {nullptr};

    std::thread rcv_thread {};
    std::mutex sock_mtx {};
    std::vector<std::pair<Blob, sockaddr_storage>> rcv {};
    std::atomic<time_t> tosleep {0};

    std::thread dht_thread {};
    std::mutex dht_mtx {};
    std::condition_variable cv {};

    // IPC temporary storage
    std::vector<std::tuple<InfoHash, Dht::GetCallback, Dht::DoneCallback>> dht_gets {};
    std::vector<std::tuple<InfoHash, Value, Dht::DoneCallback>> dht_puts {};
    std::vector<std::tuple<InfoHash, InfoHash, Value, Dht::DoneCallback>> dht_eputs {};
    std::vector<sockaddr_storage> bootstrap_ips {};
    std::vector<Dht::NodeExport> bootstrap_nodes {};

    std::atomic<bool> running {false};
};

}
