/**
 * @file        tests/unit/kernel/video_mode_test.cpp
 *
 * @copyright   Copyright (c) 2026 i2kiselev
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#include <catch2/catch_test_macros.hpp>

#include <rex/cvar.h>
#include <rex/kernel/xboxkrnl/video.h>
#include <rex/ui/flags.h>

TEST_CASE("the host window's size does not reach the guest video mode", "[video_mode]") {
  REXCVAR_SET(window_width, 853);
  REXCVAR_SET(window_height, 480);
  rex::system::X_VIDEO_MODE mode;
  rex::kernel::xboxkrnl::VdQueryVideoMode(&mode);
  REXCVAR_SET(window_width, 0);
  REXCVAR_SET(window_height, 0);
  CHECK(uint32_t(mode.display_width) == 1280);
  CHECK(uint32_t(mode.display_height) == 720);
  CHECK(uint32_t(mode.is_hi_def) == 1);
}
