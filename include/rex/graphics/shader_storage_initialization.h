// Copyright (c) 2026, i2kiselev
// BSD 3-Clause License: see LICENSE in the repository root.
#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <utility>

namespace rex::graphics {

class ShaderStorageInitialization {
 public:
  enum class Phase { kPreparing, kCompiling, kSaving, kSucceeded, kFailed, kCancelled };
  struct Snapshot {
    Phase phase = Phase::kPreparing;
    size_t completed = 0;
    size_t total = 0;
    size_t failed = 0;
    bool total_known = false;
    std::string error;
  };

  Snapshot GetSnapshot() const {
    std::lock_guard lock(mutex_);
    return snapshot_;
  }
  void SetTotal(size_t total) {
    Change([&] {
      snapshot_.total = total;
      snapshot_.total_known = true;
      if (!Stopped())
        snapshot_.phase = Phase::kCompiling;
    });
  }
  void CompletePipeline(bool success) {
    Change([&] {
      if (success)
        ++snapshot_.completed;
      else {
        ++snapshot_.failed;
        if (!cancel_requested_)
          snapshot_.phase = Phase::kFailed;
      }
    });
  }
  void SetSaving() {
    Change([&] {
      if (!Stopped())
        snapshot_.phase = Phase::kSaving;
    });
  }
  void Fail(std::string error) {
    Change([&] {
      if (snapshot_.error.empty())
        snapshot_.error = std::move(error);
      if (!cancel_requested_)
        snapshot_.phase = Phase::kFailed;
    });
  }
  void Finish() {
    Change([&] {
      if (cancel_requested_)
        snapshot_.phase = Phase::kCancelled;
      else if (snapshot_.phase != Phase::kFailed) {
        if (snapshot_.total_known && snapshot_.completed != snapshot_.total) {
          snapshot_.phase = Phase::kFailed;
          if (snapshot_.error.empty())
            snapshot_.error = "Shader initialization did not finish.";
        } else
          snapshot_.phase = Phase::kSucceeded;
      }
    });
  }
  void RequestCancel() {
    Change([&] {
      cancel_requested_ = true;
      snapshot_.phase = Phase::kCancelled;
    });
  }
  bool cancel_requested() const {
    std::lock_guard lock(mutex_);
    return cancel_requested_;
  }
  void SetChangedCallback(std::function<void()> callback) {
    std::lock_guard lock(mutex_);
    changed_ = std::move(callback);
  }

 private:
  bool Stopped() const { return cancel_requested_ || snapshot_.phase == Phase::kFailed; }
  template <typename F>
  void Change(F&& change) {
    std::function<void()> changed;
    {
      std::lock_guard lock(mutex_);
      change();
      changed = changed_;
    }
    if (changed)
      changed();
  }
  mutable std::mutex mutex_;
  Snapshot snapshot_;
  bool cancel_requested_ = false;
  std::function<void()> changed_;
};

}  // namespace rex::graphics
