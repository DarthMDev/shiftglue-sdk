// Copyright (c) 2026, i2kiselev
// BSD 3-Clause License: see LICENSE in the repository root.
#include <catch2/catch_test_macros.hpp>

#include <rex/graphics/pipeline_cache_file.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
struct CacheDirectory {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("rex-shader-cache-test-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  CacheDirectory() { std::filesystem::create_directory(path); }
  ~CacheDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};
std::vector<uint8_t> Read(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
}  // namespace

TEST_CASE("Pipeline cache replacement keeps the last complete snapshot", "[pipeline_cache_file]") {
  CacheDirectory directory;
  const auto path = directory.path / "cache.bin";
  const std::array<uint8_t, 3> first{1, 2, 3};
  const std::array<uint8_t, 4> second{8, 7, 6, 5};
  std::string error;
  REQUIRE(rex::graphics::WritePipelineCacheFile(path, first, &error));
  SECTION("previous interrupted temporary write") {
    std::ofstream(path.string() + ".tmp") << "incomplete";
  }
  REQUIRE(rex::graphics::WritePipelineCacheFile(path, second, &error));
  REQUIRE(Read(path) == std::vector<uint8_t>{8, 7, 6, 5});
}

TEST_CASE("Pipeline cache failed temporary open preserves old data", "[pipeline_cache_file]") {
  CacheDirectory directory;
  const auto path = directory.path / "cache.bin";
  const std::array<uint8_t, 3> data{1, 2, 3};
  REQUIRE(rex::graphics::WritePipelineCacheFile(path, data, nullptr));
  std::filesystem::create_directory(path.string() + ".tmp");
  std::string error;
  REQUIRE_FALSE(rex::graphics::WritePipelineCacheFile(path, std::array<uint8_t, 1>{9}, &error));
  REQUIRE_FALSE(error.empty());
  REQUIRE(Read(path) == std::vector<uint8_t>{1, 2, 3});
}

TEST_CASE("Pipeline cache failed replacement preserves destination", "[pipeline_cache_file]") {
  CacheDirectory directory;
  const auto path = directory.path / "cache.bin";
  std::filesystem::create_directory(path);
  std::ofstream(path / "keep") << "old";
  std::string error;
  REQUIRE_FALSE(rex::graphics::WritePipelineCacheFile(path, std::array<uint8_t, 1>{9}, &error));
  REQUIRE(std::filesystem::exists(path / "keep"));
  REQUIRE_FALSE(error.empty());
}
