// ppocr_worker - Worker process for PaddleOCR service
// Reads image paths from stdin, performs OCR, writes results to stdout
//
// Protocol:
//   Startup:  Worker prints "READY\n" when models are loaded
//   Request:  Parent writes "<image_path>\n" to stdin
//   Response: Worker writes "OK <json>\n" or "ERR <message>\n" to stdout

#include "src/api/pipelines/ocr.h"
#include "src/utils/cpu_features.h"
#include "src/utils/ilogger.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <chrono>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#endif

using json = nlohmann::json;

static bool g_running = true;

// Capture stdout from a function call
std::string CaptureStdout(std::function<void()> func) {
    std::ostringstream oss;
    std::streambuf* old_buf = std::cout.rdbuf(oss.rdbuf());
    func();
    std::cout.rdbuf(old_buf);
    return oss.str();
}

// Print usage
void PrintUsage() {
    std::cerr << "Usage: ppocr_worker [options]" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  --model_dir <dir>           Model directory" << std::endl;
    std::cerr << "  --cpu_threads <num>         CPU threads (default: 8)" << std::endl;
    std::cerr << "  --use_doc_orientation <bool> Use doc orientation (default: true)" << std::endl;
    std::cerr << "  --help                      Show this help" << std::endl;
}

int main(int argc, char* argv[]) {
    // Disable sync with stdio for faster I/O
    std::ios::sync_with_stdio(false);

    // Parse arguments
    std::string model_dir;
    int cpu_threads = 8;
    bool use_doc_orientation = true;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            PrintUsage();
            return 0;
        } else if (arg == "--model_dir" && i + 1 < argc) {
            model_dir = argv[++i];
        } else if (arg == "--cpu_threads" && i + 1 < argc) {
            cpu_threads = std::stoi(argv[++i]);
        } else if (arg == "--use_doc_orientation" && i + 1 < argc) {
            use_doc_orientation = (std::string(argv[++i]) == "true");
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            PrintUsage();
            return 1;
        }
    }

    if (model_dir.empty()) {
        std::cerr << "Error: --model_dir is required" << std::endl;
        PrintUsage();
        return 1;
    }

    // Initialize PaddleOCR
    std::cerr << "[Worker] Initializing PaddleOCR..." << std::endl;
    std::cerr << "[Worker] Model dir: " << model_dir << std::endl;
    std::cerr << "[Worker] CPU threads: " << cpu_threads << std::endl;
    std::cerr << "[Worker] Doc orientation: " << (use_doc_orientation ? "true" : "false") << std::endl;

    try {
        PaddleOCRParams params;

        // Set model paths
        std::string det_dir = model_dir + "/PP-OCRv4_mobile_det_infer";
        std::string rec_dir = model_dir + "/PP-OCRv4_mobile_rec_infer";
        std::string cls_dir = model_dir + "/PP-LCNet_x1_0_doc_ori_infer";

        struct stat st;
        if (stat(det_dir.c_str(), &st) == 0) {
            params.text_detection_model_dir = det_dir;
            params.text_detection_model_name = "PP-OCRv4_mobile_det";
        }
        if (stat(rec_dir.c_str(), &st) == 0) {
            params.text_recognition_model_dir = rec_dir;
            params.text_recognition_model_name = "PP-OCRv4_mobile_rec";
        }
        if (stat(cls_dir.c_str(), &st) == 0) {
            params.doc_orientation_classify_model_dir = cls_dir;
            params.doc_orientation_classify_model_name = "PP-LCNet_x1_0_doc_ori";
        }

        params.use_doc_orientation_classify = use_doc_orientation;
        params.use_doc_unwarping = false;
        params.use_textline_orientation = false;
        params.cpu_threads = cpu_threads;
        params.thread_num = 1;

        // Create OCR engine
        PaddleOCR ocr_engine(params);

        std::cerr << "[Worker] PaddleOCR initialized successfully" << std::endl;

        // Signal ready
        std::cout << "READY" << std::endl;
        std::cout.flush();

        // Main loop: read image paths from stdin, do OCR, write results
        std::string image_path;
        while (g_running && std::getline(std::cin, image_path)) {
            if (image_path.empty()) {
                continue;
            }

            auto start = std::chrono::high_resolution_clock::now();

            try {
                // Check if file exists
                if (access(image_path.c_str(), F_OK) != 0) {
                    std::cout << "ERR File not found: " << image_path << std::endl;
                    std::cout.flush();
                    continue;
                }

                // Execute OCR
                auto results = ocr_engine.Predict(image_path);

                // Convert results to JSON
                json output = json::array();
                for (const auto& result : results) {
                    // Capture Print() output
                    std::string str = CaptureStdout([&result]() {
                        result->Print();
                    });

                    try {
                        json item = json::parse(str);
                        output.push_back(item);
                    } catch (const std::exception&) {
                        output.push_back({{"result", str}});
                    }
                }

                auto end = std::chrono::high_resolution_clock::now();
                double time_ms = std::chrono::duration<double, std::milli>(
                    end - start).count();

                // Build response
                json response = {
                    {"results", output},
                    {"time_ms", time_ms}
                };

                std::cout << "OK " << response.dump() << std::endl;
                std::cout.flush();

            } catch (const std::exception& e) {
                std::cout << "ERR " << e.what() << std::endl;
                std::cout.flush();
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "[Worker] Failed to initialize: " << e.what() << std::endl;
        std::cout << "ERR Initialization failed: " << e.what() << std::endl;
        std::cout.flush();
        return 1;
    }

    std::cerr << "[Worker] Exiting" << std::endl;
    return 0;
}
