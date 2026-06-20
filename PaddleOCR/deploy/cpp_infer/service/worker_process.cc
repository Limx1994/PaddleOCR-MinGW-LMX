#include "worker_process.h"
#include "src/utils/ilogger.h"
#include <cstring>
#include <sstream>
#include <chrono>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

WorkerProcess::WorkerProcess() = default;

WorkerProcess::~WorkerProcess() {
    Stop();
}

bool WorkerProcess::Start(const std::string& worker_exe,
                          const std::string& model_dir,
                          int cpu_threads,
                          bool use_doc_orientation,
                          const std::string& det_model_dir,
                          const std::string& det_model_name,
                          const std::string& rec_model_dir,
                          const std::string& rec_model_name,
                          const std::string& cls_model_dir,
                          const std::string& cls_model_name,
                          bool use_doc_unwarping,
                          bool use_textline_orientation) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Save config for restart
    worker_exe_ = worker_exe;
    model_dir_ = model_dir;
    cpu_threads_ = cpu_threads;
    use_doc_orientation_ = use_doc_orientation;
    use_doc_unwarping_ = use_doc_unwarping;
    use_textline_orientation_ = use_textline_orientation;
    det_model_dir_ = det_model_dir;
    det_model_name_ = det_model_name;
    rec_model_dir_ = rec_model_dir;
    rec_model_name_ = rec_model_name;
    cls_model_dir_ = cls_model_dir;
    cls_model_name_ = cls_model_name;

    status_ = WorkerStatus::STARTING;

#ifdef _WIN32
    // Create pipes for stdin
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE stdin_read = INVALID_HANDLE_VALUE;
    HANDLE stdin_write = INVALID_HANDLE_VALUE;
    HANDLE stdout_read = INVALID_HANDLE_VALUE;
    HANDLE stdout_write = INVALID_HANDLE_VALUE;

    // Create stdin pipe (parent writes, child reads)
    if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0)) {
        INFOE("Failed to create stdin pipe, error: %lu", GetLastError());
        status_ = WorkerStatus::DEAD;
        return false;
    }

    // Create stdout pipe (child writes, parent reads)
    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        INFOE("Failed to create stdout pipe, error: %lu", GetLastError());
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        status_ = WorkerStatus::DEAD;
        return false;
    }

    // Ensure parent-side handles are not inherited
    SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

    // Build command line
    std::string cmd = "\"" + worker_exe + "\"";

    // Use specific model dirs if provided, otherwise use model_dir
    if (!det_model_dir.empty()) {
        cmd += " --det_model_dir \"" + det_model_dir + "\"";
        if (!det_model_name.empty()) cmd += " --det_model_name \"" + det_model_name + "\"";
    }
    if (!rec_model_dir.empty()) {
        cmd += " --rec_model_dir \"" + rec_model_dir + "\"";
        if (!rec_model_name.empty()) cmd += " --rec_model_name \"" + rec_model_name + "\"";
    }
    if (!cls_model_dir.empty()) {
        cmd += " --cls_model_dir \"" + cls_model_dir + "\"";
        if (!cls_model_name.empty()) cmd += " --cls_model_name \"" + cls_model_name + "\"";
    }

    // Always pass model_dir for auto-detection fallback
    if (!model_dir.empty()) {
        cmd += " --model_dir \"" + model_dir + "\"";
    }

    cmd += " --cpu_threads " + std::to_string(cpu_threads);
    cmd += " --use_doc_orientation " + std::string(use_doc_orientation ? "true" : "false");
    cmd += " --use_doc_unwarping " + std::string(use_doc_unwarping ? "true" : "false");
    cmd += " --use_textline_orientation " + std::string(use_textline_orientation ? "true" : "false");

    // Set up startup info
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = stdin_read;
    si.hStdOutput = stdout_write;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;

    // Create process
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // Create mutable command line buffer
    std::vector<char> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back('\0');

    BOOL result = CreateProcessA(
        NULL,                           // Application name
        cmd_buf.data(),                 // Command line
        NULL,                           // Process security attributes
        NULL,                           // Thread security attributes
        TRUE,                           // Inherit handles
        0,                              // Creation flags
        NULL,                           // Environment
        NULL,                           // Current directory
        &si,                            // Startup info
        &pi                             // Process information
    );

    // Close child-side handles in parent
    CloseHandle(stdin_read);
    CloseHandle(stdout_write);

    if (!result) {
        INFOE("Failed to create worker process, error: %lu", GetLastError());
        CloseHandle(stdin_write);
        CloseHandle(stdout_read);
        status_ = WorkerStatus::DEAD;
        return false;
    }

    // Save handles
    process_handle_ = pi.hProcess;
    CloseHandle(pi.hThread);  // We don't need the thread handle
    stdin_write_ = stdin_write;
    stdout_read_ = stdout_read;
    process_id_ = pi.dwProcessId;

#else
    // Linux: create pipes
    int stdin_pipe[2];   // [0]=read, [1]=write
    int stdout_pipe[2];  // [0]=read, [1]=write

    if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
        INFOE("Failed to create pipes");
        status_ = WorkerStatus::DEAD;
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        INFOE("Failed to fork");
        status_ = WorkerStatus::DEAD;
        return false;
    }

    if (pid == 0) {
        // Child process
        close(stdin_pipe[1]);   // Close write end of stdin
        close(stdout_pipe[0]);  // Close read end of stdout

        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        // Build args (same as Windows path)
        std::vector<std::string> arg_strs;
        arg_strs.push_back(worker_exe);

        if (!det_model_dir.empty()) {
            arg_strs.push_back("--det_model_dir");
            arg_strs.push_back(det_model_dir);
            if (!det_model_name.empty()) {
                arg_strs.push_back("--det_model_name");
                arg_strs.push_back(det_model_name);
            }
        }
        if (!rec_model_dir.empty()) {
            arg_strs.push_back("--rec_model_dir");
            arg_strs.push_back(rec_model_dir);
            if (!rec_model_name.empty()) {
                arg_strs.push_back("--rec_model_name");
                arg_strs.push_back(rec_model_name);
            }
        }
        if (!cls_model_dir.empty()) {
            arg_strs.push_back("--cls_model_dir");
            arg_strs.push_back(cls_model_dir);
            if (!cls_model_name.empty()) {
                arg_strs.push_back("--cls_model_name");
                arg_strs.push_back(cls_model_name);
            }
        }
        if (!model_dir.empty()) {
            arg_strs.push_back("--model_dir");
            arg_strs.push_back(model_dir);
        }
        arg_strs.push_back("--cpu_threads");
        arg_strs.push_back(std::to_string(cpu_threads));
        arg_strs.push_back("--use_doc_orientation");
        arg_strs.push_back(use_doc_orientation ? "true" : "false");
        arg_strs.push_back("--use_doc_unwarping");
        arg_strs.push_back(use_doc_unwarping ? "true" : "false");
        arg_strs.push_back("--use_textline_orientation");
        arg_strs.push_back(use_textline_orientation ? "true" : "false");

        // Convert to char* array for execv
        std::vector<char*> argv;
        for (auto& s : arg_strs) {
            argv.push_back(const_cast<char*>(s.c_str()));
        }
        argv.push_back(nullptr);

        execv(worker_exe.c_str(), argv.data());

        // If execv fails
        _exit(1);
    }

    // Parent process
    close(stdin_pipe[0]);   // Close read end of stdin
    close(stdout_pipe[1]);  // Close write end of stdout

    stdin_fd_ = stdin_pipe[1];
    stdout_fd_ = stdout_pipe[0];
    process_id_ = pid;
#endif

    // Wait a bit for worker to start and load models
    // The worker will print "READY" when it's done loading
    std::string ready_line;
    if (!ReadLine(ready_line)) {
        INFOE("Worker did not send READY signal");
        Stop();
        return false;
    }

    if (ready_line.find("READY") == std::string::npos) {
        INFOE("Worker sent unexpected startup message: %s", ready_line.c_str());
        Stop();
        return false;
    }

    status_ = WorkerStatus::IDLE;
    INFO("Worker started, PID: %d", GetPid());
    return true;
}

bool WorkerProcess::SendRequest(const std::string& image_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != WorkerStatus::IDLE) {
        return false;
    }

    if (!WriteLine(image_path)) {
        INFOE("Failed to send request to worker PID %d", GetPid());
        status_ = WorkerStatus::DEAD;
        return false;
    }

    status_ = WorkerStatus::BUSY;
    return true;
}

bool WorkerProcess::ReadResponse(std::string& response) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != WorkerStatus::BUSY) {
        return false;
    }

    if (!ReadLine(response)) {
        INFOE("Failed to read response from worker PID %d", GetPid());
        status_ = WorkerStatus::DEAD;
        return false;
    }

    status_ = WorkerStatus::IDLE;
    return true;
}

bool WorkerProcess::ProcessRequest(const std::string& image_path,
                                   std::string& response) {
    // Send request
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_ != WorkerStatus::IDLE) {
            return false;
        }
        if (!WriteLine(image_path)) {
            INFOE("Failed to send request to worker PID %d", GetPid());
            status_ = WorkerStatus::DEAD;
            return false;
        }
        status_ = WorkerStatus::BUSY;
    }

    // Read response (outside lock to allow concurrent processing)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ReadLine(response)) {
            INFOE("Failed to read response from worker PID %d", GetPid());
            status_ = WorkerStatus::DEAD;
            return false;
        }
        status_ = WorkerStatus::IDLE;
    }

    return true;
}

bool WorkerProcess::IsAlive() {
#ifdef _WIN32
    if (process_handle_ == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD exit_code;
    if (!GetExitCodeProcess(process_handle_, &exit_code)) {
        return false;
    }
    return exit_code == STILL_ACTIVE;
#else
    if (process_id_ <= 0) {
        return false;
    }
    return kill(process_id_, 0) == 0;
#endif
}

int WorkerProcess::GetPid() const {
#ifdef _WIN32
    return static_cast<int>(process_id_);
#else
    return static_cast<int>(process_id_);
#endif
}

void WorkerProcess::Stop() {
#ifdef _WIN32
    if (stdin_write_ != INVALID_HANDLE_VALUE) {
        CloseHandle(stdin_write_);
        stdin_write_ = INVALID_HANDLE_VALUE;
    }
    if (stdout_read_ != INVALID_HANDLE_VALUE) {
        CloseHandle(stdout_read_);
        stdout_read_ = INVALID_HANDLE_VALUE;
    }
    if (process_handle_ != INVALID_HANDLE_VALUE) {
        // Try graceful termination first
        if (IsAlive()) {
            TerminateProcess(process_handle_, 0);
            WaitForSingleObject(process_handle_, 3000);
        }
        CloseHandle(process_handle_);
        process_handle_ = INVALID_HANDLE_VALUE;
    }
    process_id_ = 0;
#else
    if (stdin_fd_ >= 0) {
        close(stdin_fd_);
        stdin_fd_ = -1;
    }
    if (stdout_fd_ >= 0) {
        close(stdout_fd_);
        stdout_fd_ = -1;
    }
    if (process_id_ > 0) {
        kill(process_id_, SIGTERM);
        waitpid(process_id_, NULL, 0);
        process_id_ = -1;
    }
#endif

    status_ = WorkerStatus::DEAD;

    // Reset read buffer
    read_buf_len_ = 0;
    read_buf_pos_ = 0;
}

bool WorkerProcess::Restart() {
    INFO("Restarting worker PID %d", GetPid());
    Stop();
    return Start(worker_exe_, model_dir_, cpu_threads_, use_doc_orientation_,
                 det_model_dir_, det_model_name_, rec_model_dir_, rec_model_name_,
                 cls_model_dir_, cls_model_name_, use_doc_unwarping_, use_textline_orientation_);
}

bool WorkerProcess::ReadLine(std::string& line) {
    line.clear();

    while (true) {
        // If buffer has data, scan for newline
        while (read_buf_pos_ < read_buf_len_) {
            char ch = read_buf_[read_buf_pos_++];
            if (ch == '\n') {
                return true;
            }
            if (ch != '\r') {
                line += ch;
            }
        }

        // Buffer exhausted, refill
        read_buf_pos_ = 0;
        read_buf_len_ = 0;

#ifdef _WIN32
        DWORD bytes = 0;
        BOOL result = ReadFile(stdout_read_, read_buf_,
                               static_cast<DWORD>(READ_BUF_SIZE),
                               &bytes, NULL);
        if (!result || bytes == 0) {
            return false;
        }
        read_buf_len_ = bytes;
#else
        ssize_t bytes = read(stdout_fd_, read_buf_, READ_BUF_SIZE);
        if (bytes <= 0) {
            return false;
        }
        read_buf_len_ = bytes;
#endif
    }
}

bool WorkerProcess::WriteLine(const std::string& line) {
    std::string data = line + "\n";

#ifdef _WIN32
    DWORD bytes;
    BOOL result = WriteFile(stdin_write_, data.c_str(),
                            static_cast<DWORD>(data.size()), &bytes, NULL);
    if (!result) {
        return false;
    }
    // Flush to ensure data is sent
    FlushFileBuffers(stdin_write_);
#else
    ssize_t written = write(stdin_fd_, data.c_str(), data.size());
    if (written != static_cast<ssize_t>(data.size())) {
        return false;
    }
#endif

    return true;
}

bool WorkerProcess::CheckProcessExit() {
#ifdef _WIN32
    if (process_handle_ == INVALID_HANDLE_VALUE) {
        return true;
    }
    DWORD exit_code;
    if (!GetExitCodeProcess(process_handle_, &exit_code)) {
        return true;
    }
    return exit_code != STILL_ACTIVE;
#else
    if (process_id_ <= 0) {
        return true;
    }
    int status;
    pid_t result = waitpid(process_id_, &status, WNOHANG);
    return result > 0;
#endif
}
