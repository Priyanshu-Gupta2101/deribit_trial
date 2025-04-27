#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdio>

namespace deribit {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel level);
    void set_log_file(const std::string& filename);

    template<typename... Args>
    void debug(const std::string& format, Args... args);

    template<typename... Args>
    void info(const std::string& format, Args... args);

    template<typename... Args>
    void warning(const std::string& format, Args... args);

    template<typename... Args>
    void error(const std::string& format, Args... args);

    template<typename... Args>
    void critical(const std::string& format, Args... args);

private:
    Logger();
    ~Logger();

    template<typename... Args>
    void log(LogLevel level, const std::string& format, Args... args);

    LogLevel level_;
    std::ofstream file_;
    std::mutex mutex_;
};

#define LOG_DEBUG(...) deribit::Logger::instance().debug(__VA_ARGS__)
#define LOG_INFO(...) deribit::Logger::instance().info(__VA_ARGS__)
#define LOG_WARNING(...) deribit::Logger::instance().warning(__VA_ARGS__)
#define LOG_ERROR(...) deribit::Logger::instance().error(__VA_ARGS__)
#define LOG_CRITICAL(...) deribit::Logger::instance().critical(__VA_ARGS__)

} // namespace deribit