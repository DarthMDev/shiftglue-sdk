/**
 * @file        tests/unit/ui/window_aspect_ratio_test.cpp
 *
 * @copyright   Copyright (c) 2026 i2kiselev
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#include <catch2/catch_test_macros.hpp>

#include <rex/ui/window.h>

using rex::ui::ParseAspectRatio;

TEST_CASE("an aspect ratio is width:height", "[window_aspect]") {
  CHECK(ParseAspectRatio("16:9") == 16.0f / 9.0f);
  CHECK(ParseAspectRatio("21:9") == 21.0f / 9.0f);
  CHECK(ParseAspectRatio("32:9") == 32.0f / 9.0f);
}

TEST_CASE("anything else leaves the window free", "[window_aspect]") {
  for (const char* value : {"", "free", "16", "16:", ":9", "0:9", "16:0", "-16:9", "16:9x", "1.7"}) {
    CHECK(ParseAspectRatio(value) == 0.0f);
  }
}
