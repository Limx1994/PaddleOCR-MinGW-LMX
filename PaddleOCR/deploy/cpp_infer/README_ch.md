[English](README.md) | [简体中文](README_ch.md)

# PaddleOCR C++ 推理引擎（MinGW）

本文档介绍如何使用 MinGW GCC 在 Windows 上编译和运行 PaddleOCR C++ 推理引擎。

## 目录

- [环境要求](#环境要求)
- [编译步骤](#编译步骤)
- [运行推理](#运行推理)
  - [通用 OCR](#通用-ocr)
  - [车牌识别](#车牌识别)
  - [服务模式（进程池）](#服务模式进程池)
- [模型准备](#模型准备)
- [已知问题](#已知问题)
- [故障排除](#故障排除)

## 环境要求

| 组件 | 版本 | 说明 |
|------|------|------|
| Git LFS | 最新版 | 大文件跟踪（克隆前安装） |
| MinGW GCC | 11.2.0+ | gcc, g++, mingw32-make（已包含在 toolchain/） |
| CMake | 3.15+ | 构建系统生成器（需在系统 PATH 中） |
| Paddle Inference | 最新版 | GCC 编译版本（已包含在 libs/） |
| OpenCV | 4.7.0 | GCC 编译版本（已包含在 libs/） |
| oneDNN | 3.6.2 | 可选，MKLDNN 加速（已包含在 libs/） |

## 编译步骤

1. **克隆仓库**（需要 Git LFS）：

   ```batch
   git lfs install
   git clone https://github.com/Limx1994/PaddleOCR-MinGW-LMX.git
   cd PaddleOCR-MinGW-LMX
   ```

2. **执行编译**：

   ```batch
   cd PaddleOCR\deploy\cpp_infer
   build_mingw.bat
   ```

3. **输出目录**：`D:\tmp\tmp\dist\ppocr\`（ppocr.exe、ppocr_service.exe、ppocr_worker.exe、ppocr_client.exe）

### DLL 模式编译

使用 DLL 模式可生成更小的可执行文件（4-DLL 拆分）：

```batch
cd PaddleOCR\deploy\cpp_infer
build_mingw.bat --dll
```

输出目录：`D:\tmp\tmp\dist\ppocr_dll\`（ppocr.exe、ppocr_service.exe、ppocr_worker.exe、ppocr_client.exe + DLL）

DLL 模式运行时需要 10 个 DLL 文件：
- `libcommon.dll`, `libpir.dll`, `libphi_core.dll`, `libpaddle_inference.dll`（Paddle）
- `libopencv_world470.dll`（OpenCV）
- `libpolyclipping.dll`（Clipper）
- `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`, `libgomp-1.dll`（MinGW）

## 运行推理

### 通用 OCR（静态模式）

```batch
cd dist\ppocr

ppocr.exe ocr --input <图片路径> ^
  --text_detection_model_dir ./models/PP-OCRv4_mobile_det_infer ^
  --text_detection_model_name PP-OCRv4_mobile_det ^
  --text_recognition_model_dir ./models/PP-OCRv4_mobile_rec_infer ^
  --text_recognition_model_name PP-OCRv4_mobile_rec ^
  --use_doc_orientation_classify false ^
  --use_doc_unwarping false ^
  --use_textline_orientation false
```

### 通用 OCR（DLL 模式）

```batch
cd dist\ppocr_dll

ppocr.exe ocr --input <图片路径> ^
  --text_detection_model_dir ./models/PP-OCRv4_mobile_det_infer ^
  --text_detection_model_name PP-OCRv4_mobile_det ^
  --text_recognition_model_dir ./models/PP-OCRv4_mobile_rec_infer ^
  --text_recognition_model_name PP-OCRv4_mobile_rec ^
  --use_doc_orientation_classify false ^
  --use_doc_unwarping false ^
  --use_textline_orientation false
```

### 车牌识别

PP-OCRv4 通用模型可直接识别车牌（省份简称 + 字母 + 数字）：

```batch
cd dist\ppocr

ppocr.exe ocr --input <车牌图片> ^
  --text_detection_model_dir ./models/PP-OCRv4_mobile_det_infer ^
  --text_detection_model_name PP-OCRv4_mobile_det ^
  --text_recognition_model_dir ./models/PP-OCRv4_mobile_rec_infer ^
  --text_recognition_model_name PP-OCRv4_mobile_rec ^
  --use_doc_orientation_classify false ^
  --use_doc_unwarping false ^
  --use_textline_orientation false
```

识别示例（车牌：`赣G·0522Y`）：
```json
{
  "rec_texts": ["赣G·0522Y"],
  "rec_scores": [0.999318]
}
```

### 服务模式（进程池）

服务模式支持多次 OCR 请求而无需重启进程。采用进程池架构，每个 worker 进程独立处理 OCR。

**编译（静态模式）：**

```batch
cd PaddleOCR\deploy\cpp_infer
build_mingw.bat
```

输出目录：`D:\tmp\tmp\dist\ppocr\`（ppocr_service.exe、ppocr_worker.exe、ppocr_client.exe）

**编译（DLL 模式）：**

```batch
cd PaddleOCR\deploy\cpp_infer
build_mingw.bat --dll
```

输出目录：`D:\tmp\tmp\dist\ppocr_dll\`（ppocr_service.exe、ppocr_worker.exe、ppocr_client.exe + DLL）

**启动服务（静态模式）：**

```batch
cd D:\tmp\tmp\dist\ppocr
ppocr_service.exe --model_dir ./models --port 8081 --pool_size 2
```

**启动服务（DLL 模式）：**

```batch
cd D:\tmp\tmp\dist\ppocr_dll
ppocr_service.exe --model_dir ./models --port 8081 --pool_size 2
```

**服务参数：**

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--host` | 127.0.0.1 | 监听地址 |
| `--port` | 8080 | 监听端口 |
| `--model_dir` | （必填） | 模型目录 |
| `--pool_size` | 2 | Worker 进程数 |
| `--cpu_threads` | 8 | 每个 worker 的 CPU 线程数 |
| `--use_doc_orientation` | true | 使用文档方向分类 |

**发送请求：**

```batch
ppocr_client.exe <图片路径> <主机> <端口>
```

**架构：**

```
ppocr_service.exe（主进程）
├── TCP Server（接受客户端连接）
├── Process Pool（管理 N 个 worker 进程）
│   ├── ppocr_worker.exe（stdin/stdout 管道）
│   └── ppocr_worker.exe（stdin/stdout 管道）
└── Request Router（分配请求到空闲 worker）
```

**核心特性：**
- Worker 进程相互独立 - Paddle 运行时状态互不影响
- Worker 崩溃自动重启
- 支持多个并发请求
- 每次请求约 200ms（模型加载后）

## 模型准备

下载 PP-OCRv4 模型并放置在 `dist/ppocr/models/` 目录下：

```
dist/ppocr/models/
├── PP-OCRv4_mobile_det_infer/          # 文本检测模型
│   ├── inference.json                  # 或 inference.pdmodel
│   └── inference.pdiparams
├── PP-OCRv4_mobile_rec_infer/          # 文本识别模型
│   ├── inference.json                  # 或 inference.pdmodel
│   └── inference.pdiparams
├── PP-LCNet_x1_0_doc_ori_infer/        # 文档方向分类模型（4方向：0°/90°/180°/270°）
│   ├── inference.json                  # 或 inference.pdmodel
│   └── inference.pdiparams
└── PP-LCNet_x1_0_textline_ori_infer/   # 文本行方向分类器（可选）
    ├── inference.json                  # 或 inference.pdmodel
    └── inference.pdiparams
```

**模型文件格式**：PaddlePaddle 3.0 支持两种格式：
- `.json` + `.pdiparams`（新格式，推荐）
- `.pdmodel` + `.pdiparams`（旧格式，兼容）

**下载链接：**
- 文本检测：[PP-OCRv4_mobile_det_infer](https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/PP-OCRv4_mobile_det_infer.tar)
- 文本识别：[PP-OCRv4_mobile_rec_infer](https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/PP-OCRv4_mobile_rec_infer.tar)
- 文档方向分类：[PP-LCNet_x1_0_doc_ori_infer](https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/PP-LCNet_x1_0_doc_ori_infer.tar)
- 文本行方向分类：[PP-LCNet_x1_0_textline_ori_infer](https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/PP-LCNet_x1_0_textline_ori_infer.tar)

### 文档方向分类

`PP-LCNet_x1_0_doc_ori` 模型支持 4 方向分类：

| 类别 ID | 角度 | 说明 |
|---------|------|------|
| 0 | 0° | 正常方向 |
| 1 | 90° | 顺时针旋转 90° |
| 2 | 180° | 上下颠倒 |
| 3 | 270° | 逆时针旋转 90° |

使用示例：
```batch
ppocr.exe ocr --input image.jpg ^
  --use_doc_orientation_classify true ^
  --doc_orientation_classify_model_name PP-LCNet_x1_0_doc_ori ^
  --doc_orientation_classify_model_dir ./models/PP-LCNet_x1_0_doc_ori_infer
```

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
| Git LFS | 大文件未下载 | 克隆前安装 Git LFS：`git lfs install` |
| OpenCV imwrite 警告 | 结果图片可能无法保存 | OCR 推理正常工作；图片保存为尽力而为 |
| CMake 4.x 兼容性 | 旧版 CMakeLists.txt 报错 | 添加 `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`（已自动处理） |
| GCC 版本兼容 | 链接失败 | GCC 11.2.0 编译的库不能用 GCC 16.1.0 链接 |

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

### DLL 模式服务启动失败（exit code 127 或 exception c0000139）
- 这是由于 MinGW 运行时 DLL 版本不匹配导致的
- 解决方案：从 toolchain 目录复制正确的 DLL：
  ```batch
  cp toolchain\mingw\bin\libgcc_s_seh-1.dll dist\ppocr_dll\
  cp toolchain\mingw\bin\libstdc++-6.dll dist\ppocr_dll\
  cp toolchain\mingw\bin\libwinpthread-1.dll dist\ppocr_dll\
  cp toolchain\mingw\bin\libgomp-1.dll dist\ppocr_dll\
  ```
