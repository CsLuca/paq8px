#pragma once

#include "SegmentationTrace.hpp"

struct TransformOptions
{
  bool skipRgb = false;
  bool useBruteForceDeflateDetection = false;
  SegmentationTrace* trace = nullptr;
  TransformOptions(const Shared * const shared) {
    skipRgb = shared->GetOptionSkipRGB();
    useBruteForceDeflateDetection = shared->GetOptionBruteforceDeflateDetection();
  }
};
