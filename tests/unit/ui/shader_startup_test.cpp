// Copyright (c) 2026, i2kiselev
// BSD 3-Clause License: see LICENSE in the repository root.
#include <catch2/catch_test_macros.hpp>

#include <rex/ui/shader_startup.h>
#include <rex/ui/windowed_app_context.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;
using rex::ui::ShaderStartup;
using Phase = rex::graphics::ShaderStorageInitialization::Phase;

namespace {
struct Notification {
  std::mutex mutex;
  std::condition_variable condition;
  bool pending = false;
  void Post() {
    std::lock_guard lock(mutex);
    pending = true;
    condition.notify_all();
  }
  bool Wait() {
    std::unique_lock lock(mutex);
    const bool received = condition.wait_for(lock, 2s, [&] { return pending; });
    pending = false;
    return received;
  }
};
class DeferredContext final : public rex::ui::WindowedAppContext {
 public:
  int quits = 0;

 private:
  void NotifyUILoopOfPendingFunctions() override {}
  void PlatformQuitFromUIThread() override { ++quits; }
};
auto Result(ShaderStartup& task, Notification& notification) {
  auto result = task.TakeResult();
  while (!result && notification.Wait())
    result = task.TakeResult();
  return result;
}
}  // namespace

TEST_CASE("Shader startup completion arrives without rendering and is consumed once",
          "[shader_startup]") {
  Notification notification;
  ShaderStartup task([](auto operation) { operation->SetTotal(0); }, [&] { notification.Post(); });
  task.Start();
  const auto result = Result(task, notification);
  REQUIRE(result);
  REQUIRE(result->phase == Phase::kSucceeded);
  REQUIRE_FALSE(task.TakeResult());
}

TEST_CASE("Shader startup errors cannot launch the guest", "[shader_startup]") {
  Notification notification;
  ShaderStartup task([](auto) { throw std::runtime_error("initialization failed"); },
                     [&] { notification.Post(); });
  task.Start();
  const auto result = Result(task, notification);
  REQUIRE(result);
  REQUIRE(result->phase == Phase::kFailed);
  REQUIRE(result->error == "initialization failed");
}

TEST_CASE("Shader startup close cannot be undone by a late successful worker", "[shader_startup]") {
  Notification notification;
  std::promise<void> release;
  auto gate = release.get_future().share();
  ShaderStartup task(
      [gate](auto operation) {
        gate.wait();
        operation->SetTotal(0);
      },
      [&] { notification.Post(); });
  SECTION("close before start") {
    task.RequestCancel();
    task.Start();
  }
  SECTION("close during work") {
    task.Start();
    task.RequestCancel();
  }
  release.set_value();
  const auto result = Result(task, notification);
  REQUIRE(result);
  REQUIRE(result->phase == Phase::kCancelled);
}

TEST_CASE("Shader startup close wins before completion is consumed", "[shader_startup]") {
  Notification notification;
  ShaderStartup task([](auto operation) { operation->SetTotal(0); }, [&] { notification.Post(); });
  task.Start();
  REQUIRE(notification.Wait());
  task.RequestCancel();
  const auto result = Result(task, notification);
  REQUIRE(result);
  REQUIRE(result->phase == Phase::kCancelled);
}

TEST_CASE("Shader startup destruction drains work before releasing its owner", "[shader_startup]") {
  std::promise<void> release;
  auto gate = release.get_future().share();
  std::promise<void> entered;
  auto owner = std::make_shared<int>(42);
  std::weak_ptr<int> lifetime = owner;
  auto task = std::make_unique<ShaderStartup>(
      [gate, owner, &entered](auto) {
        entered.set_value();
        gate.wait();
      },
      [] {});
  owner.reset();
  task->Start();
  entered.get_future().wait();
  auto destroy =
      std::async(std::launch::async, [task = std::move(task)]() mutable { task.reset(); });
  const bool waiting = destroy.wait_for(20ms) == std::future_status::timeout;
  const bool alive = !lifetime.expired();
  release.set_value();
  destroy.get();
  REQUIRE(waiting);
  REQUIRE(alive);
  REQUIRE(lifetime.expired());
}

TEST_CASE("Context quit drains an initial launch without preparing the guest",
          "[shader_startup][startup_quit]") {
  DeferredContext context;
  std::atomic<bool> closing{false};
  auto alive = std::make_shared<bool>(true);
  int prepared = 0;
  ShaderStartup::QueueLaunch(context, alive, closing, [&] { ++prepared; });
  context.QuitFromUIThread();
  REQUIRE(context.quits == 1);
  REQUIRE(prepared == 0);
}

TEST_CASE("Context quit drains queued shader success without launching the guest",
          "[shader_startup][startup_quit]") {
  DeferredContext context;
  Notification notification;
  ShaderStartup task([](auto operation) { operation->SetTotal(0); }, [&] { notification.Post(); });
  task.Start();
  const auto result = Result(task, notification);
  REQUIRE(result);
  REQUIRE(result->phase == Phase::kSucceeded);
  std::atomic<bool> closing{false};
  int post_launch = 0;
  int resumed = 0;
  context.CallInUIThreadDeferred([&] {
    ShaderStartup::CompleteLaunch(context, closing, [&] { ++post_launch; }, [&] { ++resumed; });
  });
  context.QuitFromUIThread();
  REQUIRE(context.quits == 1);
  REQUIRE(post_launch == 0);
  REQUIRE(resumed == 0);
}

TEST_CASE("Quit inside post-launch prevents guest resume", "[shader_startup][startup_quit]") {
  DeferredContext context;
  std::atomic<bool> closing{false};
  int post_launch = 0;
  int resumed = 0;
  ShaderStartup::CompleteLaunch(
      context, closing,
      [&] {
        ++post_launch;
        context.QuitFromUIThread();
      },
      [&] { ++resumed; });
  REQUIRE(post_launch == 1);
  REQUIRE(context.quits == 1);
  REQUIRE(resumed == 0);
}

TEST_CASE("Deferred launch cannot access a destroyed host", "[shader_startup][startup_quit]") {
  DeferredContext context;
  std::atomic<bool> closing{false};
  int prepared = 0;
  {
    auto alive = std::make_shared<bool>(true);
    ShaderStartup::QueueLaunch(context, alive, closing, [&] { ++prepared; });
  }
  context.ExecutePendingFunctionsFromUIThread();
  REQUIRE(prepared == 0);
}

TEST_CASE("Live startup hands off once and respects close intent",
          "[shader_startup][startup_quit]") {
  DeferredContext context;
  std::atomic<bool> closing{false};
  auto alive = std::make_shared<bool>(true);
  int post_launch = 0;
  int resumed = 0;
  ShaderStartup::QueueLaunch(context, alive, closing, [&] {
    ShaderStartup::CompleteLaunch(context, closing, [&] { ++post_launch; }, [&] { ++resumed; });
  });
  context.ExecutePendingFunctionsFromUIThread();
  REQUIRE(post_launch == 1);
  REQUIRE(resumed == 1);
  closing = true;
  ShaderStartup::QueueLaunch(context, alive, closing, [&] { ++resumed; });
  ShaderStartup::CompleteLaunch(context, closing, [&] { ++post_launch; }, [&] { ++resumed; });
  context.ExecutePendingFunctionsFromUIThread();
  REQUIRE(post_launch == 1);
  REQUIRE(resumed == 1);
}
