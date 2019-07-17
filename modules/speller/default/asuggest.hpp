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

  struct SuggestParms {
    // implementation at the end of suggest.cc

    EditDistanceWeights     edit_distance_weights;
    CachePtr<const TypoEditDistanceInfo> ti;

    bool try_one_edit_word, try_scan_1, try_scan_2, try_ngram;

    int ngram_threshold, ngram_keep;

    bool check_after_one_edit_word;

    bool use_typo_analysis;
    bool use_repl_table;

    bool have_keyboard_def_file; 

    int soundslike_weight;
    int word_weight;

    int skip;
    int span;
    int limit;

    String split_chars;

    SuggestParms() {}
    
    PosibErr<void> set(ParmString mode, SpellerImpl * sp);
    PosibErr<void> fill_distance_lookup(const Config * c, const Language & l);
    
    virtual ~SuggestParms() {}
    virtual SuggestParms * clone() const;
  };
  
  Suggest * new_default_suggest(const Speller *, const SuggestParms &);
}

#endif
