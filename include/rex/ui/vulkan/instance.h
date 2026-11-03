#pragma once
/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2025 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <rex/platform.h>
#include <rex/platform/dynlib.h>
#include <rex/ui/renderdoc_api.h>
#include <rex/ui/vulkan/api.h>

namespace rex {
namespace ui {
namespace vulkan {

class VulkanInstance {
 public:
  static std::unique_ptr<VulkanInstance> Create(bool with_surface, bool try_enable_validation);

  VulkanInstance(const VulkanInstance&) = delete;
  VulkanInstance& operator=(const VulkanInstance&) = delete;
  VulkanInstance(VulkanInstance&&) = delete;
  VulkanInstance& operator=(VulkanInstance&&) = delete;

  ~VulkanInstance();

  // nullptr if RenderDoc is not connected.
  RenderDocAPI* renderdoc_api() const { return renderdoc_api_.get(); }

  struct Functions {
    // From the loader module.
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
    PFN_vkDestroyInstance vkDestroyInstance = nullptr;

    // From vkGetInstanceProcAddr for nullptr.
    PFN_vkCreateInstance vkCreateInstance = nullptr;
    PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties = nullptr;
    PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties = nullptr;
    // Vulkan 1.1.
    PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion = nullptr;

    // From vkGetInstanceProcAddr for the instance.
#define XE_UI_VULKAN_FUNCTION(name) PFN_##name name = nullptr;
#define XE_UI_VULKAN_FUNCTION_PROMOTED(extension_name, core_name) \
  PFN_##core_name core_name = nullptr;
#include <rex/ui/vulkan/functions/instance_1_0.inc>
    // VK_KHR_surface (#1)
#include <rex/ui/vulkan/functions/instance_khr_surface.inc>
    // VK_KHR_xcb_surface (#6)
#ifdef VK_USE_PLATFORM_XCB_KHR
#include <rex/ui/vulkan/functions/instance_khr_xcb_surface.inc>
#endif
    // VK_KHR_wayland_surface (#7)
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
#include <rex/ui/vulkan/functions/instance_khr_wayland_surface.inc>
#endif
    // VK_KHR_android_surface (#9)
#ifdef VK_USE_PLATFORM_ANDROID_KHR
#include <rex/ui/vulkan/functions/instance_khr_android_surface.inc>
#endif
    // VK_KHR_win32_surface (#10)
#ifdef VK_USE_PLATFORM_WIN32_KHR
#include <rex/ui/vulkan/functions/instance_khr_win32_surface.inc>
#endif
    // VK_EXT_metal_surface (#217)
#ifdef VK_USE_PLATFORM_METAL_EXT
#include <rex/ui/vulkan/functions/instance_ext_metal_surface.inc>
#endif
    // VK_KHR_get_physical_device_properties2 (#60, promoted to 1.1)
#include <rex/ui/vulkan/functions/instance_1_1_khr_get_physical_device_properties2.inc>
    // VK_EXT_debug_utils (#129)
#include <rex/ui/vulkan/functions/instance_ext_debug_utils.inc>
#undef XE_UI_VULKAN_FUNCTION_PROMOTED
#undef XE_UI_VULKAN_FUNCTION
  };

  const Functions& functions() const { return functions_; }

  uint32_t api_version() const { return api_version_; }

  // Also set to true if the version of the Vulkan API they were promoted to it
  // supported (with the `ext_major_minor_` prefix rather than `ext_`).
  struct Extensions {
    bool ext_KHR_surface = false;  // #1
#ifdef VK_USE_PLATFORM_XCB_KHR
    bool ext_KHR_xcb_surface = false;  // #6
#endif
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    bool ext_KHR_wayland_surface = false;  // #7
#endif
#ifdef VK_USE_PLATFORM_ANDROID_KHR
    bool ext_KHR_android_surface = false;  // #9
#endif
#ifdef VK_USE_PLATFORM_WIN32_KHR
    bool ext_KHR_win32_surface = false;  // #10
#endif
#ifdef VK_USE_PLATFORM_METAL_EXT
    bool ext_EXT_metal_surface = false;  // #217
#endif
    bool ext_1_1_KHR_get_physical_device_properties2 = false;  // #60
    bool ext_EXT_debug_utils = false;                          // #129
    bool ext_KHR_portability_enumeration = false;              // #395
    bool ext_EXT_swapchain_colorspace = false;                 // #105
  };

  const Extensions& extensions() const { return extensions_; }

  VkInstance instance() const { return instance_; }

  void EnumeratePhysicalDevices(std::vector<VkPhysicalDevice>& physical_devices_out) const;

  // With vulkan_device_address_bindings, GPU address ranges of objects as the
  // driver binds and unbinds them (VK_EXT_device_address_binding_report), to
  // name what a faulting address belonged to after a device loss.
  bool tracks_device_address_bindings() const {
    return address_binding_messenger_ != VK_NULL_HANDLE;
  }
  // One line per object bound over [address, address + size), then per object
  // unbound from it, the most recent first.
  std::vector<std::string> DescribeDeviceAddress(uint64_t address, uint64_t size) const;

 private:
  explicit VulkanInstance() = default;

  std::unique_ptr<RenderDocAPI> renderdoc_api_;

  rex::platform::DynamicLibrary loader_;

  Functions functions_;

  uint32_t api_version_ = VK_MAKE_API_VERSION(0, 1, 0, 0);

  Extensions extensions_;

  VkInstance instance_ = nullptr;

  static VkBool32 DebugUtilsMessengerCallback(
      VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
      VkDebugUtilsMessageTypeFlagsEXT message_types,
      const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data);

  VkDebugUtilsMessengerEXT debug_utils_messenger_ = VK_NULL_HANDLE;

  static VkBool32 AddressBindingMessengerCallback(
      VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
      VkDebugUtilsMessageTypeFlagsEXT message_types,
      const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data);

  struct AddressBinding {
    uint64_t base = 0;
    uint64_t size = 0;
    VkObjectType object_type = VK_OBJECT_TYPE_UNKNOWN;
    uint64_t object_handle = 0;
    bool internal = false;
    std::chrono::steady_clock::time_point bound;
    std::chrono::steady_clock::time_point unbound;
  };
  // Freed ranges kept for the report; older ones are dropped.
  static constexpr size_t kUnboundAddressBindings = 65536;

  VkDebugUtilsMessengerEXT address_binding_messenger_ = VK_NULL_HANDLE;
  mutable std::mutex address_bindings_mutex_;
  std::unordered_multimap<uint64_t, AddressBinding> bound_address_bindings_;
  std::deque<AddressBinding> unbound_address_bindings_;
};

}  // namespace vulkan
}  // namespace ui
}  // namespace rex
