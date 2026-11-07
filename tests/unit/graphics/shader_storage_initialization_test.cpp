// Copyright (c) 2026, i2kiselev
// BSD 3-Clause License: see LICENSE in the repository root.
#include <catch2/catch_test_macros.hpp>

#include <rex/graphics/shader_storage_initialization.h>

#include <thread>
#include <vector>

using rex::graphics::ShaderStorageInitialization;
using Phase = ShaderStorageInitialization::Phase;

TEST_CASE("Shader startup reports returned work and retains failure", "[shader_startup]") {
  ShaderStorageInitialization operation;
  REQUIRE_FALSE(operation.GetSnapshot().total_known);
  operation.SetTotal(3);
  REQUIRE(operation.GetSnapshot().completed == 0);
  operation.CompletePipeline(true);
  operation.CompletePipeline(false);
  operation.Fail("first driver error");
  operation.Fail("later error");
  operation.CompletePipeline(true);
  operation.Finish();
  const auto result = operation.GetSnapshot();
  REQUIRE(result.phase == Phase::kFailed);
  REQUIRE(result.completed == 2);
  REQUIRE(result.total == 3);
  REQUIRE(result.failed == 1);
  REQUIRE(result.error == "first driver error");
}

TEST_CASE("Shader startup cannot finish incomplete or cancelled work", "[shader_startup]") {
  ShaderStorageInitialization operation;
  operation.SetTotal(2);
  operation.CompletePipeline(true);
  operation.Finish();
  REQUIRE(operation.GetSnapshot().phase != Phase::kSucceeded);
  operation.RequestCancel();
  operation.CompletePipeline(true);
  operation.Finish();
  REQUIRE(operation.GetSnapshot().phase == Phase::kCancelled);
}

TEST_CASE("Shader startup zero work and disabled storage can finish", "[shader_startup]") {
  ShaderStorageInitialization operation;
  SECTION("known empty") {
    operation.SetTotal(0);
  }
  operation.Finish();
  REQUIRE(operation.GetSnapshot().phase == Phase::kSucceeded);
  REQUIRE(operation.GetSnapshot().completed == 0);
}

TEST_CASE("Shader startup snapshots stay coherent during worker completion", "[shader_startup]") {
  ShaderStorageInitialization operation;
  operation.SetTotal(400);
  std::vector<std::thread> workers;
  for (size_t i = 0; i != 4; ++i) {
    workers.emplace_back([&] {
      for (size_t j = 0; j != 100; ++j)
        operation.CompletePipeline(true);
    });
  }
  while (operation.GetSnapshot().completed != 400) {
    const auto snapshot = operation.GetSnapshot();
    REQUIRE(snapshot.completed <= snapshot.total);
    std::this_thread::yield();
  }
  for (auto& worker : workers)
    worker.join();
  operation.Finish();
  REQUIRE(operation.GetSnapshot().phase == Phase::kSucceeded);
}

TEST_CASE("Shader startup notification can read and cancel its operation", "[shader_startup]") {
  ShaderStorageInitialization operation;
  operation.SetChangedCallback([&] {
    if (operation.GetSnapshot().completed == 1 && !operation.cancel_requested()) {
      operation.RequestCancel();
    }
  });
  operation.SetTotal(2);
  operation.CompletePipeline(true);
  operation.Finish();
  REQUIRE(operation.GetSnapshot().phase == Phase::kCancelled);
}
