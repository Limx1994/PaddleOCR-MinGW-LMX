// Copyright (c) 2023 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>

namespace phi {

#ifdef __cpp_lib_void_t
using std::void_t;
#else
template <typename...>
using void_t = void;
#endif

template <typename T, typename = void>
struct IsAccumulatable : std::false_type {};

template <>
struct IsAccumulatable<std::string, void> : std::false_type {};

template <typename T>
struct IsAccumulatable<T,
                       void_t<decltype(std::declval<T>() += std::declval<T>())>>
    : std::true_type {};

template <typename T>
class ThreadDataRegistry {
 public:
  // Immortal singleton: heap-allocated, never deleted.
  static ThreadDataRegistry& GetInstance() {
    static ThreadDataRegistry* instance = new ThreadDataRegistry();
    return *instance;
  }

  T* GetMutableCurrentThreadData() { return &CurrentThreadData(); }

  const T& GetCurrentThreadData() { return CurrentThreadData(); }

  template <typename Alias = T,
            typename = std::enable_if_t<std::is_copy_assignable<Alias>::value>>
  void SetCurrentThreadData(const T& val) {
    CurrentThreadData() = val;
  }

  template <
      typename Alias = T,
      typename = std::enable_if_t<std::is_copy_constructible<Alias>::value>>
  std::unordered_map<uint64_t, T> GetAllThreadDataByValue() {
    return impl_->GetAllThreadDataByValue();
  }

  std::unordered_map<uint64_t, std::reference_wrapper<T>>
  GetAllThreadDataByRef() {
    return impl_->GetAllThreadDataByRef();
  }

 private:
#if defined(__clang__) || defined(__GNUC__)
#ifndef __APPLE__
#if __cplusplus >= 201703L
  using LockType = std::shared_mutex;
  using SharedLockGuardType = std::shared_lock<std::shared_mutex>;
#elif __cplusplus >= 201402L
  using LockType = std::shared_timed_mutex;
  using SharedLockGuardType = std::shared_lock<std::shared_timed_mutex>;
#else
  using LockType = std::mutex;
  using SharedLockGuardType = std::lock_guard<std::mutex>;
#endif
#else
  using LockType = std::mutex;
  using SharedLockGuardType = std::lock_guard<std::mutex>;
#endif
#elif defined(_MSC_VER)
#if _MSVC_LANG >= 201703L
  using LockType = std::shared_mutex;
  using SharedLockGuardType = std::shared_lock<std::shared_mutex>;
#elif _MSVC_LANG >= 201402L
  using LockType = std::shared_timed_mutex;
  using SharedLockGuardType = std::shared_lock<std::shared_timed_mutex>;
#else
  using LockType = std::mutex;
  using SharedLockGuardType = std::lock_guard<std::mutex>;
#endif
#else
  using LockType = std::mutex;
  using SharedLockGuardType = std::lock_guard<std::mutex>;
#endif

  class ThreadDataHolder;
  class ThreadDataRegistryImpl {
   public:
    void RegisterData(uint64_t tid, ThreadDataHolder* tls_obj) {
      std::lock_guard<LockType> guard(lock_);
      tid_map_[tid] = tls_obj;
    }

    template <typename Alias = T>
    void AccumulateToAnotherThread(...) {}

    template <typename Alias = T,
              typename = std::enable_if_t<IsAccumulatable<Alias>::value>>
    void AccumulateToAnotherThread(uint64_t tid) {
      auto it = tid_map_.find(tid);
      if (it == tid_map_.end()) return;
      auto& data = it->second->GetData();
      for (auto& kv : tid_map_) {
        if (kv.first != tid) {
          auto& data_in_another_thread = kv.second->GetData();
          data_in_another_thread += data;
          VLOG(2) << "Add data " << data << " from thread " << tid << " to "
                  << kv.first << " , after update, data is "
                  << data_in_another_thread << ".";
          break;
        }
      }
    }

    void UnregisterData(uint64_t tid) {
      std::lock_guard<LockType> guard(lock_);
      AccumulateToAnotherThread(tid);
      tid_map_.erase(tid);
    }

    template <
        typename Alias = T,
        typename = std::enable_if_t<std::is_copy_constructible<Alias>::value>>
    std::unordered_map<uint64_t, T> GetAllThreadDataByValue() {
      std::unordered_map<uint64_t, T> data_copy;
      SharedLockGuardType guard(lock_);
      data_copy.reserve(tid_map_.size());
      for (auto& kv : tid_map_) {
        data_copy.emplace(kv.first, kv.second->GetData());
      }
      return data_copy;
    }

    std::unordered_map<uint64_t, std::reference_wrapper<T>>
    GetAllThreadDataByRef() {
      std::unordered_map<uint64_t, std::reference_wrapper<T>> data_ref;
      SharedLockGuardType guard(lock_);
      data_ref.reserve(tid_map_.size());
      for (auto& kv : tid_map_) {
        data_ref.emplace(kv.first, std::ref(kv.second->GetData()));
      }
      return data_ref;
    }

   private:
    LockType lock_;
    std::unordered_map<uint64_t, ThreadDataHolder*> tid_map_;
  };

  class ThreadDataHolder {
   public:
    explicit ThreadDataHolder(ThreadDataRegistryImpl* registry) {
      registry_ = registry;
      tid_ = std::hash<std::thread::id>()(std::this_thread::get_id());
      // Placement-new into aligned char storage — the compiler sees a POD
      // member, so no implicit destructor is generated for it.
      new (data_buf_) T();
      registry_->RegisterData(tid_, this);
    }

    ~ThreadDataHolder() {
#if defined(__MINGW64__) || defined(__MINGW32__)
      // MinGW TLS destructor ordering: the CRT heap is in teardown.
      // Both UnregisterData (erase→free) and T::~T() (e.g. string→free)
      // crash RtlFreeHeap. We skip ALL cleanup: the T object in data_buf_
      // is leaked (OS reclaims on exit), and the map entry stays (dead).
      (void)registry_;
      (void)tid_;
#else
      reinterpret_cast<T*>(data_buf_)->~T();
      registry_->UnregisterData(tid_);
#endif
    }

    T& GetData() { return *reinterpret_cast<T*>(data_buf_); }

   private:
    ThreadDataRegistryImpl* registry_;
    uint64_t tid_;
    // Aligned POD buffer — compiler generates NO destructor for char[].
    // The T object is constructed via placement-new and (on non-MinGW)
    // destroyed explicitly in ~ThreadDataHolder.
    alignas(T) char data_buf_[sizeof(T)];
  };

  ThreadDataRegistry() { impl_ = new ThreadDataRegistryImpl(); }

  ThreadDataRegistry(const ThreadDataRegistry&) = delete;
  ThreadDataRegistry& operator=(const ThreadDataRegistry&) = delete;

  T& CurrentThreadData() {
    static thread_local ThreadDataHolder thread_data(impl_);
    return thread_data.GetData();
  }

  ThreadDataRegistryImpl* impl_;
};

}  // namespace phi
