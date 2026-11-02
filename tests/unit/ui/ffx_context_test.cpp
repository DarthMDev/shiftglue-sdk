/**
 * @file        tests/unit/ui/ffx_context_test.cpp
 *
 * @copyright   Copyright (c) 2026 i2kiselev
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#include <catch2/catch_test_macros.hpp>

#include <rex/ui/upscaler.h>

using rex::ui::FfxContextServesOutput;

TEST_CASE("an FSR 2 context serves only the output it was made for", "[ffx_context]") {
  CHECK(FfxContextServesOutput(2, 3840, 2160, 3840, 2160));
  CHECK_FALSE(FfxContextServesOutput(2, 3840, 2160, 2400, 1350));
  CHECK_FALSE(FfxContextServesOutput(2, 2400, 1350, 3840, 2160));
}

TEST_CASE("an FSR 3 context serves every output up to its own", "[ffx_context]") {
  CHECK(FfxContextServesOutput(3, 3840, 2160, 3840, 2160));
  CHECK(FfxContextServesOutput(3, 3840, 2160, 2400, 1350));
  CHECK_FALSE(FfxContextServesOutput(3, 2400, 1350, 3840, 2160));
  CHECK_FALSE(FfxContextServesOutput(3, 2400, 1350, 2400, 1351));
}
