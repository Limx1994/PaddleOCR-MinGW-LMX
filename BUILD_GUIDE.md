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
├── libs_upload/            # 预编译库头文件（用于分发）
├── scripts/                # 构建脚本
├── dist/
│   ├── ppocr/              # 静态模式运行目录（361MB exe）
│   │   └── models/
│   │       ├── PP-OCRv4_mobile_det_infer/   # 文本检测模型
│   │       ├── PP-OCRv4_mobile_rec_infer/   # 文本识别模型
│   │       └── PP-LCNet_x1_0_doc_ori_infer/ # 文档方向分类模型（4方向）
│   └── ppocr_dll/          # DLL 模式运行目录（5.6MB exe + DLL）
│       └── models/
│           ├── PP-OCRv4_mobile_det_infer/   # 文本检测模型
│           ├── PP-OCRv4_mobile_rec_infer/   # 文本识别模型
│           └── PP-LCNet_x1_0_doc_ori_infer/ # 文档方向分类模型（4方向）
└── test_images/            # 测试数据
```

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
- **编译器**: MinGW GCC 11.2+
- **构建系统**: CMake + Ninja
- **OCR 模型**: PP-OCRv4

## 许可证

- PaddleOCR: Apache License 2.0
- PaddlePaddle: Apache License 2.0
- OpenCV: Apache License 2.0
