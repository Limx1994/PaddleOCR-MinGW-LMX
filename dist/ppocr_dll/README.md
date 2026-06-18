# PaddleOCR C++ 推理工具（DLL 模式）

基于 PaddlePaddle 推理库和 PaddleOCR 的 Windows C++ OCR 文字识别工具，使用 MinGW GCC 编译，采用 DLL 模式（4-DLL 拆分）。

## 与静态模式的区别

| 项目 | DLL 模式（本目录） | 静态模式（dist/ppocr/） |
|------|------------------|----------------------|
| ppocr.exe 大小 | 5.6MB | 361MB |
| 运行时 DLL | 10 个 DLL | 仅 MinGW 运行时 |
| 部署复杂度 | 需要所有 DLL | 仅需 exe |
| 更新便利性 | 可单独更新 DLL | 需重新编译 |

## 目录结构

```
dist/ppocr_dll/
├── ppocr.exe                    # 主程序 (5.6MB)
├── libcommon.dll                # Paddle 基础库 (~413K)
├── libpir.dll                   # Paddle IR (~2.7M)
├── libphi_core.dll              # Paddle Phi (~194M)
├── libpaddle_inference.dll      # Paddle 推理 (~348M)
├── libopencv_world470.dll       # OpenCV (~53M)
├── libpolyclipping.dll          # Clipper 库 (~2M)
├── libgcc_s_seh-1.dll           # MinGW 运行时 (~74K)
├── libstdc++-6.dll              # MinGW 运行时 (~1.9M)
├── libwinpthread-1.dll          # MinGW 运行时 (~52K)
├── libgomp-1.dll                # OpenMP 运行时 (~237K)
├── models/                      # 推理模型目录
│   ├── PP-OCRv4_mobile_det_infer/   # 文本检测模型
│   └── PP-OCRv4_mobile_rec_infer/   # 文本识别模型
├── output/                      # 输出目录
└── test.jpg                     # 测试图片
```

## 快速开始

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

## 性能数据

测试环境：Intel CPU, 20 逻辑处理器, PP-OCRv4_mobile 模型, 800×1079 图片

| 场景 | paddle 模式 | mkldnn 模式 |
|------|-----------|-----------|
| 单张图片 | 1.61s | 1.68s |
| 10 张图片 | 1.63s | 3.55s |

## 常见问题

### Q: 启动时报 DLL 缺失错误

A: 确保所有 10 个 DLL 文件都在 ppocr.exe 同一目录下。

### Q: 如何更新 Paddle 推理库？

A: 重新编译 Paddle DLL 模式（`scripts\build_paddle_dll.bat`），然后替换 4 个 Paddle DLL。

### Q: 可以与其他项目共享 DLL 吗？

A: 可以，将 Paddle DLL 放到系统 PATH 目录下即可共享。

## 相关链接

- [项目主页](https://github.com/Limx1994/PaddleOCR-MinGW-LMX)
- [静态模式文档](../ppocr/README.md)
- [编译指南](../../BUILD_GUIDE.md)

## 许可证

本项目禁止用于商业
