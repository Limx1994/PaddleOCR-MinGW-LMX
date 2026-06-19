# 编译指南

## 快速开始（使用预编译版本）

如果你只想运行 OCR，不需要编译：

1. 下载预编译归档：
   
   ```
   https://github.com/Limx1994/PaddleOCR-MinGW-LMX/releases/download/v2.0.0/PaddleOCR-MinGW-v2.0.0.tar.gz
   ```

2. 解压后直接运行：
   
   ```batch
   cd dist\ppocr
   ppocr.exe ocr --input <图片路径> ^
     --text_detection_model_dir ./models/PP-OCRv4_mobile_det_infer ^
     --text_recognition_model_dir ./models/PP-OCRv4_mobile_rec_infer ^
     --text_detection_model_name PP-OCRv4_mobile_det ^
     --text_recognition_model_name PP-OCRv4_mobile_rec ^
     --use_doc_orientation_classify false ^
     --use_doc_unwarping false ^
     --use_textline_orientation false
   ```

3. 启用 4 方向文档分类（可选）：
   
   ```batch
   ppocr.exe ocr --input <图片路径> ^
     --use_doc_orientation_classify true ^
     --doc_orientation_classify_model_name PP-LCNet_x1_0_doc_ori ^
     --doc_orientation_classify_model_dir ./models/PP-LCNet_x1_0_doc_ori_infer ^
     --text_detection_model_dir ./models/PP-OCRv4_mobile_det_infer ^
     --text_recognition_model_dir ./models/PP-OCRv4_mobile_rec_infer ^
     --text_detection_model_name PP-OCRv4_mobile_det ^
     --text_recognition_model_name PP-OCRv4_mobile_rec
   ```

## 从源码编译

### 前提条件

- Windows 10/11
- Git LFS（克隆前安装：`git lfs install`）
- CMake 3.15+（需在系统 PATH 中）
- 至少 10GB 磁盘空间
- 网络连接（下载依赖）

### 步骤 1: 克隆仓库

```batch
git lfs install
git clone https://github.com/Limx1994/PaddleOCR-MinGW-LMX.git
cd PaddleOCR-MinGW-LMX
```

### 步骤 2: 下载依赖（可选）

Paddle 框架的第三方依赖已包含在仓库中（通过 Git LFS 跟踪）。如需重新下载：

```batch
cd src\Paddle
git clone https://github.com/PaddlePaddle/Paddle-third_party.git third_party
cd ..\..
```

### 步骤 3: 编译 OpenCV

OpenCV 源码已包含在 `src/opencv-4.7.0/` 中。

```batch
cd src\opencv-4.7.0
mkdir build && cd build
cmake .. -G "MinGW Makefiles" ^
  -DCMAKE_C_COMPILER=..\..\..\toolchain\mingw\bin\gcc.exe ^
  -DCMAKE_CXX_COMPILER=..\..\..\toolchain\mingw\bin\g++.exe ^
  -DCMAKE_INSTALL_PREFIX=..\..\..\libs\opencv_install_gcc ^
  -DBUILD_SHARED_LIBS=ON
..\..\..\toolchain\mingw\bin\mingw32-make.exe -j12
..\..\..\toolchain\mingw\bin\mingw32-make.exe install
cd ..\..\..
```

### 步骤 4: 编译 oneDNN（可选，加速推理）

```batch
cd scripts
build_onednn.bat
cd ..
```

### 步骤 5: 编译 Paddle 推理库

**方式 A：静态链接（传统方式）**

```batch
cd scripts
build_paddle.bat
cd ..
```

编译完成后合并静态库：

```batch
cd src\Paddle\build_gcc_onednn10\paddle\fluid\inference
..\..\..\..\..\toolchain\mingw\bin\ar.exe -M < paddle_inference.mri
cd ..\..\..\..\..
```

复制库文件：

```batch
mkdir libs\paddle_inference_gcc\paddle\lib
copy src\Paddle\build_gcc_onednn10\paddle\fluid\inference\libpaddle_inference.a libs\paddle_inference_gcc\paddle\lib\
copy src\Paddle\build_gcc_onednn10\paddle\phi\libphi_core.a libs\paddle_inference_gcc\paddle\lib\
copy src\Paddle\build_gcc_onednn10\paddle\common\libcommon.a libs\paddle_inference_gcc\paddle\lib\
```

**方式 B：DLL 模式（推荐，可执行文件更小）**

```batch
cd scripts
build_paddle_dll.bat
cd ..
```

生成 4 个 Paddle DLL（libcommon.dll, libpir.dll, libphi_core.dll, libpaddle_inference.dll）。

### 步骤 6: 编译 PaddleOCR

**静态模式**（361MB exe，无需额外 DLL）：

```batch
cd PaddleOCR\deploy\cpp_infer
build_mingw.bat
cd ..\..\..
```

**DLL 模式**（5.6MB exe，需要 Paddle DLL）：

```batch
cd PaddleOCR\deploy\cpp_infer
build_mingw.bat --dll
cd ..\..\..
```

### 步骤 7: 运行

**静态模式**：

```batch
cd dist\ppocr
ppocr.exe ocr --input ..\..\test_images\test.jpg ^
  --text_detection_model_dir ./models/PP-OCRv4_mobile_det_infer ^
  --text_recognition_model_dir ./models/PP-OCRv4_mobile_rec_infer ^
  --text_detection_model_name PP-OCRv4_mobile_det ^
  --text_recognition_model_name PP-OCRv4_mobile_rec ^
  --use_doc_orientation_classify false ^
  --use_doc_unwarping false ^
  --use_textline_orientation false
```

**DLL 模式**：

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

**服务模式**：

```batch
cd dist\ppocr
ppocr_service.exe --model_dir ./models --port 8080 --pool_size 2
```

**快速检测模式（车牌场景）**：

```batch
cd dist\ppocr

# 下载 RT-DETR 车牌检测模型
# https://huggingface.co/Topurrra/rtdetr-license-plate-detection-onnx

# 启动服务（快速检测模式）
ppocr_service.exe --model_dir ./models --fast_detect yolo --plate_model ./models/plate_rtdetr.onnx --port 8080

# 发送请求
ppocr_client.exe test.jpg 127.0.0.1 8080
```

## 目录结构

```
PaddleOCR-MinGW-LMX/
├── PaddleOCR/              # PaddleOCR 源码（已修改）
├── src/                    # 框架源码
│   ├── Paddle/             # PaddlePaddle 框架
│   └── opencv-4.7.0/       # OpenCV 源码
├── toolchain/              # MinGW GCC 工具链
│   └── mingw/              # GCC 11.2.0（含 gcc/g++/ninja/ar）
├── libs/                   # 预编译库（Git LFS 跟踪）
│   ├── paddle_inference_gcc/  # Paddle 推理库（含 oneDNN）
│   ├── opencv_install_gcc/    # OpenCV 库
│   └── onednn_install_gcc/    # oneDNN (MKLDNN) 库
├── scripts/                # 构建脚本
├── dist/
│   ├── ppocr/              # 静态模式运行目录（~390MB exe）
│   │   ├── ppocr.exe       # CLI 工具
│   │   ├── ppocr_service.exe # TCP 服务（进程池模式）
│   │   ├── ppocr_worker.exe  # Worker 子进程
│   │   ├── ppocr_client.exe  # 测试客户端
│   │   ├── onnxruntime.dll   # ONNX Runtime（快速检测模式需要）
│   │   └── models/
│   │       ├── PP-OCRv4_mobile_det_infer/   # 文本检测模型
│   │       ├── PP-OCRv4_mobile_rec_infer/   # 文本识别模型
│   │       ├── PP-LCNet_x1_0_doc_ori_infer/ # 文档方向分类模型（4方向）
│   │       └── plate_rtdetr.onnx            # 车牌检测模型（RT-DETR, 可选）
│   └── ppocr_dll/          # DLL 模式运行目录（5.6MB exe + DLL）
│       ├── libpaddle_inference.dll  # Paddle 推理库
│       ├── libphi_core.dll          # Paddle Phi 核心库
│       ├── libpir.dll               # Paddle IR 库
│       ├── libcommon.dll            # Paddle 基础库
│       ├── onnxruntime.dll          # ONNX Runtime（快速检测模式需要）
│       └── models/
│           ├── PP-OCRv4_mobile_det_infer/   # 文本检测模型
│           ├── PP-OCRv4_mobile_rec_infer/   # 文本识别模型
│           ├── PP-LCNet_x1_0_doc_ori_infer/ # 文档方向分类模型（4方向）
│           └── plate_rtdetr.onnx            # 车牌检测模型（RT-DETR, 可选）
└── test_images/            # 测试数据
```

## 快速检测模式（车牌场景）

针对车牌识别场景，提供 RT-DETR 快速检测模式，先裁剪车牌区域再进行 OCR 识别，大幅提升性能。

### 工作原理

```
原图 → RT-DETR 检测车牌 (~10ms) → 裁剪车牌区域 → OCR 识别 (~0.1ms)
```

### 使用方法

```batch
cd dist\ppocr

# 下载 RT-DETR 车牌检测模型
# https://huggingface.co/Topurrra/rtdetr-license-plate-detection-onnx

# 启动服务（快速检测模式）
ppocr_service.exe --model_dir ./models --fast_detect yolo --plate_model ./models/plate_rtdetr.onnx --port 8080

# 发送请求
ppocr_client.exe test.jpg 127.0.0.1 8080
```

### 性能对比

| 方案           | 检测耗时   | 识别耗时   | 总耗时   |
| ------------ | ------ | ------ | ----- |
| 完整 OCR 流程    | ~500ms | ~1.5s  | ~2s   |
| RT-DETR 快速检测 | ~10ms  | ~0.1ms | ~10ms |

### 模型要求

需要准备 RT-DETR 车牌检测 ONNX 模型（`plate_rtdetr.onnx`），可从以下来源获取：

- [Hugging Face - RT-DETR License Plate Detection](https://huggingface.co/Topurrra/rtdetr-license-plate-detection-onnx)

模型输入：640x640 RGB 图像
模型输出：logits [1, 300, 1] + pred_boxes [1, 300, 4]

### ONNX Runtime 依赖

快速检测模式需要 ONNX Runtime 库：

- **静态模式**：需要 `onnxruntime.dll` 和 `onnxruntime_providers_shared.dll`
- **DLL 模式**：同上

ONNX Runtime 库已包含在预编译版本中。如需自行编译，请参考 [ONNX Runtime 官方文档](https://onnxruntime.ai/)。

## 常见问题

### Q: 克隆后文件不完整或编译失败

A: 大文件使用 Git LFS 跟踪，克隆前必须安装 Git LFS：

```batch
git lfs install
git clone https://github.com/Limx1994/PaddleOCR-MinGW-LMX.git
```

### Q: 编译失败，提示找不到 third_party

A: 需要下载 Paddle 的第三方依赖：

```batch
cd src\Paddle
git clone https://github.com/PaddlePaddle/Paddle-third_party.git third_party
```

### Q: 链接错误，提示 multiple definition

A: 这是正常的，构建脚本已添加 `-Wl,--allow-multiple-definition` 解决此问题。

### Q: 如何启用 oneDNN 加速？

A: MKLDNN 默认已启用（`--enable_mkldnn true`）。如需禁用以提升 mobile 模型速度，使用 `--enable_mkldnn false`。

### Q: 如何修改代码并重新编译？

A:

1. 修改源码（PaddleOCR/ 或 src/Paddle/）
2. 重新运行对应的构建脚本
3. 重新编译 PaddleOCR

### Q: 快速检测模式报错 "Failed to initialize PlateDetector"

A: 确保：

1. `plate_rtdetr.onnx` 模型文件存在于 `models/` 目录
2. `onnxruntime.dll` 和 `onnxruntime_providers_shared.dll` 存在
3. 模型文件格式正确（RT-DETR 格式）

### Q: 如何获取车牌检测模型？

A: 从 Hugging Face 下载 RT-DETR 车牌检测模型：

```batch
# 下载地址
https://huggingface.co/Topurrra/rtdetr-license-plate-detection-onnx

# 将 plate_rtdetr.onnx 放到 models 目录
copy plate_rtdetr.onnx dist\ppocr\models\
```

## 性能数据

测试环境：Intel CPU, 20 逻辑处理器, PP-OCRv4_mobile 模型

| 模式          | 单张图片  | 10 张图片 |
| ----------- | ----- | ------ |
| paddle (默认) | 1.61s | 1.63s  |
| mkldnn      | 1.68s | 3.55s  |

## 技术栈

- **推理框架**: PaddlePaddle Inference
- **加速库**: oneDNN (MKLDNN) v3.6.2
- **图像处理**: OpenCV 4.7.0
- **深度学习推理**: ONNX Runtime 1.17.0（快速检测模式）
- **编译器**: MinGW GCC 11.2+
- **构建系统**: CMake + Ninja
- **OCR 模型**: PP-OCRv4
- **车牌检测模型**: RT-DETR（可选）

## 许可证

- PaddleOCR: Apache License 2.0
- PaddlePaddle: Apache License 2.0
- OpenCV: Apache License 2.0
