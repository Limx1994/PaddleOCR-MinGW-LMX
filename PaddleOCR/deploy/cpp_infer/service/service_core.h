#pragma once

#include "process_pool.h"
#include "plate_detector.h"
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <atomic>

using json = nlohmann::json;

struct ServiceConfig {
    std::string host = "127.0.0.1";
    int port = 8080;
    int thread_num = 4;

    // Model directory (auto-detect models)
    std::string model_dir;

    // Specific model directories (optional, override model_dir auto-detect)
    std::string det_model_dir;
    std::string det_model_name = "PP-OCRv4_mobile_det";
    std::string rec_model_dir;
    std::string rec_model_name = "PP-OCRv4_mobile_rec";
    std::string cls_model_dir;
    std::string cls_model_name = "PP-LCNet_x1_0_doc_ori";

    bool use_doc_orientation = true;
    bool use_doc_unwarping = false;
    bool use_textline_orientation = false;
    int cpu_threads = 8;
    int pool_size = 2;  // Number of worker processes

    // Fast detection mode for plate recognition
    std::string fast_detect = "none";  // "none", "yolo"
    std::string plate_model_path;      // Path to YOLOv8-nano plate detection model
    float plate_conf_threshold = 0.5f; // Confidence threshold for plate detection
    float plate_nms_threshold = 0.4f;  // NMS threshold for plate detection
};

class OCRServiceCore {
public:
    OCRServiceCore();
    ~OCRServiceCore();

    // Initialize service
    bool Init(const ServiceConfig& config);

    // Process OCR request
    json ProcessOCR(const std::string& image_data, const json& params);

    // Get service status
    json GetStatus();

    // Reload models (restart all workers)
    bool ReloadModels();

    // Get config
    const ServiceConfig& GetConfig() const { return config_; }

private:
    // Decode image from base64
    cv::Mat DecodeImage(const std::string& image_data);

    // Save temp file
    std::string SaveTempFile(const cv::Mat& image);

    // Cleanup temp files
    void CleanupTempFiles();

private:
    ServiceConfig config_;
    std::unique_ptr<ProcessPool> pool_;
    std::unique_ptr<PlateDetector> plate_detector_;
    bool initialized_ = false;
    std::mutex mutex_;
};
