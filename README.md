# PaddleOCR C++ Inference Engine

基于 PaddlePaddle 的 C++ OCR 推理引擎，使用 MinGW GCC 编译，支持中英文文字识别。

## 目录结构

```
D:\tmp\tmp\
├── PaddleOCR/              # OCR 源代码（Python + C++）
├── src/                    # 框架源码
│   ├── Paddle/             # PaddlePaddle 框架源码
│   └── opencv-4.7.0/       # OpenCV 源码
├── toolchain/              # 编译工具链
│   └── mingw/              # MinGW GCC 11.2+
├── libs/                   # 预编译库
│   ├── paddle_inference_gcc/  # Paddle 推理库（含 oneDNN）
│   ├── opencv_install_gcc/    # OpenCV 库
│   └── onednn_install_gcc/    # oneDNN (MKLDNN) 库
├── scripts/                # 构建脚本
│   ├── build_paddle.bat    # 编译 Paddle（含 oneDNN）
│   ├── build_onednn.bat    # 编译 oneDNN
│   ├── paddle_toolchain.cmake
│   └── merge_libs.mri      # 静态库合并脚本
├── dist/ppocr/             # 运行时分发包
│   ├── ppocr.exe           # 可执行文件（370MB，静态链接）
│   ├── *.dll               # 运行时 DLL
│   └── models/             # OCR 模型
└── test_images/            # 测试数据
```

## 快速开始

### 运行 OCR 识别

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

### 输出示例

```json
{
  "rec_texts": ["赣G·0522Y", "O"],
  "rec_scores": [0.957049, 0.417263],
  "rec_boxes": [[308, 686, 523, 756], [557, 692, 566, 701]]
}
```

## 从源码编译

### 环境要求

- Windows 10/11
- MinGW GCC 11.2+（已包含在 `toolchain/mingw/`）
- CMake 3.15+（支持 CMake 4.x）
- Ninja（已包含在 MinGW 工具链中）

### 编译 oneDNN（可选，加速推理）

```batch
cd scripts
build_onednn.bat
```

输出：`libs\onednn_install_gcc\`（静态库，含 OpenMP 支持）

### 编译 Paddle 推理库

```batch
cd scripts
build_paddle.bat
```

编译完成后合并静态库：

```batch
cd src\Paddle\build_gcc_onednn10\paddle\fluid\inference
toolchain\mingw\bin\ar.exe -M < paddle_inference.mri
```

然后复制到 `libs\paddle_inference_gcc\`：

- `paddle\lib\libpaddle_inference.a` — 主推理库
- `paddle\lib\libphi_core.a` — phi 核心库
- `third_party\install\onednn\` — oneDNN 头文件和库

### 编译 OpenCV

源码位于 `src/opencv-4.7.0/`，预编译版本位于 `libs/opencv_install_gcc/`。如需重新编译：

```batch
cd src\opencv-4.7.0
mkdir build && cd build
cmake .. -G "MinGW Makefiles" ^
  -DCMAKE_C_COMPILER=D:\tmp\tmp\toolchain\mingw\bin\gcc.exe ^
  -DCMAKE_CXX_COMPILER=D:\tmp\tmp\toolchain\mingw\bin\g++.exe ^
  -DCMAKE_INSTALL_PREFIX=D:\tmp\tmp\libs\opencv_install_gcc ^
  -DBUILD_SHARED_LIBS=ON
mingw32-make -j12
mingw32-make install
```

### 编译 PaddleOCR C++ 引擎

```batch
cd PaddleOCR\deploy\cpp_infer
build_mingw.bat
```

编译产物：`build_mingw\ppocr.exe`

## 命令行参数

| 参数                               | 说明           | 默认值                 |
| -------------------------------- | ------------ | ------------------- |
| `--input`                        | 输入图片路径       | 必填                  |
| `--text_detection_model_dir`     | 文字检测模型目录     | 空                   |
| `--text_recognition_model_dir`   | 文字识别模型目录     | 空                   |
| `--text_detection_model_name`    | 检测模型名称       | PP-OCRv5_server_det |
| `--text_recognition_model_name`  | 识别模型名称       | PP-OCRv5_server_rec |
| `--run_mode`                     | 推理模式         | paddle              |
| `--thread_num`                   | Pipeline 并行数 | 4                   |
| `--cpu_threads`                  | CPU 线程数      | 8                   |
| `--use_doc_orientation_classify` | 是否使用文档方向分类   | true                |
| `--use_doc_unwarping`            | 是否使用文档去畸变    | true                |
| `--use_textline_orientation`     | 是否使用文本行方向分类  | true                |
| `--device`                       | 推理设备         | cpu                 |
| `--precision`                    | 计算精度         | fp32                |

**推理模式说明：**

- `paddle` — 原生 Paddle 模式，对 mobile 模型更快（默认）
- `mkldnn` — oneDNN 加速模式，对 server 模型可能有益
- `mkldnn_bf16` — oneDNN BF16 模式（需要 CPU 支持）

## 模型说明

本项目使用 PP-OCRv4 移动端模型：

| 模型                  | 用途   | 大小    |
| ------------------- | ---- | ----- |
| PP-OCRv4_mobile_det | 文字检测 | 4.8MB |
| PP-OCRv4_mobile_rec | 文字识别 | 11MB  |

如需使用其他模型，下载后放入 `dist/ppocr/models/` 目录，并在命令行指定对应的模型名称。

## 已知问题

- **大文件编译**：需要 `-Wa,-mbig-obj` 参数处理大型 COFF 段
- **符号冲突**：合并静态库时需要 `-Wl,--allow-multiple-definition`
- **TLS 析构崩溃**：已通过 immortal singleton + char[] buffer 模式解决
- **DLL 共享库**：因导出符号数超限（>65535），无法构建 `paddle_inference.dll`，仅支持静态链接
- **CMake 4.x 兼容性**：需要 `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`

## 性能数据

测试环境：Intel CPU, 20 逻辑处理器, PP-OCRv4_mobile 模型, 800×1079 图片

| 场景     | 模式     | 耗时    |
| ------ | ------ | ----- |
| 单张图片   | paddle | 1.61s |
| 单张图片   | mkldnn | 1.68s |
| 10 张图片 | paddle | 1.63s |
| 10 张图片 | mkldnn | 3.55s |

结论：PP-OCRv4_mobile 模型使用 paddle 模式更快。mkldnn 对大模型可能有益。

## 技术栈

- **推理框架**：PaddlePaddle Inference
- **加速库**：oneDNN (MKLDNN) v3.6.2（可选）
- **图像处理**：OpenCV 4.7.0
- **编译器**：MinGW GCC 11.2+
- **构建系统**：CMake + Ninja
- **OCR 模型**：PP-OCRv4

## 许可证

- PaddleOCR：Apache License 2.0
- PaddlePaddle：Apache License 2.0
- OpenCV：Apache License 2.0
- MinGW版本作者：迁旭
