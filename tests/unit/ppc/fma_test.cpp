// Copyright (c) 2026, i2kiselev
// BSD 3-Clause License: see LICENSE in the repository root.
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>

#include <rex/ppc/fma.h>

TEST_CASE("Exact single-input FMA preserves the double result", "[ppc][exact_fma]") {
  const int previous = std::fegetround();
  struct Restore { int mode; ~Restore() { std::fesetround(mode); } } restore{previous};
  double (*volatile reference)(double, double, double) = std::fma;
  const std::array<float, 12> values{0.0f, -0.0f, 1.0f, -1.0f,
      std::numeric_limits<float>::denorm_min(), -std::numeric_limits<float>::denorm_min(),
      std::numeric_limits<float>::min(), std::numeric_limits<float>::max(),
      -std::numeric_limits<float>::max(), 0x1.fffffep-1f, 0x1.000002p+0f, -0x1.000002p+0f};
  for (int mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
    REQUIRE(std::fesetround(mode) == 0);
    auto check = [&](double a, double b, double c) {
      const auto expected = std::bit_cast<uint64_t>(reference(a, b, c));
      CHECK(std::bit_cast<uint64_t>(rex::ppc::FmaFromSingles(a, b, c)) == expected);
    };
    for (float a : values) for (float b : values) for (float c : values) check(a, b, c);
    std::mt19937 random(201);
    for (int i = 0; i < 25000; ++i) {
      const float a = std::bit_cast<float>(uint32_t(random()));
      const float b = std::bit_cast<float>(uint32_t(random()));
      const float c = std::bit_cast<float>(uint32_t(random()));
      if (std::isfinite(a) && std::isfinite(b) && std::isfinite(c)) check(a, b, c);
    }
  }
}

TEST_CASE("Exact single-input FMA retains exceptional FMA behavior", "[ppc][exact_fma]") {
  double (*volatile reference)(double, double, double) = std::fma;
  const std::array<double, 7> values{0.0, -0.0, 1.0, -1.0,
      std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::quiet_NaN()};
  for (double a : values) for (double b : values) for (double c : values) {
    const double expected = reference(a, b, c);
    const double actual = rex::ppc::FmaFromSingles(a, b, c);
    if (std::isnan(expected)) CHECK(std::isnan(actual));
    else CHECK(std::bit_cast<uint64_t>(actual) == std::bit_cast<uint64_t>(expected));
  }
}

TEST_CASE("Runtime single proof accepts floats and rejects adjacent doubles", "[ppc][exact_fma]") {
  for (uint32_t exponent = 0; exponent < 255; ++exponent) {
    for (uint32_t mantissa : {0u, 1u, 0x3FFFFFu, 0x7FFFFFu}) {
      for (uint32_t sign : {0u, 0x80000000u}) {
        const double value = std::bit_cast<float>(sign | (exponent << 23) | mantissa);
        CHECK(rex::ppc::IsFiniteSingle(value));
        CHECK_FALSE(rex::ppc::IsFiniteSingle(std::nextafter(value, INFINITY)));
        CHECK_FALSE(rex::ppc::IsFiniteSingle(std::nextafter(value, -INFINITY)));
      }
    }
  }
  CHECK_FALSE(rex::ppc::IsFiniteSingle(INFINITY));
  CHECK_FALSE(rex::ppc::IsFiniteSingle(std::numeric_limits<double>::quiet_NaN()));
}

TEST_CASE("Guarded FMA preserves arbitrary doubles and exact float inputs", "[ppc][exact_fma]") {
  double (*volatile reference)(double, double, double) = std::fma;
  std::mt19937_64 random(201);
  for (int i = 0; i < 50000; ++i) {
    auto value = [&] {
      const uint64_t bits = random();
      return i & 1 ? double(std::bit_cast<float>(uint32_t(bits))) : std::bit_cast<double>(bits);
    };
    const double a = value(), b = value(), c = value();
    const double expected = reference(a, b, c);
    const double actual = rex::ppc::FmaWithSingleOperands(a, b, c);
    if (std::isnan(expected)) CHECK(std::isnan(actual));
    else CHECK(std::bit_cast<uint64_t>(actual) == std::bit_cast<uint64_t>(expected));
  }
}
