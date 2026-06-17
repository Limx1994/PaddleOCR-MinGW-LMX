English | [简体中文](README_ch.md)

# PP-OCR Deployment

- [PP-OCR Deployment](#pp-ocr-deployment)
  - [Paddle Deployment Introduction](#paddle-deployment-introduction)
  - [PP-OCR Deployment](#pp-ocr-deployment-1)

<a name="1"></a>
## Paddle Deployment Introduction

Paddle provides a variety of deployment schemes to meet the deployment requirements of different scenarios. Please choose according to the actual situation:

<div align="center">
    <img src="../doc/deployment_en.png" width="800">
</div>


<a name="2"></a>
## PP-OCR Deployment

PP-OCR has supported multi deployment schemes. Click the link to get the specific tutorial.

- [Python Inference](../doc/doc_en/inference_ppocr_en.md)
- [C++ Inference (MinGW/MSVC/Linux)](./cpp_infer/README.md)
- [Serving (Python/C++)](./pdserving/README.md)
- [Paddle-Lite (ARM CPU/OpenCL ARM GPU)](./lite/readme.md)
- [Paddle2ONNX](./paddle2onnx/readme.md)

If you need the deployment tutorial of academic algorithm models other than PP-OCR, please directly enter the main page of corresponding algorithms, [entrance](../doc/doc_en/algorithm_overview_en.md)。

## MinGW Build (Windows)

Build PaddleOCR C++ inference engine with MinGW GCC. See [C++ Inference README](./cpp_infer/README.md) for details.

```batch
cd deploy\cpp_infer
build_mingw.bat
```
