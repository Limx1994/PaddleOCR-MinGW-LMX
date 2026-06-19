#pragma once

#include "worker_process.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <string>

using json = nlohmann::json;

struct PoolConfig {
    int pool_size = 2;              // Number of worker processes
    std::string worker_exe;         // Path to ppocr_worker.exe
    std::string model_dir;          // Model directory
    int cpu_threads = 8;            // CPU threads per worker
    bool use_doc_orientation = true;
    int max_retries = 2;            // Max retries per request
};

// Process pool for managing worker processes
class ProcessPool {
public:
    ProcessPool();
    ~ProcessPool();

    // Initialize pool
    bool Init(const PoolConfig& config);

    // Process OCR request
    // Returns JSON result
    json ProcessOCR(const std::string& image_path);

    // Get pool status
    json GetStatus();

    // Restart all workers
    bool RestartAll();

    // Shutdown pool
    void Shutdown();

    // Is initialized
    bool IsInitialized() const { return initialized_; }

private:
    // Find an available worker
    int FindAvailableWorker();

    // Restart a specific worker
    bool RestartWorker(int index);

    // Get worker exe path
    std::string GetWorkerExePath();

private:
    PoolConfig config_;
    std::vector<std::unique_ptr<WorkerProcess>> workers_;
    std::mutex mutex_;
    bool initialized_ = false;

    // Statistics
    std::atomic<int64_t> total_requests_{0};
    std::atomic<int64_t> success_requests_{0};
    std::atomic<int64_t> failed_requests_{0};
    std::atomic<int64_t> worker_restarts_{0};
};
