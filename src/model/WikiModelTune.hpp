#pragma once

#include <cstdint>

namespace WikiModelTune {

constexpr uint32_t LINK_DEPTH_CAP = 31;
constexpr uint32_t TEMPLATE_DEPTH_CAP = 31;
constexpr uint32_t TABLE_DEPTH_CAP = 15;
constexpr uint32_t PIPES_IN_LINE_CAP = 63;
constexpr uint32_t HEADING_MARKS_CAP = 63;
constexpr uint32_t CONTEXT_MAP_ORDER = 64;

// Gene columns:
// [0]=useRunStats(0/1), [1]=arity(2/3/4), [2..5]=feature ids, [6]=mask index.
constexpr uint8_t CONTEXT_GENES[6][7] = {
    {1, 4, 11, 12, 9, 8, 9},
    {1, 3, 2, 3, 8, 0, 0},
    {1, 3, 4, 7, 8, 0, 0},
    {1, 3, 5, 12, 8, 0, 8},
    {1, 3, 6, 1, 8, 0, 0},
    {1, 4, 0, 3, 4, 8, 9},
};

}  // namespace WikiModelTune
