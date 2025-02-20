// Copyright (c) 2022 PaddlePaddle Authors. All Rights Reserved.
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
#include <thread>
#include <type_traits>
#include <unordered_map>

namespace paddle {
namespace framework {

template <typename T>
class ThreadDataRegistry {
 public:
  // Singleton
  static ThreadDataRegistry& GetInstance() {
    static ThreadDataRegistry instance;
    return instance;
  }

  T* GetMutableCurrentThreadData() { return &CurrentThreadData(); }

  const T& GetCurrentThreadData() { return CurrentThreadData(); }

  template <typename Alias = T,
            typename = std::enable_if_t<std::is_copy_assignable<Alias>::value>>
  void SetCurrentThreadData(const T& val) {
    CurrentThreadData() = val;
  }

  // Returns current snapshot of all threads. Make sure there is no thread
  // create/destory when using it.
  template <typename Alias = T, typename = std::enable_if_t<
                                    std::is_copy_constructible<Alias>::value>>
  std::unordered_map<uint64_t, T> GetAllThreadDataByValue() {
    return impl_->GetAllThreadDataByValue();
  }

  // Returns current snapshot of all threads. Make sure there is no thread
  // create/destory when using it.
  std::unordered_map<uint64_t, std::reference_wrapper<T>>
  GetAllThreadDataByRef() {
    return impl_->GetAllThreadDataByRef();
  }

 private:
  // types
  class ThreadDataHolder;
  class ThreadDataRegistryImpl {
   public:
    void RegisterData(uint64_t tid, ThreadDataHolder* tls_obj) {
      std::lock_guard<std::mutex> lock(lock_);
      tid_map_[tid] = tls_obj;
    }

    void UnregisterData(uint64_t tid) {
      std::lock_guard<std::mutex> lock(lock_);
      tid_map_.erase(tid);
    }

    template <typename Alias = T, typename = std::enable_if_t<
                                      std::is_copy_constructible<Alias>::value>>
    std::unordered_map<uint64_t, T> GetAllThreadDataByValue() {
      std::unordered_map<uint64_t, T> data_copy;
      std::lock_guard<std::mutex> lock(lock_);
      data_copy.reserve(tid_map_.size());
      for (auto& kv : tid_map_) {
        data_copy.emplace(kv.first, kv.second->GetData());
      }
      return data_copy;
    }

    std::unordered_map<uint64_t, std::reference_wrapper<T>>
    GetAllThreadDataByRef() {
      std::unordered_map<uint64_t, std::reference_wrapper<T>> data_ref;
      std::lock_guard<std::mutex> lock(lock_);
      data_ref.reserve(tid_map_.size());
      for (auto& kv : tid_map_) {
        data_ref.emplace(kv.first, std::ref(kv.second->GetData()));
      }
      return data_ref;
    }

   private:
    std::mutex lock_;
    std::unordered_map<uint64_t, ThreadDataHolder*> tid_map_;  // not owned
  };

  class ThreadDataHolder {
   public:
    explicit ThreadDataHolder(
        std::shared_ptr<ThreadDataRegistryImpl> registry) {
      registry_ = std::move(registry);
      tid_ = std::hash<std::thread::id>()(std::this_thread::get_id());
      registry_->RegisterData(tid_, this);
    }

    ~ThreadDataHolder() { registry_->UnregisterData(tid_); }

    T& GetData() { return data_; }

   private:
    std::shared_ptr<ThreadDataRegistryImpl> registry_;
    uint64_t tid_;
    T data_;
  };

  // methods
  ThreadDataRegistry() { impl_ = std::make_shared<ThreadDataRegistryImpl>(); }

  ThreadDataRegistry(const ThreadDataRegistry&) = delete;

  ThreadDataRegistry& operator=(const ThreadDataRegistry&) = delete;

  T& CurrentThreadData() {
    static thread_local ThreadDataHolder thread_data(impl_);
    return thread_data.GetData();
  }

  // data
  std::shared_ptr<ThreadDataRegistryImpl> impl_;
};

}  // namespace framework
}  // namespace paddle
