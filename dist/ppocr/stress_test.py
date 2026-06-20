#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
压力测试脚本 - 测试 OCR 服务并发性能
"""

import socket
import json
import base64
import struct
import time
import threading
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed

# 配置
HOST = "127.0.0.1"
PORT = 8080
IMAGE_PATH = "test.jpg"
NUM_REQUESTS = 20  # 总请求数
CONCURRENCY = 4    # 并发数


def send_ocr_request(image_data_base64, request_id=1):
    """发送 OCR 请求"""
    start_time = time.time()

    # 重试机制
    for retry in range(3):
        try:
            # 创建请求 JSON
            request_json = {
                "id": request_id,
                "method": "ocr",
                "params": {
                    "use_doc_orientation_classify": True,
                    "use_doc_unwarping": False,
                    "use_textline_orientation": False
                },
                "image_data": image_data_base64
            }

            json_str = json.dumps(request_json)
            json_bytes = json_str.encode("utf-8")

            # 构建消息：magic (4 bytes) + length (4 bytes) + data
            magic = 0x4F435251  # "OCRQ"
            message = struct.pack("II", magic, len(json_bytes)) + json_bytes

            # 连接服务器
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(60)  # 60 秒超时

            try:
                sock.connect((HOST, PORT))
            except ConnectionRefusedError:
                time.sleep(0.1)
                continue

            # 发送请求
            sock.sendall(message)

            # 接收响应头 (8 bytes: magic + length)
            header_data = b""
            while len(header_data) < 8:
                chunk = sock.recv(8 - len(header_data))
                if not chunk:
                    sock.close()
                    time.sleep(0.1)
                    continue
                header_data += chunk

            resp_magic, resp_length = struct.unpack("II", header_data)

            # 验证响应 magic
            if resp_magic != 0x4F435253:  # "OCRS"
                sock.close()
                time.sleep(0.1)
                continue

            # 接收响应数据
            response_data = b""
            while len(response_data) < resp_length:
                chunk = sock.recv(min(4096, resp_length - len(response_data)))
                if not chunk:
                    break
                response_data += chunk

            sock.close()

            # 解析响应
            if len(response_data) < resp_length:
                time.sleep(0.1)
                continue

            response = json.loads(response_data.decode("utf-8"))
            elapsed = time.time() - start_time

            return {
                "success": response.get("code") == 0,
                "time": elapsed,
                "result": response
            }

        except socket.timeout:
            error_msg = f"Timeout (retry {retry+1})"
            time.sleep(0.1)
            continue
        except ConnectionResetError as e:
            error_msg = f"Connection reset (retry {retry+1}): {e}"
            time.sleep(0.1)
            continue
        except ConnectionRefusedError as e:
            error_msg = f"Connection refused (retry {retry+1}): {e}"
            time.sleep(0.1)
            continue
        except Exception as e:
            error_msg = f"Error (retry {retry+1}): {type(e).__name__}: {e}"
            time.sleep(0.1)
            continue

    return {"success": False, "time": time.time() - start_time, "error": error_msg}


def run_stress_test():
    """运行压力测试"""
    print("=" * 60)
    print("OCR 服务压力测试")
    print("=" * 60)
    print(f"目标地址: {HOST}:{PORT}")
    print(f"测试图片: {IMAGE_PATH}")
    print(f"总请求数: {NUM_REQUESTS}")
    print(f"并发数:   {CONCURRENCY}")
    print("=" * 60)

    # 读取图片
    try:
        with open(IMAGE_PATH, "rb") as f:
            image_data = base64.b64encode(f.read()).decode()
        print(f"图片大小: {len(image_data)} bytes (base64)")
    except FileNotFoundError:
        print(f"错误: 找不到图片文件 {IMAGE_PATH}")
        return

    # 预热连接
    print("\n预热连接...")
    warmup_result = send_ocr_request(image_data)
    if warmup_result["success"]:
        print(f"预热成功: {warmup_result['time']:.3f}s")
    else:
        print(f"预热失败: {warmup_result.get('error', 'Unknown error')}")
        return

    # 压力测试
    print(f"\n开始压力测试 ({NUM_REQUESTS} 请求, {CONCURRENCY} 并发)...")
    print("-" * 60)

    results = []
    start_time = time.time()

    with ThreadPoolExecutor(max_workers=CONCURRENCY) as executor:
        futures = []
        for i in range(NUM_REQUESTS):
            future = executor.submit(send_ocr_request, image_data)
            futures.append((i + 1, future))

        for i, future in futures:
            result = future.result()
            results.append(result)
            status = "OK" if result["success"] else "FAIL"
            print(f"[{i:3d}/{NUM_REQUESTS}] {status} {result['time']:.3f}s")

    total_time = time.time() - start_time

    # 统计结果
    print("\n" + "=" * 60)
    print("测试结果统计")
    print("=" * 60)

    success_count = sum(1 for r in results if r["success"])
    fail_count = NUM_REQUESTS - success_count

    times = [r["time"] for r in results if r["success"]]
    if times:
        avg_time = sum(times) / len(times)
        min_time = min(times)
        max_time = max(times)
        throughput = success_count / total_time
    else:
        avg_time = min_time = max_time = throughput = 0

    print(f"总请求数:     {NUM_REQUESTS}")
    print(f"成功请求:     {success_count}")
    print(f"失败请求:     {fail_count}")
    print(f"成功率:       {success_count / NUM_REQUESTS * 100:.1f}%")
    print(f"总耗时:       {total_time:.3f}s")
    print(f"吞吐量:       {throughput:.2f} req/s")
    print(f"平均响应时间: {avg_time:.3f}s")
    print(f"最小响应时间: {min_time:.3f}s")
    print(f"最大响应时间: {max_time:.3f}s")

    # 错误详情
    if fail_count > 0:
        print("\n错误详情:")
        for i, r in enumerate(results):
            if not r["success"]:
                print(f"  [{i+1}] {r.get('error', 'Unknown error')}")

    print("\n" + "=" * 60)


if __name__ == "__main__":
    run_stress_test()
