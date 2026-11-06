// Copyright (c) 2026, i2kiselev
// BSD 3-Clause License: see LICENSE in the repository root.
#include <concepts>
#include <filesystem>
#include <memory>

#include <rex/graphics/null/graphics_system.h>
#include <rex/graphics/vulkan/graphics_system.h>
#include <rex/system/interfaces/graphics.h>

#if REX_PLATFORM_WIN32
#include <rex/graphics/d3d12/command_processor.h>
#include <rex/graphics/d3d12/graphics_system.h>
#endif

namespace {
template <typename T>
concept SupportsShaderStorageInitialization =
    requires(T& receiver, const std::filesystem::path& cache,
             std::shared_ptr<rex::graphics::ShaderStorageInitialization> operation) {
      { receiver.InitializeShaderStorage(cache, uint32_t(1), true) } -> std::same_as<void>;
      {
        receiver.InitializeShaderStorage(cache, uint32_t(1), true, operation)
      } -> std::same_as<void>;
    };

static_assert(SupportsShaderStorageInitialization<rex::system::IGraphicsSystem>);
static_assert(SupportsShaderStorageInitialization<rex::graphics::null::NullGraphicsSystem>);
static_assert(SupportsShaderStorageInitialization<rex::graphics::null::NullCommandProcessor>);
static_assert(SupportsShaderStorageInitialization<rex::graphics::vulkan::VulkanGraphicsSystem>);
#if REX_PLATFORM_WIN32
static_assert(SupportsShaderStorageInitialization<rex::graphics::d3d12::D3D12GraphicsSystem>);
static_assert(SupportsShaderStorageInitialization<rex::graphics::d3d12::D3D12CommandProcessor>);
#endif
}  // namespace
