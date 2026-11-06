// Copyright (c) 2026, i2kiselev
// BSD 3-Clause License: see LICENSE in the repository root.
#include <rex/ui/shader_startup.h>
#include <rex/ui/windowed_app_context.h>

#include <exception>
#include <utility>

namespace rex::ui {

void ShaderStartup::QueueLaunch(WindowedAppContext& context, std::weak_ptr<bool> alive,
                                const std::atomic<bool>& closing, std::function<void()> prepare) {
  context.CallInUIThreadDeferred([&context, alive, &closing, prepare = std::move(prepare)] {
    if (!alive.expired() && !context.HasQuitFromUIThread() &&
        !closing.load(std::memory_order_acquire))
      prepare();
  });
}

void ShaderStartup::CompleteLaunch(WindowedAppContext& context, const std::atomic<bool>& closing,
                                   std::function<void()> post_launch,
                                   std::function<void()> resume) {
  if (context.HasQuitFromUIThread() || closing.load(std::memory_order_acquire))
    return;
  post_launch();
  if (context.HasQuitFromUIThread() || closing.load(std::memory_order_acquire))
    return;
  resume();
}

ShaderStartup::ShaderStartup(Work work, std::function<void()> notify)
    : work_(std::move(work)), notify_(std::move(notify)) {
  initialization_->SetChangedCallback([this] { Notify(); });
}

ShaderStartup::~ShaderStartup() {
  initialization_->SetChangedCallback({});
  if (!done_.load(std::memory_order_acquire))
    initialization_->RequestCancel();
  if (worker_.joinable())
    worker_.join();
}

void ShaderStartup::Start() {
  try {
    worker_ = std::thread([this] {
      try {
        if (!initialization_->cancel_requested())
          work_(initialization_);
      } catch (const std::exception& error) {
        initialization_->Fail(error.what());
      } catch (...) {
        initialization_->Fail("Shader initialization failed.");
      }
      initialization_->Finish();
      done_.store(true, std::memory_order_release);
      Notify();
    });
  } catch (const std::exception& error) {
    initialization_->Fail(error.what());
    done_.store(true, std::memory_order_release);
    Notify();
  }
}

void ShaderStartup::RequestCancel() {
  initialization_->RequestCancel();
}

void ShaderStartup::Notify() {
  if (!notification_pending_.exchange(true, std::memory_order_acq_rel))
    notify_();
}

std::optional<graphics::ShaderStorageInitialization::Snapshot> ShaderStartup::TakeResult() {
  notification_pending_.store(false, std::memory_order_release);
  if (consumed_ || !done_.load(std::memory_order_acquire))
    return std::nullopt;
  if (worker_.joinable())
    worker_.join();
  consumed_ = true;
  return initialization_->GetSnapshot();
}

}  // namespace rex::ui
