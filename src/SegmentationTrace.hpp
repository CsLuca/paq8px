#pragma once

#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <cstdint>
#include "BlockType.hpp"

// Research-only side channel. Never consumes input or updates coding state.
class SegmentationTrace {
  std::ofstream output;
  uint64_t sequence = 0;
  uint64_t nextDecision = 1;
  uint64_t nextStream = 1;
public:
  uint64_t stream = 0;
  uint64_t parent = 0;
  unsigned depth = 0;

  static std::string quote(const std::string& value) {
    std::string result = "\"";
    const char* hex = "0123456789abcdef";
    for (unsigned char c : value) {
      if (c == '"' || c == '\\') { result += '\\'; result += c; }
      else if (c < 32) {
        result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15];
      } else result += c;
    }
    return result + "\"";
  }

  explicit SegmentationTrace(const char* filename) {
    // Do not overwrite an existing artifact, especially an input/archive.
    const auto path = std::filesystem::u8path(filename);
    if (std::filesystem::exists(path)) throw std::runtime_error("Trace output already exists");
    output.open(path, std::ios::binary | std::ios::out);
    if (!output) throw std::runtime_error("Cannot create trace output");
  }

  void event(const char* kind, const std::string& fields = "") {
    output << "{\"schema_version\":1,\"seq\":" << sequence++
      << ",\"event\":" << quote(kind) << ",\"stream_id\":" << stream
      << ",\"parent_id\":" << parent << ",\"depth\":" << depth << fields << "}\n";
    if (!output) throw std::runtime_error("Cannot write trace output");
  }

  void begin(const char* filename, uint64_t size) {
    event("file_begin", ",\"input\":" + quote(filename) + ",\"size\":" + std::to_string(size)
      + ",\"upstream_commit\":\"c84f576fc2c522194cd320743708652a154daf6b\"");
  }
  void finish(uint64_t archiveSize) {
    event("file_end", ",\"archive_position_preflush\":" + std::to_string(archiveSize));
    output.flush();
    if (!output) throw std::runtime_error("Cannot flush trace output");
  }
  uint64_t decision(BlockType type, uint64_t start, uint64_t size, int info, const char* origin) {
    const uint64_t id = nextDecision++;
    event("decision", ",\"decision_id\":" + std::to_string(id) + range(start, size)
      + ",\"type\":" + std::to_string(static_cast<int>(type))
      + ",\"info\":" + std::to_string(info) + ",\"origin\":" + quote(origin));
    return id;
  }
  static std::string range(uint64_t start, uint64_t size) {
    if (size > UINT64_MAX - start) throw std::runtime_error("Trace interval overflow");
    return ",\"start\":" + std::to_string(start) + ",\"end\":" + std::to_string(start + size);
  }

  class DecisionScope {
    SegmentationTrace* trace;
    uint64_t savedParent;
    unsigned savedDepth;
  public:
    DecisionScope(SegmentationTrace* t, uint64_t id) : trace(t), savedParent(t ? t->parent : 0), savedDepth(t ? t->depth : 0) {
      if (trace) { trace->parent = id; trace->depth++; }
    }
    ~DecisionScope() { if (trace) { trace->parent = savedParent; trace->depth = savedDepth; } }
  };
  class StreamScope {
    SegmentationTrace* trace;
    uint64_t saved;
  public:
    StreamScope(SegmentationTrace* t, uint64_t size) : trace(t), saved(t ? t->stream : 0) {
      if (trace) { trace->stream = trace->nextStream++; trace->event("stream_begin", ",\"size\":" + std::to_string(size)); }
    }
    ~StreamScope() { if (trace) trace->stream = saved; }
  };
};
