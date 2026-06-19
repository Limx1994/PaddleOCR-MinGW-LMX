// plate_detector.cc - License plate detector implementation using ONNX Runtime

// Define SAL annotations for MinGW compatibility
#ifndef _Frees_ptr_opt_
#define _Frees_ptr_opt_
#endif
#ifndef _In_
#define _In_
#endif
#ifndef _Out_
#define _Out_
#endif
#ifndef _Inout_
#define _Inout_
#endif

#include "plate_detector.h"
#include "src/utils/ilogger.h"
#include <onnxruntime_cxx_api.h>
#include <algorithm>
#include <cmath>

PlateDetector::PlateDetector() = default;

PlateDetector::~PlateDetector() = default;

bool PlateDetector::Init(const std::string& model_path,
                         float confidence_threshold,
                         float nms_threshold) {
    try {
        INFO("Initializing PlateDetector...");
        INFO("Model path: %s", model_path.c_str());

        confidence_threshold_ = confidence_threshold;
        nms_threshold_ = nms_threshold;

        // Create ONNX Runtime environment
        env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "PlateDetector");

        // Create session options
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // Create session - convert path to wide string for Windows
        std::wstring wide_model_path(model_path.begin(), model_path.end());
        session_ = std::make_unique<Ort::Session>(*env_, wide_model_path.c_str(), session_options);

        // Get input/output info
        size_t num_input_nodes = session_->GetInputCount();
        size_t num_output_nodes = session_->GetOutputCount();

        INFO("Model inputs: %zu, outputs: %zu", num_input_nodes, num_output_nodes);

        // Check if this is RT-DETR format (2 outputs) or YOLO format (1 output)
        if (num_output_nodes >= 2) {
            is_rtdetr_ = true;
            INFO("Model format: RT-DETR");
        } else {
            is_rtdetr_ = false;
            INFO("Model format: YOLO");
        }

        // Get input shape
        Ort::TypeInfo input_type_info = session_->GetInputTypeInfo(0);
        auto input_tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
        std::vector<int64_t> input_shape = input_tensor_info.GetShape();

        INFO("Input shape: [%lld, %lld, %lld, %lld]",
             input_shape[0], input_shape[1], input_shape[2], input_shape[3]);

        if (input_shape.size() == 4) {
            input_height_ = (int)input_shape[2];
            input_width_ = (int)input_shape[3];
        }

        initialized_ = true;
        INFO("PlateDetector initialized successfully");
        return true;

    } catch (const Ort::Exception& e) {
        INFOE("Failed to initialize PlateDetector: %s", e.what());
        return false;
    } catch (const std::exception& e) {
        INFOE("Failed to initialize PlateDetector: %s", e.what());
        return false;
    }
}

std::vector<float> PlateDetector::Preprocess(const cv::Mat& frame) {
    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(input_width_, input_height_));

    // Convert to float and normalize to [0, 1]
    cv::Mat float_img;
    resized.convertTo(float_img, CV_32F, 1.0 / 255.0);

    // Convert HWC to CHW format
    std::vector<float> input_tensor(1 * 3 * input_height_ * input_width_);
    int channel_size = input_height_ * input_width_;

    for (int c = 0; c < 3; c++) {
        for (int h = 0; h < input_height_; h++) {
            for (int w = 0; w < input_width_; w++) {
                input_tensor[c * channel_size + h * input_width_ + w] =
                    float_img.at<cv::Vec3f>(h, w)[c];
            }
        }
    }

    return input_tensor;
}

std::vector<Detection> PlateDetector::PostprocessRTDETR(
    const std::vector<float>& logits,
    const std::vector<float>& boxes,
    const cv::Size& orig_size) {

    std::vector<Detection> detections;
    int num_detections = 300;  // RT-DETR typically outputs 300 detections

    for (int i = 0; i < num_detections; i++) {
        float score = logits[i];

        // Filter by confidence
        if (score < confidence_threshold_) {
            continue;
        }

        // Get bounding box (cx, cy, w, h format, normalized to [0, 1])
        float cx = boxes[i * 4 + 0];
        float cy = boxes[i * 4 + 1];
        float w = boxes[i * 4 + 2];
        float h = boxes[i * 4 + 3];

        // Convert to original image coordinates
        int left = (int)((cx - w / 2) * orig_size.width);
        int top = (int)((cy - h / 2) * orig_size.height);
        int width = (int)(w * orig_size.width);
        int height = (int)(h * orig_size.height);

        // Clip to image bounds
        left = std::max(0, std::min(left, orig_size.width - 1));
        top = std::max(0, std::min(top, orig_size.height - 1));
        width = std::min(width, orig_size.width - left);
        height = std::min(height, orig_size.height - top);

        Detection det;
        det.box = cv::Rect(left, top, width, height);
        det.confidence = score;
        det.class_id = 0;

        detections.push_back(det);
    }

    return detections;
}

std::vector<Detection> PlateDetector::PostprocessYOLO(
    const std::vector<float>& output,
    const cv::Size& orig_size) {

    std::vector<Detection> detections;

    // YOLOv8 output: [1, 4+num_classes, num_detections]
    int num_detections = input_width_ * input_height_ / 64;  // Approximate
    int num_classes = 1;  // License plate class

    float scale_x = (float)orig_size.width / input_width_;
    float scale_y = (float)orig_size.height / input_height_;

    for (int i = 0; i < num_detections; i++) {
        float max_score = 0;
        int max_class_id = 0;

        for (int j = 4; j < 4 + num_classes; j++) {
            float score = output[j * num_detections + i];
            if (score > max_score) {
                max_score = score;
                max_class_id = j - 4;
            }
        }

        if (max_score < confidence_threshold_) {
            continue;
        }

        float cx = output[0 * num_detections + i];
        float cy = output[1 * num_detections + i];
        float w = output[2 * num_detections + i];
        float h = output[3 * num_detections + i];

        int left = (int)((cx - w / 2) * scale_x);
        int top = (int)((cy - h / 2) * scale_y);
        int width = (int)(w * scale_x);
        int height = (int)(h * scale_y);

        left = std::max(0, std::min(left, orig_size.width - 1));
        top = std::max(0, std::min(top, orig_size.height - 1));
        width = std::min(width, orig_size.width - left);
        height = std::min(height, orig_size.height - top);

        Detection det;
        det.box = cv::Rect(left, top, width, height);
        det.confidence = max_score;
        det.class_id = max_class_id;

        detections.push_back(det);
    }

    return detections;
}

std::vector<Detection> PlateDetector::ApplyNMS(
    std::vector<Detection>& detections) {

    if (detections.empty()) {
        return {};
    }

    // Sort by confidence (descending)
    std::sort(detections.begin(), detections.end(),
              [](const Detection& a, const Detection& b) {
                  return a.confidence > b.confidence;
              });

    std::vector<bool> suppressed(detections.size(), false);
    std::vector<Detection> result;

    for (size_t i = 0; i < detections.size(); i++) {
        if (suppressed[i]) continue;

        result.push_back(detections[i]);

        // Suppress overlapping boxes
        for (size_t j = i + 1; j < detections.size(); j++) {
            if (suppressed[j]) continue;

            // Calculate IoU
            cv::Rect intersection = detections[i].box & detections[j].box;
            float intersection_area = (float)intersection.area();
            float union_area = (float)(detections[i].box.area() +
                                       detections[j].box.area() -
                                       intersection_area);

            float iou = (union_area > 0) ? (intersection_area / union_area) : 0;

            if (iou > nms_threshold_) {
                suppressed[j] = true;
            }
        }
    }

    return result;
}

std::vector<Detection> PlateDetector::Detect(const cv::Mat& frame) {
    if (!initialized_ || frame.empty() || !session_) {
        return {};
    }

    try {
        // Preprocess
        std::vector<float> input_tensor = Preprocess(frame);

        // Create input tensor
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
            OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

        std::vector<int64_t> input_shape = {1, 3, input_height_, input_width_};
        Ort::Value input_ort_value = Ort::Value::CreateTensor<float>(
            memory_info, input_tensor.data(), input_tensor.size(),
            input_shape.data(), input_shape.size());

        // Get input/output names
        Ort::AllocatorWithDefaultOptions allocator;
        auto input_name = session_->GetInputNameAllocated(0, allocator);
        auto output_name = session_->GetOutputNameAllocated(0, allocator);

        std::vector<const char*> input_names = {input_name.get()};
        std::vector<const char*> output_names;

        // For RT-DETR, we need both outputs
        if (is_rtdetr_) {
            auto output_name2 = session_->GetOutputNameAllocated(1, allocator);
            output_names = {output_name.get(), output_name2.get()};
        } else {
            output_names = {output_name.get()};
        }

        // Run inference
        auto output_tensors = session_->Run(
            Ort::RunOptions{nullptr},
            input_names.data(), &input_ort_value, 1,
            output_names.data(), output_names.size());

        // Process output
        std::vector<Detection> detections;

        if (is_rtdetr_) {
            // RT-DETR format: logits [1, 300, 1] + pred_boxes [1, 300, 4]
            const float* logits_data = output_tensors[0].GetTensorMutableData<float>();
            const float* boxes_data = output_tensors[1].GetTensorMutableData<float>();

            std::vector<float> logits(logits_data, logits_data + 300);
            std::vector<float> boxes(boxes_data, boxes_data + 300 * 4);

            detections = PostprocessRTDETR(logits, boxes, frame.size());
        } else {
            // YOLO format: [1, 4+num_classes, num_detections]
            const float* output_data = output_tensors[0].GetTensorMutableData<float>();
            auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();

            size_t output_size = 1;
            for (auto dim : output_shape) {
                output_size *= dim;
            }

            std::vector<float> output(output_data, output_data + output_size);
            detections = PostprocessYOLO(output, frame.size());
        }

        // Apply NMS
        detections = ApplyNMS(detections);

        return detections;

    } catch (const Ort::Exception& e) {
        INFOE("Detection failed: %s", e.what());
        return {};
    } catch (const std::exception& e) {
        INFOE("Detection failed: %s", e.what());
        return {};
    }
}

cv::Mat PlateDetector::CropPlate(const cv::Mat& frame, int padding) {
    auto detections = Detect(frame);

    if (detections.empty()) {
        return cv::Mat();
    }

    // Find largest detection
    auto largest = std::max_element(detections.begin(), detections.end(),
                                    [](const Detection& a, const Detection& b) {
                                        return a.box.area() < b.box.area();
                                    });

    // Apply padding
    cv::Rect roi = largest->box;
    roi.x = std::max(0, roi.x - padding);
    roi.y = std::max(0, roi.y - padding);
    roi.width = std::min(frame.cols - roi.x, roi.width + 2 * padding);
    roi.height = std::min(frame.rows - roi.y, roi.height + 2 * padding);

    return frame(roi).clone();
}

std::vector<cv::Mat> PlateDetector::CropAllPlates(const cv::Mat& frame,
                                                   int padding) {
    auto detections = Detect(frame);
    std::vector<cv::Mat> plates;

    for (const auto& det : detections) {
        cv::Rect roi = det.box;
        roi.x = std::max(0, roi.x - padding);
        roi.y = std::max(0, roi.y - padding);
        roi.width = std::min(frame.cols - roi.x, roi.width + 2 * padding);
        roi.height = std::min(frame.rows - roi.y, roi.height + 2 * padding);

        plates.push_back(frame(roi).clone());
    }

    return plates;
}
