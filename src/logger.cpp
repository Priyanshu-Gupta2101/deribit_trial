#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <mutex>
#include <ctime>
#include <iomanip>

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
    static Logger& instance() {
        static Logger instance;
        return instance;
    }

    void set_level(LogLevel level) { 
        level_ = level; 
    }

    void set_log_file(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_.close();
        }
        file_.open(filename, std::ios::app);
    }

    template<typename... Args>
    void debug(const std::string& format, Args... args) {
        log(LogLevel::DEBUG, format, args...);
    }

    template<typename... Args>
    void info(const std::string& format, Args... args) {
        log(LogLevel::INFO, format, args...);
    }

    template<typename... Args>
    void warning(const std::string& format, Args... args) {
        log(LogLevel::WARNING, format, args...);
    }

    template<typename... Args>
    void error(const std::string& format, Args... args) {
        log(LogLevel::ERROR, format, args...);
    }

    template<typename... Args>
    void critical(const std::string& format, Args... args) {
        log(LogLevel::CRITICAL, format, args...);
    }

private:
    Logger() : level_(LogLevel::INFO) {}
    ~Logger() {
        if (file_.is_open()) {
            file_.close();
        }
    }

    template<typename... Args>
    void log(LogLevel level, const std::string& format, Args... args) {
        if (level < level_) return;

        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        
        std::string level_str;
        switch (level) {
            case LogLevel::DEBUG: level_str = "DEBUG"; break;
            case LogLevel::INFO: level_str = "INFO"; break;
            case LogLevel::WARNING: level_str = "WARNING"; break;
            case LogLevel::ERROR: level_str = "ERROR"; break;
            case LogLevel::CRITICAL: level_str = "CRITICAL"; break;
        }
        
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), format.c_str(), args...);
        
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " [" << level_str << "] " << buffer;
        
        std::cout << oss.str() << std::endl;
        
        if (file_.is_open()) {
            file_ << oss.str() << std::endl;
        }
    }

    LogLevel level_;
    std::ofstream file_;
    std::mutex mutex_;
};

#define LOG_DEBUG(...) deribit::Logger::instance().debug(__VA_ARGS__)
#define LOG_INFO(...) deribit::Logger::instance().info(__VA_ARGS__)
#define LOG_WARNING(...) deribit::Logger::instance().warning(__VA_ARGS__)
#define LOG_ERROR(...) deribit::Logger::instance().error(__VA_ARGS__)
#define LOG_CRITICAL(...) deribit::Logger::instance().critical(__VA_ARGS__)

}