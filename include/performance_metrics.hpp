#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace deribit {

class PerformanceMetrics {
public:
    static PerformanceMetrics& instance() {
        static PerformanceMetrics instance;
        return instance;
    }

    void start_measurement(const std::string& operation_id);
    void end_measurement(const std::string& operation_id);
    
    struct LatencyStats {
        double min_ms = std::numeric_limits<double>::max();
        double max_ms = 0;
        double avg_ms = 0;
        size_t count = 0;
    };
    
    LatencyStats get_stats(const std::string& operation_id);
    void reset_stats(const std::string& operation_id);
    void print_all_stats();

private:
    PerformanceMetrics() = default;
    ~PerformanceMetrics() = default;
    
    struct OperationTiming {
        std::chrono::high_resolution_clock::time_point start_time;
        std::vector<double> measurements_ms;
    };
    
    std::unordered_map<std::string, OperationTiming> operations_;
    std::mutex mutex_;
};

#define START_TIMING(id) deribit::PerformanceMetrics::instance().start_measurement(id)
#define END_TIMING(id) deribit::PerformanceMetrics::instance().end_measurement(id)

} // namespace deribit