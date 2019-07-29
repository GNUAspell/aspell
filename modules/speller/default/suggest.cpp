// Copyright 2000-2005 by Kevin Atkinson under the terms of the LGPL

// suggest.cpp Suggestion code for Aspell

// The magic behind my spell checker comes from merging Lawrence
// Philips excellent metaphone algorithm and Ispell's near miss
// strategy which is inserting a space or hyphen, interchanging two
// adjacent letters, changing one letter, deleting a letter, or adding
// a letter.
// 
// The process goes something like this.
// 
// 1.     Convert the misspelled word to its soundslike equivalent (its
//        metaphone for English words).
// 
// 2.     Find words that have the same soundslike pattern.
//
// 3.     Find words that have similar soundslike patterns. A similar
//        soundlike pattern is a pattern that is obtained by
//        interchanging two adjacent letters, changing one letter,
//        deleting a letter, or adding a letter.
//
// 4.     Score the result list and return the words with the lowest
//        score. The score is roughly the weighed average of the edit
//        distance of the word to the misspelled word, the soundslike
//        equivalent of the two words, and the phoneme of the two words.
//        The edit distance is the weighed total of the number of
//        deletions, insertions, exchanges, or adjacent swaps needed to
//        make one string equivalent to the other.
//
// Please note that the soundlike equivalent is a rough approximation
// of how the words sounds. It is not the phoneme of the word by any
// means.  For more information on the metaphone algorithm please see
// the file metaphone.cc which included a detailed description of it.
//
// NOTE: It is assumed that that strlen(soundslike) <= strlen(word)
//       for any possible word

// POSSIBLE OPTIMIZATION:
//   store the number of letters that are the same as the previous 
//     soundslike so that it can possible be skipped

#include <list>

#include "getdata.hpp"

#include "fstream.hpp"

#include "speller_impl.hpp"
#include "asuggest.hpp"
#include "clone_ptr-t.hpp"
#include "config.hpp"
#include "data.hpp"
#include "editdist.hpp"
#include "editdist2.hpp"
#include "errors.hpp"
#include "file_data_util.hpp"
#include "hash-t.hpp"
#include "language.hpp"
#include "leditdist.hpp"
#include "speller_impl.hpp"
#include "stack_ptr.hpp"
#include "suggest.hpp"
#include "vararray.hpp"
#include "string_list.hpp"

#include "gettext.h"

//#include "iostream.hpp"
//#define DEBUG_SUGGEST

using namespace aspeller;
using namespace acommon;
using namespace std;

namespace {

  typedef vector<String> NearMissesFinal;

  template <class Iterator>
  inline Iterator preview_next (Iterator i) {
    return ++i;
  }
  
  //
  // OriginalWord stores information about the original misspelled word
  //   for convince and speed.
  //
  struct OriginalWord {
    String   word;
    String   lower;
    String   clean;
    String   soundslike;
    CasePattern  case_pattern;
    OriginalWord() {}
  };

  //
  // struct ScoreWordSound - used for storing the possible words while
  //   they are being processed.
  //

  static const char * NO_SOUNDSLIKE = "";

  struct ScoreWordSound {
    char * word;
    char * word_clean;
    //unsigned word_size;
    const char * soundslike;
    int           score;
    int           adj_score;
    int           word_score;
    int           soundslike_score;
    bool          count;
    bool          split; // true the result of splitting a word
    bool          repl_table;
    WordEntry * repl_list;
    ScoreWordSound() : adj_score(LARGE_NUM), repl_list(0) {}
    ~ScoreWordSound() {delete repl_list;}
  };

  inline int compare (const ScoreWordSound &lhs, 
		      const ScoreWordSound &rhs) 
  {
    int temp = lhs.score - rhs.score;
    if (temp) return temp;
    return strcmp(lhs.word,rhs.word);
  }

  inline int adj_score_lt(const ScoreWordSound &lhs,
                          const ScoreWordSound &rhs)
  {
    int temp = lhs.adj_score - rhs.adj_score;
    if (temp) return temp < 0;
    return strcmp(lhs.word,rhs.word) < 0;
  }

  inline bool operator < (const ScoreWordSound & lhs, 
			  const ScoreWordSound & rhs) {
    return compare(lhs, rhs) < 0;
  }

  inline bool operator <= (const ScoreWordSound & lhs, 
			   const ScoreWordSound & rhs) {
    return compare(lhs, rhs) <= 0;
  }

  inline bool operator == (const ScoreWordSound & lhs, 
			   const ScoreWordSound & rhs) {
    return compare(lhs, rhs) == 0;
  }

  typedef std::list<ScoreWordSound> NearMisses;
 
  class Common {
  protected:
    const Language *     lang;
    OriginalWord         original;
    SpellerImpl    *     sp;

  public:
    Common(const Language *l, const String &w, SpellerImpl * sp)
      : lang(l), original(), sp(sp)
    {
      original.word = w;
      l->to_lower(original.lower, w.str());
      l->to_clean(original.clean, w.str());
      l->to_soundslike(original.soundslike, w.str());
      original.case_pattern = l->case_pattern(w);
    }

    void fix_case(char * str) {
      lang->LangImpl::fix_case(original.case_pattern, str, str);
    }
    const char * fix_case(const char * str, String & buf) {
      return lang->LangImpl::fix_case(original.case_pattern, str, buf);
    }
  };

  class Suggestions;
  
  class Working : private Common {
    const SuggestParms * parms;

    int min_score;
    int num_scored;
    bool have_typo;

    // setting have_one_edit_word indicates that we already considered
    // all words within one edit and they exists in scored_near_misses or
    // in near_misses wih the word_score defined
    bool have_one_edit_word;

    // have_soundslike_up_to indicates that we already considered
    // all soundslike within X edit and they exists in scored_near_misses or
    // in near_misses wih the soundslike_score defined
    int have_soundslike_up_to;
    
    int threshold;
    int adj_threshold;
    Threshold try_harder;

    EditDist (* edit_dist_fun)(const char *, const char *,
                               const EditDistanceWeights &);

    unsigned int max_word_length;

    NearMisses      scored_near_misses;
    NearMisses      near_misses;

    char * temp_end;

    ObjStack           buffer;
    ObjStack           temp_buffer;

    static const bool do_count = true;
    static const bool dont_count = false;

    CheckInfo check_info[8];

    void commit_temp(const char * b) {
      if (temp_end) {
        buffer.resize_temp(temp_end - b + 1);
        buffer.commit_temp();
        temp_end = 0; }}
    void abort_temp() {
      buffer.abort_temp();
      temp_end = 0;}
    const char * to_soundslike_temp(const char * w, unsigned s, unsigned * len = 0) {
      char * sl = (char *)buffer.alloc_temp(s + 1);
      temp_end = lang->LangImpl::to_soundslike(sl, w, s);
      if (len) *len = temp_end - sl;
      return sl;}
    const char * to_soundslike_temp(const WordEntry & sw) {
      char * sl = (char *)buffer.alloc_temp(sw.word_size + 1);
      temp_end = lang->LangImpl::to_soundslike(sl, sw.word, sw.word_size, sw.word_info);
      if (temp_end == 0) return sw.word;
      else return sl;}
    const char * to_soundslike(const char * w, unsigned s) {
      char * sl = (char *)buffer.alloc_temp(s + 1);
      temp_end = lang->LangImpl::to_soundslike(sl, w, s);
      commit_temp(sl);
      return sl;}

    struct ScoreInfo {
      const char *  soundslike;
      int           word_score;
      int           soundslike_score;
      bool          count;
      bool          split; // true the result of splitting a word
      bool          repl_table;
      WordEntry *   repl_list;
      ScoreInfo()
        : soundslike(), word_score(LARGE_NUM), soundslike_score(LARGE_NUM),
          count(true), split(false), repl_table(false), repl_list() {}
    };

    MutableString form_word(CheckInfo & ci);
    void try_word_n(ParmString str, const ScoreInfo & inf);
    bool check_word_s(ParmString word, CheckInfo * ci);
    unsigned check_word(char * word, char * word_end, CheckInfo * ci,
                        /* it WILL modify word */
                        unsigned pos = 1);
    void try_word_c(char * word, char * word_end, const ScoreInfo & inf);

    void try_word(char * word, char * word_end, const ScoreInfo & inf) {
      if (sp->unconditional_run_together_)
        try_word_c(word,word_end,inf);
      else
        try_word_n(word,inf);
    }
    void try_word(char * word, char * word_end, int score) {
      ScoreInfo inf;
      inf.word_score = score;
      try_word(word, word_end, inf);
    }

    void add_sound(SpellerImpl::WS::const_iterator i,
                   WordEntry * sw, const char * sl, int score = LARGE_NUM);
    void add_nearmiss(char * word, unsigned word_size, WordInfo word_info,
                      const ScoreInfo &);
    void add_nearmiss_w(SpellerImpl::WS::const_iterator, const WordEntry & w,
                        const ScoreInfo &);
    void add_nearmiss_a(SpellerImpl::WS::const_iterator, const WordAff * w,
                        const ScoreInfo &);
    bool have_score(int score) {return score < LARGE_NUM;}
    int needed_score(int want, int score, int weight) {
      // ((100 - weight)*??? + score*weight)/100 <= want
      // (100 - weight)*??? +  score*weight <= want*100
      // (100 - weight)*??? <= want*100 - score*weight
      // ??? <= (want*100 - score*weight)/(100-weight)
      return (want*100 - score*weight)/(100-weight);
    }
    int needed_level(int want, int soundslike_score) {
      // (word_weight*??? + soundlike_weight*soundslike_score)/100 <= want
      // word_weight*??? + soundlike_weight*soundslike_score <= want*100
      // word_weight*??? <= want*100 - soundlike_weight*soundslike_score
      // ??? <= (want*100 - soundlike_weight*soundslike_score) / word_weight
      // level = ceil(???/edit_distance_weights.min)
      int n = 100*want - parms->soundslike_weight*soundslike_score;
      if (n <= 0) return 0;
      int d = parms->word_weight*parms->edit_distance_weights.min;
      return (n-1)/d+1; // roundup
    }
    int weighted_average(int soundslike_score, int word_score) {
      return (parms->word_weight*word_score 
	      + parms->soundslike_weight*soundslike_score)/100;
    }
    void set_adj_wighted_average(NearMisses::iterator i, int one_edit_max) {
      int soundslike_weight = parms->soundslike_weight;
      int word_weight = parms->word_weight;
      if (i->word_score <= one_edit_max) {
        if (i->word_score < 100) {
          have_typo = true;
          const int factor = 8;
          soundslike_weight = (parms->soundslike_weight+factor-1)/factor;
        } else {
          const int factor = 2;
          soundslike_weight = (parms->soundslike_weight+factor-1)/factor;
        }
      }
      // NOTE: Theoretical if the soundslike is might be beneficial to
      // adjust the word score so it doesn't contribute as much.  If
      // the score is already around 100 (one edit dist) then it may
      // not be a good idea to lower it more, but if the word score is
      // 200 or more then it might make sence to reduce it some.
      // HOWEVER, this will likely not work well, espacally with small
      // words and there are just too many words with the same
      // soundlike.  In any case that what the special "soundslike"
      // and "bad-spellers" mode is for.
      i->adj_score = (word_weight*i->word_score
                      + soundslike_weight*i->soundslike_score)/100;
    }

    bool scan_for_more_soundslikes();

    void try_split();
    void try_one_edit_word();
    void try_scan();
    void try_scan_root();
    void try_repl();
    void try_ngram();

    void score_list();
    void score_nearmiss(NearMisses::iterator & i, int try_for, int limit);
    void set_threshold();
    void fine_tune_score(int thres);
    void transfer();
  public:
    Working(SpellerImpl * m, const Language *l,
	    const String & w, const SuggestParms * p)
      : Common(l,w,m), parms(p), min_score(LARGE_NUM), num_scored(0), have_typo(false), threshold(-1), adj_threshold(-1), max_word_length(0) {
      memset(check_info, 0, sizeof(check_info));
    }
    Suggestions * suggestions(); 
  };

  class Suggestions : private Common {
  public:
    Vector<ObjStack::Memory *> buf;
    NearMisses                 scored_near_misses;
    void transfer(NearMissesFinal &near_misses_final, int limit);
    Suggestions(const Common & other)
      : Common(other) {}
    ~Suggestions() {
      for (Vector<ObjStack::Memory *>::iterator i = buf.begin(), e = buf.end();
           i != e; ++i)
        ObjStack::dealloc(*i);
    }
    void merge(Suggestions & other) {
      buf.insert(buf.begin(), other.buf.begin(), other.buf.end());
      other.buf.clear();
      scored_near_misses.merge(other.scored_near_misses, adj_score_lt);
    }
  };

  Suggestions * Working::suggestions() {

    Suggestions * sug = new Suggestions(*this);
    have_one_edit_word = false;
    have_soundslike_up_to = -1;

    if (original.word.size() * parms->edit_distance_weights.max >= 0x8000)
      return sug; // to prevent overflow in the editdist functions

    try_split();

    if (parms->use_repl_table) {

#ifdef DEBUG_SUGGEST
      COUT.printl("TRYING REPLACEMENT TABLE");
#endif

      try_repl();
    }

    if (parms->try_one_edit_word) {
#ifdef DEBUG_SUGGEST
      COUT.printl("TRYING ONE EDIT WORD");
#endif

      try_one_edit_word();
      score_list();
      if (try_harder < parms->scan_threshold) goto done;
      // need to fine tune the score to account for special weights
      // applied to typos, otherwise some typos that produce very
      // different soundslike may be missed
      fine_tune_score(LARGE_NUM);
      set_threshold();
      have_one_edit_word = true;
    }

    if (parms->try_scan_0) {
#ifdef DEBUG_SUGGEST
      COUT.printl("TRYING SCAN 0");
#endif
      edit_dist_fun = limit0_edit_distance;
      
      if (sp->soundslike_root_only)
        try_scan_root();
      else
        try_scan();

      score_list();

      have_soundslike_up_to = 0;
    }

    if (!scan_for_more_soundslikes())
      goto done;

    if (parms->try_scan_1) {
#ifdef DEBUG_SUGGEST
      COUT.printl("TRYING SCAN 1");
#endif
      edit_dist_fun = limit1_edit_distance;

      if (sp->soundslike_root_only)
        try_scan_root();
      else
        try_scan();

      score_list();
      
      have_soundslike_up_to = 1;

      if (try_harder < parms->scan_2_threshold) goto done;
    }

    if (!scan_for_more_soundslikes())
      goto done;

    if (parms->try_scan_2) {
#ifdef DEBUG_SUGGEST
      COUT.printl("TRYING SCAN 2");
#endif

      edit_dist_fun = limit2_edit_distance;

      if (sp->soundslike_root_only)
        try_scan_root();
      else
        try_scan();

      score_list();
      
      have_soundslike_up_to = 2;

      if (try_harder < parms->ngram_threshold) goto done;
    }

    if (!scan_for_more_soundslikes())
      goto done;

    if (parms->try_ngram) {
#ifdef DEBUG_SUGGEST
      COUT.printl("TRYING NGRAM");
#endif

      try_ngram();

      score_list();
    }

  done:

    fine_tune_score(threshold);
    scored_near_misses.sort(adj_score_lt);
    sug->buf.push_back(buffer.freeze());
    sug->scored_near_misses.swap(scored_near_misses);
    return sug;
  }

    bool Working::scan_for_more_soundslikes() {
      if (have_one_edit_word && threshold != -1 && have_soundslike_up_to >= 0) {
        int level = have_soundslike_up_to + 1;
        int needed_word_score = needed_score(
          threshold, /* want */
          parms->edit_distance_weights.min*level, 
          parms->soundslike_weight);
        if (needed_word_score < parms->edit_distance_weights.min*2) {
          return false;
        }
      }
      return true;
    }
  
  // Forms a word by combining CheckInfo fields.
  // Will grow the grow the temp in the buffer.  The final
  // word must be null terminated and committed.
  // It returns a MutableString of what was appended to the buffer.
  MutableString Working::form_word(CheckInfo & ci) 
  {
    size_t slen = ci.word.size() - ci.pre_strip_len - ci.suf_strip_len;
    size_t wlen = slen + ci.pre_add_len + ci.suf_add_len;
    char * tmp = (char *)buffer.grow_temp(wlen);
    if (ci.pre_add_len) 
      memcpy(tmp, ci.pre_add, ci.pre_add_len);
    memcpy(tmp + ci.pre_add_len, ci.word.str() + ci.pre_strip_len, slen);
    if (ci.suf_add_len) 
      memcpy(tmp + ci.pre_add_len + slen, ci.suf_add, ci.suf_add_len);
    return MutableString(tmp,wlen);
  }

  void Working::try_word_n(ParmString str, const ScoreInfo & inf)
  {
    String word;
    String buf;
    WordEntry sw;
    for (SpellerImpl::WS::const_iterator i = sp->suggest_ws.begin();
         i != sp->suggest_ws.end();
         ++i)
    {
      (*i)->clean_lookup(str, sw);
      for (;!sw.at_end(); sw.adv())
        add_nearmiss_w(i, sw, inf);
    }
    if (sp->affix_compress) {
      CheckInfo ci; memset(static_cast<void *>(&ci), 0, sizeof(ci));
      bool res = lang->affix()->affix_check(LookupInfo(sp, LookupInfo::Clean), str, ci, 0);
      if (!res) return;
      form_word(ci);
      char * end = (char *)buffer.grow_temp(1);
      char * tmp = (char *)buffer.temp_ptr();
      buffer.commit_temp();
      *end = '\0';
      add_nearmiss(tmp, end - tmp, 0, inf);
    }
  }

  bool Working::check_word_s(ParmString word, CheckInfo * ci)
  {
    WordEntry sw;
    for (SpellerImpl::WS::const_iterator i = sp->suggest_ws.begin();
         i != sp->suggest_ws.end();
         ++i)
    {
      (*i)->clean_lookup(word, sw);
      if (!sw.at_end()) {
        ci->word = sw.word;
        return true;
      }
    }
    if (sp->affix_compress) {
      return lang->affix()->affix_check(LookupInfo(sp, LookupInfo::Clean), word, *ci, 0);
    }
    return false;
  }

  unsigned Working::check_word(char * word, char * word_end,  CheckInfo * ci,
                          /* it WILL modify word */
                          unsigned pos)
  {
    unsigned res = check_word_s(word, ci);
    if (res) return pos + 1;
    if (pos + 1 >= sp->run_together_limit_) return 0;
    for (char * i = word + sp->run_together_min_; 
         i <= word_end - sp->run_together_min_;
         ++i)
    {
      char t = *i;
      *i = '\0';
      res = check_word_s(word, ci);
      *i = t;
      if (!res) continue;
      res = check_word(i, word_end, ci + 1, pos + 1);
      if (res) return res;
    }
    memset(static_cast<void *>(ci), 0, sizeof(CheckInfo));
    return 0;
  }

  void Working::try_word_c(char * word, char * word_end, const ScoreInfo & inf)
  {
    unsigned res = check_word(word, word_end, check_info);
    assert(res <= sp->run_together_limit_);
    //CERR.printf(">%s\n", word);
    if (!res) return;
    buffer.abort_temp();
    MutableString tmp = form_word(check_info[0]);
    CasePattern cp = lang->case_pattern(tmp, tmp.size);
    for (unsigned i = 1; i <= res; ++i) {
      char * t = form_word(check_info[i]);
      if (cp == FirstUpper && lang->is_lower(t[1])) 
        t[0] = lang->to_lower(t[0]);
    }
    char * end = (char *)buffer.grow_temp(1);
    char * beg = (char *)buffer.temp_ptr(); // since the original string may of moved
    *end = 0;
    buffer.commit_temp();
    add_nearmiss(beg, end - beg, 0, inf);
    //CERR.printl(tmp);
    memset(check_info, 0, sizeof(CheckInfo)*res);
  }

  void Working::add_nearmiss(char * word, unsigned word_size,
                             WordInfo word_info,
                             const ScoreInfo & inf)
  {
    if (word_size * parms->edit_distance_weights.max >= 0x8000) 
      return; // to prevent overflow in the editdist functions

    near_misses.push_front(ScoreWordSound());
    ScoreWordSound & d = near_misses.front();
    d.word = word;
    d.soundslike = inf.soundslike;

    d.word_score = inf.word_score;
    d.soundslike_score = inf.soundslike_score;

    if (!sp->have_soundslike) {
      if (d.word_score >= LARGE_NUM) d.word_score = d.soundslike_score;
      else if (d.soundslike_score >= LARGE_NUM) d.soundslike_score = d.word_score;
    }

    unsigned int l = word_size;
    if (l > max_word_length) max_word_length = l;
    
    if (!(word_info & ALL_CLEAN)) {
      d.word_clean = (char *)buffer.alloc(word_size + 1);
      lang->LangImpl::to_clean((char *)d.word_clean, word);
    } else {
      d.word_clean = d.word;
    }

    if (!sp->have_soundslike && !d.soundslike)
      d.soundslike = d.word_clean;
    
    d.split = inf.split;
    d.repl_table = inf.repl_table;
    d.count = inf.count;
    d.repl_list = inf.repl_list;
  }

  void Working::add_nearmiss_w(SpellerImpl::WS::const_iterator i,
                               const WordEntry & w, const ScoreInfo & inf0)
  {
    assert(w.word_size == strlen(w.word));
    ScoreInfo inf = inf0;
    if (w.what == WordEntry::Misspelled) {
      inf.repl_list = new WordEntry;
      const ReplacementDict * repl_dict
        = static_cast<const ReplacementDict *>(*i);
      repl_dict->repl_lookup(w, *inf.repl_list);
    }
    add_nearmiss(buffer.dup(ParmString(w.word, w.word_size)), 
                 w.word_size, w.word_info, inf);
  }
  
  void Working::add_nearmiss_a(SpellerImpl::WS::const_iterator i,
                               const WordAff * w, const ScoreInfo & inf)
  {
    add_nearmiss(buffer.dup(w->word), w->word.size, 0, inf);
  }

  void Working::try_split() {
    const String & word = original.word;
    
    if (word.size() < 4 || parms->split_chars.empty()) return;
    size_t i = 0;
    
    String new_word_str;
    String buf;
    new_word_str.resize(word.size() + 1);
    char * new_word = new_word_str.data();
    memcpy(new_word, word.data(), word.size());
    new_word[word.size() + 1] = '\0';
    new_word[word.size() + 0] = new_word[word.size() - 1];
    
    for (i = word.size() - 2; i >= 2; --i) {
      new_word[i+1] = new_word[i];
      new_word[i] = '\0';
      
      if (sp->check(new_word) && sp->check(new_word + i + 1)) {
        for (size_t j = 0; j != parms->split_chars.size(); ++j)
        {
          new_word[i] = parms->split_chars[j];
          ScoreInfo inf;
          inf.word_score = parms->edit_distance_weights.max + 2;
          inf.soundslike_score = inf.word_score;
          inf.soundslike = NO_SOUNDSLIKE;
          inf.count = false;
          inf.split = true;
          add_nearmiss(buffer.dup(new_word), word.size() + 1, 0, inf);
        }
      }
    }
  }

  void Working::try_one_edit_word() 
  {
    const String & orig = original.clean;
    const char * replace_list = lang->clean_chars();
    char a,b;
    const char * c;
    VARARRAY(char, new_word, orig.size() + 2);
    char * new_word_end = new_word + orig.size();
    size_t i;

    memcpy(new_word, orig.str(), orig.size() + 1);

    // Try word as is (in case of case difference etc)

    try_word(new_word,  new_word_end, 0);

    // Change one letter
    
    for (i = 0; i != orig.size(); ++i) {
      for (c = replace_list; *c; ++c) {
        if (*c == orig[i]) continue;
        new_word[i] = *c;
        try_word(new_word, new_word_end, parms->edit_distance_weights.sub);
      }
      new_word[i] = orig[i];
    }
    
    // Interchange two adjacent letters.
    
    for (i = 0; i+1 < orig.size(); ++i) {
      a = new_word[i];
      b = new_word[i+1];
      new_word[i] = b;
      new_word[i+1] = a;
      try_word(new_word, new_word_end, parms->edit_distance_weights.swap);
      new_word[i] = a;
      new_word[i+1] = b;
    }

    // Add one letter

    *new_word_end = ' ';
    new_word_end++;
    *new_word_end = '\0';
    i = new_word_end - new_word - 1;
    while(true) {
      for (c=replace_list; *c; ++c) {
        new_word[i] = *c;
        try_word(new_word, new_word_end, parms->edit_distance_weights.del1);
      }
      if (i == 0) break;
      new_word[i] = new_word[i-1];
      --i;
    }
    
    // Delete one letter

    if (orig.size() > 1) {
      memcpy(new_word, orig.str(), orig.size() + 1);
      new_word_end = new_word + orig.size() - 1;
      a = *new_word_end;
      *new_word_end = '\0';
      i = orig.size() - 1;
      while (true) {
        try_word(new_word, new_word_end, parms->edit_distance_weights.del2);
        if (i == 0) break;
        b = a;
        a = new_word[i-1];
        new_word[i-1] = b;
        --i;
      }
    }
  }

  void Working::add_sound(SpellerImpl::WS::const_iterator i,
                          WordEntry * sw, const char * sl, int score)
  {
    WordEntry w;
    (*i)->soundslike_lookup(*sw, w);

    for (; !w.at_end(); w.adv()) {

      ScoreInfo inf;
      inf.soundslike = sl;
      inf.soundslike_score = score;
      add_nearmiss_w(i, w, inf);
      
      if (w.aff[0]) {
        String sl_buf;
        temp_buffer.reset();
        WordAff * exp_list;
        exp_list = lang->affix()->expand(w.word, w.aff, temp_buffer);
        for (WordAff * p = exp_list->next; p; p = p->next) {
          add_nearmiss_a(i, p, ScoreInfo());
        }
      }
      
    }
  }

  void Working::try_scan() 
  {
    const char * original_soundslike = original.soundslike.str();
    
    WordEntry * sw;
    WordEntry w;
    const char * sl = 0;
    EditDist score;
    unsigned int stopped_at = LARGE_NUM;
    WordAff * exp_list;
    WordAff single;
    single.next = 0;

    for (SpellerImpl::WS::const_iterator i = sp->suggest_ws.begin();
         i != sp->suggest_ws.end();
         ++i) 
    {
      //CERR.printf(">>%p %s\n", *i, typeid(**i).name());
      StackPtr<SoundslikeEnumeration> els((*i)->soundslike_elements());

      int have_soundslike_up_to_score = have_soundslike_up_to*parms->edit_distance_weights.max;

      while ( (sw = els->next(stopped_at)) ) {

        //CERR.printf("[%s (%d) %d]\n", sw->word, sw->word_size, sw->what);
        //assert(strlen(sw->word) == sw->word_size);
          
        if (sw->what != WordEntry::Word) {
          sl = sw->word;
          abort_temp();
        } else if (!*sw->aff) {
          sl = to_soundslike_temp(*sw);
        } else {
          goto affix_case;
        }

        //CERR.printf("SL = %s\n", sl);
        
        score = edit_dist_fun(sl, original_soundslike, parms->edit_distance_weights);
        stopped_at = score.stopped_at - sl;
        if (score >= LARGE_NUM) continue;
        if (score <= have_soundslike_up_to_score) continue;
        stopped_at = LARGE_NUM;
        commit_temp(sl);
        add_sound(i, sw, sl, score);
        continue;
        
      affix_case:
        
        temp_buffer.reset();
        
        // first expand any prefixes
        if (sp->fast_scan) { // if fast_scan than no prefixes
          single.word.str = sw->word;
          single.word.size = strlen(sw->word);
          single.aff = (const unsigned char *)sw->aff;
          exp_list = &single;
        } else {
          exp_list = lang->affix()->expand_prefix(sw->word, sw->aff, temp_buffer);
        }
        
        // iterate through each semi-expanded word, any affix flags
        // are now guaranteed to be suffixes
        for (WordAff * p = exp_list; p; p = p->next)
        {
          // try the root word
          unsigned sl_len;
          sl = to_soundslike_temp(p->word.str, p->word.size, &sl_len);
          score = edit_dist_fun(sl, original_soundslike, parms->edit_distance_weights);
          stopped_at = score.stopped_at - sl;
          stopped_at += p->word.size - sl_len;
          
          if (score < LARGE_NUM) {
            commit_temp(sl);
            ScoreInfo inf;
            inf.soundslike = sl;
            inf.soundslike_score = score;
            add_nearmiss_a(i, p, inf);
          }
          
          // expand any suffixes, using stopped_at as a hint to avoid
          // unneeded expansions.  Note stopped_at is the last character
          // looked at by limit_edit_dist.  Thus if the character
          // at stopped_at is changed it might effect the result
          // hence the "limit" is stopped_at + 1
          if (p->word.size - lang->affix()->max_strip() > stopped_at)
            exp_list = 0;
          else
            exp_list = lang->affix()->expand_suffix(p->word, p->aff, 
                                                    temp_buffer, 
                                                    stopped_at + 1);
          
          // reset stopped_at if necessary
          if (score < LARGE_NUM) stopped_at = LARGE_NUM;
          
          // iterate through fully expanded words, if any
          for (WordAff * q = exp_list; q; q = q->next) {
            sl = to_soundslike_temp(q->word.str, q->word.size);
            score = edit_dist_fun(sl, original_soundslike, parms->edit_distance_weights);
            if (score >= LARGE_NUM) continue;
            commit_temp(sl);
            ScoreInfo inf;
            inf.soundslike = sl;
            inf.soundslike_score = score;
            add_nearmiss_a(i, q, inf);
          }
        }
      }
    }
  }

  void Working::try_scan_root() 
  {

    WordEntry * sw;
    WordEntry w;
    const char * sl = 0;
    EditDist score;
    int stopped_at = LARGE_NUM;
    GuessInfo gi;
    lang->munch(original.word, &gi);
    Vector<const char *> sls;
    sls.push_back(original.soundslike.str());
#ifdef DEBUG_SUGGEST
    COUT.printf("will try soundslike: %s\n", sls.back());
#endif
    for (const aspeller::CheckInfo * ci = gi.head;
         ci; 
         ci = ci->next) 
    {
      sl = to_soundslike(ci->word.str(), ci->word.size());
      Vector<const char *>::iterator i = sls.begin();
      while (i != sls.end() && strcmp(*i, sl) != 0) ++i;
      if (i == sls.end()) {
        sls.push_back(to_soundslike(ci->word.str(), ci->word.size()));
#ifdef DEBUG_SUGGEST
        COUT.printf("will try root soundslike: %s\n", sls.back());
#endif
      }
    }
    const char * * begin = sls.pbegin();
    const char * * end   = sls.pend();
    for (SpellerImpl::WS::const_iterator i = sp->suggest_ws.begin();
         i != sp->suggest_ws.end();
         ++i) 
    {
      StackPtr<SoundslikeEnumeration> els((*i)->soundslike_elements());

      while ( (sw = els->next(stopped_at)) ) {
          
        if (sw->what != WordEntry::Word) {
          sl = sw->word;
          abort_temp();
        } else {
          sl = to_soundslike_temp(*sw);
        } 

        stopped_at = LARGE_NUM;
        for (const char * * s = begin; s != end; ++s) {
          score = edit_dist_fun(sl, *s, 
                                parms->edit_distance_weights);
          if (score.stopped_at - sl < stopped_at)
            stopped_at = score.stopped_at - sl;
          if (score >= LARGE_NUM) continue;
          stopped_at = LARGE_NUM;
          commit_temp(sl);
          add_sound(i, sw, sl, score);
          //CERR.printf("using %s: will add %s with score %d\n", *s, sl, (int)score);
          break;
        }
      }
    }
  }

  struct ReplTry 
  {
    const char * begin;
    const char * end;
    const char * repl;
    size_t repl_len;
    ReplTry(const char * b, const char * e, const char * r)
      : begin(b), end(e), repl(r), repl_len(strlen(r)) {}
  };

  void Working::try_repl() 
  {
    String buf;
    Vector<ReplTry> repl_try;
    StackPtr<SuggestReplEnumeration> els(lang->repl());
    const SuggestRepl * r = 0;
    const char * word = original.clean.str();
    const char * wend = word + original.clean.size();
    while (r = els->next(), r) 
    {
      const char * p = word;
      while ((p = strstr(p, r->substr))) {
        buf.clear();
        buf.append(word, p);
        buf.append(r->repl, strlen(r->repl));
        p += strlen(r->substr);
        buf.append(p, wend + 1);
        buf.ensure_null_end();
        //COUT.printf("%s (%s) => %s (%s)\n", word, r->substr, buf.str(), r->repl);
        ScoreInfo inf;
        inf.word_score = parms->edit_distance_weights.sub*3/2;
        inf.repl_table = true;
        try_word(buf.pbegin(), buf.pend(), inf);
      }
    }
  }

  // generate an n-gram score comparing s1 and s2
  static int ngram(int n, char * s1, int l1, const char * s2, int l2)
  {
    int nscore = 0;
    int ns;
    for (int j=1;j<=n;j++) {
      ns = 0;
      for (int i=0;i<=(l1-j);i++) {
        char c = *(s1 + i + j);
        *(s1 + i + j) = '\0';
        if (strstr(s2,(s1+i))) ns++;
        *(s1 + i + j ) = c;
      }
      nscore = nscore + ns;
      if (ns < 2) break;
    }
    ns = 0;
    ns = (l2-l1)-2;
    return (nscore - ((ns > 0) ? ns : 0));
  }

  struct NGramScore {
    SpellerImpl::WS::const_iterator i;
    WordEntry info;
    const char * soundslike;
    int score;
    NGramScore() {}
    NGramScore(SpellerImpl::WS::const_iterator i0,
               WordEntry info0, const char * sl, int score0) 
      : i(i0), info(info0), soundslike(sl), score(score0) {}
  };


  void Working::try_ngram()
  {
    String original_soundslike = original.soundslike;
    original_soundslike.ensure_null_end();
    WordEntry * sw = 0;
    const char * sl = 0;
    typedef Vector<NGramScore> Candidates;
    hash_set<const char *> already_have;
    Candidates candidates;
    int min_score = 0;
    int count = 0;

    for (NearMisses::iterator i = scored_near_misses.begin();
         i != scored_near_misses.end(); ++i)
    {
      if (!i->soundslike)
        i->soundslike = to_soundslike(i->word, strlen(i->word));
      already_have.insert(i->soundslike);
    }

    for (SpellerImpl::WS::const_iterator i = sp->suggest_ws.begin();
         i != sp->suggest_ws.end();
         ++i) 
    {
      StackPtr<SoundslikeEnumeration> els((*i)->soundslike_elements());
      
      while ( (sw = els->next(LARGE_NUM)) ) {

        if (sw->what != WordEntry::Word) {
          abort_temp();
          sl = sw->word;
        } else {
          sl = to_soundslike_temp(sw->word, sw->word_size);
        }
        
        if (already_have.have(sl)) continue;

        int ng = ngram(3, original_soundslike.data(), original_soundslike.size(),
                       sl, strlen(sl));

        if (ng > 0 && ng >= min_score) {
          commit_temp(sl);
          candidates.push_back(NGramScore(i, *sw, sl, ng));
          if (ng > min_score) count++;
          if (count >= parms->ngram_keep) {
            int orig_min = min_score;
            min_score = LARGE_NUM;
            Candidates::iterator i = candidates.begin();
            Candidates::iterator j = candidates.begin();
            for (; i != candidates.end(); ++i) {
              if (i->score == orig_min) continue;
              if (min_score > i->score) min_score = i->score;
              *j = *i;
              ++j;
            }
            count = 0;
            candidates.resize(j-candidates.begin());
            for (i = candidates.begin(); i != candidates.end(); ++i) {
              if (i->score != min_score) count++;
            }
          }
        }
      }
    }
    
    for (Candidates::iterator i = candidates.begin();
         i != candidates.end();
         ++i)
    {
      //COUT.printf("ngram: %s %d\n", i->soundslike, i->score);
      add_sound(i->i, &i->info, i->soundslike);
    }
  }
  
  void Working::score_list() {

#  ifdef DEBUG_SUGGEST
    COUT.printl("SCORING LIST");
#  endif

    NearMisses::iterator i;

    int try_for = (parms->word_weight*parms->edit_distance_weights.max)/100;
    while (threshold == -1 && !near_misses.empty()) {
      try_for += (parms->word_weight*parms->edit_distance_weights.max)/100;
#    ifdef DEBUG_SUGGEST
      COUT.printf("try_for = %d; size of scored/unscored %lu/%lu\n", try_for,
                  scored_near_misses.size(), near_misses.size());
#    endif

      i = near_misses.begin();
      while (i != near_misses.end()) {
        //CERR.printf("%s %s %s %d %d\n", i->word, i->word_clean, i->soundslike,
        //            i->word_score, i->soundslike_score);
        score_nearmiss(i, try_for, try_for);
      }

      set_threshold();
    }
      
#  ifdef DEBUG_SUGGEST
    COUT << "Threshold is: " << threshold << "\n";
    COUT << "try_for: " << try_for << "\n";
    COUT << "Size of scored: " << scored_near_misses.size() << "\n";
    COUT << "Size of ! scored: " << near_misses.size() << "\n";
#  endif

    i = near_misses.begin();
    while (i != near_misses.end()) {
      //CERR.printf("%s %s %s %d %d\n", i->word, i->word_clean, i->soundslike,
      //            i->word_score, i->soundslike_score);
      score_nearmiss(i, try_for, threshold);
    }

    set_threshold();

    if (num_scored < 3) {
      try_harder = T_PROBABLY;
    } else if (near_misses.empty()) {
      try_harder = T_MAYBE;
    } else {
      try_harder = T_UNLIKELY;
    }
    
#  ifdef DEBUG_SUGGEST
    COUT << "FINISHED SCORING\n";
    COUT << "Size of scored: " << scored_near_misses.size() << "\n";
    COUT << "Size of ! scored: " << near_misses.size() << "\n";
    COUT << "Try Harder: " << try_harder << "\n";
#  endif
  }

  void Working::score_nearmiss(NearMisses::iterator & i, int try_for, int limit) {
    if (i->word_score >= LARGE_NUM) {
      int sl_score = i->soundslike_score < LARGE_NUM ? i->soundslike_score : 0;
      if (try_for == limit) {
        int level = needed_level(try_for, sl_score);
        if (level > 0) {
          i->word_score = edit_distance(original.clean.c_str(),
                                        i->word_clean,
                                        level, level,
                                        parms->edit_distance_weights);
        }
      } else {
        int initial_level = needed_level(try_for, sl_score);
        int max_level = needed_level(limit, sl_score);
        if (initial_level < max_level)
          i->word_score = edit_distance(original.clean.c_str(),
                                        i->word_clean,
                                        initial_level+1,max_level,
                                        parms->edit_distance_weights);
      }

      if (have_one_edit_word && i->word_score <= parms->edit_distance_weights.max)
        goto del;
    }
    
    if (i->word_score >= LARGE_NUM) goto skip;
    
    if (i->soundslike_score >= LARGE_NUM) {
      if (weighted_average(0, i->word_score) > limit) goto skip;
      
      if (i->soundslike == 0) 
        i->soundslike = to_soundslike(i->word, strlen(i->word));
      
      i->soundslike_score = edit_distance(original.soundslike, i->soundslike,
                                          parms->edit_distance_weights);
    }
    
    i->score = weighted_average(i->soundslike_score, i->word_score);
    if (i->score > limit)
      goto skip;

    {
      //CERR.printf("using %s %s %d (%d %d)\n", i->word, i->soundslike, i->score, i->word_score, i->soundslike_score);
      if (i->count && i->score > parms->skip_score) {
        if (i->score < min_score) min_score = i->score;
        num_scored++;
      }
      NearMisses::iterator prev = i;
      ++i;
      scored_near_misses.splice(scored_near_misses.begin(), near_misses, prev);
    }
    return;
  skip:
    ++i;
    return;
    //CERR.printf("skipping %s\n", i->word);
  del:
    NearMisses::iterator prev = i;
    ++i;
    scored_near_misses.erase(prev);
  }

  void Working::set_threshold() {
    if (min_score == LARGE_NUM)
      return;
    if (parms->span_levels >= 0) {
      int level = -1;
      if (have_typo || min_score == 0) {
        level = 0;
      } else {
        level = 2*(min_score-1) / parms->edit_distance_weights.max;
      }
      level += parms->span_levels;
      if (level == 1)
        threshold = parms->edit_distance_weights.max + 2; // special case to include splitting of a word into two
      else
        threshold = ((level + 1) * parms->edit_distance_weights.max)/2;
      //CERR.printf("min-score/thres/level: %d %d %d\n", min_score, threshold, level);
    } else {
      threshold = min_score + parms->span;
    }
  }

  void Working::fine_tune_score(int thres) {
#  ifdef DEBUG_SUGGEST
    COUT.printf("FINE TUNE SCORE with thres=%d\n", thres);
#  endif

    NearMisses::iterator i;

    if (parms->use_typo_analysis) {
      adj_threshold = 0;
      unsigned int j;
      
      CharVector orig_norm, word;
      orig_norm.resize(original.word.size() + 1);
      for (j = 0; j != original.word.size(); ++j)
        orig_norm[j] = parms->ti->to_normalized(original.word[j]);
      orig_norm[j] = 0;
      ParmString orig(orig_norm.data(), j);
      word.resize(max_word_length + 1);
      
      for (i = scored_near_misses.begin();
           i != scored_near_misses.end();
           ++i)
      {
          if (i->score > thres)
            continue;
        if (i->split) {
          i->word_score = parms->ti->max + 2;
          i->soundslike_score = i->word_score;
          i->adj_score = i->word_score;
        } else if (i->adj_score >= LARGE_NUM) {
          for (j = 0; (i->word)[j] != 0; ++j)
            word[j] = parms->ti->to_normalized((i->word)[j]);
          word[j] = 0;
          int new_score = typo_edit_distance(ParmString(word.data(), j), orig, *parms->ti);
          // if a repl. table was used we don't want to increase the score
          if (!i->repl_table || new_score < i->word_score)
            i->word_score = new_score;
          set_adj_wighted_average(i, parms->ti->max);
        }
        if (i->adj_score > adj_threshold)
          adj_threshold = i->adj_score;
      }
    } else {
      for (i = scored_near_misses.begin();
           i != scored_near_misses.end();
           ++i)
      {
          if (i->score > thres)
            continue;
          set_adj_wighted_average(i, parms->edit_distance_weights.max);
        i->adj_score = i->score;
      }
      adj_threshold = threshold;
    }
    
#  ifdef DEBUG_SUGGEST
    COUT.printf("fine tune score adj_threshold is now %d\n",  adj_threshold);
#  endif
    
    for (i = scored_near_misses.begin();
         i != scored_near_misses.end();
         ++i)
    {
      if (i->adj_score > adj_threshold)
        i->adj_score = LARGE_NUM;
    }
  }

  void Suggestions::transfer(NearMissesFinal & near_misses_final, int limit) {
#  ifdef DEBUG_SUGGEST
    COUT << "\n" << "\n" 
	 << original.word << '\t' 
	 << original.soundslike << '\t'
	 << "\n";
    String sl;
#  endif
    int c = 1;
    hash_set<String,HashString<String> > duplicates_check;
    String buf;
    String final_word;
    pair<hash_set<String,HashString<String> >::iterator, bool> dup_pair;
    for (NearMisses::const_iterator i = scored_near_misses.begin();
	 i != scored_near_misses.end() && c <= limit
           && ( i->adj_score < LARGE_NUM || c <= 3 );
	 ++i) {
#    ifdef DEBUG_SUGGEST
      //COUT.printf("%p %p: ",  i->word, i->soundslike);
      COUT << i->word
           << '\t' << i->adj_score 
           << '\t' << i->score 
           << '\t' << i->word_score
           << '\t' << i->soundslike
           << '\t' << i->soundslike_score << "\n";
#    endif
      if (i->repl_list != 0) {
 	String::size_type pos;
	do {
 	  dup_pair = duplicates_check.insert(fix_case(i->repl_list->word, buf));
 	  if (dup_pair.second && 
 	      ((pos = dup_pair.first->find(' '), pos == String::npos)
 	       ? (bool)sp->check(*dup_pair.first)
 	       : (sp->check((String)dup_pair.first->substr(0,pos)) 
 		  && sp->check((String)dup_pair.first->substr(pos+1))) ))
          {
 	    near_misses_final.push_back(*dup_pair.first);
            ++c;
          }
 	} while (i->repl_list->adv());
      } else {
        fix_case(i->word);
	dup_pair = duplicates_check.insert(i->word);
	if (dup_pair.second ) {
	  near_misses_final.push_back(*dup_pair.first);
          ++c;
        }
      }
    }
  }
  
  class SuggestionListImpl : public SuggestionList {
    struct Parms {
      typedef const char *                    Value;
      typedef NearMissesFinal::const_iterator Iterator;
      Iterator end;
      Parms(Iterator e) : end(e) {}
      bool endf(Iterator e) const {return e == end;}
      Value end_state() const {return 0;}
      Value deref(Iterator i) const {return i->c_str();}
    };
  public:
    NearMissesFinal suggestions;

    SuggestionList * clone() const {return new SuggestionListImpl(*this);}
    void assign(const SuggestionList * other) {
      *this = *static_cast<const SuggestionListImpl *>(other);
    }

    bool empty() const { return suggestions.empty(); }
    Size size() const { return suggestions.size(); }
    VirEmul * elements() const {
      return new MakeEnumeration<Parms, StringEnumeration>
	(suggestions.begin(), Parms(suggestions.end()));
    }
  };

  class SuggestImpl : public Suggest {
    SpellerImpl * speller_;
    SuggestionListImpl  suggestion_list;
    SuggestParms parms_;
    SuggestParms parms_extra_pass_;
    bool dual_pass_mode;
  public:
    SuggestImpl(SpellerImpl * sp) : speller_(sp) {}
    PosibErr<void> setup(String mode = "");
    PosibErr<void> set_mode(ParmString mode) {
      return setup(mode);
    }
    SuggestionList & suggest(const char * word);
  };
  
  PosibErr<void> SuggestImpl::setup(String mode)
  {
    if (mode == "") 
      mode = speller_->config()->retrieve("sug-mode");

    if (mode == "bad-spellers") {
      dual_pass_mode = true;
      RET_ON_ERR(parms_.init("soundslike", speller_, speller_->config()));
      RET_ON_ERR(parms_extra_pass_.init("slow", speller_, speller_->config()));
    } else {
      dual_pass_mode = false;
      RET_ON_ERR(parms_.init(mode, speller_, speller_->config()));
    }

    return no_err;
  }

  SuggestionList & SuggestImpl::suggest(const char * word) { 
#   ifdef DEBUG_SUGGEST
    COUT << "=========== begin suggest " << word << " ===========\n";
#   endif
    suggestion_list.suggestions.resize(0);
    Working * sug = new Working(speller_, &speller_->lang(),word, &parms_);
    Suggestions * sugs = sug->suggestions();
    delete sug;
    if (dual_pass_mode) {
      sug = new Working(speller_, &speller_->lang(),word, &parms_extra_pass_);
      Suggestions * sugs2 = sug->suggestions();
      sugs->merge(*sugs2);
      delete sugs2;
    }
    sugs->transfer(suggestion_list.suggestions, parms_.limit);
    delete sugs;
#   ifdef DEBUG_SUGGEST
    COUT << "^^^^^^^^^^^  end suggest " << word << "  ^^^^^^^^^^^\n";
#   endif
    return suggestion_list;
  }
  
}

namespace aspeller {
  PosibErr<Suggest *> new_default_suggest(SpellerImpl * m) {
    StackPtr<SuggestImpl> s(new SuggestImpl(m));
    RET_ON_ERR(s->setup());
    return s.release();
  }

  PosibErr<void> SuggestParms::init(ParmString mode, SpellerImpl * sp) {

    edit_distance_weights.del1 =  95;
    edit_distance_weights.del2 =  95;
    edit_distance_weights.swap =  90;
    edit_distance_weights.sub =  100;
    edit_distance_weights.max = 100;
    edit_distance_weights.min =  90;

    soundslike_weight = 50;

    split_chars = " -";

    skip_score = 10;
    limit = 100;
    span_levels = 1;
    span = 50;
    ngram_keep = 10;
    use_typo_analysis = true;
    use_repl_table = sp->have_repl;
    try_one_edit_word = true; // always a good idea, even when
                              // soundslike lookup is used
    try_scan_0 = true; // very fast, get words that sounds alike
    try_scan_1 = false;
    try_scan_2 = false;
    try_ngram = false;
    scan_threshold = T_UNLIKELY;
    scan_2_threshold = T_MAYBE;
    ngram_threshold = sp->have_soundslike ? T_PROBABLY : T_MAYBE;

    if (mode == "ultra") {
    } else if (mode == "fast") {
      try_scan_1 = true;
    } else if (mode == "normal") {
      try_scan_1 = true;
      try_scan_2 = true;
    } else if (mode == "slow") {
      try_scan_1 = true;
      try_scan_2 = true;
      try_ngram = true;
      scan_2_threshold = T_UNLIKELY;
      limit = 200;
      span_levels = 2;
    } else if (mode == "soundslike") {
      try_scan_1 = true;
      try_scan_2 = true;
      scan_2_threshold = T_UNLIKELY;
      try_ngram = true;
      use_typo_analysis = false;
      soundslike_weight = 75;
      span_levels = -1;
      span = 100;
      //span = 125;
      limit = 200;
    } else {
      return make_err(bad_value, "sug-mode", mode, _("one of ultra, fast, normal, slow, bad-spellers or soundslike"));
    }

    if (!sp->have_soundslike) {
      // in this case try_scan_0/1 will not get better results than
      // try_one_edit_word
      if (try_scan_0 || try_scan_1) {
        // check after one edit
        scan_threshold = T_MAYBE;
        // but don't bother doing scan_0 or scan_1, but still do
        // scan_2 is it was set above
        try_scan_0 = false;
        try_scan_1 = false;
      }
    }

    word_weight = 100 - soundslike_weight;
    
    return no_err;
  }

  PosibErr<void> SuggestParms::init(ParmString mode, SpellerImpl * sp, Config * config) {
    RET_ON_ERR(init(mode,sp));

    if (config->have("sug-typo-analysis"))
      use_typo_analysis = config->retrieve_bool("sug-typo-analysis");
    if (config->have("sug-repl-table"))
      use_repl_table = config->retrieve_bool("sug-repl-table");
    
    StringList sl;
    config->retrieve_list("sug-split-char", &sl);
    StringListEnumeration els = sl.elements_obj();
    const char * s;
    split_chars.clear();
    while ((s = els.next()) != 0) {
      split_chars.push_back(*s);
    }

    if (use_typo_analysis) {
      String keyboard = config->retrieve("keyboard");
      RET_ON_ERR(aspeller::setup(ti, config, &sp->lang(), keyboard));
    }
    
    return no_err;
  }
  
}

