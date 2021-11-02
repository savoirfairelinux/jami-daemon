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
#include <condition_variable>
#include <functional>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <ios>
#include <mutex>
#include <thread>
#include <array>

#include "fileutils.h"
#include "logger.h"

#ifdef __linux__
#include <unistd.h>
#include <syslog.h>
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

static constexpr auto ENDL = '\n';

// extract the last component of a pathname (extract a filename from its dirname)
static const char*
stripDirName(const char* path)
{
    const char* occur = strrchr(path, DIR_SEPARATOR_CH);

    return occur ? occur + 1 : path;
}

static std::string
contextHeader(const char* const file, int line)
{
    static char* timestamp_fmt = getenv("JAMI_TIMESTAMP_FMT");

#ifdef __linux__
    auto tid = syscall(__NR_gettid) & 0xffff;
#else
    auto tid = std::this_thread::get_id();
#endif // __linux__

    std::ostringstream out;

    out << '[';

    // Timestamp
    if (timestamp_fmt) {

        time_t t;
        struct tm tm;
        char buf[128];

        time(&t);

#ifdef _WIN32
        /* NOTE!  localtime(3) is MT-Safe on win32 */
        tm = *localtime(&t);
#else
        localtime_r(&t, &tm);
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
        strftime(buf, sizeof(buf), timestamp_fmt, &tm);
#pragma GCC diagnostic pop

        out << buf;

    } else {

        unsigned int secs, milli;
        struct timeval tv;

        if (!gettimeofday(&tv, NULL)) {
            secs = tv.tv_sec;
            milli = tv.tv_usec / 1000; // suppose that milli < 1000
        } else {
            secs = time(NULL);
            milli = 0;
        }

        const auto prev_fill = out.fill();

        out << secs << '.' << std::right << std::setw(3) << std::setfill('0') << milli
            << std::left << '|' << std::right << std::setw(5) << std::setfill(' ') << tid << std::left;
        out.fill(prev_fill);
    }

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

struct BufDeleter
{
    void operator()(char* ptr)
    {
        if (ptr) {
            free(ptr);
        }
    }
};

struct Logger::Msg
{
    Msg() = delete;

    Msg(int level, const char* file, int line, bool linefeed, const char* fmt, va_list ap)
        : header_(contextHeader(file, line))
        , level_(level)
        , linefeed_(linefeed)
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

    Msg(Msg&& other)
    {
        payload_ = std::move(other.payload_);
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
    bool isEnable() { return enabled_.load(); }

private:
    std::atomic<bool> enabled_;
};

class ConsoleLog : public jami::Logger::Handler
{
public:
    static ConsoleLog& instance()
    {
        // This is an intentional memory leak!!!
        //
        // Some thread can still be logging after DRing::fini and even
        // during the static destructors called by libstdc++.  Thus, we
        // allocate the logger on the heap and never free it.
        static ConsoleLog* self = new ConsoleLog();

        return *self;
    }

    virtual void consume(jami::Logger::Msg& msg) override
    {
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

void
Logger::setConsoleLog(bool en)
{
    ConsoleLog::instance().enable(en);
}

class SysLog : public jami::Logger::Handler
{
public:
    static SysLog& instance()
    {
        // This is an intentional memory leak!!!
        //
        // Some thread can still be logging after DRing::fini and even
        // during the static destructors called by libstdc++.  Thus, we
        // allocate the logger on the heap and never free it.
        static SysLog* self = new SysLog();

        return *self;
    }

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
        __android_log_print(msg.level_, APP_NAME, "%s%s", msg.header_.c_str(), msg.payload_.get());
#else
        ::syslog(msg.level_, "%s", msg.payload_.get());
#endif
    }

private:
    std::mutex mtx_;
};

void
Logger::setSysLog(bool en)
{
    SysLog::instance().enable(en);
}

class MonitorLog : public jami::Logger::Handler
{
public:
    static MonitorLog& instance()
    {
        // This is an intentional memory leak!!!
        //
        // Some thread can still be logging after DRing::fini and even
        // during the static destructors called by libstdc++.  Thus, we
        // allocate the logger on the heap and never free it.
        static MonitorLog* self = new MonitorLog();

        return *self;
    }

    virtual void consume(jami::Logger::Msg& msg) override
    {
        /*
         * TODO - Maybe change the MessageSend sigature to avoid copying
         * of message payload?
         */
        auto tmp = msg.header_ + std::string(msg.payload_.get());

        jami::emitSignal<DRing::ConfigurationSignal::MessageSend>(tmp);
    }
};

void
Logger::setMonitorLog(bool en)
{
    MonitorLog::instance().enable(en);
}

class FileLog : public jami::Logger::Handler
{
public:
    static FileLog& instance()
    {
        // This is an intentional memory leak!!!
        //
        // Some thread can still be logging after DRing::fini and even
        // during the static destructors called by libstdc++.  Thus, we
        // allocate the logger on the heap and never free it.
        static FileLog* self = new FileLog();

        return *self;
    }

    void setFile(const std::string& path)
    {
        if (thread_.joinable()) {
            notify([this] { enable(false); });
            thread_.join();
        }

        if (file_.is_open()) {
            file_.close();
        }

        if (not path.empty()) {
            file_.open(path, std::ofstream::out | std::ofstream::app);
            enable(true);
        } else {
            enable(false);
            return;
        }

        thread_ = std::thread([this] {
            while (isEnable()) {

                {
                    std::unique_lock lk(mtx_);

                    cv_.wait(lk, [&] { return not isEnable() or not currentQ_.empty(); });

                    if (not isEnable()) {
                        break;
                    }

                    std::swap(currentQ_, pendingQ_);
                }

                do_consume(pendingQ_);
                pendingQ_.clear();
            }
        });
    }

    ~FileLog()
    {
        notify([=] { enable(false); });

        if (thread_.joinable()) {
            thread_.join();
        }
    }

    virtual void consume(jami::Logger::Msg& msg) override
    {
        notify([&, this] { currentQ_.push_back(std::move(msg)); });
    }

private:
    template<typename T>
    void notify(T func)
    {
        std::unique_lock lk(mtx_);
        func();
        cv_.notify_one();
    }

    void do_consume(const std::vector<jami::Logger::Msg>& messages)
    {
        for (const auto& msg : messages) {
            file_ << msg.header_ << msg.payload_.get();

            if (msg.linefeed_) {
                file_ << ENDL;
            }
        }

        file_.flush();
    }

    std::vector<jami::Logger::Msg> currentQ_;
    std::vector<jami::Logger::Msg> pendingQ_;
    std::mutex mtx_;
    std::condition_variable cv_;

    std::ofstream file_;
    std::thread thread_;
};

void
Logger::setFileLog(const std::string& path)
{
    FileLog::instance().setFile(path);
}

void
Logger::log(int level, const char* file, int line, bool linefeed, const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);

    vlog(level, file, line, linefeed, fmt, ap);

    va_end(ap);
}

template<typename T>
void log_to_if_enabled(T& handler, Logger::Msg& msg)
{
    if (handler.isEnable()) {
        handler.consume(msg);
    }
}

static std::atomic_bool debugEnabled{false};

void
Logger::setDebugMode(bool enable)
{
    debugEnabled.store(enable);
}

void
Logger::vlog(int level, const char* file, int line, bool linefeed, const char* fmt, va_list ap)
{
    if (not debugEnabled.load() and
        level < LOG_WARNING) {
        return;
    }

    if (not(ConsoleLog::instance().isEnable() or
            SysLog::instance().isEnable() or
            MonitorLog::instance().isEnable() or
            FileLog::instance().isEnable())) {
        return;
    }

    /* Timestamp is generated here. */
    Msg msg(level, file, line, linefeed, fmt, ap);

    log_to_if_enabled(ConsoleLog::instance(), msg);
    log_to_if_enabled(SysLog::instance(), msg);
    log_to_if_enabled(MonitorLog::instance(), msg);
    log_to_if_enabled(FileLog::instance(), msg); // Takes ownership of msg if enabled
}

void
Logger::fini()
{
    // Force close on file and join thread
    FileLog::instance().setFile({});
}

} // namespace jami
