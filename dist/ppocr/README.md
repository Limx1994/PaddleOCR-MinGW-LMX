# PaddleOCR C++ 推理工具使用说明

基于 PaddlePaddle 推理库和 PaddleOCR 的 Windows C++ OCR 文字识别工具，使用 MinGW GCC 编译。

## 目录结构

```
dist/ppocr/
├── ppocr.exe                    # 主程序 (361MB, 静态链接模式)
├── models/                      # 推理模型目录
│   ├── PP-OCRv4_mobile_det_infer/   # 文本检测模型 (mobile)
│   └── PP-OCRv4_mobile_rec_infer/   # 文本识别模型 (mobile)
├── output/                      # 输出目录
├── .json                        # 最近一次推理结果
├── libopencv_world470.dll       # OpenCV 库
└── MinGW 运行时 DLL             # libgcc_s_seh-1.dll, libstdc++-6.dll 等
```

## 快速开始

### 基本用法

```batch
cd dist\ppocr

ppocr.exe ocr --input <图片路径>
```

### 示例

```batch
# 识别单张图片
ppocr.exe ocr --input ..\test_images\test.jpg

# 指定模型目录
ppocr.exe ocr --input test.jpg ^
  --text_detection_model_dir ./models/PP-OCRv4_mobile_det_infer ^
  --text_recognition_model_dir ./models/PP-OCRv4_mobile_rec_infer ^
  --text_detection_model_name PP-OCRv4_mobile_det ^
  --text_recognition_model_name PP-OCRv4_mobile_rec

# 禁用文档预处理（更快）
ppocr.exe ocr --input test.jpg ^
  --use_doc_orientation_classify false ^
  --use_doc_unwarping false ^
  --use_textline_orientation false

# 指定输出路径
ppocr.exe ocr --input test.jpg --save_path ./my_output/
```

## 命令行参数

### 必需参数

| 参数        | 说明            | 示例                 |
| --------- | ------------- | ------------------ |
| `ocr`     | 子命令，执行 OCR 识别 | `ppocr.exe ocr`    |
| `--input` | 输入图片路径        | `--input test.jpg` |

### 模型配置

| 参数                                     | 默认值                   | 说明          |
| -------------------------------------- | --------------------- | ----------- |
| `--text_detection_model_dir`           | 空                     | 文本检测模型目录    |
| `--text_recognition_model_dir`         | 空                     | 文本识别模型目录    |
| `--text_detection_model_name`          | `PP-OCRv5_server_det` | 检测模型名称      |
| `--text_recognition_model_name`        | `PP-OCRv5_server_rec` | 识别模型名称      |
| `--doc_orientation_classify_model_dir` | 空                     | 文档方向分类模型目录  |
| `--doc_unwarping_model_dir`            | 空                     | 文档矫正模型目录    |
| `--textline_orientation_model_dir`     | 空                     | 文本行方向分类模型目录 |

> **重要**: 使用 PP-OCRv4_mobile 模型时，必须通过 `--text_detection_model_name` 和 `--text_recognition_model_name` 指定模型名称，否则默认使用 PP-OCRv5_server 模型名称会导致加载失败。

### 流水线控制

| 参数                               | 默认值    | 说明          |
| -------------------------------- | ------ | ----------- |
| `--use_doc_orientation_classify` | `true` | 是否启用文档方向分类  |
| `--use_doc_unwarping`            | `true` | 是否启用文档矫正    |
| `--use_textline_orientation`     | `true` | 是否启用文本行方向分类 |
| `--lang`                         | 空      | 语言设置        |
| `--ocr_version`                  | 空      | OCR 版本      |

### 推理引擎配置

| 参数              | 默认值    | 说明               |
| --------------- | ------ | ---------------- |
| `--device`      | `cpu`  | 推理设备 (cpu/gpu:0) |
| `--precision`   | `fp32` | 计算精度 (fp32/fp16) |
| `--cpu_threads` | `8`    | CPU 推理线程数        |
| `--thread_num`  | `4`    | 流水线并行线程数         |

### 文本检测参数

| 参数                          | 默认值   | 说明                    |
| --------------------------- | ----- | --------------------- |
| `--text_det_limit_side_len` | `64`  | 输入图片边长限制              |
| `--text_det_limit_type`     | `min` | 边长限制类型 (min/max)      |
| `--text_det_thresh`         | `0.3` | 像素阈值，大于此值视为文本像素       |
| `--text_det_box_thresh`     | `0.6` | 检测框阈值，框内平均分大于此值视为文本区域 |
| `--text_det_unclip_ratio`   | `1.5` | 文本区域扩展系数，值越大扩展越多      |
| `--text_det_input_shape`    | 空     | 检测模型输入形状 (C,H,W)      |

### 文本识别参数

| 参数                              | 默认值 | 说明                |
| ------------------------------- | --- | ----------------- |
| `--text_recognition_batch_size` | `6` | 识别批处理大小           |
| `--text_rec_score_thresh`       | `0` | 识别分数阈值，低于此值的结果被过滤 |
| `--text_rec_input_shape`        | 空   | 识别模型输入形状 (C,H,W)  |

### 文本行方向分类参数

| 参数                                  | 默认值                          | 说明        |
| ----------------------------------- | ---------------------------- | --------- |
| `--textline_orientation_batch_size` | `6`                          | 方向分类批处理大小 |
| `--textline_orientation_model_name` | `PP-LCNet_x1_0_textline_ori` | 方向分类模型名称  |

### 输出配置

| 参数            | 默认值         | 说明     |
| ------------- | ----------- | ------ |
| `--save_path` | `./output/` | 结果保存路径 |

## 可用模型

### PP-OCRv4_mobile (已内置)

轻量级模型，适合移动端和边缘设备：

| 模型                  | 路径                                   | 用途   |
| ------------------- | ------------------------------------ | ---- |
| PP-OCRv4_mobile_det | `./models/PP-OCRv4_mobile_det_infer` | 文本检测 |
| PP-OCRv4_mobile_rec | `./models/PP-OCRv4_mobile_rec_infer` | 文本识别 |

### PP-OCRv5_server (需下载)

高精度服务端模型，需自行下载并放置到 `models/` 目录：

| 模型                  | 说明      |
| ------------------- | ------- |
| PP-OCRv5_server_det | 高精度文本检测 |
| PP-OCRv5_server_rec | 高精度文本识别 |

## 使用示例

### 示例 1: 最简单用法

```batch
ppocr.exe ocr --input test.jpg
```

使用默认配置，自动检测和识别图片中的文字。

### 示例 2: 指定 mobile 模型 (推荐)

```batch
ppocr.exe ocr --input test.jpg ^
  --text_detection_model_dir ./models/PP-OCRv4_mobile_det_infer ^
  --text_recognition_model_dir ./models/PP-OCRv4_mobile_rec_infer ^
  --text_detection_model_name PP-OCRv4_mobile_det ^
  --text_recognition_model_name PP-OCRv4_mobile_rec
```

### 示例 3: 快速模式 (禁用预处理)

```batch
ppocr.exe ocr --input test.jpg ^
  --text_detection_model_dir ./models/PP-OCRv4_mobile_det_infer ^
  --text_recognition_model_dir ./models/PP-OCRv4_mobile_rec_infer ^
  --text_detection_model_name PP-OCRv4_mobile_det ^
  --text_recognition_model_name PP-OCRv4_mobile_rec ^
  --use_doc_orientation_classify false ^
  --use_doc_unwarping false ^
  --use_textline_orientation false
```

禁用文档方向分类、文档矫正和文本行方向分类，显著提升速度。

### 示例 4: 高线程数 (多核 CPU)

```batch
ppocr.exe ocr --input test.jpg ^
  --text_detection_model_dir ./models/PP-OCRv4_mobile_det_infer ^
  --text_recognition_model_dir ./models/PP-OCRv4_mobile_rec_infer ^
  --text_detection_model_name PP-OCRv4_mobile_det ^
  --text_recognition_model_name PP-OCRv4_mobile_rec ^
  --cpu_threads 16 ^
  --thread_num 8
```

### 示例 5: 批量处理

```batch
for %%f in (*.jpg) do (
  ppocr.exe ocr --input "%%f" --save_path ./output/%%~nf/
)
```

## 输出格式

### JSON 输出

每次运行会在 `--save_path` 目录生成 JSON 结果文件，格式如下：

```json
{
    "input_path": "test.jpg",
    "rec_texts": ["识别的文字1", "识别的文字2"],
    "rec_scores": [0.95, 0.87],
    "rec_boxes": [[x1,y1,x2,y2], ...],
    "dt_polys": [[[x1,y1],[x2,y2],[x3,y3],[x4,y4]], ...]
}
```

| 字段           | 说明                           |
| ------------ | ---------------------------- |
| `rec_texts`  | 识别出的文本数组                     |
| `rec_scores` | 每个文本的置信度 (0-1)               |
| `rec_boxes`  | 文本区域边界框 [左上x, 左上y, 右下x, 右下y] |
| `dt_polys`   | 文本区域多边形顶点坐标                  |

### 可视化输出

默认在 `output/` 目录生成带标注的图片，显示检测到的文本区域和识别结果。

## 性能基准

### PP-OCRv4_mobile 模型

测试环境: Intel CPU, 20 逻辑处理器, 800×1079 图片

| 场景     | paddle 模式 | mkldnn 模式 |
| ------ | --------- | --------- |
| 单张图片   | 1.61s     | 1.68s     |
| 10 张图片 | 1.63s     | 3.55s     |

> **提示**: 对于 PP-OCRv4_mobile 模型，`paddle` 模式 (默认) 比 `mkldnn` 模式快约 2 倍。仅在使用大型 server 模型时考虑启用 mkldnn。

## 常见问题

### Q: 如何切换到 mkldnn 模式？

使用 server 模型时，mkldnn 可能更高效：

```batch
ppocr.exe ocr --input test.jpg ^
  --text_detection_model_dir ./models/PP-OCRv5_server_det ^
  --text_recognition_model_dir ./models/PP-OCRv5_server_rec ^
  --text_detection_model_name PP-OCRv5_server_det ^
  --text_recognition_model_name PP-OCRv5_server_rec
```

### Q: 模型加载失败怎么办？

确保 `--text_detection_model_name` 和 `--text_recognition_model_name` 与模型目录中的配置匹配。默认值为 `PP-OCRv5_server`，如果使用 `PP-OCRv4_mobile` 模型，必须显式指定模型名称。

### Q: 如何提高识别精度？

1. 使用 server 模型代替 mobile 模型
2. 增加 `--text_det_limit_side_len` 值（如 960）
3. 降低 `--text_det_box_thresh`（如 0.3）
4. 降低 `--text_rec_score_thresh` 以保留更多结果

### Q: 如何提高识别速度？

1. 使用 mobile 模型
2. 禁用文档预处理: `--use_doc_orientation_classify false --use_doc_unwarping false --use_textline_orientation false`
3. 增加线程数: `--cpu_threads 16 --thread_num 8`
4. 减小输入尺寸: `--text_det_limit_side_len 32`

### Q: 支持哪些图片格式？

支持 OpenCV 支持的所有格式：JPG、PNG、BMP、TIFF 等。

### Q: 如何处理中文路径？

Windows 下中文路径需要使用 UTF-8 编码的命令行。如果遇到问题，可以将图片复制到英文路径下。

## 编译说明

本工具使用 MinGW GCC 11.2.0 编译，采用静态链接模式（361MB）。Paddle 推理库、OpenCV 和 oneDNN 均静态链接到可执行文件中，运行时仅需 MinGW 运行时 DLL。

详细编译说明请参考项目根目录的 `BUILD_GUIDE.md`。

## 相关链接

- [PaddleOCR 官方文档](https://paddlepaddle.github.io/PaddleOCR/)
- [PaddlePaddle 推理库](https://www.paddlepaddle.org.cn/)
- [项目 GitHub](https://github.com/Limx1994/PaddleOCR-MinGW-LMX)

## 许可证

本项目禁止用于商业
