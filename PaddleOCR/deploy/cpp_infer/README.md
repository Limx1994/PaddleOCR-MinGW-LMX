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
| Git LFS | Latest | Large file tracking (install before clone) |
| MinGW GCC | 11.2.0+ | gcc, g++, mingw32-make (included in toolchain/) |
| CMake | 3.15+ | Build system generator (must be in PATH) |
| Paddle Inference | Latest | GCC-compiled version (included in libs/) |
| OpenCV | 4.7.0 | GCC-compiled version (included in libs/) |
| oneDNN | 3.6.2 | Optional, for MKLDNN acceleration (included in libs/) |

## Build Steps

1. **Clone repository** (requires Git LFS):

   ```batch
   git lfs install
   git clone https://github.com/Limx1994/PaddleOCR-MinGW-LMX.git
   cd PaddleOCR-MinGW-LMX
   ```

2. **Run build**:

   ```batch
   cd PaddleOCR\deploy\cpp_infer
   build_mingw.bat
   ```

3. **Output**: `build_mingw\ppocr.exe`

### Build with DLL Mode

To build with DLL mode (smaller executable, 4-DLL split):

```batch
cd PaddleOCR\deploy\cpp_infer
build_mingw.bat --dll
```

Output: `build_mingw_dll\ppocr.exe` (5.6MB, requires DLLs in runtime directory)

DLL mode requires 10 DLL files in the runtime directory:
- `libcommon.dll`, `libpir.dll`, `libphi_core.dll`, `libpaddle_inference.dll` (Paddle)
- `libopencv_world470.dll` (OpenCV)
- `libpolyclipping.dll` (Clipper)
- `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`, `libgomp-1.dll` (MinGW)

## Run Inference

### General OCR (Static Mode)

```batch
cd dist\ppocr

ppocr.exe ocr --input <image_path> ^
  --text_detection_model_dir ./models/PP-OCRv4_mobile_det_infer ^
  --text_detection_model_name PP-OCRv4_mobile_det ^
  --text_recognition_model_dir ./models/PP-OCRv4_mobile_rec_infer ^
  --text_recognition_model_name PP-OCRv4_mobile_rec ^
  --use_doc_orientation_classify false ^
  --use_doc_unwarping false ^
  --use_textline_orientation false
```

### General OCR (DLL Mode)

```batch
cd dist\ppocr_dll

ppocr.exe ocr --input <image_path> ^
  --text_detection_model_dir ./models/PP-OCRv4_mobile_det_infer ^
  --text_detection_model_name PP-OCRv4_mobile_det ^
  --text_recognition_model_dir ./models/PP-OCRv4_mobile_rec_infer ^
  --text_recognition_model_name PP-OCRv4_mobile_rec ^
  --use_doc_orientation_classify false ^
  --use_doc_unwarping false ^
  --use_textline_orientation false
```

**Inference modes** (`--enable_mkldnn`):
- `--enable_mkldnn true` — oneDNN acceleration enabled (default)
- `--enable_mkldnn false` — Native Paddle mode (faster for mobile models)

**Pipeline parallelism** (`--thread_num`):
- Default: 4 (processes multiple images concurrently)
- Set to 1 for single-image sequential processing

### License Plate Recognition

The same PP-OCRv4 models can recognize license plates (Chinese province abbreviations + letters + digits):

```batch
cd dist\ppocr

ppocr.exe ocr --input <plate_image> ^
  --text_detection_model_dir ./models/PP-OCRv4_mobile_det_infer ^
  --text_detection_model_name PP-OCRv4_mobile_det ^
  --text_recognition_model_dir ./models/PP-OCRv4_mobile_rec_infer ^
  --text_recognition_model_name PP-OCRv4_mobile_rec ^
  --use_doc_orientation_classify false ^
  --use_doc_unwarping false ^
  --use_textline_orientation false
```

Example output (license plate: `赣G·0522Y`):
```json
{
  "rec_texts": ["赣G·0522Y"],
  "rec_scores": [0.999318]
}
```

## Model Preparation

Download PP-OCRv4 models and place them in `dist/ppocr/models/`:

```
dist/ppocr/models/
├── PP-OCRv4_mobile_det_infer/          # Text detection model
│   ├── inference.json                  # or inference.pdmodel
│   └── inference.pdiparams
├── PP-OCRv4_mobile_rec_infer/          # Text recognition model
│   ├── inference.json                  # or inference.pdmodel
│   └── inference.pdiparams
├── PP-LCNet_x1_0_doc_ori_infer/        # Document orientation classifier (4 directions: 0°/90°/180°/270°)
│   ├── inference.json                  # or inference.pdmodel
│   └── inference.pdiparams
└── PP-LCNet_x1_0_textline_ori_infer/   # Text line orientation classifier (optional)
    ├── inference.json                  # or inference.pdmodel
    └── inference.pdiparams
```

**Model file format**: PaddlePaddle 3.0 supports two formats:
- `.json` + `.pdiparams` (new format, recommended)
- `.pdmodel` + `.pdiparams` (old format, compatible)

**Download links:**
- Text detection: [PP-OCRv4_mobile_det_infer](https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/PP-OCRv4_mobile_det_infer.tar)
- Text recognition: [PP-OCRv4_mobile_rec_infer](https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/PP-OCRv4_mobile_rec_infer.tar)
- Document orientation: [PP-LCNet_x1_0_doc_ori_infer](https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/PP-LCNet_x1_0_doc_ori_infer.tar)
- Text line orientation: [PP-LCNet_x1_0_textline_ori_infer](https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/PP-LCNet_x1_0_textline_ori_infer.tar)

### Document Orientation Classification

The `PP-LCNet_x1_0_doc_ori` model supports 4-direction classification:

| Class ID | Angle | Description |
|----------|-------|-------------|
| 0 | 0° | Normal orientation |
| 1 | 90° | Rotated 90° clockwise |
| 2 | 180° | Upside down |
| 3 | 270° | Rotated 90° counter-clockwise |

Usage:
```batch
ppocr.exe ocr --input image.jpg ^
  --use_doc_orientation_classify true ^
  --doc_orientation_classify_model_name PP-LCNet_x1_0_doc_ori ^
  --doc_orientation_classify_model_dir ./models/PP-LCNet_x1_0_doc_ori_infer
```

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
| Git LFS | Large files not downloaded | Install Git LFS before clone: `git lfs install` |
| OpenCV imwrite warning | Result images may not be saved | OCR inference works correctly; image saving is best-effort |
| CMake 4.x compatibility | Some old cmake files need updates | Add `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` (handled automatically) |
| GCC version compatibility | Link errors | GCC 11.2.0 libraries cannot be linked with GCC 16.1.0 runtime |

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
