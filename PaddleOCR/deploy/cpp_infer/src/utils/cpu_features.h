// Copyright (c) 2025 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#else
#include <cpuid.h>
#include <unistd.h>
#endif

struct CPUFeatures {
  bool sse = false;
  bool sse2 = false;
  bool sse3 = false;
  bool ssse3 = false;
  bool sse41 = false;
  bool sse42 = false;
  bool avx = false;
  bool avx2 = false;
  bool avx512f = false;
  bool avx512bw = false;
  bool avx512dq = false;
  bool avx512vl = false;
  bool fma = false;
  bool f16c = false;
  bool bmi1 = false;
  bool bmi2 = false;
  bool popcnt = false;
  bool aes = false;
  bool pclmulqdq = false;

  std::string vendor;
  std::string brand;
  int family = 0;
  int model = 0;
  int stepping = 0;
  int num_cores = 0;
  int num_threads = 0;

  static CPUFeatures Detect() {
    CPUFeatures features;

    // Get vendor string
    int regs[4] = {0};
    char vendor[13] = {0};

#ifdef _WIN32
    __cpuid(regs, 0);
#else
    __cpuid(0, regs[0], regs[1], regs[2], regs[3]);
#endif

    *reinterpret_cast<int*>(vendor + 0) = regs[1];  // EBX
    *reinterpret_cast<int*>(vendor + 4) = regs[3];  // EDX
    *reinterpret_cast<int*>(vendor + 8) = regs[2];  // ECX
    features.vendor = vendor;

    // Get CPU features
#ifdef _WIN32
    __cpuid(regs, 1);
#else
    __cpuid(1, regs[0], regs[1], regs[2], regs[3]);
#endif

    int cpu_info1[4] = {regs[0], regs[1], regs[2], regs[3]};

    // Family and model
    int family_id = (cpu_info1[0] >> 8) & 0xF;
    int ext_family = (cpu_info1[0] >> 20) & 0xFF;
    int model_id = (cpu_info1[0] >> 4) & 0xF;
    int ext_model = (cpu_info1[0] >> 16) & 0xF;

    features.family = family_id + (family_id == 15 ? ext_family : 0);
    features.model = model_id + (model_id == 15 || model_id == 6 ? (ext_model << 4) : 0);
    features.stepping = cpu_info1[0] & 0xF;

    // ECX features
    features.sse3 = (cpu_info1[2] >> 0) & 1;
    features.pclmulqdq = (cpu_info1[2] >> 1) & 1;
    features.ssse3 = (cpu_info1[2] >> 9) & 1;
    features.fma = (cpu_info1[2] >> 12) & 1;
    features.sse41 = (cpu_info1[2] >> 19) & 1;
    features.sse42 = (cpu_info1[2] >> 20) & 1;
    features.aes = (cpu_info1[2] >> 25) & 1;
    features.avx = (cpu_info1[2] >> 28) & 1;
    features.f16c = (cpu_info1[2] >> 29) & 1;

    // EDX features
    features.sse = (cpu_info1[3] >> 25) & 1;
    features.sse2 = (cpu_info1[3] >> 26) & 1;
    features.popcnt = (cpu_info1[3] >> 23) & 1;

    // Extended features (CPUID leaf 7)
#ifdef _WIN32
    __cpuidex(regs, 7, 0);
#else
    __cpuid_count(7, 0, regs[0], regs[1], regs[2], regs[3]);
#endif

    int cpu_info7[4] = {regs[0], regs[1], regs[2], regs[3]};

    features.bmi1 = (cpu_info7[1] >> 3) & 1;
    features.avx2 = (cpu_info7[1] >> 5) & 1;
    features.bmi2 = (cpu_info7[1] >> 8) & 1;
    features.avx512f = (cpu_info7[1] >> 16) & 1;
    features.avx512dq = (cpu_info7[1] >> 17) & 1;
    features.avx512bw = (cpu_info7[1] >> 30) & 1;
    features.avx512vl = (cpu_info7[1] >> 31) & 1;

    // Get brand string
    char brand[49] = {0};
    for (int i = 0; i < 3; i++) {
#ifdef _WIN32
      __cpuid(regs, 0x80000002 + i);
#else
      __cpuid(0x80000002 + i, regs[0], regs[1], regs[2], regs[3]);
#endif
      *reinterpret_cast<int*>(brand + i * 16 + 0) = regs[0];
      *reinterpret_cast<int*>(brand + i * 16 + 4) = regs[1];
      *reinterpret_cast<int*>(brand + i * 16 + 8) = regs[2];
      *reinterpret_cast<int*>(brand + i * 16 + 12) = regs[3];
    }
    features.brand = brand;

    // Get core count
    features.num_cores = GetNumCores();
    features.num_threads = GetNumThreads();

    return features;
  }

  std::string GetSIMDLevel() const {
    if (avx512f && avx512bw && avx512dq && avx512vl) {
      return "AVX-512";
    } else if (avx2) {
      return "AVX2";
    } else if (avx) {
      return "AVX";
    } else if (sse42) {
      return "SSE4.2";
    } else if (sse41) {
      return "SSE4.1";
    } else if (ssse3) {
      return "SSSE3";
    } else if (sse3) {
      return "SSE3";
    } else if (sse2) {
      return "SSE2";
    } else if (sse) {
      return "SSE";
    }
    return "Unknown";
  }

  std::string GetRecommendedFlags() const {
    std::string flags;
    if (avx512f && avx512bw && avx512dq && avx512vl) {
      flags = "-mavx512f -mavx512bw -mavx512dq -mavx512vl";
    } else if (avx2) {
      flags = "-mavx2";
    } else if (avx) {
      flags = "-mavx";
    } else if (sse42) {
      flags = "-msse4.2";
    } else if (sse41) {
      flags = "-msse4.1";
    } else if (ssse3) {
      flags = "-mssse3";
    } else if (sse3) {
      flags = "-msse3";
    } else if (sse2) {
      flags = "-msse2";
    } else if (sse) {
      flags = "-msse";
    }

    if (fma) {
      flags += " -mfma";
    }
    if (f16c) {
      flags += " -mf16c";
    }
    if (bmi1) {
      flags += " -mbmi";
    }
    if (bmi2) {
      flags += " -mbmi2";
    }

    return flags;
  }

  std::string ToString() const {
    std::string info;
    info += "CPU: " + brand + "\n";
    info += "Vendor: " + vendor + "\n";
    info += "Family: " + std::to_string(family) + ", Model: " + std::to_string(model) + ", Stepping: " + std::to_string(stepping) + "\n";
    info += "Logical Processors: " + std::to_string(num_cores) + "\n";
    info += "Available Threads: " + std::to_string(num_threads) + "\n";
    info += "SIMD Level: " + GetSIMDLevel() + "\n";
    info += "Recommended Flags: " + GetRecommendedFlags() + "\n";
    info += "Features: ";
    if (sse) info += "SSE ";
    if (sse2) info += "SSE2 ";
    if (sse3) info += "SSE3 ";
    if (ssse3) info += "SSSE3 ";
    if (sse41) info += "SSE4.1 ";
    if (sse42) info += "SSE4.2 ";
    if (avx) info += "AVX ";
    if (avx2) info += "AVX2 ";
    if (avx512f) info += "AVX-512F ";
    if (avx512bw) info += "AVX-512BW ";
    if (avx512dq) info += "AVX-512DQ ";
    if (avx512vl) info += "AVX-512VL ";
    if (fma) info += "FMA ";
    if (f16c) info += "F16C ";
    if (bmi1) info += "BMI1 ";
    if (bmi2) info += "BMI2 ";
    if (popcnt) info += "POPCNT ";
    if (aes) info += "AES ";
    if (pclmulqdq) info += "PCLMULQDQ ";
    return info;
  }

private:
  static int GetNumCores() {
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#else
    return sysconf(_SC_NPROCESSORS_ONLN);
#endif
  }

  static int GetNumThreads() {
#ifdef _WIN32
    // Count logical processors
    DWORD_PTR process_affinity_mask, system_affinity_mask;
    if (GetProcessAffinityMask(GetCurrentProcess(), &process_affinity_mask, &system_affinity_mask)) {
      int count = 0;
      DWORD_PTR m = process_affinity_mask;
      while (m) {
        count += m & 1;
        m >>= 1;
      }
      return count;
    }
    return GetNumCores();
#else
    return sysconf(_SC_NPROCESSORS_ONLN);
#endif
  }
};
