// Copyright (c) 2026, i2kiselev
// BSD 3-Clause License: see LICENSE in the repository root.
#pragma once

#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <rex/runtime_api.h>

namespace rex::ppc {

extern REX_RUNTIME_DATA std::atomic<bool> g_exact_single_fma;

inline bool IsFiniteSingle(double value) {
  const uint64_t magnitude = std::bit_cast<uint64_t>(value) & 0x7FFFFFFFFFFFFFFFull;
  if (!magnitude)
    return true;
  const uint32_t exponent = uint32_t(magnitude >> 52);
  if (exponent < 874 || exponent > 1150)
    return false;
  // Float subnormals require more trailing zero bits than normal 24-bit significands.
  const uint32_t discarded = exponent < 897 ? 926 - exponent : 29;
  return (magnitude & ((uint64_t{1} << discarded) - 1)) == 0;
}

inline double FmaFromSingles(double a, double b, double c) {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
  if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c))
    return std::fma(a, b, c);
  // Proven binary32 inputs have an exact binary64 product, including subnormals.
#if defined(__clang__)
  const double product = a * b;
#else
  const volatile double product = a * b;
#endif
  const double result = product + c;
  // Software CRT FMA differs on the sign of exact cancellation under downward rounding.
  return result == 0.0 ? std::fma(a, b, c) : result;
}

inline double FmaWithSingleOperands(double a, double b, double c) {
  if (IsFiniteSingle(a) && IsFiniteSingle(b) && IsFiniteSingle(c))
    return FmaFromSingles(a, b, c);
  return std::fma(a, b, c);
}

}  // namespace rex::ppc
