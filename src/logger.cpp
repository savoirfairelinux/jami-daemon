/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/compile.h>

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

namespace jami {

static constexpr auto ENDL = '\n';

#ifndef __GLIBC__
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
#endif

void
strErr()
{
#ifdef __GLIBC__
    JAMI_ERR("%m");
#else
    char buf[1000];
    JAMI_ERR("%s", check_error(strerror_r(errno, buf, sizeof(buf)), buf));
#endif
}

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
#ifdef __linux__
    auto tid = syscall(__NR_gettid) & 0xffff;
#else
    auto tid = std::this_thread::get_id();
#endif // __linux__

    unsigned int secs, milli;
    struct timeval tv;
    if (!gettimeofday(&tv, NULL)) {
        secs = tv.tv_sec;
        milli = tv.tv_usec / 1000; // suppose that milli < 1000
    } else {
        secs = time(NULL);
        milli = 0;
    }

    if (file) {
        return fmt::format(FMT_COMPILE("[{: >3d}.{:0<3d}|{: >4}|{: <24s}:{: <4d}] "),
                           secs,
                           milli,
                           tid,
                           stripDirName(file),
                           line);
    } else {
        return fmt::format(FMT_COMPILE("[{: >3d}.{:0<3d}|{: >4}] "), secs, milli, tid);
    }
}

std::string
formatPrintfArgs(const char* format, va_list ap)
{
    std::string ret;
    /* A good guess of what we might encounter. */
    static constexpr size_t default_buf_size = 80;

    ret.resize(default_buf_size);

    /* Necessary if we don't have enough space in buf. */
    va_list cp;
    va_copy(cp, ap);

    int size = vsnprintf(ret.data(), ret.size(), format, ap);

    /* Not enough space?  Well try again. */
    if ((size_t) size >= ret.size()) {
        ret.resize(size + 1);
        vsnprintf((char*) ret.data(), ret.size(), format, cp);
    }

    ret.resize(size);

    va_end(cp);

    return ret;
}

struct Logger::Msg
{
    Msg() = delete;

    Msg(int level, const char* file, int line, bool linefeed, std::string&& message)
        : payload_(std::move(message))
        , header_(contextHeader(file, line))
        , level_(level)
        , linefeed_(linefeed)
    {}

    Msg(int level, const char* file, int line, bool linefeed, const char* fmt, va_list ap)
        : payload_(formatPrintfArgs(fmt, ap))
        , header_(contextHeader(file, line))
        , level_(level)
        , linefeed_(linefeed)
    {}

    Msg(Msg&& other)
    {
        payload_ = std::move(other.payload_);
        header_ = std::move(other.header_);
        level_ = other.level_;
        linefeed_ = other.linefeed_;
    }

    std::string payload_;
    std::string header_;
    int level_;
    bool linefeed_;
};

class Logger::Handler
{
public:
    virtual ~Handler() = default;

    virtual void consume(Msg& msg) = 0;

    void enable(bool en) { enabled_.store(en, std::memory_order_relaxed); }
    bool isEnable() { return enabled_.load(std::memory_order_relaxed); }

private:
    std::atomic_bool enabled_ {false};
};

class ConsoleLog : public Logger::Handler
{
public:
    static ConsoleLog& instance()
    {
        // Intentional memory leak:
        // Some thread can still be logging even during static destructors.
        static ConsoleLog* self = new ConsoleLog();
        return *self;
    }

#ifdef _WIN32
    void printLogImpl(jami::Logger::Msg& msg, bool with_color)
    {
        // If we are using Visual Studio, we can use OutputDebugString to print
        // to the "Output" window. Otherwise, we just use fputs to stderr.
        static std::function<void(const char* str)> fputsFunc = [](const char* str) {
            fputs(str, stderr);
        };
        static std::function<void(const char* str)> outputDebugStringFunc = [](const char* str) {
            OutputDebugStringA(str);
        };
        static std::function<void()> putcFunc = []() {
            putc(ENDL, stderr);
        };
        // These next two functions will be used to print the message and line ending.
        static auto printFunc = IsDebuggerPresent() ? outputDebugStringFunc : fputsFunc;
        static auto endlFunc = IsDebuggerPresent() ? []() { OutputDebugStringA("\n"); } : putcFunc;

        WORD saved_attributes;
        static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (with_color) {
            static WORD color_header = CYAN;
            WORD color_prefix = LIGHT_GREEN;
            CONSOLE_SCREEN_BUFFER_INFO consoleInfo;

            switch (msg.level_) {
            case LOG_ERR:
                color_prefix = RED;
                break;

            case LOG_WARNING:
                color_prefix = YELLOW;
                break;
            }

            GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
            saved_attributes = consoleInfo.wAttributes;
            SetConsoleTextAttribute(hConsole, color_header);

            printFunc(msg.header_.c_str());

            SetConsoleTextAttribute(hConsole, saved_attributes);
            SetConsoleTextAttribute(hConsole, color_prefix);
        } else {
            printFunc(msg.header_.c_str());
        }

        printFunc(msg.payload_.c_str());

        if (msg.linefeed_) {
            endlFunc();
        }

        if (with_color) {
            SetConsoleTextAttribute(hConsole, saved_attributes);
        }
    }
#else
    void printLogImpl(jami::Logger::Msg& msg, bool with_color)
    {
        if (with_color) {
            const char* color_header = CYAN;
            const char* color_prefix = "";

            switch (msg.level_) {
            case LOG_ERR:
                color_prefix = RED;
                break;

            case LOG_WARNING:
                color_prefix = YELLOW;
                break;
            }

            fputs(color_header, stderr);
            fputs(msg.header_.c_str(), stderr);
            fputs(END_COLOR, stderr);
            fputs(color_prefix, stderr);
        } else {
            fputs(msg.header_.c_str(), stderr);
        }

        fputs(msg.payload_.c_str(), stderr);

        if (msg.linefeed_) {
            putc(ENDL, stderr);
        }

        if (with_color) {
            fputs(END_COLOR, stderr);
        }
    }
#endif /* _WIN32 */

    virtual void consume(jami::Logger::Msg& msg) override
    {
        static bool with_color = !(getenv("NO_COLOR") || getenv("NO_COLORS") || getenv("NO_COLOUR")
                                   || getenv("NO_COLOURS"));

        printLogImpl(msg, with_color);
    }
};

void
Logger::setConsoleLog(bool en)
{
    ConsoleLog::instance().enable(en);
#ifdef _WIN32
    static WORD original_attributes;
    if (en) {
        if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
            FILE *fpstdout = stdout, *fpstderr = stderr;
            freopen_s(&fpstdout, "CONOUT$", "w", stdout);
            freopen_s(&fpstderr, "CONOUT$", "w", stderr);
            // Save the original state of the console window(in case AttachConsole worked).
            CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
            GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &consoleInfo);
            original_attributes = consoleInfo.wAttributes;
            SetConsoleCP(CP_UTF8);
            SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE),
                           ENABLE_QUICK_EDIT_MODE | ENABLE_EXTENDED_FLAGS);
        }
    } else {
        // Restore the original state of the console window in case we attached.
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), original_attributes);
        FreeConsole();
    }
#endif
}

class SysLog : public Logger::Handler
{
public:
    static SysLog& instance()
    {
        // Intentional memory leak:
        // Some thread can still be logging even during static destructors.
        static SysLog* self = new SysLog();
        return *self;
    }

    SysLog()
    {
#ifdef _WIN32
        ::openlog(LOGFILE, WINLOG_PID, WINLOG_MAIL);
#else
#ifndef __ANDROID__
        ::openlog(LOGFILE, LOG_NDELAY, LOG_USER);
#endif
#endif /* _WIN32 */
    }

    virtual void consume(Logger::Msg& msg) override
    {
#ifdef __ANDROID__
        __android_log_print(msg.level_, APP_NAME, "%s%s", msg.header_.c_str(), msg.payload_.c_str());
#else
        ::syslog(msg.level_, "%.*s", (int) msg.payload_.size(), msg.payload_.data());
#endif
    }
};

void
Logger::setSysLog(bool en)
{
    SysLog::instance().enable(en);
}

class MonitorLog : public Logger::Handler
{
public:
    static MonitorLog& instance()
    {
        // Intentional memory leak
        // Some thread can still be logging even during static destructors.
        static MonitorLog* self = new MonitorLog();
        return *self;
    }

    virtual void consume(jami::Logger::Msg& msg) override
    {
        /*
         * TODO - Maybe change the MessageSend sigature to avoid copying
         * of message payload?
         */
        auto tmp = msg.header_ + msg.payload_;

        jami::emitSignal<libjami::ConfigurationSignal::MessageSend>(tmp);
    }
};

void
Logger::setMonitorLog(bool en)
{
    MonitorLog::instance().enable(en);
}

class FileLog : public Logger::Handler
{
public:
    static FileLog& instance()
    {
        // Intentional memory leak:
        // Some thread can still be logging even during static destructors.
        static FileLog* self = new FileLog();
        return *self;
    }

    void setFile(const std::string& path)
    {
        if (thread_.joinable()) {
            notify([this] { enable(false); });
            thread_.join();
        }

        std::ofstream file;
        if (not path.empty()) {
            file.open(path, std::ofstream::out | std::ofstream::app);
            enable(true);
        } else {
            enable(false);
            return;
        }

        thread_ = std::thread([this, file = std::move(file)]() mutable {
            std::vector<Logger::Msg> pendingQ_;
            while (isEnable()) {
                {
                    std::unique_lock lk(mtx_);
                    cv_.wait(lk, [&] { return not isEnable() or not currentQ_.empty(); });
                    if (not isEnable())
                        break;

                    std::swap(currentQ_, pendingQ_);
                }

                do_consume(file, pendingQ_);
                pendingQ_.clear();
            }
        });
    }

    ~FileLog()
    {
        notify([=] { enable(false); });
        if (thread_.joinable())
            thread_.join();
    }

    virtual void consume(Logger::Msg& msg) override
    {
        notify([&, this] { currentQ_.emplace_back(std::move(msg)); });
    }

private:
    template<typename T>
    void notify(T func)
    {
        std::lock_guard lk(mtx_);
        func();
        cv_.notify_one();
    }

    void do_consume(std::ofstream& file, const std::vector<Logger::Msg>& messages)
    {
        for (const auto& msg : messages) {
            file << msg.header_ << msg.payload_;

            if (msg.linefeed_)
                file << ENDL;
        }
        file.flush();
    }

    std::vector<Logger::Msg> currentQ_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::thread thread_;
};

void
Logger::setFileLog(const std::string& path)
{
    FileLog::instance().setFile(path);
}

LIBJAMI_PUBLIC void
Logger::log(int level, const char* file, int line, bool linefeed, const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);

    vlog(level, file, line, linefeed, fmt, ap);

    va_end(ap);
}

template<typename T>
void
log_to_if_enabled(T& handler, Logger::Msg& msg)
{
    if (handler.isEnable()) {
        handler.consume(msg);
    }
}

static std::atomic_bool debugEnabled_ {false};

void
Logger::setDebugMode(bool enable)
{
    debugEnabled_.store(enable, std::memory_order_relaxed);
}

bool
Logger::debugEnabled()
{
    return debugEnabled_.load(std::memory_order_relaxed);
}

void
Logger::vlog(int level, const char* file, int line, bool linefeed, const char* fmt, va_list ap)
{
    if (level < LOG_WARNING and not debugEnabled_.load(std::memory_order_relaxed)) {
        return;
    }

    if (not(ConsoleLog::instance().isEnable() or SysLog::instance().isEnable()
            or MonitorLog::instance().isEnable() or FileLog::instance().isEnable())) {
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
Logger::write(int level, const char* file, int line, std::string&& message)
{
    /* Timestamp is generated here. */
    Msg msg(level, file, line, true, std::move(message));

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

#ifdef _WIN32
    Logger::setConsoleLog(false);
#endif /* _WIN32 */
}

} // namespace jami
