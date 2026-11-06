// Copyright (c) 2026, i2kiselev
// BSD 3-Clause License: see LICENSE in the repository root.
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <thread>

#include <rex/graphics/shader_storage_initialization.h>

namespace rex::ui {

class WindowedAppContext;

class ShaderStartup {
 public:
  using Work = std::function<void(std::shared_ptr<graphics::ShaderStorageInitialization>)>;
  ShaderStartup(Work work, std::function<void()> notify);
  ~ShaderStartup();
  void Start();
  void RequestCancel();
  std::shared_ptr<graphics::ShaderStorageInitialization> initialization() const {
    return initialization_;
  }
  std::optional<graphics::ShaderStorageInitialization::Snapshot> TakeResult();
  static void QueueLaunch(WindowedAppContext& context, std::weak_ptr<bool> alive,
                          const std::atomic<bool>& closing, std::function<void()> prepare);
  static void CompleteLaunch(WindowedAppContext& context, const std::atomic<bool>& closing,
                             std::function<void()> post_launch, std::function<void()> resume);

 private:
  void Notify();
  Work work_;
  std::function<void()> notify_;
  std::shared_ptr<graphics::ShaderStorageInitialization> initialization_ =
      std::make_shared<graphics::ShaderStorageInitialization>();
  std::thread worker_;
  std::atomic<bool> done_{false};
  std::atomic<bool> notification_pending_{false};
  bool consumed_ = false;
};

}  // namespace rex::ui
