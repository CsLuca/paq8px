#include "WikiModel.hpp"
#include "../CharacterNames.hpp"

#include "WikiModelTune.hpp"

namespace {

constexpr uint32_t kMasks[] = {
    0x000000FFu,
    0x000001FFu,
    0x000003FFu,
    0x000007FFu,
    0x00000FFFu,
    0x00001FFFu,
    0x00003FFFu,
    0x00007FFFu,
    0x0000FFFFu,
    0x0000E0FFu,
};

constexpr uint8_t kFeatureLinkDepth = 0;
constexpr uint8_t kFeatureTemplateDepth = 1;
constexpr uint8_t kFeatureTableDepth = 2;
constexpr uint8_t kFeaturePipesInLine = 3;
constexpr uint8_t kFeatureHeadingMarks = 4;
constexpr uint8_t kFeatureEntityType = 5;
constexpr uint8_t kFeatureListMode = 6;
constexpr uint8_t kFeatureLineStart = 7;
constexpr uint8_t kFeatureCharLow8 = 8;
constexpr uint8_t kFeatureCharLow16 = 9;
constexpr uint8_t kFeatureTableActive = 10;
constexpr uint8_t kFeatureLinkTemplateMix = 11;

inline auto incrementSaturating(const uint32_t value, const uint32_t cap) -> uint32_t {
  return value < cap ? value + 1 : cap;
}

inline auto incrementWrapping(const uint32_t value, const uint32_t cap) -> uint32_t {
  if( cap == 0 ) {
    return 0;
  }
  return value >= cap ? 0 : value + 1;
}

inline auto featureValue(
    const uint8_t feature,
    const uint32_t linkDepth,
    const uint32_t templateDepth,
    const uint32_t tableDepth,
    const uint32_t pipesInLine,
    const uint32_t headingMarks,
    const uint32_t entityType,
    const uint32_t listMode,
    const bool lineStart,
    const uint32_t c4
) -> uint32_t {
  switch( feature ) {
    case kFeatureLinkDepth:
      return linkDepth;
    case kFeatureTemplateDepth:
      return templateDepth;
    case kFeatureTableDepth:
      return tableDepth;
    case kFeaturePipesInLine:
      return pipesInLine;
    case kFeatureHeadingMarks:
      return headingMarks;
    case kFeatureEntityType:
      return entityType;
    case kFeatureListMode:
      return listMode;
    case kFeatureLineStart:
      return static_cast<uint32_t>(lineStart);
    case kFeatureCharLow8:
      return c4 & 0xFFu;
    case kFeatureCharLow16:
      return c4 & 0xFFFFu;
    case kFeatureTableActive:
      return static_cast<uint32_t>(tableDepth > 0);
    case kFeatureLinkTemplateMix:
      return (linkDepth << 6) | (templateDepth << 1) | static_cast<uint32_t>(tableDepth > 0);
    default:
      return 0;
  }
}

}  // namespace

WikiModel::WikiModel(const Shared* const sh, const uint64_t size)
    : shared(sh), cm(sh, size, nCM, WikiModelTune::CONTEXT_MAP_ORDER) {}

void WikiModel::mix(Mixer &m) {
  INJECT_SHARED_bpos
  if( bpos == 0 ) {
    INJECT_SHARED_c1
    INJECT_SHARED_c4
    const uint8_t c = c1;
    const uint16_t c2 = static_cast<uint16_t>(c4 & 0xFFFF);

    if( c2 == 0x5B5B ) {
      linkDepth = incrementSaturating(linkDepth, WikiModelTune::LINK_DEPTH_CAP);
    } else if( c2 == 0x5D5D ) {
      linkDepth -= static_cast<uint32_t>(linkDepth > 0);
    }

    if( c2 == 0x7B7B ) {
      templateDepth = incrementSaturating(templateDepth, WikiModelTune::TEMPLATE_DEPTH_CAP);
    } else if( c2 == 0x7D7D ) {
      templateDepth -= static_cast<uint32_t>(templateDepth > 0);
    }

    if( c2 == 0x7B7C ) {
      tableDepth = incrementSaturating(tableDepth, WikiModelTune::TABLE_DEPTH_CAP);
    } else if( c2 == 0x7C7D ) {
      tableDepth -= static_cast<uint32_t>(tableDepth > 0);
    }

    if( c2 == 0x7C7C || c == '|' ) {
      pipesInLine = incrementWrapping(pipesInLine, WikiModelTune::PIPES_IN_LINE_CAP);
    }

    if( c == '=' ) {
      headingMarks = incrementWrapping(headingMarks, WikiModelTune::HEADING_MARKS_CAP);
    }

    if((c4 & 0xFFFFFF) == 0x26616D70 ) {
      entityType = 1;
    } else if((c4 & 0xFFFFFF) == 0x2671756F ) {
      entityType = 2;
    } else if((c4 & 0xFFFFFF) == 0x266C743B ) {
      entityType = 3;
    } else if((c4 & 0xFFFFFF) == 0x2667743B ) {
      entityType = 4;
    } else {
      entityType = 0;
    }

    if( lineStart ) {
      if( c == '*' ) {
        listMode = 1;
      } else if( c == '#' ) {
        listMode = 2;
      } else if( c == ';' ) {
        listMode = 3;
      } else if( c == ':' ) {
        listMode = 4;
      } else {
        listMode = 0;
      }
    }

    if( c == NEW_LINE ) {
      lineStart = true;
      pipesInLine = 0;
      headingMarks = 0;
      listMode = 0;
    } else if( c != CARRIAGE_RETURN && c != TAB && c != SPACE ) {
      lineStart = false;
    }

    uint64_t i = 0;
    for( int ci = 0; ci < nCM; ++ci ) {
      const uint8_t useRunStats = WikiModelTune::CONTEXT_GENES[ci][0];
      const uint8_t arity = WikiModelTune::CONTEXT_GENES[ci][1];
      const uint8_t f1 = WikiModelTune::CONTEXT_GENES[ci][2];
      const uint8_t f2 = WikiModelTune::CONTEXT_GENES[ci][3];
      const uint8_t f3 = WikiModelTune::CONTEXT_GENES[ci][4];
      const uint8_t f4 = WikiModelTune::CONTEXT_GENES[ci][5];
      const uint8_t maskIndex = WikiModelTune::CONTEXT_GENES[ci][6] % static_cast<uint8_t>(sizeof(kMasks) / sizeof(kMasks[0]));
      const uint8_t mode = useRunStats != 0 ? CM_USE_RUN_STATS : 0;
      const uint32_t v1 = featureValue(f1, linkDepth, templateDepth, tableDepth, pipesInLine, headingMarks, entityType, listMode, lineStart, c4);
      const uint32_t v2 = featureValue(f2, linkDepth, templateDepth, tableDepth, pipesInLine, headingMarks, entityType, listMode, lineStart, c4);
      const uint32_t v3 = featureValue(f3, linkDepth, templateDepth, tableDepth, pipesInLine, headingMarks, entityType, listMode, lineStart, c4);
      const uint32_t v4 = featureValue(f4, linkDepth, templateDepth, tableDepth, pipesInLine, headingMarks, entityType, listMode, lineStart, c4);
      const uint32_t masked = c4 & kMasks[maskIndex];
      if( arity <= 2 ) {
        cm.set(mode, hash(++i, v1, v2, masked));
      } else if( arity == 3 ) {
        cm.set(mode, hash(++i, v1, v2, v3, masked));
      } else {
        cm.set(mode, hash(++i, v1, v2, v3, v4, masked));
      }
    }
  }
  cm.mix(m);
}
