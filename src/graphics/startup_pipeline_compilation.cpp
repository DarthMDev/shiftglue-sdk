// Copyright (c) 2026, i2kiselev
// BSD 3-Clause License: see LICENSE in the repository root.
#include <rex/graphics/startup_pipeline_compilation.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <thread>
#include <vector>

namespace rex::graphics {

void RunStartupPipelineCompilation(size_t count, size_t workers,
                                   ShaderStorageInitialization& initialization,
                                   const std::function<bool(size_t)>& compile,
                                   const std::function<void()>& save,
                                   std::chrono::milliseconds save_interval) {
  initialization.SetTotal(count);
  std::atomic<size_t> next{0};
  std::mutex timer_mutex;
  std::condition_variable_any timer;
  const auto persist = [&] {
    try {
      save();
    } catch (const std::exception& error) {
      initialization.Fail(error.what());
    } catch (...) {
      initialization.Fail("Could not save the shader cache.");
    }
  };
  std::jthread saver;
  std::vector<std::jthread> threads;
  try {
    if (count && !initialization.cancel_requested()) {
      saver = std::jthread([&](std::stop_token stop) {
        std::unique_lock lock(timer_mutex);
        while (!stop.stop_requested()) {
          timer.wait_for(lock, stop, save_interval, [] { return false; });
          if (stop.stop_requested())
            break;
          lock.unlock();
          persist();
          lock.lock();
        }
      });
      const auto worker = [&] {
        while (!initialization.cancel_requested() &&
               initialization.GetSnapshot().phase != ShaderStorageInitialization::Phase::kFailed) {
          const size_t index = next.fetch_add(1, std::memory_order_relaxed);
          if (index >= count)
            return;
          bool success = false;
          try {
            success = compile(index);
          } catch (const std::exception& error) {
            initialization.Fail(error.what());
          } catch (...) {
            initialization.Fail("Shader pipeline creation failed.");
          }
          if (!success)
            initialization.Fail("Shader pipeline creation failed. See the game log.");
          initialization.CompletePipeline(success);
        }
      };
      const size_t thread_count = std::min(std::max(workers, size_t(1)), count);
      for (size_t i = 0; i != thread_count; ++i)
        threads.emplace_back(worker);
    }
  } catch (const std::exception& error) {
    initialization.Fail(error.what());
  }
  for (auto& thread : threads)
    thread.join();
  if (saver.joinable()) {
    saver.request_stop();
    timer.notify_all();
    saver.join();
  }
  initialization.SetSaving();
  persist();
}

}  // namespace rex::graphics
