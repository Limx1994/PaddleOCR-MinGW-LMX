# PaddleOCR 服务性能调优指南

## 目录

- [1. 概述](#1-概述)
- [2. 架构分析](#2-架构分析)
- [3. 关键参数说明](#3-关键参数说明)
- [4. 性能测试结果](#4-性能测试结果)
- [5. 最佳配置建议](#5-最佳配置建议)
- [6. 调优策略](#6-调优策略)
- [7. 常见问题与解决方案](#7-常见问题与解决方案)
- [8. 监控与诊断](#8-监控与诊断)

---

## 1. 概述

本文档基于项目实际代码和压力测试结果，提供 PaddleOCR 服务模式的性能调优指南。涵盖进程池配置、CPU 线程优化、并发处理策略等关键方面。

### 1.1 适用范围

| 模式     | 适用性   | 说明           |
| ------ | ----- | ------------ |
| 单次模式   | ⚠️ 有限 | 无进程池，每次启动开销大 |
| 服务模式   | ✅ 推荐  | 进程池复用，适合高并发  |
| 快速检测模式 | ✅ 推荐  | 车牌专用，吞吐量高    |

---

## 2. 架构分析

### 2.1 服务模式架构

```
┌─────────────────────────────────────────────────────────────┐
│                    ppocr_service.exe                         │
│  ┌───────────────────────────────────────────────────────┐  │
│  │                   TcpServer                           │  │
│  │   - 多线程处理客户端连接                               │  │
│  │   - 自定义二进制协议 (Magic + Length + JSON)           │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↓                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │                  ProcessPool                          │  │
│  │   - 管理多个 worker 进程                              │  │
│  │   - 负载均衡与故障恢复                                │  │
│  └───────────────────────────────────────────────────────┘  │
│         ↓              ↓              ↓                     │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐               │
│  │ Worker 1 │   │ Worker 2 │   │ Worker N │               │
│  │ (独立进程)│   │ (独立进程)│   │ (独立进程)│               │
│  └──────────┘   └──────────┘   └──────────┘               │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Worker 进程内部流程

```
stdin 接收图片路径
       ↓
  文件存在检查
       ↓
  PaddleOCR Predict
       ↓
  结果转 JSON
       ↓
stdout 输出 "OK {json}"
```

### 2.3 关键代码位置

| 组件        | 文件路径                                                   | 关键函数                        |
| --------- | ------------------------------------------------------ | --------------------------- |
| 服务主程序     | `PaddleOCR/deploy/cpp_infer/service/service_main.cc`   | `main()`                    |
| 进程池       | `PaddleOCR/deploy/cpp_infer/service/process_pool.cc`   | `Init()`, `ProcessOCR()`    |
| Worker 进程 | `PaddleOCR/deploy/cpp_infer/service/worker_main.cc`    | `main()`                    |
| TCP 服务器   | `PaddleOCR/deploy/cpp_infer/service/tcp_server.h`      | `Start()`, `HandleClient()` |
| 车牌检测      | `PaddleOCR/deploy/cpp_infer/service/plate_detector.cc` | `Detect()`                  |

---

## 3. 关键参数说明

### 3.1 进程池参数

| 参数              | 默认值 | 范围   | 说明                  |
| --------------- | --- | ---- | ------------------- |
| `--pool_size`   | 2   | 1-16 | Worker 进程数量         |
| `--cpu_threads` | 8   | 1-32 | 每个 Worker 的 CPU 线程数 |
| `--max_retries` | 2   | 0-5  | 请求最大重试次数            |

### 3.2 模型参数

| 参数                 | 默认值                   | 说明             |
| ------------------ | --------------------- | -------------- |
| `--model_dir`      | (必填)                  | 模型根目录（自动检测子模型） |
| `--det_model_dir`  | (可选)                  | 检测模型目录（覆盖自动检测） |
| `--det_model_name` | PP-OCRv4_mobile_det   | 检测模型名称         |
| `--rec_model_dir`  | (可选)                  | 识别模型目录（覆盖自动检测） |
| `--rec_model_name` | PP-OCRv4_mobile_rec   | 识别模型名称         |
| `--cls_model_dir`  | (可选)                  | 分类模型目录（覆盖自动检测） |
| `--cls_model_name` | PP-LCNet_x1_0_doc_ori | 分类模型名称         |

### 3.3 功能开关参数

| 参数                           | 默认值   | 说明               |
| ---------------------------- | ----- | ---------------- |
| `--use_doc_orientation`      | true  | 文档方向分类           |
| `--use_doc_unwarping`        | false | 文档去扭曲            |
| `--use_textline_orientation` | false | 文本行方向分类          |
| `--fast_detect`              | none  | 快速检测模式：none/yolo |
| `--plate_model`              | (空)   | 车牌检测 ONNX 模型路径   |

### 3.4 网络参数

| 参数             | 默认值       | 说明         |
| -------------- | --------- | ---------- |
| `--host`       | 127.0.0.1 | 监听地址       |
| `--port`       | 8080      | 监听端口       |
| `--thread_num` | 4         | TCP 服务器线程数 |

---

## 4. 性能测试结果

### 4.1 测试环境

| 项目   | 配置                            |
| ---- | ----------------------------- |
| CPU  | Intel Core i7-10700 @ 2.90GHz |
| 内存   | 32GB DDR4                     |
| 操作系统 | Windows 10 Pro                |
| 编译器  | MinGW GCC 11.2.0              |
| 测试图片 | 1920x1080 JPEG                |

### 4.2 单次模式 vs 服务模式

| 指标       | 单次模式       | 服务模式 (2 workers) | 服务模式 (4 workers) |
| -------- | ---------- | ---------------- | ---------------- |
| 吞吐量      | ~0.6 req/s | 5.49 req/s       | 6.07 req/s       |
| 平均响应时间   | ~1.5s      | 0.664s           | 0.654s           |
| P95 响应时间 | ~2.0s      | 0.820s           | 0.780s           |
| 成功率      | 100%       | 95%              | 100%             |
| 内存占用     | ~200MB     | ~600MB           | ~1.2GB           |

### 4.3 并发测试结果

**配置：2 workers, 4 并发**

| 指标     | 值          |
| ------ | ---------- |
| 总请求数   | 20         |
| 成功请求数  | 19         |
| 失败请求数  | 1          |
| 成功率    | 95%        |
| 总耗时    | 3.64s      |
| 吞吐量    | 5.49 req/s |
| 平均响应时间 | 0.664s     |
| 最小响应时间 | 0.512s     |
| 最大响应时间 | 0.820s     |

**配置：4 workers, 4 并发**

| 指标     | 值          |
| ------ | ---------- |
| 总请求数   | 20         |
| 成功请求数  | 20         |
| 失败请求数  | 0          |
| 成功率    | 100%       |
| 总耗时    | 3.30s      |
| 吞吐量    | 6.07 req/s |
| 平均响应时间 | 0.654s     |
| 最小响应时间 | 0.508s     |
| 最大响应时间 | 0.780s     |

### 4.4 快速检测模式

| 指标     | 值         |
| ------ | --------- |
| 吞吐量    | ~10 req/s |
| 平均响应时间 | ~0.3s     |
| 成功率    | 100%      |
| 内存占用   | ~300MB    |

---

## 5. 最佳配置建议

### 5.1 配置矩阵

| 场景               | pool_size | cpu_threads | 推荐理由                |
| ---------------- | --------- | ----------- | ------------------- |
| 低并发 (1-2 req/s)  | 2         | 4           | 节省内存，满足需求           |
| 中并发 (3-5 req/s)  | 4         | 4           | 平衡性能与资源             |
| 高并发 (6-10 req/s) | 4-8       | 2-4         | 最大化吞吐量              |
| 车牌识别专用           | 2-4       | 4           | 配合 fast_detect=yolo |

### 5.2 推荐配置

#### 配置 1：标准配置（推荐）

```bash
ppocr_service.exe \
  --model_dir "D:/models" \
  --pool_size 4 \
  --cpu_threads 4 \
  --port 8080
```

**适用场景：** 通用 OCR 服务，中等并发

**预期性能：**

- 吞吐量：5-6 req/s
- 响应时间：0.6-0.8s
- 成功率：100%

#### 配置 2：高并发配置

```bash
ppocr_service.exe \
  --model_dir "D:/models" \
  --pool_size 8 \
  --cpu_threads 2 \
  --port 8080
```

**适用场景：** 高并发场景，多核 CPU

**预期性能：**

- 吞吐量：8-10 req/s
- 响应时间：0.8-1.2s
- 成功率：100%

#### 配置 3：车牌识别配置

```bash
ppocr_service.exe \
  --model_dir "D:/models" \
  --fast_detect yolo \
  --plate_model "D:/models/plate_detect.onnx" \
  --pool_size 2 \
  --cpu_threads 4 \
  --port 8080
```

**适用场景：** 车牌识别专用

**预期性能：**

- 吞吐量：10-15 req/s
- 响应时间：0.2-0.4s
- 成功率：100%

#### 配置 4：资源受限配置

```bash
ppocr_service.exe \
  --model_dir "D:/models" \
  --pool_size 2 \
  --cpu_threads 4 \
  --use_doc_orientation false \
  --port 8080
```

**适用场景：** 内存或 CPU 资源有限

**预期性能：**

- 吞吐量：3-4 req/s
- 响应时间：0.8-1.0s
- 成功率：100%

---

## 6. 调优策略

### 6.1 Worker 数量调优

#### 公式

```
推荐 pool_size = min(CPU核心数 / 2, 预期并发数)
```

#### 示例

| CPU 核心数 | 预期并发 | 推荐 pool_size |
| ------- | ---- | ------------ |
| 4       | 2    | 2            |
| 8       | 4    | 4            |
| 16      | 8    | 8            |
| 32      | 16   | 16           |

#### 注意事项

1. **内存限制**：每个 Worker 约占用 200-300MB 内存
2. **CPU 争用**：过多 Worker 会导致 CPU 争用，反而降低性能
3. **故障恢复**：建议至少 2 个 Worker，确保单点故障不影响服务

### 6.2 CPU 线程数调优

#### 原则

- **低并发**：增加 `cpu_threads` 提升单请求速度
- **高并发**：减少 `cpu_threads` 避免争用

#### 推荐值

| pool_size | 推荐 cpu_threads | 说明       |
| --------- | -------------- | -------- |
| 1-2       | 8-16           | 充分利用 CPU |
| 4         | 4              | 平衡配置     |
| 8         | 2-4            | 高并发优化    |
| 16        | 1-2            | 最大并发     |

### 6.3 功能开关优化

| 功能                         | 性能影响      | 建议        |
| -------------------------- | --------- | --------- |
| `use_doc_orientation`      | +20-30ms  | 保留（提升准确率） |
| `use_doc_unwarping`        | +50-100ms | 关闭（除非必要）  |
| `use_textline_orientation` | +10-20ms  | 关闭（除非必要）  |

### 6.4 模型选择

| 模型                            | 速度  | 准确率 | 推荐场景  |
| ----------------------------- | --- | --- | ----- |
| PP-OCRv4_mobile               | 快   | 中   | 通用场景  |
| PP-OCRv4_server               | 慢   | 高   | 高精度需求 |
| PP-OCRv4_mobile + fast_detect | 最快  | 中   | 车牌识别  |

---

## 7. 常见问题与解决方案

### 7.1 Worker 启动失败

**症状：**

```
Failed to start worker 0
No workers started
```

**原因：**

- 模型路径错误
- 内存不足
- 依赖库缺失

**解决方案：**

1. 检查模型目录是否存在
2. 检查内存使用情况
3. 检查 PaddleOCR 动态库是否在 PATH 中

### 7.2 请求超时

**症状：**

```
No available worker
All retries failed
```

**原因：**

- 所有 Worker 都在忙
- Worker 进程崩溃
- 网络问题

**解决方案：**

1. 增加 `pool_size`
2. 检查 Worker 日志
3. 检查网络连接

### 7.3 内存溢出

**症状：**

- 进程被 OOM Killer 终止
- 响应时间急剧增加

**解决方案：**

1. 减少 `pool_size`
2. 减少 `cpu_threads`
3. 关闭不必要的功能（如 `use_doc_unwarping`）

### 7.4 CPU 占用过高

**症状：**

- CPU 使用率 100%
- 响应时间不稳定

**解决方案：**

1. 减少 `pool_size`
2. 减少 `cpu_threads`
3. 增加 Worker 间隔（修改代码中的 sleep）

---

## 8. 监控与诊断

### 8.1 状态查询

使用 `status` 方法查询服务状态：

```json
{
  "method": "status"
}
```

响应示例：

```json
{
  "initialized": true,
  "pool_size": 4,
  "alive_workers": 4,
  "idle_workers": 2,
  "busy_workers": 2,
  "dead_workers": 0,
  "total_requests": 1000,
  "success_requests": 995,
  "failed_requests": 5,
  "worker_restarts": 0
}
```

### 8.2 关键指标

| 指标              | 正常范围        | 异常处理          |
| --------------- | ----------- | ------------- |
| alive_workers   | = pool_size | 检查 Worker 日志  |
| idle_workers    | > 0         | 增加 pool_size  |
| dead_workers    | = 0         | 检查模型路径和内存     |
| failed_requests | < 1%        | 检查图片格式和大小     |
| worker_restarts | = 0         | 检查 Worker 稳定性 |

### 8.3 压力测试

使用 `stress_test.py` 进行压力测试：

```bash
# 基础测试
python stress_test.py --host 127.0.0.1 --port 8080 -n 20 -c 4

# 高并发测试
python stress_test.py --host 127.0.0.1 --port 8080 -n 100 -c 10

# 持续测试
python stress_test.py --host 127.0.0.1 --port 8080 -n 1000 -c 8 --interval 0.1
```

### 8.4 性能监控脚本

```python
import socket
import json
import time

def monitor_service(host, port, interval=5):
    """监控服务状态"""
    while True:
        try:
            # 发送 status 请求
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect((host, port))

            request = {
                "method": "status"
            }

            # 发送请求（使用自定义协议）
            data = json.dumps(request).encode('utf-8')
            header = b'\x00OCRQ' + len(data).to_bytes(4, 'little')
            sock.send(header + data)

            # 接收响应
            response = sock.recv(4096)
            sock.close()

            # 解析响应
            if len(response) > 8:
                status = json.loads(response[8:])
                print(f"[{time.strftime('%H:%M:%S')}] "
                      f"Workers: {status['alive_workers']}/{status['pool_size']}, "
                      f"Idle: {status['idle_workers']}, "
                      f"Busy: {status['busy_workers']}, "
                      f"Requests: {status['total_requests']}, "
                      f"Failed: {status['failed_requests']}")

        except Exception as e:
            print(f"[{time.strftime('%H:%M:%S')}] Error: {e}")

        time.sleep(interval)

if __name__ == "__main__":
    monitor_service("127.0.0.1", 8080)
```

---

## 附录 A：性能优化检查清单

- [ ] 确认 `pool_size` >= 预期并发数
- [ ] 确认 `cpu_threads` * `pool_size` <= CPU 核心数
- [ ] 确认内存足够（每个 Worker 约 200-300MB）
- [ ] 关闭不必要的功能（`use_doc_unwarping`, `use_textline_orientation`）
- [ ] 使用快速检测模式（如果适用）
- [ ] 定期监控服务状态
- [ ] 定期进行压力测试

## 附录 B：性能优化公式

### 吞吐量估算

```
吞吐量 ≈ pool_size / 平均响应时间
```

### 内存估算

```
总内存 ≈ pool_size * 250MB + 基础内存 100MB
```

### CPU 估算

```
总 CPU 线程数 = pool_size * cpu_threads
建议：总 CPU 线程数 <= CPU 核心数 * 1.5
```

---

**文档版本：** v1.0  
**最后更新：** 2026-06-20  
**适用版本：** PaddleOCR v2.4.0
