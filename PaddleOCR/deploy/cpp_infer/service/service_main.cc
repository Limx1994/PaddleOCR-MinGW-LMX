#include "service_core.h"
#include "tcp_server.h"
#include "base64_utils.h"
#include "src/utils/cpu_features.h"
#include <iostream>
#include <signal.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static TcpServer* g_server = nullptr;

void signal_handler(int signal) {
    if (g_server) {
        g_server->Stop();
    }
    std::cout << "Server stopped" << std::endl;
    exit(0);
}

void print_usage() {
    std::cout << "Usage: ppocr_service [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --host <host>                    Server host (default: 127.0.0.1)" << std::endl;
    std::cout << "  --port <port>                    Server port (default: 8080)" << std::endl;
    std::cout << "  --model_dir <dir>                Model directory (auto-detect PP-OCRv4_mobile models)" << std::endl;
    std::cout << "  --det_model_dir <dir>            Detection model directory" << std::endl;
    std::cout << "  --det_model_name <name>          Detection model name (default: PP-OCRv4_mobile_det)" << std::endl;
    std::cout << "  --rec_model_dir <dir>            Recognition model directory" << std::endl;
    std::cout << "  --rec_model_name <name>          Recognition model name (default: PP-OCRv4_mobile_rec)" << std::endl;
    std::cout << "  --cls_model_dir <dir>            Classification model directory" << std::endl;
    std::cout << "  --cls_model_name <name>          Classification model name (default: PP-LCNet_x1_0_doc_ori)" << std::endl;
    std::cout << "  --cpu_threads <num>              CPU threads per worker (default: 8)" << std::endl;
    std::cout << "  --pool_size <num>                Number of worker processes (default: 2)" << std::endl;
    std::cout << "  --use_doc_orientation <bool>     Use document orientation (default: true)" << std::endl;
    std::cout << "  --use_doc_unwarping <bool>       Use document unwarping (default: false)" << std::endl;
    std::cout << "  --use_textline_orientation <bool> Use textline orientation (default: false)" << std::endl;
    std::cout << "  --fast_detect <mode>             Fast detection mode: none, yolo (default: none)" << std::endl;
    std::cout << "  --plate_model <path>             Path to plate detection ONNX model" << std::endl;
    std::cout << "  --help                           Show this help" << std::endl;
}

int main(int argc, char* argv[]) {
    // Detect CPU features
    auto cpu = CPUFeatures::Detect();
    std::cout << "========================================" << std::endl;
    std::cout << "CPU Features Detection" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << cpu.ToString() << std::endl;
    std::cout << "========================================" << std::endl;

    // Parse command line arguments
    ServiceConfig config;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = std::stoi(argv[++i]);
        } else if (arg == "--model_dir" && i + 1 < argc) {
            config.model_dir = argv[++i];
        } else if (arg == "--det_model_dir" && i + 1 < argc) {
            config.det_model_dir = argv[++i];
        } else if (arg == "--det_model_name" && i + 1 < argc) {
            config.det_model_name = argv[++i];
        } else if (arg == "--rec_model_dir" && i + 1 < argc) {
            config.rec_model_dir = argv[++i];
        } else if (arg == "--rec_model_name" && i + 1 < argc) {
            config.rec_model_name = argv[++i];
        } else if (arg == "--cls_model_dir" && i + 1 < argc) {
            config.cls_model_dir = argv[++i];
        } else if (arg == "--cls_model_name" && i + 1 < argc) {
            config.cls_model_name = argv[++i];
        } else if (arg == "--cpu_threads" && i + 1 < argc) {
            config.cpu_threads = std::stoi(argv[++i]);
        } else if (arg == "--pool_size" && i + 1 < argc) {
            config.pool_size = std::stoi(argv[++i]);
        } else if (arg == "--use_doc_orientation" && i + 1 < argc) {
            config.use_doc_orientation = (std::string(argv[++i]) == "true");
        } else if (arg == "--use_doc_unwarping" && i + 1 < argc) {
            config.use_doc_unwarping = (std::string(argv[++i]) == "true");
        } else if (arg == "--use_textline_orientation" && i + 1 < argc) {
            config.use_textline_orientation = (std::string(argv[++i]) == "true");
        } else if (arg == "--fast_detect" && i + 1 < argc) {
            config.fast_detect = argv[++i];
        } else if (arg == "--plate_model" && i + 1 < argc) {
            config.plate_model_path = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            print_usage();
            return 1;
        }
    }

    // Initialize service core
    auto core = std::make_shared<OCRServiceCore>();
    if (!core->Init(config)) {
        std::cerr << "Failed to initialize OCR service" << std::endl;
        return 1;
    }

    // Create TCP server
    TcpServer server;
    g_server = &server;

    // Set request handler
    server.SetRequestHandler([core](const RequestMessage& req) -> ResponseMessage {
        ResponseMessage res;
        res.id = req.id;

        try {
            if (req.method == "ocr") {
                // OCR request
                json params = json::parse(req.params.empty() ? "{}" : req.params);
                json result = core->ProcessOCR(req.image_data, params);

                res.code = result.value("code", 0);
                res.message = result.value("message", "success");
                res.data = result.dump();
            } else if (req.method == "status") {
                // Status request
                json status = core->GetStatus();
                res.code = 0;
                res.message = "success";
                res.data = status.dump();
            } else if (req.method == "reload") {
                // Reload request
                bool success = core->ReloadModels();
                res.code = success ? 0 : -1;
                res.message = success ? "success" : "failed";
                res.data = "{}";
            } else {
                res.code = -1;
                res.message = "Unknown method: " + req.method;
                res.data = "{}";
            }
        } catch (const std::exception& e) {
            res.code = -1;
            res.message = e.what();
            res.data = "{}";
        }

        return res;
    });

    // Set signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Start server
    if (!server.Start(config.host, config.port, config.thread_num)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "OCR Service running on " << config.host
              << ":" << config.port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;

    // Wait for server to stop
    while (server.IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
