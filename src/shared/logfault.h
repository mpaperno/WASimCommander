#pragma once

/*
MIT License

Copyright (c) 2018 - 2021 Jarle Aase

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Home: https://github.com/jgaa/logfault
*/
/*
Maxim Paperno changes Apr/May 2022:
 * Added FileHandler with rudimentary log rotations.
 * Added IdProxyHandler (passes numeric ID to callback).
 * Added ability to name handlers.
 * Added ability to change logging level of named handler.
 * Handlers can be removed and queried, by name.
 * Added LFLOG_HEX() macros.
 * Added LOGFAULT_LEVEL_NAMES macro.
 * Added LOGFAULT_USE_SHORT_LOG_MACROS option macro.
 * Added LOGFAULT_USE_THREADING option macro for omitting threading/mutex bits for
   environments which don't support it (LOGFAULT_THREAD_NAME must also be set).
*/

#ifndef _LOGFAULT_H
#define _LOGFAULT_H

#ifndef LOGFAULT_USE_THREADING
#   define LOGFAULT_USE_THREADING 1
#endif

#include <array>
#include <assert.h>
#include <chrono>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#if LOGFAULT_USE_THREADING
#include <mutex>
#include <thread>
#endif

#ifndef LOGFAULT_USE_UTCZONE
#   define LOGFAULT_USE_UTCZONE 0
#endif

#ifndef LOGFAULT_TIME_FORMAT
#   define LOGFAULT_TIME_FORMAT "%Y-%m-%d %H:%M:%S."
#endif

#ifndef LOGFAULT_TIME_PRINT_MILLISECONDS
#   define LOGFAULT_TIME_PRINT_MILLISECONDS 1
#endif

#ifndef LOGFAULT_TIME_PRINT_TIMEZONE
#   define LOGFAULT_TIME_PRINT_TIMEZONE 1
#endif

#ifndef LOGFAULT_TIME_CLOCK
#   define LOGFAULT_TIME_CLOCK  std::chrono::system_clock
#endif

#ifndef LOGFAULT_LEVEL_NAMES
#    define LOGFAULT_LEVEL_NAMES {"DISABLED", "ERROR", "WARNING", "NOTICE", "INFO", "DEBUG", "TRACE"}
#endif

#ifdef ERROR
#	undef ERROR
#endif


#ifndef LOGFAULT_LOCATION__
#   if defined(LOGFAULT_ENABLE_LOCATION) && LOGFAULT_ENABLE_LOCATION
#       define LOGFAULT_LOCATION__ << logfault::Handler::ShortenPath(__FILE__) << ':' << __LINE__ << " {" << __func__ << "} "
#       ifndef LOGFAULT_LOCATION_LEVELS
#           define LOGFAULT_LOCATION_LEVELS 3
#       endif
#   else
#       define LOGFAULT_LOCATION__
#   endif
#endif

// Internal implementation detail
#define LOGFAULT_LOG__(level) \
    ::logfault::LogManager::Instance().IsRelevant(level) && \
    ::logfault::Log(level).Line() LOGFAULT_LOCATION__

#define LFLOG_ERROR LOGFAULT_LOG__(logfault::LogLevel::ERROR)
#define LFLOG_WARN LOGFAULT_LOG__(logfault::LogLevel::WARN)
#define LFLOG_NOTICE LOGFAULT_LOG__(logfault::LogLevel::NOTICE)
#define LFLOG_INFO LOGFAULT_LOG__(logfault::LogLevel::INFO)
#define LFLOG_DEBUG LOGFAULT_LOG__(logfault::LogLevel::DEBUGGING)
#define LFLOG_TRACE LOGFAULT_LOG__(logfault::LogLevel::TRACE)

#ifdef LOGFAULT_ENABLE_ALL
#   define LFLOG_IFALL_ERROR(msg) LFLOG_ERROR << msg
#   define LFLOG_IFALL_WARN(msg) LFLOG_WARN << msg
#   define LFLOG_IFALL_NOTICE(msg) LFLOG_NOTICE << msg
#   define LFLOG_IFALL_INFO(msg) LFLOG_INFO << msg
#   define LFLOG_IFALL_DEBUG(msg) LFLOG_DEBUG << msg
#   define LFLOG_IFALL_TRACE(msg) LFLOG_TRACE << msg
# else
#   define LFLOG_IFALL_ERROR(msg)
#   define LFLOG_IFALL_WARN(msg)
#   define LFLOG_IFALL_NOTICE(msg)
#   define LFLOG_IFALL_INFO(msg)
#   define LFLOG_IFALL_DEBUG(msg)
#   define LFLOG_IFALL_TRACE(msg)
#endif

#define LFLOG_HEX(VAL, W, C)   std::hex << std::setw(W) << std::setfill('0') << std::C << VAL << std::resetiosflags(std::ios_base::basefield|std::ios_base::C|std::ios_base::left)
#define LFLOG_HEX2(VAL)        LFLOG_HEX(VAL, 2, uppercase)
#define LFLOG_HEX4(VAL)        LFLOG_HEX(VAL, 4, uppercase)
#define LFLOG_HEX8(VAL)        LFLOG_HEX(VAL, 8, uppercase)

#if defined(LOGFAULT_USE_SHORT_LOG_MACROS) && LOGFAULT_USE_SHORT_LOG_MACROS
#   define LOG_CRT   LFLOG_ERROR
#   define LOG_ERR   LFLOG_WARN
#   define LOG_WRN   LFLOG_NOTICE
#   define LOG_INF   LFLOG_INFO
#   define LOG_DBG   LFLOG_DEBUG
#   define LOG_TRC   LFLOG_TRACE
#   define LOG_HEX   LFLOG_HEX
#   define LOG_HEX2  LFLOG_HEX2
#   define LOG_HEX4  LFLOG_HEX4
#   define LOG_HEX8  LFLOG_HEX8
#endif

#ifndef LOGFAULT_THREAD_NAME
#   if defined (LOGFAULT_USE_THREAD_NAME)
#       include <pthread.h>
        inline const char *logfault_get_thread_name_() noexcept {
            thread_local std::array<char, 16> buffer = {};
            if (pthread_getname_np(pthread_self(), buffer.data(), buffer.size()) == 0) {
                return buffer.data();
            }
            return "err pthread_getname_np";
        }
#       define LOGFAULT_THREAD_NAME logfault_get_thread_name_()
#
#   elif defined(LOGFAULT_USE_TID_AS_NAME)
#       include <unistd.h>
#       include <sys/syscall.h>
#       define LOGFAULT_THREAD_NAME syscall(__NR_gettid)
#   else
#       define LOGFAULT_THREAD_NAME std::this_thread::get_id()
#   endif
#endif

// Enum class type generic stream handler
template<typename T>
std::ostream& operator<<(typename std::enable_if<std::is_enum<T>::value, std::ostream>::type& stream, const T& e)
{
  return stream << static_cast<typename std::underlying_type<T>::type>(e);
}

namespace logfault {

    enum class LogLevel { DISABLED, ERROR, WARN, NOTICE, INFO, DEBUGGING, TRACE };

    struct Message {
        Message(/*const*/ std::string && msg, const LogLevel level)
            : msg_{std::move(msg)}, level_{level} {}

        const std::string msg_;
        const LOGFAULT_TIME_CLOCK::time_point when_ = LOGFAULT_TIME_CLOCK::now();
        const LogLevel level_;
    };

    class Handler {
    public:
      Handler(LogLevel level = LogLevel::INFO, const std::string &name = std::string(LOGFAULT_THREAD_NAME)) : level_{level}, name_{name} {}
        virtual ~Handler() = default;
        using ptr_t = std::unique_ptr<Handler>;

        virtual void LogMessage(const Message& msg) = 0;
        LogLevel level_;
        const std::string name_;
        bool printLoggerName = true;

        static const std::string& LevelName(const LogLevel level) {
            static const std::array<std::string, 7> names = {LOGFAULT_LEVEL_NAMES};
            return names.at(static_cast<size_t>(level));
        }

        std::ostream& PrintMessage(std::ostream& out, const logfault::Message& msg) {
            auto tt = msg.when_.time_since_epoch();
            auto durs = std::chrono::duration_cast<std::chrono::seconds>(tt).count();
            if (const auto tm = (LOGFAULT_USE_UTCZONE ? std::gmtime(&durs) : std::localtime(&durs))) {
                out << std::put_time(tm, LOGFAULT_TIME_FORMAT)
#if LOGFAULT_TIME_PRINT_MILLISECONDS
                ;
                auto durms = std::chrono::duration_cast<std::chrono::milliseconds>(tt).count();
                out << std::setw(3) << std::setfill('0') << int(durms - durs * 1000) << std::setw(0)
#endif
#if LOGFAULT_TIME_PRINT_TIMEZONE
#   if LOGFAULT_USE_UTCZONE
                    << " UTC";
#   else
                    << std::put_time(tm, " %Z")
#   endif
#endif
                    ;
            } else {
                out << "0000-00-00 00:00:00.000";
            }

            out << ' ' << LevelName(msg.level_);
            if (printLoggerName)
                out << ' ' << name_;

            return out << ": " << msg.msg_;
        }

#if defined(LOGFAULT_ENABLE_LOCATION) && LOGFAULT_ENABLE_LOCATION
        static const char *ShortenPath(const char *path) {
            assert(path);
            if (LOGFAULT_LOCATION_LEVELS <= 0) {
                return path;
            }
            std::vector<const char *> seperators;
            for(const char *p = path; *p; ++p) {
                if (((*p == '/')
#if defined(WIN32) || defined(__WIN32) || defined(MSC_VER) || defined(WINDOWS)
                    || (*p == '\\')
#endif
                ) && p[1]) {
                    if (seperators.size() > LOGFAULT_LOCATION_LEVELS) {
                        seperators.erase(seperators.begin());
                    }
                    seperators.push_back(p + 1);
                }
            }
            return seperators.empty() ? path : seperators.front();
        }
#endif

    };

    class StreamHandler : public Handler {
    public:
        StreamHandler(std::ostream& out, LogLevel level, const std::string &name = std::string(LOGFAULT_THREAD_NAME)) : Handler(level, name), out_{out} {}
        StreamHandler(std::string& path, LogLevel level, const bool truncate = false, const std::string &name = std::string(LOGFAULT_THREAD_NAME)) : Handler(level, name)
          , file_{new std::ofstream{path, truncate ? std::ios::trunc : std::ios::app}}, out_{*file_} {}

        void LogMessage(const Message& msg) override {
            PrintMessage(out_, msg) << std::endl;
        }

    private:
        std::unique_ptr<std::ostream> file_;
        std::ostream& out_;
    };

    class FileHandler : public Handler
    {
    public:
      enum class Handling { Append, Truncate, Rotate };

      FileHandler(const char * file, LogLevel level, const Handling handling = Handling::Append, const std::string &name = std::string(LOGFAULT_THREAD_NAME)) :
          Handler(level, name)
      {
        printLoggerName = false;
        std::string path(file);
        path += ".log";
        if (handling == Handling::Rotate && existsFile(path)) {
#ifndef _LIBCPP_NO_EXCEPTIONS
          try {
#endif
            // Remove any existing previous file
            const std::string logFileNamePrev = std::string(file) + ".log.old";
            if (existsFile(logFileNamePrev))
              std::remove(logFileNamePrev.c_str());
            std::rename(path.c_str(), logFileNamePrev.c_str());
#ifndef _LIBCPP_NO_EXCEPTIONS
          }
          catch (const std::exception &) { /* oh well... ? */ }
#endif
        }
        file_ = std::unique_ptr<std::ostream>(new std::ofstream { path, handling == Handling::Truncate ? std::ios::trunc : std::ios::app });
      }

      void LogMessage(const Message& msg) override {
        PrintMessage(*file_, msg) << std::endl;
      }

    private:
      bool existsFile(const std::string& name) {
        if (FILE* file = std::fopen(name.c_str(), "r")) {
          fclose(file);
          return true;
        }
        else {
          return false;
        }
      }

      std::unique_ptr<std::ostream> file_;
    };

    class ProxyHandler : public Handler {
    public:
        using fn_t = std::function<void(const Message&)>;

        ProxyHandler(const fn_t& fn, LogLevel level, const std::string &name = std::string(LOGFAULT_THREAD_NAME)) : Handler(level, name), fn_{fn} {
            assert(fn_);
        }

        void LogMessage(const logfault::Message& msg) override {
            fn_(msg);
        }

    private:
        const fn_t fn_;
    };

    class IdProxyHandler : public Handler {
    public:
      using fn_t = std::function<void(const uint32_t, const Message&)>;

      IdProxyHandler(const fn_t& fn, LogLevel level, const std::string &name, const uint32_t id) :
        Handler(level, name), fn_{fn}, id_{id}
      {
        assert(fn_);
      }

      void LogMessage(const logfault::Message& msg) override {
        fn_(id_, msg);
      }

    private:
      const fn_t fn_;
      const uint32_t id_;
    };

    class LogManager {
        LogManager() = default;
        LogManager(const LogManager&) = delete;
        LogManager(LogManager &&) = delete;
        void operator = (const LogManager&) = delete;
        void operator = (LogManager&&) = delete;
    public:

        static LogManager& Instance() {
            static LogManager instance;
            return instance;
        }

        void LogMessage(Message message) const {
#if LOGFAULT_USE_THREADING
            std::lock_guard<std::mutex> lock{mutex_};
#endif
            for(const auto& h : handlers_) {
                if (h->level_ >= message.level_) {
                    h->LogMessage(message);
                }
            }
        }

        /*! Add a handler. */
        void AddHandler(Handler::ptr_t && handler) {
#if LOGFAULT_USE_THREADING
            std::lock_guard<std::mutex> lock{mutex_};
#endif
            // Make sure we log at the most detailed level used
            if (level_ < handler->level_) {
                level_ = handler->level_;
            }
            handlers_.push_back(std::move(handler));
        }

        /*! Set a single handler. Removes any existing handlers. */
        void SetHandler(Handler::ptr_t && handler) {
#if LOGFAULT_USE_THREADING
            std::lock_guard<std::mutex> lock{mutex_};
#endif
            handlers_.clear();
            level_ = handler->level_;
            handlers_.push_back(std::move(handler));
        }

        /*! Add or replace a single handler by name. Removes any existing handler with the same name. */
        void SetHandler(const std::string &handlerName, Handler::ptr_t && handler) {
            RemoveHandler(handlerName);
            AddHandler(std::move(handler));
        }

         /*! Remove specific named handler(s) */
        void RemoveHandler(const std::string &handlerName)
        {
#if LOGFAULT_USE_THREADING
            std::lock_guard<std::mutex> lock{mutex_};
#endif
            for (auto it = handlers_.begin(), en = handlers_.end(); it != en; ++it) {
                if ((*it)->name_ == handlerName) {
                   it = handlers_.erase(it);
                   break;
                }
            }
        }

         /*! Remove all existing handlers */
        void ClearHandlers() {
#if LOGFAULT_USE_THREADING
            std::lock_guard<std::mutex> lock{mutex_};
#endif
            handlers_.clear();
            level_ = LogLevel::DISABLED;
        }


        /*! Check if a named handler exists. */
        bool HaveHandler(const std::string &handlerName) const
        {
#if LOGFAULT_USE_THREADING
            std::lock_guard<std::mutex> lock{mutex_};
#endif
            for(const auto& h : handlers_)
                if (h->name_ == handlerName)
                    return true;
            return false;
        }

        /*! Sets level on all loggers */
        void SetLevel(LogLevel level) {
#if LOGFAULT_USE_THREADING
            std::lock_guard<std::mutex> lock{mutex_};
#endif
            level_ = level;
            for(auto& h : handlers_)
                h->level_ = level;
        }

        /*! Sets level on specific handler and adjusts overall log level if needed. */
        void SetLevel(const std::string &handlerName, const LogLevel level) {
#if LOGFAULT_USE_THREADING
          std::lock_guard<std::mutex> lock{mutex_};
#endif
            LogLevel maxLevel = LogLevel::DISABLED;
            for(auto& h : handlers_) {
                if (h->name_ == handlerName)
                  h->level_ = level;
                if (h->level_ > maxLevel)
                  maxLevel = h->level_;
            }
            level_ = maxLevel;
        }

        /*! Gets the maximum level on any handler */
        LogLevel GetLoglevel() const noexcept {
            return level_;
        }

        /*! Gets the level of specific handler */
        LogLevel GetLoglevel(const std::string &handlerName) const {
#if LOGFAULT_USE_THREADING
          std::lock_guard<std::mutex> lock{mutex_};
#endif
          for(const auto& h : handlers_)
            if (h->name_ == handlerName)
              return h->level_;
          return LogLevel::DISABLED;
        }

        bool IsRelevant(const LogLevel level) const noexcept {
            return !handlers_.empty() && (level <= level_);
        }

    private:
        std::vector<Handler::ptr_t> handlers_;
        LogLevel level_ = LogLevel::ERROR;
#if LOGFAULT_USE_THREADING
        mutable std::mutex mutex_;
#endif
    };

    class Log {
    public:
        Log(const LogLevel level) : level_{level} {}
        ~Log() {
            Message message(out_.str(), level_);
            LogManager::Instance().LogMessage(message);
        }

        std::ostringstream& Line() { return out_; }

    private:
        const LogLevel level_;
        std::ostringstream out_;
    };

} // namespace


#endif // _LOGFAULT_H

