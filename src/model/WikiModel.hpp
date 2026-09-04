#pragma once

#include "../ContextMap2.hpp"
#include "../Shared.hpp"
#include <cstdint>

class WikiModel {
private:
  static constexpr int nCM = 6;
  const Shared * const shared;
  ContextMap2 cm;
  uint32_t linkDepth = 0;
  uint32_t templateDepth = 0;
  uint32_t tableDepth = 0;
  uint32_t pipesInLine = 0;
  uint32_t headingMarks = 0;
  uint32_t entityType = 0;
  uint32_t listMode = 0;
  bool lineStart = true;

public:
  static constexpr int MIXERINPUTS = nCM * (ContextMap2::MIXERINPUTS + ContextMap2::MIXERINPUTS_RUN_STATS);
  static constexpr int MIXERCONTEXTS = 0;
  static constexpr int MIXERCONTEXTSETS = 0;

  explicit WikiModel(const Shared* const sh, uint64_t size);
  void mix(Mixer &m);
};
