// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef _asuggest_hh_
#define _asuggest_hh_

#include "suggest.hpp"
#include "editdist.hpp"
#include "typo_editdist.hpp"
#include "cache.hpp"

namespace aspeller {
  class Speller;
  class SpellerImpl;
  class Suggest;

  enum Threshold {
    T_UNLIKELY = 0,
    T_MAYBE = 1,
    T_PROBABLY = 2,
  };
  
  struct SuggestParms {
    // implementation at the end of suggest.cc

    EditDistanceWeights     edit_distance_weights;
    CachePtr<const TypoEditDistanceInfo> ti;

    bool try_one_edit_word, try_scan_0, try_scan_1, try_scan_2, try_ngram;

    // continue if >=, setting to T_UNLIKELY means to always continue
    Threshold scan_threshold, scan_2_threshold, ngram_threshold;

    int ngram_keep;

    bool use_typo_analysis;
    bool use_repl_table;

    int soundslike_weight;
    int word_weight;

    int skip_score;
    int span_levels, span;
    int limit;

    String split_chars;

    SuggestParms() {}

    PosibErr<void> init(ParmString mode, SpellerImpl * sp);
    PosibErr<void> init(ParmString mode, SpellerImpl * sp, Config *);
  };
  
  Suggest * new_default_suggest(const Speller *, const SuggestParms &);
}

#endif
