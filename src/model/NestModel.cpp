#include "NestModel.hpp"

NestModel::NestModel(const Shared* const sh, const uint64_t size) : shared(sh), cm(sh, size, nCM, 64) {}

void NestModel::mix(Mixer &m) {
  INJECT_SHARED_bpos
  if( bpos == 0 ) {
    INJECT_SHARED_c1
    INJECT_SHARED_blockType
    int c = c1;
    int matched = 1;
    int vv = 0;
    w *= static_cast<int>((vc & 7) > 0 && (vc & 7) < 3);
    if((c & 0x80) != 0 ) {
      w = w * 11 * 32 + c;
    }
    const int lc = (c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c);
    if( lc == 'a' || lc == 'e' || lc == 'i' || lc == 'o' || lc == 'u' ) {
      vv = 1;
      w = w * 997 * 8 + (lc / 4 - 22);
    } else if( lc >= 'a' && lc <= 'z' ) {
      vv = 2;
      w = w * 271 * 32 + lc - 97;
    } else if( lc == ' ' || lc == '.' || lc == ',' || lc == '\n' ) {
      vv = 3;
    } else if( lc >= '0' && lc <= '9' ) {
      vv = 4;
    } else if( lc == 'y' ) {
      vv = 5;
    } else if( lc == '\'' ) {
      vv = 6;
    } else {
      vv = (c & 32) != 0 ? 7 : 0;
    }
    vc = (vc << 3) | vv;
    if( vv != lvc ) {
      wc = (wc << 3) | vv;
      lvc = vv;
    }
    INJECT_SHARED_c4

    int wikiSignal = 0;
    const uint32_t c2c1 = c4 & 0xFFFF;
    if (c2c1 == 0x5B5B) { // [[
      wikiSignal = 4;
      wikiTok = 1;
      wikiLinkDepth = min(63, wikiLinkDepth + 1);
    }
    else if (c2c1 == 0x5D5D) { // ]]
      wikiSignal = 4;
      wikiTok = 2;
      wikiLinkDepth -= static_cast<int>(wikiLinkDepth > 0);
    }
    else if (c2c1 == 0x7B7B) { // {{
      wikiSignal = 5;
      wikiTok = 3;
      wikiTplDepth = min(63, wikiTplDepth + 1);
    }
    else if (c2c1 == 0x7D7D) { // }}
      wikiSignal = 5;
      wikiTok = 4;
      wikiTplDepth -= static_cast<int>(wikiTplDepth > 0);
    }
    else if (c == '|') {
      wikiSignal = 3;
      wikiTok = 5;
    }
    else if (c2c1 == 0x3D3D) { // ==
      wikiSignal = 3;
      wikiTok = 6;
    }
    else if (c4 == 0x266C743B) { // &lt;
      wikiSignal = 2;
      wikiTok = 7;
    }
    else if (c4 == 0x2667743B) { // &gt;
      wikiSignal = 2;
      wikiTok = 8;
    }
    else if (c4 == 0x3C726566 || c4 == 0x2F726566) { // <ref /ref
      wikiSignal = 4;
      wikiTok = 9;
    }
    else {
      wikiTok = (wikiTok * 3 + vv) & 0x0F;
    }

    wikiScore += wikiSignal * 6;
    wikiScore -= static_cast<int>(wikiSignal == 0 && wikiScore > 0);
    wikiScore = wikiScore < 0 ? 0 : wikiScore;
    wikiScore = min(255, wikiScore);
    wikiMode = static_cast<int>(wikiScore >= 24);

    switch( c ) {
      case ' ':
        qc = 0;
        break;
      case '(':
        ic += 31;
        break;
      case ')':
        ic -= 31;
        break;
      case '[':
        ic += 11;
        break;
      case ']':
        ic -= 11;
        break;
      case '<':
        ic += 23;
        qc += 34;
        break;
      case '>':
        ic -= 23;
        qc /= 5;
        break;
      case ':':
        pc = 20;
        break;
      case '{':
        ic += 17;
        break;
      case '}':
        ic -= 17;
        break;
      case '|':
        pc += 223;
        break;
      case '"':
        pc += 0x40;
        break;
      case '\'':
        pc += 0x42;
        if( c != static_cast<uint8_t>(c4 >> 8)) {
          sense2 ^= 1;
        } else {
          ac += (2 * sense2 - 1);
        }
        break;
      case '\n':
        pc = qc = 0;
        break;
      case '.':
      case '!':
      case '?':
        pc = 0;
        break;
      case '#':
        pc += 0x08;
        break;
      case '%':
        pc += 0x76;
        break;
      case '$':
        pc += 0x45;
        break;
      case '*':
        pc += 0x35;
        break;
      case '-':
        pc += 0x3;
        break;
      case '@':
        pc += 0x72;
        break;
      case '&':
        qc += 0x12;
        break;
      case ';':
        qc /= 3;
        break;
      case '\\':
        pc += 0x29;
        break;
      case '/':
        pc += 0x11;
        if( c1 == '<' ) {
          qc += 74;
        }
        break;
      case '=':
        pc += 87;
        if( c != static_cast<uint8_t>(c4 >> 8)) {
          sense1 ^= 1;
        } else {
          ec += (2 * sense1 - 1);
        }
        break;
      default:
        matched = 0;
    }
    if( c4 == 0x266C743B ) {
      uc = min(7, uc + 1); //&lt;
    } else if( c4 == 0x2667743B ) {
      uc -= static_cast<int>(uc > 0); //&gt; 
    }
    if( matched != 0 ) {
      bc = 0;
    } else {
      bc += 1;
    }
    if( bc > 300 ) {
      bc = ic = pc = qc = uc = 0;
    }
    const uint8_t R_ = CM_USE_RUN_STATS;
    uint64_t i = 0;
    cm.set(R_, hash(++i, (vv > 0 && vv < 3) ? 0 : (lc | 0x100), ic & 0x03FF, ec & 0x07, ac & 0x07, uc));
    cm.set(R_, hash(++i, ic, w, ilog2(bc + 1)));
    cm.set(R_, hash(++i, (3 * vc + 77 * pc + 373 * ic + qc) & 0xffff));
    cm.set(R_, hash(++i, (31 * vc + 27 * pc + 281 * qc) & 0xffff));
    cm.set(R_, hash(++i, (13 * vc + 271 * ic + qc + bc) & 0xffff));
    cm.set(R_, hash(++i, (17 * pc + 7 * ic) & 0xffff));
    cm.set(R_, hash(++i, (13 * vc + ic) & 0xffff));
    cm.set(R_, hash(++i, (vc / 3 + pc) & 0xffff));
    cm.set(R_, hash(++i, (7 * wc + qc) & 0xffff));
    cm.set(R_, hash(++i, vc & 0xffff, c4 & 0xff));
    cm.set(R_, hash(++i, (3 * pc) & 0xffff, c4 & 0xff));
    cm.set(R_, hash(++i, ic & 0xffff, c4 & 0xff));

    if (wikiMode != 0 && isTEXT(blockType)) {
      cm.set(R_, hash(++i, wikiTok, vc & 0xffff, pc & 0x03ff, ic & 0x03ff));
      cm.set(R_, hash(++i, wikiTok, qc & 0x03ff, uc & 0x0f, ilog2(bc + 1)));
      cm.set(R_, hash(++i, wikiTok, c4 & 0xffff, (c4 >> 16) & 0xff));
      cm.set(R_, hash(++i, wikiTok, wc & 0xffff, (c1 | 0x100), wikiLinkDepth & 0x3f, wikiTplDepth & 0x3f));     
    }
    else {
      cm.skip(R_);
      cm.skip(R_);
      cm.skip(R_);
      cm.skip(R_);
    }
  }
  cm.mix(m);
}
