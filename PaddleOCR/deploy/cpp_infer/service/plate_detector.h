// plate_detector.h - License plate detector using ONNX Runtime
// Fast license plate detection for preprocessing before OCR recognition

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>

// Forward declare ONNX Runtime types
namespace Ort {
    class Session;
    class Env;
    struct SessionOptions;
}

struct Detection {
    cv::Rect box;
    float confidence;
    int class_id;
};

class PlateDetector {
public:
    PlateDetector();
    ~PlateDetector();

    // Initialize detector with ONNX model
    bool Init(const std::string& model_path, float confidence_threshold = 0.5f,
              float nms_threshold = 0.4f);

    // Detect plates in image, returns list of detections
    std::vector<Detection> Detect(const cv::Mat& frame);

    // Crop the largest plate region from image
    // Returns empty Mat if no plate detected
    cv::Mat CropPlate(const cv::Mat& frame, int padding = 10);

    // Crop all detected plates
    std::vector<cv::Mat> CropAllPlates(const cv::Mat& frame, int padding = 10);

    // Check if detector is initialized
    bool IsInitialized() const { return initialized_; }

private:
    // Preprocess image
    std::vector<float> Preprocess(const cv::Mat& frame);

    // Postprocess model output (RT-DETR format)
    std::vector<Detection> PostprocessRTDETR(const std::vector<float>& logits,
                                              const std::vector<float>& boxes,
                                              const cv::Size& orig_size);

    // Postprocess model output (YOLOv8 format)
    std::vector<Detection> PostprocessYOLO(const std::vector<float>& output,
                                            const cv::Size& orig_size);

    // Apply NMS to detections
    std::vector<Detection> ApplyNMS(std::vector<Detection>& detections);

    // ONNX Runtime components
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;

    // Model parameters
    int input_width_ = 640;
    int input_height_ = 640;
    float confidence_threshold_ = 0.5f;
    float nms_threshold_ = 0.4f;
    bool initialized_ = false;
    bool is_rtdetr_ = false;  // Whether model is RT-DETR format
};
