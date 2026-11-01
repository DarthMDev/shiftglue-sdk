/**
 * @file        rex/graphics/fm4_shadow_motion.h
 *
 * @copyright   Copyright (c) 2026 i2kiselev
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#pragma once
// The motion a temporal upscaler gets for FM4's car ground shadow. The track draws darken the
// ground under the car from the grey map, downsampled from the car's own shadow map, which the
// game draws again from the car every frame. The shadow therefore moves with the car, while the
// asphalt it falls on moves with the world. Given the asphalt's motion, a shadow pixel's history
// came from where the asphalt was a frame earlier; in reverse look that is the lit ground behind
// the shadow's edge, which DLSS kept as thin light lines across the shadow.
// The shadow map's own frame is no stand-in for the car's: it follows the car's position but
// keeps the sun's orientation, so in a turn it gave the shadow under the bumper 1-9 pixels of
// motion the car did not have, and DLSS ghosted the exhaust pipes into it.

#include <array>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <rex/graphics/xenos.h>

namespace rex::graphics {

// Whether a world-view-projection (row-major for column vectors, Fm4ObjectMatrix) is
// orthographic: its w row is (0, 0, 0, 1). The car's shadow map pass draws the car so; the scene
// and the reflection cube draw it in perspective.
bool Fm4OrthographicMatrix(const std::array<float, 16>& wvp);

// Whether a draw is the player car's shadow map pass: orthographic, into the variance map
// (16_16). Other cars' shadow maps are orthographic too but 8_8_8_8; their meshes, drawn once
// there and once in the scene, outvoted the player car's in races with AI.
bool Fm4CarShadowMapDraw(const std::array<float, 16>& wvp,
                         xenos::ColorRenderTargetFormat color_format);

// The object slot the ground shadow moves with: of the scene draws of the vertex buffers the
// shadow map pass draws, the slot most share (the body's); 0 without one. A vertex buffer holds
// several of a car's parts, so counting only those drawn once left a part with a slot of its own,
// and the ground followed it instead of the body. Slot 0 (the camera's motion, meshes new this
// frame) does not vote.
uint32_t Fm4ShadowSlot(const std::unordered_set<uint32_t>& shadow_map_meshes,
                       const std::unordered_map<uint32_t, std::vector<uint32_t>>& scene_slots);

}  // namespace rex::graphics
