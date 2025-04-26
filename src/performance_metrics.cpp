#include "performance_metrics.hpp"
#include <iostream>
#include <algorithm>
#include <numeric>

namespace deribit {

void PerformanceMetrics::start_measurement(const std::string& operation_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    operations_[operation_id].start_time = std::chrono::high_resolution_clock::now();
}

void PerformanceMetrics::end_measurement(const std::string& operation_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    if (operations_.find(operation_id) == operations_.end()) {
        std::cerr << "Warning: Operation " << operation_id << " not started" << std::endl;
        return;
    }
    
    auto& operation = operations_[operation_id];
    auto duration = end_time - operation.start_time;
    double milliseconds = std::chrono::duration<double, std::milli>(duration).count();
    operation.measurements_ms.push_back(milliseconds);
}

PerformanceMetrics::LatencyStats PerformanceMetrics::get_stats(const std::string& operation_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    LatencyStats stats;
    
    if (operations_.find(operation_id) == operations_.end()) {
        return stats;
    }
    
    const auto& measurements = operations_[operation_id].measurements_ms;
    if (measurements.empty()) {
        return stats;
    }
    
    stats.count = measurements.size();
    stats.min_ms = *std::min_element(measurements.begin(), measurements.end());
    stats.max_ms = *std::max_element(measurements.begin(), measurements.end());
    stats.avg_ms = std::accumulate(measurements.begin(), measurements.end(), 0.0) / measurements.size();
    
    return stats;
}

void PerformanceMetrics::reset_stats(const std::string& operation_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (operations_.find(operation_id) != operations_.end()) {
        operations_[operation_id].measurements_ms.clear();
    }
}

void PerformanceMetrics::print_all_stats() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "\n===== PERFORMANCE METRICS =====\n";
    
    for (const auto& [operation_id, timing] : operations_) {
        if (timing.measurements_ms.empty()) {
            continue;
        }
        
        double min = *std::min_element(timing.measurements_ms.begin(), timing.measurements_ms.end());
        double max = *std::max_element(timing.measurements_ms.begin(), timing.measurements_ms.end());
        double avg = std::accumulate(timing.measurements_ms.begin(), timing.measurements_ms.end(), 0.0) / 
                    timing.measurements_ms.size();
        
        std::cout << "Operation: " << operation_id << std::endl;
        std::cout << "  Count: " << timing.measurements_ms.size() << std::endl;
        std::cout << "  Min: " << min << " ms" << std::endl;
        std::cout << "  Max: " << max << " ms" << std::endl;
        std::cout << "  Avg: " << avg << " ms" << std::endl;
    }
    
    std::cout << "==============================\n";
}

}