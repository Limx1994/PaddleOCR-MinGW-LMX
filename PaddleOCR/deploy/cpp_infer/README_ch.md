[English](README.md) | [简体中文](README_ch.md)

# PaddleOCR C++ 推理引擎（MinGW）

本文档介绍如何使用 MinGW GCC 在 Windows 上编译和运行 PaddleOCR C++ 推理引擎。

## 目录

- [环境要求](#环境要求)
- [编译步骤](#编译步骤)
- [运行推理](#运行推理)
  - [通用 OCR](#通用-ocr)
  - [车牌识别](#车牌识别)
- [模型准备](#模型准备)
- [已知问题](#已知问题)
- [故障排除](#故障排除)

## 环境要求

| 组件 | 版本 | 说明 |
|------|------|------|
| MinGW GCC | 11.2.0+ | gcc, g++, mingw32-make |
| CMake | 3.14+ | 构建系统生成器 |
| Paddle Inference | 最新版 | GCC 编译版本 |
| OpenCV | 4.7.0 | GCC 编译版本 |

## 编译步骤

1. **配置路径**（编辑 `build_mingw.bat`）：

   ```batch
   set MINGW_DIR=D:\path\to\mingw
   set PADDLE_LIB=D:\path\to\paddle_inference_gcc
   set OPENCV_DIR=D:\path\to\opencv_install_gcc
   ```

2. **执行编译**：

   ```batch
   cd deploy\cpp_infer
   build_mingw.bat
   ```

3. **输出文件**：`build_mingw\ppocr.exe`

## 运行推理

### 通用 OCR

```bash
cd build_mingw

FLAGS_enable_memory_stats=false FLAGS_allocator_strategy=auto_growth \
./ppocr.exe ocr \
  --input <图片路径> \
  --text_detection_model_dir ./models/ch_PP-OCRv4_det_infer \
  --text_detection_model_name PP-OCRv4_mobile_det \
  --text_recognition_model_dir ./models/ch_PP-OCRv4_rec_infer \
  --text_recognition_model_name PP-OCRv4_mobile_rec \
  --use_doc_orientation_classify false \
  --use_doc_unwarping false \
  --use_textline_orientation false \
  --cpu_threads 4
```

### 车牌识别

PP-OCRv4 通用模型可直接识别车牌（省份简称 + 字母 + 数字）：

```bash
FLAGS_enable_memory_stats=false FLAGS_allocator_strategy=auto_growth \
./ppocr.exe ocr \
  --input <车牌图片> \
  --text_detection_model_dir ./models/ch_PP-OCRv4_det_infer \
  --text_detection_model_name PP-OCRv4_mobile_det \
  --text_recognition_model_dir ./models/ch_PP-OCRv4_rec_infer \
  --text_recognition_model_name PP-OCRv4_mobile_rec \
  --use_doc_orientation_classify false \
  --use_doc_unwarping false \
  --use_textline_orientation false \
  --cpu_threads 4
```

识别示例（车牌：`赣G·0522Y`）：
```json
{
  "rec_texts": ["赣G·0522Y"],
  "rec_scores": [0.999318]
}
```

## 模型准备

下载 PP-OCRv4 模型并放置在 `build_mingw/models/` 目录下：

```
build_mingw/models/
├── ch_PP-OCRv4_det_infer/              # 文本检测模型
│   ├── inference.pdmodel
│   ├── inference.pdiparams
│   └── inference.yml
├── ch_PP-OCRv4_rec_infer/              # 文本识别模型
│   ├── inference.pdmodel
│   ├── inference.pdiparams
│   └── inference.yml
└── PP-LCNet_x1_0_textline_ori_infer/   # 文本行方向分类器（可选）
    ├── inference.pdmodel
    ├── inference.pdiparams
    └── inference.yml
```

**下载链接：**
- 文本检测：[ch_PP-OCRv4_det_infer](https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/ch_PP-OCRv4_det_infer.tar)
- 文本识别：[ch_PP-OCRv4_rec_infer](https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/ch_PP-OCRv4_rec_infer.tar)
- 文本行方向分类：[PP-LCNet_x1_0_textline_ori_infer](https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/PP-LCNet_x1_0_textline_ori_infer.tar)

## CPU 特性检测

程序启动时会自动检测 CPU 特性并显示：
- CPU 型号和厂商
- 支持的 SIMD 指令集（SSE、AVX、AVX2、AVX-512）
- 推荐的编译器标志以获得最佳性能

示例输出：
```
========================================
CPU Features Detection
========================================
CPU: Genuine Intel(R) 0000
Vendor: GenuineIntel
Family: 6, Model: 10, Stepping: 2
Logical Processors: 20
Available Threads: 9
SIMD Level: AVX2
Recommended Flags: -mavx2 -mfma -mf16c -mbmi -mbmi2
Features: SSE SSE2 SSE3 SSSE3 SSE4.1 SSE4.2 AVX AVX2 FMA F16C BMI1 BMI2 POPCNT AES PCLMULQDQ
========================================
```

## 已知问题

| 问题 | 影响 | 解决方案 |
|------|------|---------|
| OpenCV imwrite 警告 | 结果图片可能无法保存 | OCR 推理正常工作；图片保存为尽力而为 |
| MKLDNN 不可用 | 推理速度略慢 | 使用 `--cpu_threads` 控制并行数 |

## 故障排除

### CMake 配置失败
- 确认 MinGW GCC 已在 PATH 中，或显式设置 `CMAKE_C_COMPILER`/`CMAKE_CXX_COMPILER`
- 确保 Paddle Inference 和 OpenCV 是 GCC 编译版本

### TLS 析构崩溃（堆损坏）
- 此问题已通过永生单例 + char[] buffer 方案修复
- 如遇到 RtlFreeHeap 崩溃，请确保使用修补后的 Paddle Inference 库

### 多重定义链接错误
- 构建使用 `-Wl,--allow-multiple-definition` 处理符号冲突
- 如出现新的冲突，请在 CMakeLists.txt 中添加对象文件到合并列表
