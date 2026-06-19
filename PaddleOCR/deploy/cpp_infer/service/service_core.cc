#include "service_core.h"
#include "base64_utils.h"
#include "src/utils/ilogger.h"
#include <opencv2/opencv.hpp>
#include <chrono>
#include <sys/stat.h>
#include <dirent.h>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define mkdir _mkdir
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#endif

OCRServiceCore::OCRServiceCore() = default;

OCRServiceCore::~OCRServiceCore() {
    if (pool_) {
        pool_->Shutdown();
    }
    CleanupTempFiles();
}

bool OCRServiceCore::Init(const ServiceConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    config_ = config;

    INFO("Initializing OCR Service (process pool mode)...");
    INFO("Host: %s", config_.host.c_str());
    INFO("Port: %d", config_.port);
    INFO("Model dir: %s", config_.model_dir.c_str());
    INFO("Pool size: %d", config_.pool_size);
    INFO("Fast detect: %s", config_.fast_detect.c_str());

    // Initialize plate detector if fast detection is enabled
    if (config_.fast_detect == "yolo") {
        if (config_.plate_model_path.empty()) {
            INFOE("Plate model path is required for yolo fast detection");
            return false;
        }

        plate_detector_ = std::unique_ptr<PlateDetector>(new PlateDetector());
        if (!plate_detector_->Init(config_.plate_model_path,
                                   config_.plate_conf_threshold,
                                   config_.plate_nms_threshold)) {
            INFOE("Failed to initialize plate detector");
            plate_detector_.reset();
            return false;
        }
        INFO("Plate detector initialized successfully");
    }

    // Create and initialize process pool
    pool_ = std::unique_ptr<ProcessPool>(new ProcessPool());

    PoolConfig pool_config;
    pool_config.pool_size = config_.pool_size;
    pool_config.model_dir = config_.model_dir;
    pool_config.cpu_threads = config_.cpu_threads;
    pool_config.use_doc_orientation = config_.use_doc_orientation;

    if (!pool_->Init(pool_config)) {
        INFOE("Failed to initialize process pool");
        pool_.reset();
        return false;
    }

    initialized_ = true;
    INFO("OCR Service initialized successfully (process pool mode)");
    return true;
}

json OCRServiceCore::ProcessOCR(const std::string& image_data,
                                 const json& params) {
    if (!initialized_ || !pool_) {
        return {{"code", -1}, {"message", "Service not initialized"}};
    }

    try {
        // Decode image
        cv::Mat image = DecodeImage(image_data);
        if (image.empty()) {
            return {{"code", -1}, {"message", "Failed to decode image"}};
        }

        // Fast detection mode - crop plate region before OCR
        if (plate_detector_ && plate_detector_->IsInitialized()) {
            auto start = std::chrono::high_resolution_clock::now();

            cv::Mat plate = plate_detector_->CropPlate(image);

            auto end = std::chrono::high_resolution_clock::now();
            double detect_ms = std::chrono::duration<double, std::milli>(
                end - start).count();

            if (!plate.empty()) {
                INFO("Plate detected in %.2f ms, using cropped region", detect_ms);
                image = plate;
            } else {
                INFO("No plate detected, using original image");
            }
        }

        // Save temp file
        std::string tmp_path = SaveTempFile(image);

        // Process via worker pool
        json result = pool_->ProcessOCR(tmp_path);

        // Cleanup temp file
        try {
            remove(tmp_path.c_str());
        } catch (...) {}

        return result;

    } catch (const std::exception& e) {
        INFOE("OCR processing failed: %s", e.what());
        return {{"code", -1}, {"message", e.what()}};
    }
}

json OCRServiceCore::GetStatus() {
    if (!pool_) {
        return {
            {"initialized", false},
            {"message", "Process pool not created"}
        };
    }

    json pool_status = pool_->GetStatus();

    return {
        {"initialized", initialized_},
        {"mode", "process_pool"},
        {"pool", pool_status},
        {"fast_detect", config_.fast_detect},
        {"plate_detector", plate_detector_ ? plate_detector_->IsInitialized() : false},
        {"config", {
            {"host", config_.host},
            {"port", config_.port},
            {"model_dir", config_.model_dir},
            {"pool_size", config_.pool_size},
            {"use_doc_orientation", config_.use_doc_orientation},
            {"use_doc_unwarping", config_.use_doc_unwarping},
            {"use_textline_orientation", config_.use_textline_orientation},
            {"fast_detect", config_.fast_detect},
            {"plate_model_path", config_.plate_model_path}
        }}
    };
}

bool OCRServiceCore::ReloadModels() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!pool_) {
        return false;
    }

    INFO("Reloading models (restarting all workers)...");
    return pool_->RestartAll();
}

cv::Mat OCRServiceCore::DecodeImage(const std::string& image_data) {
    try {
        // If base64 encoded
        if (image_data.find("data:image") != std::string::npos) {
            // Extract base64 part
            size_t pos = image_data.find(",");
            if (pos != std::string::npos) {
                std::string base64 = image_data.substr(pos + 1);
                std::vector<uchar> data = Base64Utils::Decode(base64);
                return cv::imdecode(data, cv::IMREAD_COLOR);
            }
        }

        // If pure base64
        std::vector<uchar> data = Base64Utils::Decode(image_data);
        if (!data.empty()) {
            return cv::imdecode(data, cv::IMREAD_COLOR);
        }

        // If file path
        if (access(image_data.c_str(), F_OK) == 0) {
            return cv::imread(image_data, cv::IMREAD_COLOR);
        }

        return cv::Mat();
    } catch (const std::exception& e) {
        INFOE("Failed to decode image: %s", e.what());
        return cv::Mat();
    }
}

std::string OCRServiceCore::SaveTempFile(const cv::Mat& image) {
    // Create temp directory
    std::string tmp_dir = "tmp";
    if (access(tmp_dir.c_str(), F_OK) != 0) {
        mkdir(tmp_dir.c_str());
    }

    // Generate unique filename
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    std::string filename = tmp_dir + "/ocr_" +
        std::to_string(timestamp) + ".jpg";

    // Save image
    cv::imwrite(filename, image);

    return filename;
}

void OCRServiceCore::CleanupTempFiles() {
    try {
        std::string tmp_dir = "tmp";
        if (access(tmp_dir.c_str(), F_OK) == 0) {
            DIR* dir = opendir(tmp_dir.c_str());
            if (dir) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != NULL) {
                    std::string name = entry->d_name;
                    if (name.find(".jpg") != std::string::npos ||
                        name.find(".png") != std::string::npos) {
                        std::string path = tmp_dir + "/" + name;
                        remove(path.c_str());
                    }
                }
                closedir(dir);
            }
        }
    } catch (const std::exception& e) {
        INFOW("Failed to cleanup temp files: %s", e.what());
    }
}
