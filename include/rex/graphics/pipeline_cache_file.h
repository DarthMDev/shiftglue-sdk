// Copyright (c) 2026, i2kiselev
// BSD 3-Clause License: see LICENSE in the repository root.
#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace rex::graphics {

bool WritePipelineCacheFile(const std::filesystem::path& destination, std::span<const uint8_t> data,
                            std::string* error);

}  // namespace rex::graphics
