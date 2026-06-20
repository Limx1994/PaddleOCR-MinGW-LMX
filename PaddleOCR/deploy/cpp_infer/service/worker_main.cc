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
    std::cerr << "  --model_dir <dir>                          Model directory (auto-detect models)" << std::endl;
    std::cerr << "  --det_model_dir <dir>                      Detection model directory" << std::endl;
    std::cerr << "  --det_model_name <name>                    Detection model name (default: PP-OCRv4_mobile_det)" << std::endl;
    std::cerr << "  --rec_model_dir <dir>                      Recognition model directory" << std::endl;
    std::cerr << "  --rec_model_name <name>                    Recognition model name (default: PP-OCRv4_mobile_rec)" << std::endl;
    std::cerr << "  --cls_model_dir <dir>                      Classification model directory" << std::endl;
    std::cerr << "  --cls_model_name <name>                    Classification model name (default: PP-LCNet_x1_0_doc_ori)" << std::endl;
    std::cerr << "  --cpu_threads <num>                        CPU threads (default: 8)" << std::endl;
    std::cerr << "  --use_doc_orientation <bool>               Use doc orientation (default: true)" << std::endl;
    std::cerr << "  --use_doc_unwarping <bool>                 Use doc unwarping (default: false)" << std::endl;
    std::cerr << "  --use_textline_orientation <bool>          Use textline orientation (default: false)" << std::endl;
    std::cerr << "  --help                                     Show this help" << std::endl;
}

int main(int argc, char* argv[]) {
    // Disable sync with stdio for faster I/O
    std::ios::sync_with_stdio(false);

    // Parse arguments
    std::string model_dir;
    std::string det_model_dir, det_model_name;
    std::string rec_model_dir, rec_model_name;
    std::string cls_model_dir, cls_model_name;
    int cpu_threads = 8;
    bool use_doc_orientation = true;
    bool use_doc_unwarping = false;
    bool use_textline_orientation = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            PrintUsage();
            return 0;
        } else if (arg == "--model_dir" && i + 1 < argc) {
            model_dir = argv[++i];
        } else if (arg == "--det_model_dir" && i + 1 < argc) {
            det_model_dir = argv[++i];
        } else if (arg == "--det_model_name" && i + 1 < argc) {
            det_model_name = argv[++i];
        } else if (arg == "--rec_model_dir" && i + 1 < argc) {
            rec_model_dir = argv[++i];
        } else if (arg == "--rec_model_name" && i + 1 < argc) {
            rec_model_name = argv[++i];
        } else if (arg == "--cls_model_dir" && i + 1 < argc) {
            cls_model_dir = argv[++i];
        } else if (arg == "--cls_model_name" && i + 1 < argc) {
            cls_model_name = argv[++i];
        } else if (arg == "--cpu_threads" && i + 1 < argc) {
            cpu_threads = std::stoi(argv[++i]);
        } else if (arg == "--use_doc_orientation" && i + 1 < argc) {
            use_doc_orientation = (std::string(argv[++i]) == "true");
        } else if (arg == "--use_doc_unwarping" && i + 1 < argc) {
            use_doc_unwarping = (std::string(argv[++i]) == "true");
        } else if (arg == "--use_textline_orientation" && i + 1 < argc) {
            use_textline_orientation = (std::string(argv[++i]) == "true");
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            PrintUsage();
            return 1;
        }
    }

    // Auto-detect models from model_dir if specific model dirs not provided
    struct stat st;
    if (!model_dir.empty()) {
        if (det_model_dir.empty()) {
            std::string dir = model_dir + "/PP-OCRv4_mobile_det_infer";
            if (stat(dir.c_str(), &st) == 0) {
                det_model_dir = dir;
                if (det_model_name.empty()) det_model_name = "PP-OCRv4_mobile_det";
            }
        }
        if (rec_model_dir.empty()) {
            std::string dir = model_dir + "/PP-OCRv4_mobile_rec_infer";
            if (stat(dir.c_str(), &st) == 0) {
                rec_model_dir = dir;
                if (rec_model_name.empty()) rec_model_name = "PP-OCRv4_mobile_rec";
            }
        }
        if (cls_model_dir.empty()) {
            std::string dir = model_dir + "/PP-LCNet_x1_0_doc_ori_infer";
            if (stat(dir.c_str(), &st) == 0) {
                cls_model_dir = dir;
                if (cls_model_name.empty()) cls_model_name = "PP-LCNet_x1_0_doc_ori";
            }
        }
    }

    // Set default model names if not specified
    if (det_model_name.empty()) det_model_name = "PP-OCRv4_mobile_det";
    if (rec_model_name.empty()) rec_model_name = "PP-OCRv4_mobile_rec";
    if (cls_model_name.empty()) cls_model_name = "PP-LCNet_x1_0_doc_ori";

    // Validate required models
    if (det_model_dir.empty() || rec_model_dir.empty()) {
        std::cerr << "Error: Detection and recognition models are required" << std::endl;
        std::cerr << "  Use --model_dir to auto-detect, or --det_model_dir and --rec_model_dir" << std::endl;
        PrintUsage();
        return 1;
    }

    // Initialize PaddleOCR
    std::cerr << "[Worker] Initializing PaddleOCR..." << std::endl;
    std::cerr << "[Worker] Det model: " << det_model_dir << " (" << det_model_name << ")" << std::endl;
    std::cerr << "[Worker] Rec model: " << rec_model_dir << " (" << rec_model_name << ")" << std::endl;
    if (!cls_model_dir.empty()) {
        std::cerr << "[Worker] Cls model: " << cls_model_dir << " (" << cls_model_name << ")" << std::endl;
    }
    std::cerr << "[Worker] CPU threads: " << cpu_threads << std::endl;
    std::cerr << "[Worker] Doc orientation: " << (use_doc_orientation ? "true" : "false") << std::endl;
    std::cerr << "[Worker] Doc unwarping: " << (use_doc_unwarping ? "true" : "false") << std::endl;
    std::cerr << "[Worker] Textline orientation: " << (use_textline_orientation ? "true" : "false") << std::endl;

    try {
        PaddleOCRParams params;

        // Set model paths
        params.text_detection_model_dir = det_model_dir;
        params.text_detection_model_name = det_model_name;
        params.text_recognition_model_dir = rec_model_dir;
        params.text_recognition_model_name = rec_model_name;

        if (!cls_model_dir.empty()) {
            params.doc_orientation_classify_model_dir = cls_model_dir;
            params.doc_orientation_classify_model_name = cls_model_name;
        }

        params.use_doc_orientation_classify = use_doc_orientation;
        params.use_doc_unwarping = use_doc_unwarping;
        params.use_textline_orientation = use_textline_orientation;
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
