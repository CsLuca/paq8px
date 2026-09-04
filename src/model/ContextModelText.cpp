#include "../MixerFactory.hpp"
#include "../Models.hpp"
#include "../CharacterNames.hpp"
#include "../filter/TextParserStateInfo.hpp"

class ContextModelText : public IContextModel {

private:
  Shared* const shared;
  Models* const models;
  Mixer* m;
  uint32_t stableTextBytes = 0;
  uint8_t utf8State = TextParserStateInfo::utf8Accept;
  static constexpr uint32_t WIKI_ENABLE_AFTER_STABLE_BYTES = 512;

public:
  ContextModelText(Shared* const sh, Models* const models, const MixerFactory* const mf) : shared(sh), models(models) {
    const bool useLSTM = shared->GetOptionUseLSTM();
    m = mf->createMixer (
      1 +  //bias
      MatchModel::MIXERINPUTS + NormalModel::MIXERINPUTS + SparseMatchModel::MIXERINPUTS +
      SparseModel::MIXERINPUTS_TEXT + SparseBitModel::MIXERINPUTS_TEXT + ChartModel::MIXERINPUTS_TEXT +
      RecordModel::MIXERINPUTS + CharGroupModel::MIXERINPUTS +
      TextModel::MIXERINPUTS + WordModel::MIXERINPUTS_TEXT + IndirectModel::MIXERINPUTS_TEXT +
      DmcForest::MIXERINPUTS + NestModel::MIXERINPUTS + WikiModel::MIXERINPUTS + XMLModel::MIXERINPUTS +
      (useLSTM ? LstmModelContainer::MIXERINPUTS : 0)
      ,
      MatchModel::MIXERCONTEXTS + NormalModel::MIXERCONTEXTS_PRE + NormalModel::MIXERCONTEXTS_POST + SparseMatchModel::MIXERCONTEXTS +
      SparseModel::MIXERCONTEXTS_TEXT + SparseBitModel::MIXERCONTEXTS + ChartModel::MIXERCONTEXTS +
      RecordModel::MIXERCONTEXTS + CharGroupModel::MIXERCONTEXTS +
      TextModel::MIXERCONTEXTS + WordModel::MIXERCONTEXTS + IndirectModel::MIXERCONTEXTS +
      DmcForest::MIXERCONTEXTS + NestModel::MIXERCONTEXTS + WikiModel::MIXERCONTEXTS + XMLModel::MIXERCONTEXTS +
      (useLSTM ? LstmModelContainer::MIXERCONTEXTS : 0)
      ,
      MatchModel::MIXERCONTEXTSETS + NormalModel::MIXERCONTEXTSETS_PRE + NormalModel::MIXERCONTEXTSETS_POST + SparseMatchModel::MIXERCONTEXTSETS +
      SparseModel::MIXERCONTEXTSETS_TEXT + SparseBitModel::MIXERCONTEXTSETS + ChartModel::MIXERCONTEXTSETS +
      RecordModel::MIXERCONTEXTSETS + CharGroupModel::MIXERCONTEXTSETS +
      TextModel::MIXERCONTEXTSETS + WordModel::MIXERCONTEXTSETS + IndirectModel::MIXERCONTEXTSETS +
      DmcForest::MIXERCONTEXTSETS + NestModel::MIXERCONTEXTSETS + WikiModel::MIXERCONTEXTSETS + XMLModel::MIXERCONTEXTSETS +
      (useLSTM ? LstmModelContainer::MIXERCONTEXTSETS : 0)
      ,
      (useLSTM ? 1 : 0)
    );
    m->setScaleFactor(940, 60);
  }


  int p() {

    INJECT_SHARED_bpos
    if( bpos == 0 ) {
      INJECT_SHARED_c1
      const uint8_t c = c1;
      const bool isAsciiText = c == TAB || c == NEW_LINE || c == CARRIAGE_RETURN || (c >= SPACE && c <= '~');
      if( isAsciiText && utf8State == TextParserStateInfo::utf8Accept ) {
        ++stableTextBytes;
      } else {
        const uint32_t t = TextParserStateInfo::utf8StateTable[c];
        const uint8_t nextUtf8State = TextParserStateInfo::utf8StateTable[256 + utf8State + t];
        const bool isUtf8Text = c >= 0x80 && nextUtf8State != TextParserStateInfo::utf8Reject;
        stableTextBytes = (isAsciiText || isUtf8Text) ? (stableTextBytes + 1) : 0;
        utf8State = nextUtf8State == TextParserStateInfo::utf8Reject ? TextParserStateInfo::utf8Accept : nextUtf8State;
      }
    }

    m->add(256); //network bias

    NormalModel& normalModel = models->normalModel();
    normalModel.mix(*m);
    normalModel.mixPost(*m);
    
    MatchModel& matchModel = models->matchModel();
    matchModel.mix(*m);

    const bool useLSTM = shared->GetOptionUseLSTM();
    if (useLSTM) {
      LstmModelContainer& lstmModel = models->lstmModelText();
      lstmModel.mix(*m);
    }

    INJECT_SHARED_blockType

    SparseMatchModel& sparseMatchModel = models->sparseMatchModel();
    sparseMatchModel.mix(*m);
    SparseBitModel& sparseBitModel = models->sparseBitModel();
    sparseBitModel.mix(*m);
    if (blockType != BlockType::DBF) {
      SparseModel& sparseModel = models->sparseModel();
      sparseModel.mix(*m);
      XMLModel& xmlModel = models->xmlModel();
      xmlModel.mix(*m);
      NestModel& nestModel = models->nestModel();
      nestModel.mix(*m);
      if( stableTextBytes >= WIKI_ENABLE_AFTER_STABLE_BYTES ) {
        WikiModel& wikiModel = models->wikiModel();
        wikiModel.mix(*m);
      } else {
        for( int i = 0; i < WikiModel::MIXERINPUTS; ++i ) {
          m->add(0);
        }
      }
    }
    ChartModel& chartModel = models->chartModel();
    chartModel.mix(*m);
    RecordModel& recordModel = models->recordModel();
    recordModel.mix(*m);
    CharGroupModel& charGroupModel = models->charGroupModel();
    charGroupModel.mix(*m);
    TextModel& textModel = models->textModel();
    textModel.mix(*m);
    WordModel& wordModel = models->wordModel();
    wordModel.mix(*m);
    IndirectModel& indirectModel = models->indirectModel();
    indirectModel.mix(*m);
    DmcForest& dmcForest = models->dmcForest();
    dmcForest.mix(*m);

    return m->p();
  }

  ~ContextModelText() {
    delete m;
  }

};
