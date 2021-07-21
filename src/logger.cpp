/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
 *
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <ciso646> // fix windows compiler bug

#include "client/ring_signal.h"

#ifdef _MSC_VER
#include <sys_time.h>
#else
#include <sys/time.h>
#endif

#include <atomic>
#include <functional>
#include <string>
#include <sstream>
#include <iomanip>
#include <ios>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <array>

#include <opendht/thread_pool.h>

#include "logger.h"

#include <syslog.h>

#ifdef __linux__
#include <unistd.h>
#include <sys/syscall.h>
#endif // __linux__

#ifdef __ANDROID__
#ifndef APP_NAME
#define APP_NAME "libjami"
#endif /* APP_NAME */
#endif

#define BLACK         "\033[22;30m"
#define GREEN         "\033[22;32m"
#define BROWN         "\033[22;33m"
#define BLUE          "\033[22;34m"
#define MAGENTA       "\033[22;35m"
#define GREY          "\033[22;37m"
#define DARK_GREY     "\033[01;30m"
#define LIGHT_RED     "\033[01;31m"
#define LIGHT_SCREEN  "\033[01;32m"
#define LIGHT_BLUE    "\033[01;34m"
#define LIGHT_MAGENTA "\033[01;35m"
#define LIGHT_CYAN    "\033[01;36m"
#define WHITE         "\033[01;37m"
#define END_COLOR     "\033[0m"

#ifndef _WIN32
#define RED    "\033[22;31m"
#define YELLOW "\033[01;33m"
#define CYAN   "\033[22;36m"
#else
#define FOREGROUND_WHITE 0x000f
#define RED              FOREGROUND_RED + 0x0008
#define YELLOW           FOREGROUND_RED + FOREGROUND_GREEN + 0x0008
#define CYAN             FOREGROUND_BLUE + FOREGROUND_GREEN + 0x0008
#define LIGHT_GREEN      FOREGROUND_GREEN + 0x0008
#endif // _WIN32

#define LOGFILE "jami"

#ifdef RING_UWP
static constexpr auto ENDL = '\0';
#else
static constexpr auto ENDL = '\n';
#endif

// extract the last component of a pathname (extract a filename from its dirname)
static const char*
stripDirName(const char* path)
{
#ifdef _MSC_VER
    static constexpr auto path_separator = '\\';
#else
    static constexpr auto path_separator = '/';
#endif

    const char* occur = strrchr(path, path_separator);

    return  occur ? occur + 1 : path;
}

static std::string
contextHeader(const char* const file, int line)
{
#ifdef __linux__
    auto tid = syscall(__NR_gettid) & 0xffff;
#else
    auto tid = std::this_thread::get_id();
#endif // __linux__
    // Timestamp
    unsigned int secs, milli;
    struct timeval tv;

    if (!gettimeofday(&tv, NULL)) {
        secs = tv.tv_sec;
        milli = tv.tv_usec / 1000; // suppose that milli < 1000
    } else {
        secs = time(NULL);
        milli = 0;
    }

    std::ostringstream out;
    const auto prev_fill = out.fill();
    out << '[' << secs << '.' << std::right << std::setw(3) << std::setfill('0') << milli
        << std::left << '|' << std::right << std::setw(5) << std::setfill(' ') << tid << std::left;
    out.fill(prev_fill);

    // Context
    if (file) {
#ifdef RING_UWP
        constexpr auto width = 26;
#else
        constexpr auto width = 18;
#endif
        out << "|" << std::setw(width) << stripDirName(file) << ":" << std::setw(5)
            << std::setfill(' ') << line;
    }

    out << "] ";
    return out.str();
}

static const char*
check_error(int result, char* buffer)
{
    switch (result) {
    case 0:
        return buffer;

    case ERANGE: /* should never happen */
        return "unknown (too big to display)";

    default:
        return "unknown (invalid error number)";
    }
}

static const char*
check_error(char* result, char*)
{
    return result;
}

void
strErr(void)
{
#ifdef __GLIBC__
    JAMI_ERR("%m");
#else
    char buf[1000];
    JAMI_ERR("%s", check_error(strerror_r(errno, buf, sizeof(buf)), buf));
#endif
}

namespace jami {

struct BufDeleter {
    void operator()(char *ptr) {
        if (ptr) {
            free(ptr);
        }
    }
};

struct Logger::Msg
{
    Msg() = delete;

    Msg(int level, const char* file, int line, bool linefeed, const char* fmt, va_list ap)
        : header_(contextHeader(file, line)),
          level_(level),
          linefeed_(linefeed)
        {
            /* A good guess of what we might encounter. */
            static constexpr size_t default_buf_size = 80;

            char* buf = (char*) malloc(default_buf_size);
            int buf_size = default_buf_size;
            va_list cp;

            /* Necessary if we don't have enough space in buf. */
            va_copy(cp, ap);

            int size = vsnprintf(buf, buf_size, fmt, ap);

            /* Not enough space?  Well try again. */
            if (size >= buf_size) {
                buf_size = size + 1;
                buf = (char*) realloc(buf, buf_size);
                vsnprintf(buf, buf_size, fmt, cp);
            }

            payload_.reset(buf);

            va_end(cp);
        }

    Msg(Msg&& other) {
        payload_.reset(other.payload_.release());
        header_ = std::move(other.header_);
        level_ = other.level_;
        linefeed_ = other.linefeed_;
    }

    std::unique_ptr<char, BufDeleter> payload_;
    std::string header_;
    int level_;
    bool linefeed_;
};

class Logger::Handler
{
public:
    virtual ~Handler() = default;

    virtual void consume(Msg& msg) = 0;

    void enable(bool en) { enabled_.store(en); }
    bool enable() { return enabled_.load(); }

private:
    std::atomic<bool> enabled_;
};

static std::shared_ptr<Logger::Handler> consoleHandler;
static std::shared_ptr<Logger::Handler> syslogHandler;
static std::shared_ptr<Logger::Handler> monitorHandler;
static std::shared_ptr<Logger::Handler> fileHandler;

void
Logger::setConsoleLog(bool en)
{
    class ConsoleLog : public jami::Logger::Handler
    {
    public:
        virtual void consume(jami::Logger::Msg& msg) override
        {
            /* TODO - This is ugly.  Refactor with portable wrappers. */
#ifndef _WIN32
            const char* color_header = CYAN;
            const char* color_prefix = "";
#else
            WORD color_prefix = LIGHT_GREEN;
            WORD color_header = CYAN;
#endif
#if defined(_WIN32) && !defined(RING_UWP)
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
            WORD saved_attributes;
#endif

            switch (msg.level_) {
            case LOG_ERR:
                color_prefix = RED;
                break;

            case LOG_WARNING:
                color_prefix = YELLOW;
                break;
            }

#ifndef _WIN32
            fputs(color_header, stderr);
#elif !defined(RING_UWP)
            GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
            saved_attributes = consoleInfo.wAttributes;
            SetConsoleTextAttribute(hConsole, color_header);
#endif
            fputs(msg.header_.c_str(), stderr);
#ifndef _WIN32
            fputs(END_COLOR, stderr);
            fputs(color_prefix, stderr);
#elif !defined(RING_UWP)
            SetConsoleTextAttribute(hConsole, saved_attributes);
            SetConsoleTextAttribute(hConsole, color_prefix);
#endif

            fputs(msg.payload_.get(), stderr);

            if (msg.linefeed_) {
                putc(ENDL, stderr);
            }

#ifndef _WIN32
            fputs(END_COLOR, stderr);
#elif !defined(RING_UWP)
            SetConsoleTextAttribute(hConsole, saved_attributes);
#endif
        }
    };

    if (not consoleHandler) {
        consoleHandler = std::make_shared<ConsoleLog>();
    }

    consoleHandler->enable(en);
}

void
Logger::setSysLog(bool en)
{
    class SysLog : public jami::Logger::Handler
    {
    public:
        SysLog()
        {
#ifdef _WIN32
            ::openlog(LOGFILE, WINLOG_PID, WINLOG_MAIL);
#else
            ::openlog(LOGFILE, LOG_NDELAY, LOG_USER);
#endif /* _WIN32 */
        }

        virtual void consume(jami::Logger::Msg& msg) override
        {
            // syslog is supposed to thread-safe, but not all implementations (Android?)
            // follow strictly POSIX rules... so we lock our mutex in any cases.
            std::lock_guard<std::mutex> lk {mtx_};
#ifdef __ANDROID__
            __android_log_buf_print(msg.level_, APP_NAME, "%s%s", msg_.header, msg_.payload);
#else
            ::syslog(msg.level_, "%s", msg.payload_.get());
#endif
        }

    private:
        std::mutex mtx_;
    };

    if (not syslogHandler) {
        syslogHandler = std::make_shared<SysLog>();
    }

    syslogHandler->enable(en);
}

void
Logger::setMonitorLog(bool en)
{
    class MonitorLog : public jami::Logger::Handler
    {
    public:
        virtual void consume(jami::Logger::Msg& msg) override
        {
            /*
             * TODO - Maybe change the MessageSend sigature to avoid copying
             * of message payload?
             */
            auto tmp = msg.header_ + std::string(msg.payload_.get());

            if (msg.linefeed_) {
                tmp += ENDL;
            }

            jami::emitSignal<DRing::ConfigurationSignal::MessageSend>(tmp);
        }
    };

    if (not monitorHandler) {
        monitorHandler = std::make_shared<MonitorLog>();
    }

    monitorHandler->enable(en);
}

void
Logger::setFileLog(const char* path)
{
    class FileLog : public jami::Logger::Handler
    {
    public:

        void setFile(const char* path)
        {

            if (thread_.joinable()) {
                enable(false);
                cv_.notify_one();
                thread_.join();
            }

            if (file_) {
                fclose(file_);
                file_ = nullptr;
            }

            if (not path) {
                enable(false);
                return;
            }

            file_ = fopen(path, "a");

            enable(true);

            thread_ = std::thread([this]{

                while (enable()) {

                    std::mutex dummy;
                    std::unique_lock lk(dummy);

                    cv_.wait(lk);

                    /* Atomic swap of queues */
                    with_messages_queues([this] {
                        std::swap(currentQ_, pendingQ_);
                    });

                    do_consume(*pendingQ_);
                    pendingQ_->clear();
                }
            });
        }

        ~FileLog()
        {
            enable(false);
            cv_.notify_one();

            if (thread_.joinable()) {
                thread_.join();
            }

            if (file_) {
                fclose(file_);
            }
        }

        virtual void consume(jami::Logger::Msg& msg) override
        {
            /*
             * We want to avoid mutex contingency here since there can be multiple
             * writers at the same time.  The queue's sizes will amortized over
             * time, thus the push into the queue is essentially a memcpy + increment
             * of index, which can be done under a spinlock.
             *
             * The only problem with this method is that the timestamps might be a
             * little out of order.  But we don't want to make a system call inside this
             * region, so it's okay.
             *
             * We also move the message to avoid double free of payload.
             */
            with_messages_queues([&] {
                currentQ_->push_back(std::move(msg));
            });

            cv_.notify_one();
        }

    private:

        void do_consume(const std::vector<jami::Logger::Msg>& messages) {

            for (const auto& msg : messages) {
                fputs(msg.header_.c_str(), file_);
                fputs(msg.payload_.get(), file_);

                if (msg.linefeed_) {
                    fputc(ENDL, file_);
                }
            }
            fflush(file_);
        }

        /**
         * Execute @func with exclusivity of @current_queue and @pending_queue.
         */
        inline void
        with_messages_queues(std::function<void(void)>&& func) {
            while (spinlock_.test_and_set(std::memory_order_acquire)) {
                /* BURN CPU */
            }
            func();
            spinlock_.clear(std::memory_order_release);
        }

        /**
         * We're using two vectors here.  One used by the producers to push new
         * messages, one used by the only consumer to print the messages.
         *
         * When the consumer has finished to consume all message in its @pending_queue,
         * it swaps it with @current_queue.
         */
        std::vector<jami::Logger::Msg> messagesA_;
        std::vector<jami::Logger::Msg> messagesB_;

        std::vector<jami::Logger::Msg>* currentQ_ = &messagesA_;
        std::vector<jami::Logger::Msg>* pendingQ_ = &messagesB_;

        std::atomic_flag spinlock_ = ATOMIC_FLAG_INIT;

        std::condition_variable cv_;

        FILE* file_;
        std::thread thread_;
    };

    if (not fileHandler) {
        fileHandler = std::make_shared<FileLog>();
    }

    std::dynamic_pointer_cast<FileLog>(fileHandler)->setFile(path);
}

void
Logger::log(int level, const char* file, int line, bool linefeed, const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);

    vlog(level, file, line, linefeed, fmt, ap);

    va_end(ap);
}

static bool
canLog()
{
    return (consoleHandler->enable() or
            syslogHandler->enable()  or
            monitorHandler->enable() or
            fileHandler->enable());
}

void
Logger::vlog(int level, const char* file, int line, bool linefeed, const char* fmt, va_list ap)
{
    if (not canLog()) {
        return;
    }

    /* Timestamp is generated here. */
    Msg msg(level, file, line, linefeed, fmt, ap);

    auto log_to_if_enable = [&](std::shared_ptr<Handler> handler) {
        if (handler and handler->enable()) {
            handler->consume(msg);
        }
    };

    log_to_if_enable(consoleHandler);
    log_to_if_enable(syslogHandler);
    log_to_if_enable(monitorHandler);
    log_to_if_enable(fileHandler); // Takes ownership of msg if enabled
}

} // namespace jami
