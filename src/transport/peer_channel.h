#include <mutex>
#include <sstream>

#include "logger.h"

namespace jami {

class PeerChannel
{
public:
    PeerChannel() {}
    ~PeerChannel() {
        stop();
    }
    PeerChannel(PeerChannel&& o) {
        std::lock_guard<std::mutex> lk(o.mutex_);
        stream_ = std::move(o.stream_);
        stop_ = o.stop_;
    }

    ssize_t isDataAvailable() {
        std::lock_guard<std::mutex> lk{mutex_};
        auto pos = stream_.tellg();
        stream_.seekg(0, std::ios_base::end);
        auto available = (stream_.tellg() - pos);
        stream_.seekg(pos);
        return available;
    }

    template <typename Duration>
    bool wait(Duration timeout) {
        std::unique_lock<std::mutex> lk {mutex_};
        return cv_.wait_for(lk, timeout, [this]{ return stop_ or !stream_.eof(); });
    }

    std::size_t read(char* output, std::size_t size) {
        std::unique_lock<std::mutex> lk {mutex_};
        cv_.wait(lk, [&, this]{
                if (stop_)
                    return true;
                stream_.read(&output[0], size);
                return stream_.gcount() > 0;
            });
        return stop_ ? 0 : stream_.gcount();
    }

    void write(const char* data, std::size_t size) {
        JAMI_WARN("PeerChannel write %zu", size);
        std::lock_guard<std::mutex> lk {mutex_};
        stream_.write(data, size);
        cv_.notify_one();
    }

    void stop() noexcept {
        //{
            std::lock_guard<std::mutex> lk {mutex_};
            if (stop_)
                return;
            stop_ = true;
        //}
        cv_.notify_all();

        // Make sure that no thread is blocked into read() or wait() methods
        //std::lock_guard<std::mutex> lk_api {apiMutex_};
    }

private:
    PeerChannel(const PeerChannel& o) = delete;
    PeerChannel& operator=(const PeerChannel& o) = delete;
    PeerChannel& operator=(PeerChannel&& o) = delete;

    std::mutex mutex_ {};
    std::condition_variable cv_ {};
    std::stringstream stream_ {};
    bool stop_ {false};

    friend void operator <<(std::vector<char>&, PeerChannel&);
};

}
