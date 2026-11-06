// Copyright (c) 2026, i2kiselev
// BSD 3-Clause License: see LICENSE in the repository root.
#pragma once

#include <chrono>
#include <functional>

#include <rex/graphics/shader_storage_initialization.h>

namespace rex::graphics {

void RunStartupPipelineCompilation(
    size_t count, size_t workers, ShaderStorageInitialization& initialization,
    const std::function<bool(size_t)>& compile, const std::function<void()>& save,
    std::chrono::milliseconds save_interval = std::chrono::seconds(10));

}  // namespace rex::graphics
