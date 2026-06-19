# OCR Service - TCP Socket 本地通讯（进程池架构）

基于 TCP Socket 的 OCR 服务，采用进程池架构，支持多次请求，其他程序可通过 TCP 协议调用 OCR 功能。

## 架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                       ppocr_service.exe                             │
├─────────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐    ┌─────────────────┐    ┌───────────────────┐  │
│  │ TCP Server  │───▶│  Process Pool   │───▶│  ppocr_worker.exe │  │
│  │ (localhost)  │    │  (请求路由)      │    │  (独立进程×N)     │  │
│  └─────────────┘    └─────────────────┘    └───────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
          │ TCP Socket (127.0.0.1:8080)
          ▼
   ┌─────────────┐
   │   Client    │
   │ (任何语言)  │
   └─────────────┘
```

**进程池工作原理：**
- 主进程（ppocr_service.exe）管理 N 个 worker 子进程
- 每个 worker 独立加载 PaddleOCR 模型，通过 stdin/stdout 管道通信
- 请求到达时，主进程分配给空闲 worker 处理
- 如果 worker 崩溃，主进程自动重启它

## 编译

### 前置条件

- Git LFS
- CMake 3.15+
- MinGW GCC 11.2.0+

### 静态模式编译（推荐）

```batch
cd PaddleOCR\deploy\cpp_infer
build_mingw.bat
```

输出目录：`D:\tmp\tmp\dist\ppocr\`

产物：
- `ppocr_service.exe` - TCP 服务（361MB）
- `ppocr_worker.exe` - Worker 子进程
- `ppocr_client.exe` - 测试客户端

### DLL 模式编译

```batch
cd PaddleOCR\deploy\cpp_infer
build_mingw.bat --dll
```

输出目录：`D:\tmp\tmp\dist\ppocr_dll\`

产物：
- `ppocr_service.exe` - TCP 服务（5.4MB）
- `ppocr_worker.exe` - Worker 子进程
- `ppocr_client.exe` - 测试客户端
- Paddle DLL + OpenCV DLL + MinGW 运行时 DLL

## 使用

### 启动服务（静态模式）

```batch
cd D:\tmp\tmp\dist\ppocr
ppocr_service.exe --model_dir ./models --port 8081 --pool_size 2
```

### 启动服务（DLL 模式）

```batch
cd D:\tmp\tmp\dist\ppocr_dll
ppocr_service.exe --model_dir ./models --port 8081 --pool_size 2
```

### 参数说明

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--host` | 127.0.0.1 | 监听地址 |
| `--port` | 8080 | 监听端口 |
| `--model_dir` | 空 | 模型目录 |
| `--pool_size` | 2 | Worker 进程数 |
| `--cpu_threads` | 8 | 每个 worker 的 CPU 线程数 |
| `--use_doc_orientation` | true | 使用文档方向分类 |
| `--use_doc_unwarping` | false | 使用文档去畸变 |
| `--use_textline_orientation` | false | 使用文本行方向分类 |
| `--fast_detect` | none | 快速检测模式：none, yolo |
| `--plate_model` | 空 | 车牌检测 ONNX 模型路径 |

## 通讯协议

### 请求格式

```
┌──────────────┬──────────────┬──────────────┐
│   Magic (4)  │  Length (4)  │   Data (N)   │
│   0xOCRREQ   │   uint32     │   JSON       │
└──────────────┴──────────────┴──────────────┘
```

### 响应格式

```
┌──────────────┬──────────────┬──────────────┐
│   Magic (4)  │  Length (4)  │   Data (N)   │
│   0xOCRRES   │   uint32     │   JSON       │
└──────────────┴──────────────┴──────────────┘
```

### 请求 JSON

```json
{
    "id": 1,
    "method": "ocr",
    "params": {
        "use_doc_orientation_classify": true,
        "use_doc_unwarping": false,
        "use_textline_orientation": false
    },
    "image_data": "base64编码的图片数据"
}
```

### 响应 JSON

```json
{
    "id": 1,
    "code": 0,
    "message": "success",
    "data": {
        "rec_texts": ["赣G·0522Y"],
        "rec_scores": [0.999],
        "rec_boxes": [[308, 686, 523, 756]]
    }
}
```

## 客户端示例

### C++ 客户端

```cpp
#include "tcp_client.h"
#include "base64_utils.h"

TcpClient client;
client.Connect("127.0.0.1", 8080);

// 读取图片并编码
std::string image_base64 = read_file_as_base64("test.jpg");

// 发送 OCR 请求
ResponseMessage res = client.SendOCRRequest(image_base64);

std::cout << "Code: " << res.code << std::endl;
std::cout << "Data: " << res.data << std::endl;

client.Disconnect();
```

### Python 客户端

```python
from client_example import OcrClient

client = OcrClient("127.0.0.1", 8080)
client.connect()

result = client.ocr("test.jpg", {
    "use_doc_orientation_classify": True,
    "use_doc_unwarping": False,
    "use_textline_orientation": False
})

print(result)

client.disconnect()
```

## API 方法

| 方法 | 说明 | 参数 |
|------|------|------|
| `ocr` | OCR 识别 | image_data, params |
| `status` | 获取状态 | 无 |
| `reload` | 重载模型 | 无 |

## 性能对比

| 方案 | 延迟 | 吞吐量 | 内存占用 |
|------|------|--------|----------|
| 命令行 | ~3.6s/次 | 低 | 每次释放 |
| 服务模式（进程池） | ~200ms/次 | 高 | 每个 worker ~500MB |
| 快速检测模式（YOLO） | ~50ms/次 | 更高 | 每个 worker ~500MB |

## 快速检测模式（车牌场景）

针对车牌识别场景，提供 YOLOv8-nano 快速检测模式，先裁剪车牌区域再进行 OCR 识别，大幅提升性能。

### 工作原理

```
原图 → YOLOv8-nano 检测车牌 (~10ms) → 裁剪车牌区域 → OCR 识别 (~0.1ms)
```

### 使用方法

```batch
cd D:\tmp\tmp\dist\ppocr
ppocr_service.exe --model_dir ./models --fast_detect yolo --plate_model ./models/plate_det.onnx
```

### 性能对比

| 方案 | 检测耗时 | 识别耗时 | 总耗时 |
|------|---------|---------|--------|
| 完整 OCR 流程 | ~500ms | ~1.5s | ~2s |
| YOLO 快速检测 | ~10ms | ~0.1ms | ~10ms |

### 模型要求

需要准备 YOLOv8-nano 车牌检测 ONNX 模型，可从以下来源获取：
- 自行训练的 YOLOv8-nano 模型
- 开源车牌检测模型（如 CCPD 数据集训练）

模型输入：640x640 RGB 图像
模型输出：[1, 4+num_classes, num_detections] 格式的检测结果

## 优势

1. **进程隔离**：每个 worker 独立进程，Paddle 运行时状态互不影响
2. **多次请求**：支持任意次数的 OCR 请求，不会崩溃
3. **自动恢复**：worker 崩溃时自动重启，服务不中断
4. **并发处理**：支持多个客户端同时请求
5. **低延迟**：模型常驻内存，每次请求约 200ms
6. **易集成**：任何语言都有 TCP Socket 库

## 注意事项

1. 服务启动时会加载模型，需要几秒钟时间
2. 每个 worker 进程独立加载模型，占用约 500MB 内存
3. 默认监听 127.0.0.1，仅本地可访问
4. 如需远程访问，修改 --host 参数为 0.0.0.0
5. 默认 2 个 worker 进程，可通过 --pool_size 调整

## 故障排除

### 服务启动失败

如果服务启动后立即退出，检查：
1. 模型目录是否存在
2. MinGW 运行时 DLL 版本是否匹配（必须使用 toolchain 目录中的 DLL）
3. 端口是否被占用

### DLL 模式启动失败

DLL 模式需要所有 DLL 文件在同一目录。如果启动失败：
1. 检查 `libpaddle_inference.dll`、`libphi_core.dll`、`libpir.dll`、`libcommon.dll` 是否存在
2. 检查 `libopencv_world470.dll` 是否存在
3. 检查 MinGW 运行时 DLL（`libgcc_s_seh-1.dll`、`libstdc++-6.dll`、`libwinpthread-1.dll`、`libgomp-1.dll`）是否为正确版本

### MinGW 运行时 DLL 版本不匹配

如果遇到 `exit code 127` 或 `exception c0000139` 错误，说明 MinGW 运行时 DLL 版本不匹配。

解决方案：
```batch
# 从 toolchain 目录复制正确的 DLL
cp toolchain\mingw\bin\libgcc_s_seh-1.dll dist\ppocr\
cp toolchain\mingw\bin\libstdc++-6.dll dist\ppocr\
cp toolchain\mingw\bin\libwinpthread-1.dll dist\ppocr\
cp toolchain\mingw\bin\libgomp-1.dll dist\ppocr\
```
