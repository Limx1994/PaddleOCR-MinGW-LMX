[English](README.md) | [简体中文](README_ch.md)

# PaddleOCR C++ Inference Engine (MinGW)

This document describes how to build and run the PaddleOCR C++ inference engine using MinGW GCC on Windows.

## Table of Contents

- [Environment Requirements](#environment-requirements)
- [Build Steps](#build-steps)
- [Run Inference](#run-inference)
  - [General OCR](#general-ocr)
  - [License Plate Recognition](#license-plate-recognition)
- [Model Preparation](#model-preparation)
- [Known Issues](#known-issues)
- [Troubleshooting](#troubleshooting)

## Environment Requirements

| Component | Version | Notes |
|-----------|---------|-------|
| MinGW GCC | 11.2.0+ | gcc, g++, mingw32-make |
| CMake | 3.14+ | Build system generator (supports CMake 4.x) |
| Paddle Inference | Latest | GCC-compiled version (with oneDNN) |
| OpenCV | 4.7.0 | GCC-compiled version |
| oneDNN | 3.6.2 | Optional, for MKLDNN acceleration |

## Build Steps

1. **Configure paths** in `build_mingw.bat`:

   ```batch
   set MINGW_DIR=D:\path\to\mingw
   set PADDLE_LIB=D:\path\to\paddle_inference_gcc
   set OPENCV_DIR=D:\path\to\opencv_install_gcc
   ```

2. **Run build**:

   ```batch
   cd deploy\cpp_infer
   build_mingw.bat
   ```

3. **Output**: `build_mingw\ppocr.exe`

### Build with DLL Mode

To build with DLL mode (smaller executable, 4-DLL split):

```batch
cd deploy\cpp_infer
mkdir build_mingw_dll && cd build_mingw_dll
cmake .. -G Ninja ^
  -DCMAKE_C_COMPILER=D:\path\to\mingw\bin\gcc.exe ^
  -DCMAKE_CXX_COMPILER=D:\path\to\mingw\bin\g++.exe ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=D:\path\to\mingw\bin\ninja.exe ^
  -DPADDLE_LIB=D:\path\to\paddle_inference_gcc ^
  -DOPENCV_DIR=D:\path\to\opencv_install_gcc ^
  -DWITH_MKL=OFF ^
  -DWITH_GPU=OFF ^
  -DWITH_DLL_LIB=ON
ninja -j16
```

Output: `build_mingw_dll\ppocr.exe` (2.2MB, requires DLLs in runtime directory)

## Run Inference

### General OCR

```bash
cd dist\ppocr

ppocr.exe ocr \
  --input <image_path> \
  --text_detection_model_dir ./models/PP-OCRv4_mobile_det_infer \
  --text_detection_model_name PP-OCRv4_mobile_det \
  --text_recognition_model_dir ./models/PP-OCRv4_mobile_rec_infer \
  --text_recognition_model_name PP-OCRv4_mobile_rec \
  --use_doc_orientation_classify false \
  --use_doc_unwarping false \
  --use_textline_orientation false
```

**Inference modes** (`--run_mode`):
- `paddle` — Native Paddle mode (default, faster for mobile models)
- `mkldnn` — oneDNN acceleration (may help for larger models)
- `mkldnn_bf16` — oneDNN with BF16 precision (requires CPU support)

**Pipeline parallelism** (`--thread_num`):
- Default: 4 (processes multiple images concurrently)
- Set to 1 for single-image sequential processing

### License Plate Recognition

The same PP-OCRv4 models can recognize license plates (Chinese province abbreviations + letters + digits):

```bash
FLAGS_enable_memory_stats=false FLAGS_allocator_strategy=auto_growth \
./ppocr.exe ocr \
  --input <plate_image> \
  --text_detection_model_dir ./models/ch_PP-OCRv4_det_infer \
  --text_detection_model_name PP-OCRv4_mobile_det \
  --text_recognition_model_dir ./models/ch_PP-OCRv4_rec_infer \
  --text_recognition_model_name PP-OCRv4_mobile_rec \
  --use_doc_orientation_classify false \
  --use_doc_unwarping false \
  --use_textline_orientation false \
  --cpu_threads 4
```

Example output (license plate: `赣G·0522Y`):
```json
{
  "rec_texts": ["赣G·0522Y"],
  "rec_scores": [0.999318]
}
```

## Model Preparation

Download PP-OCRv4 models and place them in `build_mingw/models/`:

```
build_mingw/models/
├── ch_PP-OCRv4_det_infer/              # Text detection model
│   ├── inference.pdmodel
│   ├── inference.pdiparams
│   └── inference.yml
├── ch_PP-OCRv4_rec_infer/              # Text recognition model
│   ├── inference.pdmodel
│   ├── inference.pdiparams
│   └── inference.yml
└── PP-LCNet_x1_0_textline_ori_infer/   # Text line orientation classifier (optional)
    ├── inference.pdmodel
    ├── inference.pdiparams
    └── inference.yml
```

**Download links:**
- Text detection: [ch_PP-OCRv4_det_infer](https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/ch_PP-OCRv4_det_infer.tar)
- Text recognition: [ch_PP-OCRv4_rec_infer](https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/ch_PP-OCRv4_rec_infer.tar)
- Text line orientation: [PP-LCNet_x1_0_textline_ori_infer](https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/PP-LCNet_x1_0_textline_ori_infer.tar)

## CPU Feature Detection

The program automatically detects CPU features at startup and displays:
- CPU model and vendor
- Supported SIMD instructions (SSE, AVX, AVX2, AVX-512)
- Recommended compiler flags for optimal performance

Example output:
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

## Known Issues

| Issue | Impact | Workaround |
|-------|--------|------------|
| OpenCV imwrite warning | Result images may not be saved | OCR inference works correctly; image saving is best-effort |
| CMake 4.x compatibility | Some old cmake files need updates | Add `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` |

## Performance

Test environment: Intel CPU, 20 logical processors, PP-OCRv4_mobile models, 800×1079 images.

| Mode | 1 image | 10 images |
|------|---------|-----------|
| paddle (default) | 1.61s | 1.63s |
| mkldnn | 1.68s | 3.55s |

For PP-OCRv4_mobile models, `paddle` mode is ~2× faster. For larger models (server), `mkldnn` may be beneficial.

## Troubleshooting

### CMake configuration fails
- Verify MinGW GCC is in PATH or set `CMAKE_C_COMPILER`/`CMAKE_CXX_COMPILER` explicitly
- Ensure Paddle Inference and OpenCV are GCC-compiled versions

### TLS destructor crash (heap corruption)
- This issue has been resolved with immortal singleton + char[] buffer pattern
- If you encounter RtlFreeHeap crashes, ensure you're using the patched Paddle Inference library

### Link errors with multiple definitions
- The build uses `-Wl,--allow-multiple-definition` to handle symbol conflicts
- If new conflicts arise, add object files to the merge list in CMakeLists.txt
