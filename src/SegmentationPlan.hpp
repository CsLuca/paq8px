#pragma once

#include <fstream>
#include <filesystem>
#include <vector>
#include <array>
#include <stdexcept>
#include <cstdint>
#include <limits>
#include <string>
#include "BlockType.hpp"

// External research plan, never needed by the archive decoder.
// Input bytes are embedded and compared exactly, not bound by a weak checksum.
class SegmentationPlan {
  static uint64_t readLE(std::istream& in, unsigned count) {
    uint64_t value = 0;
    for (unsigned i = 0; i < count; ++i) {
      int byte = in.get();
      if (byte < 0) throw std::runtime_error("Truncated replay plan");
      value |= uint64_t(byte) << (8 * i);
    }
    return value;
  }
public:
  struct Entry { uint64_t start, length; BlockType type; int info; };
  std::vector<Entry> entries;

  SegmentationPlan(const char* path, const char* inputPath, uint64_t inputSize,
                   int level, int simd, int changeIndex, int changeType,
                   int boundaryIndex, uint64_t boundaryOffset,
                   int splitIndex = -1, uint64_t splitStart = 0, uint64_t splitEnd = 0) {
    std::ifstream plan(std::filesystem::u8path(path), std::ios::binary);
    std::ifstream input(std::filesystem::u8path(inputPath), std::ios::binary);
    if (!plan || !input) throw std::runtime_error("Cannot open replay plan/input");
    char magic[8]; plan.read(magic, 8);
    if (plan.gcount() != 8 || std::string(magic, 8) != "PXPLAN01") throw std::runtime_error("Invalid replay plan version");
    uint64_t size = readLE(plan, 8), count = readLE(plan, 4);
    if (size != inputSize || size > UINT32_MAX || count > size + 1) throw std::runtime_error("Invalid replay size/count");
    if (readLE(plan, 4) != uint64_t(level) || readLE(plan, 4) != uint64_t(simd)) throw std::runtime_error("Replay configuration mismatch");
    // Trace SHA-256 is provenance only; validation uses the exact input below.
    for (int i = 0; i < 32; ++i) readLE(plan, 1);
    for (uint64_t i = 0; i < size; ++i) {
      int expected = int(readLE(plan, 1));
      if (input.get() != expected) throw std::runtime_error("Replay input differs byte-for-byte");
    }
    if (input.get() != EOF) throw std::runtime_error("Replay input has trailing bytes");
    uint64_t position = 0;
    for (uint64_t i = 0; i < count; ++i) {
      Entry entry;
      entry.start = readLE(plan, 8); entry.length = readLE(plan, 8);
      auto type = readLE(plan, 4); auto info = uint32_t(readLE(plan, 4));
      entry.type = BlockType(type);
      entry.info = info <= INT32_MAX ? int(info) : int(int64_t(info) - (int64_t(1) << 32));
      if (type >= uint64_t(BlockType::Count) || entry.start != position || entry.length > size - position
          || (entry.length == 0 && size != 0)) throw std::runtime_error("Invalid replay interval/type");
      position += entry.length;
      entries.push_back(entry);
    }
    if (position != size || plan.get() != EOF) throw std::runtime_error("Replay coverage/trailing data invalid");
    if (int(changeIndex >= 0) + int(boundaryIndex >= 0) + int(splitIndex >= 0) > 1)
      throw std::runtime_error("Only one replay intervention permitted");
    if (changeIndex >= 0) {
      if (size_t(changeIndex) >= entries.size()) throw std::runtime_error("Invalid replay change index");
      auto& entry = entries[changeIndex];
      if (!((entry.type == BlockType::TEXT && changeType == int(BlockType::DEFAULT))
          || (entry.type == BlockType::DEFAULT && changeType == int(BlockType::TEXT))
          || (entry.type == BlockType::TEXT_EOL && changeType == int(BlockType::TEXT))))
        throw std::runtime_error("Unsupported replay label change (EOL removal is separate)");
      entry.type = BlockType(changeType);
    }
    if (boundaryIndex >= 0) {
      if (size_t(boundaryIndex) + 1 >= entries.size()) throw std::runtime_error("Invalid replay boundary index");
      auto& left = entries[boundaryIndex]; auto& right = entries[boundaryIndex + 1];
      auto plain = [](BlockType type) { return type == BlockType::TEXT || type == BlockType::DEFAULT; };
      uint64_t end = right.start + right.length;
      if (!plain(left.type) || !plain(right.type) || boundaryOffset <= left.start || boundaryOffset >= end
          || boundaryOffset == right.start) throw std::runtime_error("Unsupported replay boundary movement");
      left.length = boundaryOffset - left.start;
      right.start = boundaryOffset; right.length = end - boundaryOffset;
    }
    if (splitIndex >= 0) {
      if (size_t(splitIndex) >= entries.size()) throw std::runtime_error("Invalid replay split index");
      const Entry original = entries[splitIndex];
      const uint64_t end = original.start + original.length;
      if (original.type != BlockType::TEXT || !(original.start < splitStart && splitStart < splitEnd && splitEnd < end))
        throw std::runtime_error("Split requires one TEXT block and three nonempty intervals");
      const Entry left{original.start, splitStart - original.start, BlockType::TEXT, original.info};
      const Entry middle{splitStart, splitEnd - splitStart, BlockType::DEFAULT, original.info};
      const Entry right{splitEnd, end - splitEnd, BlockType::TEXT, original.info};
      entries[splitIndex] = left;
      entries.insert(entries.begin() + splitIndex + 1, {middle, right});
    }
  }
};
