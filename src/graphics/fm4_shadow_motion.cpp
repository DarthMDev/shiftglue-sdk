/**
 * @file        graphics/fm4_shadow_motion.cpp
 *
 * @copyright   Copyright (c) 2026 i2kiselev
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#include <rex/graphics/fm4_shadow_motion.h>

namespace rex::graphics {

bool Fm4OrthographicMatrix(const std::array<float, 16>& wvp) {
  return wvp[12] == 0.0f && wvp[13] == 0.0f && wvp[14] == 0.0f && wvp[15] == 1.0f;
}

bool Fm4CarShadowMapDraw(const std::array<float, 16>& wvp,
                         xenos::ColorRenderTargetFormat color_format) {
  return color_format == xenos::ColorRenderTargetFormat::k_16_16 && Fm4OrthographicMatrix(wvp);
}

uint32_t Fm4ShadowSlot(const std::unordered_set<uint32_t>& shadow_map_meshes,
                       const std::unordered_map<uint32_t, std::vector<uint32_t>>& scene_slots) {
  std::unordered_map<uint32_t, size_t> votes;
  uint32_t best = 0;
  for (const uint32_t mesh : shadow_map_meshes) {
    const auto scene = scene_slots.find(mesh);
    if (scene == scene_slots.end()) continue;
    for (const uint32_t slot : scene->second) {
      if (!slot) continue;
      if (++votes[slot] > (best ? votes[best] : 0)) best = slot;
    }
  }
  return best;
}

}  // namespace rex::graphics
