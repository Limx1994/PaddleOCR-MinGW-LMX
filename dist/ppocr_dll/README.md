# PaddleOCR C++ 推理工具（DLL 模式）

基于 PaddlePaddle 推理库和 PaddleOCR 的 Windows C++ OCR 文字识别工具，使用 MinGW GCC 编译，采用 DLL 模式（4-DLL 拆分）。

## 与静态模式的区别

| 项目 | DLL 模式（本目录） | 静态模式（dist/ppocr/） |
|------|------------------|----------------------|
| ppocr.exe 大小 | 5.6MB | ~390MB |
| 运行时 DLL | 14 个 DLL | 7 个 DLL |
| 部署复杂度 | 需要所有 DLL | DLL 较少 |
| 更新便利性 | 可单独更新 DLL | 需重新编译 |

## 目录结构

```
dist/ppocr_dll/
├── ppocr.exe                    # CLI 工具 (5.6MB)
├── ppocr_service.exe            # TCP 服务（进程池模式）
├── ppocr_worker.exe             # Worker 子进程
├── ppocr_client.exe             # 测试客户端
│
├── # Paddle 推理库 (4-DLL 拆分)
├── libpaddle_inference.dll      # Paddle 推理库 (364MB)
├── libphi_core.dll              # Paddle Phi 核心库 (203MB)
├── libpir.dll                   # Paddle IR 库 (2.8MB)
├── libcommon.dll                # Paddle 基础库 (423KB)
│
├── # 第三方库
├── libopencv_world470.dll       # OpenCV 库 (56MB)
├── libpolyclipping.dll          # Clipper 库 (2.2MB)
├── onnxruntime.dll              # ONNX Runtime (10.9MB, 快速检测模式需要)
├── onnxruntime_providers_shared.dll
│
├── # MinGW 运行时
├── libgcc_s_seh-1.dll           # MinGW 运行时 (75KB)
├── libstdc++-6.dll              # MinGW C++ 运行时 (1.9MB)
├── libwinpthread-1.dll          # MinGW 线程库 (53KB)
├── libgomp-1.dll                # OpenMP 运行时 (243KB)
│
├── models/                      # 推理模型目录
│   ├── PP-OCRv4_mobile_det_infer/       # 文本检测模型
│   ├── PP-OCRv4_mobile_rec_infer/       # 文本识别模型
│   ├── PP-LCNet_x1_0_doc_ori_infer/     # 文档方向分类模型 (4方向: 0°/90°/180°/270°)
│   └── plate_rtdetr.onnx                # 车牌检测模型 (RT-DETR, 可选)
│
└── output/                      # 输出目录
```

## 快速开始

> **重要**：使用 PP-OCRv4_mobile 模型时，必须指定 `--text_detection_model_name PP-OCRv4_mobile_det` 和 `--text_recognition_model_name PP-OCRv4_mobile_rec`。默认模型名称为 `PP-OCRv5_server_det`/`PP-OCRv5_server_rec`，如果未安装 server 模型会加载失败。

### 基本用法

```batch
cd dist\ppocr_dll

ppocr.exe ocr --input test.jpg ^
  --text_detection_model_dir ./models/PP-OCRv4_mobile_det_infer ^
  --text_recognition_model_dir ./models/PP-OCRv4_mobile_rec_infer ^
  --text_detection_model_name PP-OCRv4_mobile_det ^
  --text_recognition_model_name PP-OCRv4_mobile_rec ^
  --use_doc_orientation_classify false ^
  --use_doc_unwarping false ^
  --use_textline_orientation false
```

### 启用 4 方向文档分类

```batch
ppocr.exe ocr --input test.jpg ^
  --use_doc_orientation_classify true ^
  --doc_orientation_classify_model_name PP-LCNet_x1_0_doc_ori ^
  --doc_orientation_classify_model_dir ./models/PP-LCNet_x1_0_doc_ori_infer ^
  --text_detection_model_dir ./models/PP-OCRv4_mobile_det_infer ^
  --text_recognition_model_dir ./models/PP-OCRv4_mobile_rec_infer ^
  --text_detection_model_name PP-OCRv4_mobile_det ^
  --text_recognition_model_name PP-OCRv4_mobile_rec
```

自动检测文档方向（0°/90°/180°/270°）并矫正后识别。

### 输出示例

```json
{
  "rec_texts": ["赣G·0522Y", "O"],
  "rec_scores": [0.957067, 0.417261],
  "rec_boxes": [[308, 686, 523, 756], [557, 692, 566, 701]]
}
```

## 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--input` | 输入图片路径 | 必填 |
| `--text_detection_model_dir` | 文字检测模型目录 | 空 |
| `--text_recognition_model_dir` | 文字识别模型目录 | 空 |
| `--text_detection_model_name` | 检测模型名称 | PP-OCRv5_server_det |
| `--text_recognition_model_name` | 识别模型名称 | PP-OCRv5_server_rec |
| `--use_doc_orientation_classify` | 是否使用文档方向分类 | true |
| `--use_doc_unwarping` | 是否使用文档去畸变 | true |
| `--use_textline_orientation` | 是否使用文本行方向分类 | true |
| `--cpu_threads` | CPU 线程数 | 8 |
| `--thread_num` | Pipeline 并行数 | 4 |

## 服务模式

服务模式支持多次 OCR 请求而无需重启进程。采用进程池架构，每个 worker 进程独立处理 OCR。

### 启动服务

```batch
cd D:\tmp\tmp\dist\ppocr_dll

# 标准模式
ppocr_service.exe --model_dir ./models --port 8081 --pool_size 2

# 快速检测模式（车牌场景）
ppocr_service.exe --model_dir ./models --port 8081 --pool_size 2 --fast_detect yolo --plate_model ./models/plate_rtdetr.onnx
```

### 发送请求

```batch
ppocr_client.exe <图片路径> 127.0.0.1 8081
```

### 服务参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--host` | 127.0.0.1 | 监听地址 |
| `--port` | 8080 | 监听端口 |
| `--model_dir` | （必填） | 模型目录 |
| `--pool_size` | 2 | Worker 进程数 |
| `--cpu_threads` | 8 | 每个 worker 的 CPU 线程数 |
| `--use_doc_orientation` | true | 使用文档方向分类 |
| `--fast_detect` | none | 快速检测模式：none, yolo |
| `--plate_model` | 空 | 车牌检测 ONNX 模型路径 |

## 快速检测模式（车牌场景）

针对车牌识别场景，提供 RT-DETR 快速检测模式，先裁剪车牌区域再进行 OCR 识别，大幅提升性能。

### 工作原理

```
原图 → RT-DETR 检测车牌 (~10ms) → 裁剪车牌区域 → OCR 识别 (~0.1ms)
```

### 使用方法

```batch
cd D:\tmp\tmp\dist\ppocr_dll

# 启动服务（快速检测模式）
ppocr_service.exe --model_dir ./models --fast_detect yolo --plate_model ./models/plate_rtdetr.onnx --port 8080

# 发送请求
ppocr_client.exe test.jpg 127.0.0.1 8080
```

### 性能对比

| 方案 | 检测耗时 | 识别耗时 | 总耗时 |
|------|---------|---------|--------|
| 完整 OCR 流程 | ~500ms | ~1.5s | ~2s |
| RT-DETR 快速检测 | ~10ms | ~0.1ms | ~10ms |

### 模型要求

需要准备 RT-DETR 车牌检测 ONNX 模型（`plate_rtdetr.onnx`），可从以下来源获取：
- [Hugging Face - RT-DETR License Plate Detection](https://huggingface.co/Topurrra/rtdetr-license-plate-detection-onnx)

模型输入：640x640 RGB 图像
模型输出：logits [1, 300, 1] + pred_boxes [1, 300, 4]

## 性能数据

测试环境：Intel CPU, 20 逻辑处理器, PP-OCRv4_mobile 模型, 800×1079 图片

| 模式 | 耗时 |
|------|------|
| 单次模式（CLI） | ~1.57s |
| 服务模式 | ~0.28s |
| 快速检测模式 | ~0.3s |

服务模式比单次模式快 5.6 倍（省去模型加载时间）。

## 常见问题

### Q: 启动时报 DLL 缺失错误

A: 确保所有 DLL 文件都在 ppocr.exe 同一目录下。可以使用 `ldd ppocr_service.exe` 检查缺失的 DLL。

### Q: 启动时报 exit code 127 或 exception c0000139

A: 这是 MinGW 运行时 DLL 版本不匹配导致的。解决方案：

```batch
# 从 toolchain 目录复制正确的 DLL
cp toolchain\mingw\bin\libgcc_s_seh-1.dll dist\ppocr_dll\
cp toolchain\mingw\bin\libstdc++-6.dll dist\ppocr_dll\
cp toolchain\mingw\bin\libwinpthread-1.dll dist\ppocr_dll\
cp toolchain\mingw\bin\libgomp-1.dll dist\ppocr_dll\
```

### Q: 如何更新 Paddle 推理库？

A: 重新编译 Paddle DLL 模式（`scripts\build_paddle_dll.bat`），然后替换 4 个 Paddle DLL。

### Q: 可以与其他项目共享 DLL 吗？

A: 可以，将 Paddle DLL 放到系统 PATH 目录下即可共享。

### Q: 快速检测模式报错 "Failed to initialize PlateDetector"

A: 确保：
1. `plate_rtdetr.onnx` 模型文件存在于 `models/` 目录
2. `onnxruntime.dll` 和 `onnxruntime_providers_shared.dll` 存在
3. 模型文件格式正确（RT-DETR 格式）

## 相关链接

- [项目主页](https://github.com/Limx1994/PaddleOCR-MinGW-LMX)
- [静态模式文档](../ppocr/README.md)
- [RT-DETR 车牌检测模型](https://huggingface.co/Topurrra/rtdetr-license-plate-detection-onnx)

## 许可证

本项目禁止用于商业用途
