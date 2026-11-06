// Copyright (c) 2026, i2kiselev
// BSD 3-Clause License: see LICENSE in the repository root.
#include <rex/graphics/pipeline_cache_file.h>

#include <cerrno>
#include <cstdio>
#include <system_error>

#include <rex/filesystem.h>

namespace rex::graphics {

bool WritePipelineCacheFile(const std::filesystem::path& destination, std::span<const uint8_t> data,
                            std::string* error) {
  auto temporary = destination;
  temporary += ".tmp";
  FILE* file = rex::filesystem::OpenFile(temporary, "wb");
  if (!file) {
    if (error)
      *error = std::error_code(errno, std::generic_category()).message();
    return false;
  }
  bool written = fwrite(data.data(), 1, data.size(), file) == data.size();
  written = fflush(file) == 0 && written;
  written = fclose(file) == 0 && written;
  std::error_code failure;
  if (written)
    std::filesystem::rename(temporary, destination, failure);
  if (!written || failure) {
    if (error)
      *error = failure ? failure.message() : "Could not write the complete shader cache.";
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    return false;
  }
  return true;
}

}  // namespace rex::graphics
