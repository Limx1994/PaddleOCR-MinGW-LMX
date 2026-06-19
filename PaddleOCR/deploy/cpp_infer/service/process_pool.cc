#include "process_pool.h"
#include "src/utils/ilogger.h"
#include <chrono>
#include <thread>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

ProcessPool::ProcessPool() = default;

ProcessPool::~ProcessPool() {
    Shutdown();
}

bool ProcessPool::Init(const PoolConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    config_ = config;

    // Determine worker exe path
    std::string worker_exe = GetWorkerExePath();
    if (worker_exe.empty()) {
        INFOE("Cannot determine worker executable path");
        return false;
    }
    config_.worker_exe = worker_exe;

    INFO("Initializing process pool with %d workers", config_.pool_size);
    INFO("Worker exe: %s", config_.worker_exe.c_str());
    INFO("Model dir: %s", config_.model_dir.c_str());

    // Create and start workers
    for (int i = 0; i < config_.pool_size; i++) {
        auto worker = std::unique_ptr<WorkerProcess>(new WorkerProcess());
        if (!worker->Start(config_.worker_exe, config_.model_dir,
                           config_.cpu_threads, config_.use_doc_orientation)) {
            INFOE("Failed to start worker %d", i);
            // Continue with remaining workers
            continue;
        }
        INFO("Worker %d started, PID: %d", i, worker->GetPid());
        workers_.push_back(std::move(worker));
    }

    if (workers_.empty()) {
        INFOE("No workers started");
        return false;
    }

    initialized_ = true;
    INFO("Process pool initialized with %zu workers", workers_.size());
    return true;
}

json ProcessPool::ProcessOCR(const std::string& image_path) {
    total_requests_++;

    // Find available worker
    int worker_idx = FindAvailableWorker();
    if (worker_idx < 0) {
        // No available worker, wait and retry
        INFO("No available worker, waiting...");
        for (int retry = 0; retry < 50; retry++) {  // Wait up to 5 seconds
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            worker_idx = FindAvailableWorker();
            if (worker_idx >= 0) break;
        }
        if (worker_idx < 0) {
            failed_requests_++;
            return {{"code", -1}, {"message", "No available worker"}};
        }
    }

    // Process request with retries
    for (int retry = 0; retry <= config_.max_retries; retry++) {
        if (retry > 0) {
            INFO("Retrying request, attempt %d", retry);
        }

        std::string response;
        bool ok = workers_[worker_idx]->ProcessRequest(image_path, response);

        if (!ok) {
            // Worker failed, try to restart
            INFO("Worker %d failed, restarting...", worker_idx);
            if (!RestartWorker(worker_idx)) {
                INFOE("Failed to restart worker %d", worker_idx);
                continue;
            }
            worker_idx = static_cast<int>(workers_.size()) - 1;
            continue;
        }

        // Parse response
        if (response.empty()) {
            INFOE("Empty response from worker %d", worker_idx);
            continue;
        }

        // Check response prefix
        if (response.substr(0, 3) == "OK ") {
            // Success
            std::string json_str = response.substr(3);
            try {
                json result = json::parse(json_str);
                success_requests_++;
                return {
                    {"code", 0},
                    {"message", "success"},
                    {"data", result}
                };
            } catch (const std::exception& e) {
                INFOE("Failed to parse worker response: %s", e.what());
                // Return raw response
                success_requests_++;
                return {
                    {"code", 0},
                    {"message", "success"},
                    {"data", {{"raw", json_str}}}
                };
            }
        } else if (response.substr(0, 4) == "ERR ") {
            // Worker reported error
            std::string error = response.substr(4);
            INFOE("Worker %d error: %s", worker_idx, error.c_str());

            // Check if worker is still alive
            if (!workers_[worker_idx]->IsAlive()) {
                RestartWorker(worker_idx);
                worker_idx = static_cast<int>(workers_.size()) - 1;
            }
            continue;
        } else {
            // Unknown response format
            INFOE("Unknown response format from worker %d: %s",
                  worker_idx, response.c_str());
            continue;
        }
    }

    failed_requests_++;
    return {{"code", -1}, {"message", "All retries failed"}};
}

json ProcessPool::GetStatus() {
    std::lock_guard<std::mutex> lock(mutex_);

    int alive_count = 0;
    int idle_count = 0;
    int busy_count = 0;
    int dead_count = 0;

    for (const auto& worker : workers_) {
        if (worker->IsAlive()) {
            alive_count++;
            if (worker->GetStatus() == WorkerStatus::IDLE) {
                idle_count++;
            } else if (worker->GetStatus() == WorkerStatus::BUSY) {
                busy_count++;
            }
        } else {
            dead_count++;
        }
    }

    return {
        {"initialized", initialized_},
        {"pool_size", config_.pool_size},
        {"alive_workers", alive_count},
        {"idle_workers", idle_count},
        {"busy_workers", busy_count},
        {"dead_workers", dead_count},
        {"total_requests", total_requests_.load()},
        {"success_requests", success_requests_.load()},
        {"failed_requests", failed_requests_.load()},
        {"worker_restarts", worker_restarts_.load()},
        {"config", {
            {"model_dir", config_.model_dir},
            {"cpu_threads", config_.cpu_threads},
            {"use_doc_orientation", config_.use_doc_orientation}
        }}
    };
}

bool ProcessPool::RestartAll() {
    std::lock_guard<std::mutex> lock(mutex_);

    INFO("Restarting all workers");

    for (size_t i = 0; i < workers_.size(); i++) {
        if (!workers_[i]->Restart()) {
            INFOE("Failed to restart worker %zu", i);
            return false;
        }
        INFO("Worker %zu restarted, PID: %d", i, workers_[i]->GetPid());
        worker_restarts_++;
    }

    return true;
}

void ProcessPool::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    INFO("Shutting down process pool");

    for (auto& worker : workers_) {
        worker->Stop();
    }
    workers_.clear();
    initialized_ = false;
}

int ProcessPool::FindAvailableWorker() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (size_t i = 0; i < workers_.size(); i++) {
        if (workers_[i]->IsAlive() &&
            workers_[i]->GetStatus() == WorkerStatus::IDLE) {
            return static_cast<int>(i);
        }
    }

    // Check if any dead workers can be restarted
    for (size_t i = 0; i < workers_.size(); i++) {
        if (!workers_[i]->IsAlive() ||
            workers_[i]->GetStatus() == WorkerStatus::DEAD) {
            INFO("Worker %zu is dead, restarting...", i);
            if (RestartWorker(static_cast<int>(i))) {
                return static_cast<int>(i);
            }
        }
    }

    return -1;
}

bool ProcessPool::RestartWorker(int index) {
    // Note: caller must hold mutex_ or this must be called from a locked context
    if (index < 0 || index >= static_cast<int>(workers_.size())) {
        return false;
    }

    INFO("Restarting worker %d (PID: %d)", index, workers_[index]->GetPid());

    if (!workers_[index]->Restart()) {
        INFOE("Failed to restart worker %d", index);
        return false;
    }

    worker_restarts_++;
    INFO("Worker %d restarted, new PID: %d", index, workers_[index]->GetPid());
    return true;
}

std::string ProcessPool::GetWorkerExePath() {
    // Try to find ppocr_worker.exe in the same directory as ppocr_service.exe
#ifdef _WIN32
    char path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return "";
    }

    // Find last backslash
    std::string exe_path(path, len);
    size_t pos = exe_path.find_last_of('\\');
    if (pos == std::string::npos) {
        pos = exe_path.find_last_of('/');
    }

    std::string dir;
    if (pos != std::string::npos) {
        dir = exe_path.substr(0, pos + 1);
    }

    // Try ppocr_worker.exe in same directory
    std::string worker_path = dir + "ppocr_worker.exe";

    // Check if file exists
    DWORD attr = GetFileAttributesA(worker_path.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) {
        return worker_path;
    }

    // Try build_mingw directory
    worker_path = dir + "..\\build_mingw\\ppocr_worker.exe";
    attr = GetFileAttributesA(worker_path.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) {
        return worker_path;
    }

    // Try current directory
    worker_path = "ppocr_worker.exe";
    attr = GetFileAttributesA(worker_path.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) {
        return worker_path;
    }

    INFOE("Cannot find ppocr_worker.exe");
    return "";
#else
    // Linux: similar logic
    return "ppocr_worker";
#endif
}
