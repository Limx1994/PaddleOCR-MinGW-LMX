#pragma once

#include <string>
#include <mutex>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#endif

// Worker process status
enum class WorkerStatus {
    IDLE,       // Ready to accept request
    BUSY,       // Processing request
    DEAD,       // Process crashed or exited
    STARTING    // Starting up
};

// Worker process management class
// Manages a single ppocr_worker.exe child process
// Communicates via stdin/stdout pipes
class WorkerProcess {
public:
    WorkerProcess();
    ~WorkerProcess();

    // Start worker process
    bool Start(const std::string& worker_exe,
               const std::string& model_dir,
               int cpu_threads,
               bool use_doc_orientation,
               const std::string& det_model_dir = "",
               const std::string& det_model_name = "",
               const std::string& rec_model_dir = "",
               const std::string& rec_model_name = "",
               const std::string& cls_model_dir = "",
               const std::string& cls_model_name = "",
               bool use_doc_unwarping = false,
               bool use_textline_orientation = false);

    // Send OCR request (image path)
    bool SendRequest(const std::string& image_path);

    // Read OCR response (json string)
    bool ReadResponse(std::string& response);

    // Process one OCR request (send + read)
    bool ProcessRequest(const std::string& image_path,
                        std::string& response);

    // Check if worker is alive
    bool IsAlive();

    // Get worker status
    WorkerStatus GetStatus() const { return status_; }

    // Get worker PID
    int GetPid() const;

    // Stop worker
    void Stop();

    // Restart worker (stop + start with same config)
    bool Restart();

private:
    // Read a line from stdout pipe
    bool ReadLine(std::string& line);

    // Write a line to stdin pipe
    bool WriteLine(const std::string& line);

    // Check process exit code
    bool CheckProcessExit();

#ifdef _WIN32
    HANDLE process_handle_ = INVALID_HANDLE_VALUE;
    HANDLE stdin_write_ = INVALID_HANDLE_VALUE;   // Parent writes to this
    HANDLE stdout_read_ = INVALID_HANDLE_VALUE;    // Parent reads from this
    DWORD process_id_ = 0;
#else
    int stdin_fd_ = -1;
    int stdout_fd_ = -1;
    pid_t process_id_ = -1;
#endif

    // Buffered reading for pipe I/O
    static const size_t READ_BUF_SIZE = 4096;
    char read_buf_[READ_BUF_SIZE];
    size_t read_buf_len_ = 0;  // Valid data length in buffer
    size_t read_buf_pos_ = 0;  // Current read position

    std::atomic<WorkerStatus> status_{WorkerStatus::DEAD};
    std::mutex mutex_;

    // Config for restart
    std::string worker_exe_;
    std::string model_dir_;
    int cpu_threads_ = 8;
    bool use_doc_orientation_ = true;
    bool use_doc_unwarping_ = false;
    bool use_textline_orientation_ = false;
    std::string det_model_dir_;
    std::string det_model_name_;
    std::string rec_model_dir_;
    std::string rec_model_name_;
    std::string cls_model_dir_;
    std::string cls_model_name_;
};
