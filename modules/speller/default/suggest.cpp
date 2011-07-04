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

#include "getdata.hpp"

#include "fstream.hpp"

#include "speller_impl.hpp"
#include "asuggest.hpp"
#include "basic_list.hpp"
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
  // OriginalWord stores infomation about the original misspelled word
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

  struct ScoreWordSound {
    char * word;
    char * word_clean;
    //unsigned word_size;
    const char * soundslike;
    int           score;
    int           word_score;
    int           soundslike_score;
    bool          count;
    WordEntry * repl_list;
    ScoreWordSound() {repl_list = 0;}
    ~ScoreWordSound() {delete repl_list;}
  };

  inline int compare (const ScoreWordSound &lhs, 
		      const ScoreWordSound &rhs) 
  {
    int temp = lhs.score - rhs.score;
    if (temp) return temp;
    return strcmp(lhs.word,rhs.word);
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

  typedef BasicList<ScoreWordSound> NearMisses;
 
  class Score {
  protected:
    const Language * lang;
    OriginalWord     original;
    const SuggestParms * parms;

  public:
    Score(const Language *l, const String &w, const SuggestParms * p)
      : lang(l), original(), parms(p)
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

  class Working : public Score {
   
    int threshold;
    int try_harder;

    EditDist (* edit_dist_fun)(const char *, const char *,
                               const EditDistanceWeights &);

    unsigned int max_word_length;

    SpellerImpl  *     sp;
    NearMisses         scored_near_misses;
    NearMisses         near_misses;
    NearMissesFinal  * near_misses_final;

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

    MutableString form_word(CheckInfo & ci);
    void try_word_n(ParmString str, int score);
    bool check_word_s(ParmString word, CheckInfo * ci);
    unsigned check_word(char * word, char * word_end, CheckInfo * ci,
                        /* it WILL modify word */
                        unsigned pos = 1);
    void try_word_c(char * word, char * word_end, int score);

    void try_word(char * word, char * word_end, int score) {
      if (sp->unconditional_run_together_)
        try_word_c(word,word_end,score);
      else
        try_word_n(word,score);
    }

    void add_sound(SpellerImpl::WS::const_iterator i,
                   WordEntry * sw, const char * sl, int score = -1);
    void add_nearmiss(char * word, unsigned word_size, WordInfo word_info,
                      const char * sl,
                      int w_score, int sl_score,
                      bool count = do_count, WordEntry * rl = 0);
    void add_nearmiss(SpellerImpl::WS::const_iterator, const WordEntry & w, 
                      const char * sl,
                      int w_score, int sl_score, bool count = do_count);
    void add_nearmiss(SpellerImpl::WS::const_iterator, const WordAff * w,
                      const char * sl, 
                      int w_score, int sl_score, bool count = do_count);
    bool have_score(int score) {return score < LARGE_NUM;}
    int needed_level(int want, int soundslike_score) {
      int n = (100*want - parms->soundslike_weight*soundslike_score)
	/(parms->word_weight*parms->edit_distance_weights.min);
      return n > 0 ? n : 0;
    }
    int weighted_average(int soundslike_score, int word_score) {
      return (parms->word_weight*word_score 
	      + parms->soundslike_weight*soundslike_score)/100;
    }
    int skip_first_couple(NearMisses::iterator & i) {
      int k = 0;
      InsensitiveCompare cmp(lang);
      const char * prev_word = "";
      while (preview_next(i) != scored_near_misses.end()) 
	// skip over the first couple of items as they should
	// not be counted in the threshold score.
      {
	if (!i->count || cmp(prev_word, i->word) == 0) {
	  ++i;
	} else if (k == parms->skip) {
	  break;
	} else {
          prev_word = i->word;
	  ++k;
	  ++i;
	}
      }
      return k;
    }

    void try_split();
    void try_one_edit_word();
    void try_scan();
    void try_scan_root();
    void try_repl();
    void try_ngram();

    void score_list();
    void fine_tune_score();
    void transfer();
  public:
    Working(SpellerImpl * m, const Language *l,
	    const String & w, const SuggestParms *  p)
      : Score(l,w,p), threshold(1), max_word_length(0), sp(m) {
      memset(check_info, 0, sizeof(check_info));
    }
    void get_suggestions(NearMissesFinal &sug);
  };

  void Working::get_suggestions(NearMissesFinal & sug) {

    if (original.word.size() * parms->edit_distance_weights.max >= 0x8000)
      return; // to prevent overflow in the editdist functions

    near_misses_final = & sug;

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

      if (parms->check_after_one_edit_word) {
        score_list();
        if (try_harder <= 0) goto done;
      }

    }

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
      
      if (try_harder <= 0) goto done;

    }

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
      
      if (try_harder < parms->ngram_threshold) goto done;

    }

    if (parms->try_ngram) {

#ifdef DEBUG_SUGGEST
      COUT.printl("TRYING NGRAM");
#endif

      try_ngram();

      score_list();

    }

  done:

    fine_tune_score();

    transfer();
  }

  // Forms a word by combining CheckInfo fields.
  // Will grow the grow the temp in the buffer.  The final
  // word must be null terminated and commited.
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

  void Working::try_word_n(ParmString str, int score)  
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
        add_nearmiss(i, sw, 0, score, -1, do_count);
    }
    if (sp->affix_compress) {
      CheckInfo ci; memset(&ci, 0, sizeof(ci));
      bool res = lang->affix()->affix_check(LookupInfo(sp, LookupInfo::Clean), str, ci, 0);
      if (!res) return;
      form_word(ci);
      char * end = (char *)buffer.grow_temp(1);
      char * tmp = (char *)buffer.temp_ptr();
      buffer.commit_temp();
      *end = '\0';
      add_nearmiss(tmp, end - tmp, 0, 0, score, -1, do_count);
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
    memset(ci, 0, sizeof(CheckInfo));
    return 0;
  }

  void Working::try_word_c(char * word, char * word_end, int score)
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
    char * beg = (char *)buffer.temp_ptr(); // since the orignal string may of moved
    *end = 0;
    buffer.commit_temp();
    add_nearmiss(beg, end - beg, 0, 0, score, -1, do_count);
    //CERR.printl(tmp);
    memset(check_info, 0, sizeof(CheckInfo)*res);
  }

  void Working::add_nearmiss(char * word, unsigned word_size,
                             WordInfo word_info,
                             const char * sl,
                             int w_score, int sl_score, 
                             bool count, WordEntry * rl)
  {
    if (word_size * parms->edit_distance_weights.max >= 0x8000) 
      return; // to prevent overflow in the editdist functions

    if (w_score < 0) w_score = LARGE_NUM;
    if (sl_score < 0) sl_score = LARGE_NUM;
    if (!sp->have_soundslike) {
      if (w_score >= LARGE_NUM)       w_score = sl_score;
      else if (sl_score >= LARGE_NUM) sl_score = w_score;
    }

    near_misses.push_front(ScoreWordSound());
    ScoreWordSound & d = near_misses.front();
    d.word = word;
    d.soundslike = sl;
    //d.word_size = word_size;
    
    if (parms->use_typo_analysis) {
      unsigned int l = word_size;
      if (l > max_word_length) max_word_length = l;
    }
    
    if (!(word_info & ALL_CLEAN)) {
      d.word_clean = (char *)buffer.alloc(word_size + 1);
      lang->LangImpl::to_clean((char *)d.word_clean, word);
    } else {
      d.word_clean = d.word;
    }

    if (!sp->have_soundslike && !d.soundslike)
      d.soundslike = d.word_clean;
    
    d.word_score       = w_score;
    d.soundslike_score = sl_score;
    d.count = count;
    d.repl_list = rl;
  }

  void Working::add_nearmiss(SpellerImpl::WS::const_iterator i,
                             const WordEntry & w, const char * sl,
                             int w_score, int sl_score, bool count)
  {
    assert(w.word_size == strlen(w.word));
    WordEntry * repl = 0;
    if (w.what == WordEntry::Misspelled) {
      repl = new WordEntry;
      const ReplacementDict * repl_dict
        = static_cast<const ReplacementDict *>(*i);
      repl_dict->repl_lookup(w, *repl);
    }
    add_nearmiss(buffer.dup(ParmString(w.word, w.word_size)), 
                 w.word_size, w.word_info, 
                 sl,
                 w_score, sl_score, count, repl);
  }
  
  void Working::add_nearmiss(SpellerImpl::WS::const_iterator i,
                             const WordAff * w, const char * sl,
                             int w_score, int sl_score, bool count)
  {
    add_nearmiss(buffer.dup(w->word), w->word.size, 0, 
                 sl,
                 w_score, sl_score, count);
  }

  void Working::try_split() {
    const String & word       = original.word;
    
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
          add_nearmiss(buffer.dup(new_word), word.size() + 1, 0, 0,
                       parms->edit_distance_weights.del2*3/2, -1,
                       dont_count);
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
      
      add_nearmiss(i, w, sl, -1, score);
      
      if (w.aff[0]) {
        String sl_buf;
        temp_buffer.reset();
        WordAff * exp_list;
          exp_list = lang->affix()->expand(w.word, w.aff, temp_buffer);
          for (WordAff * p = exp_list->next; p; p = p->next)
            add_nearmiss(i, p, 0, -1, -1);
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
            add_nearmiss(i, p, sl, -1, score, do_count);
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
            add_nearmiss(i, q, sl, -1, score, do_count);
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
        try_word(buf.pbegin(), buf.pend(), parms->edit_distance_weights.sub*3/2);
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

    try_harder = 3;
    if (near_misses.empty()) return;

    NearMisses::iterator i;
    NearMisses::iterator prev;

    near_misses.push_front(ScoreWordSound());
    // the first item will NEVER be looked at.
    scored_near_misses.push_front(ScoreWordSound());
    scored_near_misses.front().score = -1;
    // this item will only be looked at when sorting so 
    // make it a small value to keep it at the front.

    int try_for = (parms->word_weight*parms->edit_distance_weights.max)/100;
    while (true) {
      try_for += (parms->word_weight*parms->edit_distance_weights.max)/100;

      // put all pairs whose score <= initial_limit*max_weight
      // into the scored list

      prev = near_misses.begin();
      i = prev;
      ++i;
      while (i != near_misses.end()) {

        //CERR.printf("%s %s %s %d %d\n", i->word, i->word_clean, i->soundslike,
        //            i->word_score, i->soundslike_score);

        if (i->word_score >= LARGE_NUM) {
          int sl_score = i->soundslike_score < LARGE_NUM ? i->soundslike_score : 0;
          int level = needed_level(try_for, sl_score);
          
          if (level >= int(sl_score/parms->edit_distance_weights.min)) 
            i->word_score = edit_distance(original.clean,
                                          i->word_clean,
                                          level, level,
                                          parms->edit_distance_weights);
        }
        
        if (i->word_score >= LARGE_NUM) goto cont1;

        if (i->soundslike_score >= LARGE_NUM) 
        {
          if (weighted_average(0, i->word_score) > try_for) goto cont1;

          if (i->soundslike == 0) i->soundslike = to_soundslike(i->word, strlen(i->word));

          i->soundslike_score = edit_distance(original.soundslike, i->soundslike, 
                                              parms->edit_distance_weights);
        }

        i->score = weighted_average(i->soundslike_score, i->word_score);

        if (i->score > try_for + parms->span) goto cont1;

        //CERR.printf("2>%s %s %s %d %d\n", i->word, i->word_clean, i->soundslike,
        //            i->word_score, i->soundslike_score);

        scored_near_misses.splice_into(near_misses,prev,i);
        
        i = prev; // Yes this is right due to the slice
        ++i;

        continue;
        
      cont1:
        prev = i;
        ++i;
      }
	
      scored_near_misses.sort();
	
      i = scored_near_misses.begin();
      ++i;
	
      if (i == scored_near_misses.end()) continue;
	
      int k = skip_first_couple(i);
	
      if ((k == parms->skip && i->score <= try_for) 
	  || prev == near_misses.begin() ) // or no more left in near_misses
	break;
    }
      
    threshold = i->score + parms->span;
    if (threshold < parms->edit_distance_weights.max)
      threshold = parms->edit_distance_weights.max;

#  ifdef DEBUG_SUGGEST
    COUT << "Threshold is: " << threshold << "\n";
    COUT << "try_for: " << try_for << "\n";
    COUT << "Size of scored: " << scored_near_misses.size() << "\n";
    COUT << "Size of ! scored: " << near_misses.size() << "\n";
#  endif

    //if (threshold - try_for <=  parms->edit_distance_weights.max/2) return;
      
    prev = near_misses.begin();
    i = prev;
    ++i;
    while (i != near_misses.end()) {
	
      if (i->word_score >= LARGE_NUM) {

        int sl_score = i->soundslike_score < LARGE_NUM ? i->soundslike_score : 0;
        int initial_level = needed_level(try_for, sl_score);
        int max_level = needed_level(threshold, sl_score);
        
        if (initial_level < max_level)
          i->word_score = edit_distance(original.clean.c_str(),
                                        i->word_clean,
                                        initial_level+1,max_level,
                                        parms->edit_distance_weights);
      }

      if (i->word_score >= LARGE_NUM) goto cont2;
      
      if (i->soundslike_score >= LARGE_NUM) 
      {
        if (weighted_average(0, i->word_score) > threshold) goto cont2;
        
        if (i->soundslike == 0) 
          i->soundslike = to_soundslike(i->word, strlen(i->word));
        
        i->soundslike_score = edit_distance(original.soundslike, i->soundslike,
                                            parms->edit_distance_weights);
      }

      i->score = weighted_average(i->soundslike_score, i->word_score);

      if (i->score > threshold + parms->span) goto cont2;
      
      scored_near_misses.splice_into(near_misses,prev,i);
      
      i = prev; // Yes this is right due to the slice
      ++i;
      
      continue;
	
    cont2:
	prev = i;
	++i;
        
    }

    near_misses.pop_front();

    scored_near_misses.sort();
    scored_near_misses.pop_front();

    if (near_misses.empty()) {
      try_harder = 1;
    } else {
      i = scored_near_misses.begin();
      skip_first_couple(i);
      ++i;
      try_harder = i == scored_near_misses.end() ? 2 : 0;
    }

#  ifdef DEBUG_SUGGEST
    COUT << "Size of scored: " << scored_near_misses.size() << "\n";
    COUT << "Size of ! scored: " << near_misses.size() << "\n";
    COUT << "Try Harder: " << try_harder << "\n";
#  endif
  }

  void Working::fine_tune_score() {

    NearMisses::iterator i;

    if (parms->use_typo_analysis) {
      int max = 0;
      unsigned int j;

      CharVector orig_norm, word;
      orig_norm.resize(original.word.size() + 1);
      for (j = 0; j != original.word.size(); ++j)
          orig_norm[j] = parms->ti->to_normalized(original.word[j]);
      orig_norm[j] = 0;
      ParmString orig(orig_norm.data(), j);
      word.resize(max_word_length + 1);
      
      for (i = scored_near_misses.begin();
	   i != scored_near_misses.end() && i->score <= threshold;
	   ++i)
      {
	for (j = 0; (i->word)[j] != 0; ++j)
	  word[j] = parms->ti->to_normalized((i->word)[j]);
	word[j] = 0;
	int word_score 
	  = typo_edit_distance(ParmString(word.data(), j), orig, *parms->ti);
	i->score = weighted_average(i->soundslike_score, word_score);
	if (max < i->score) max = i->score;
      }
      threshold = max;
      for (;i != scored_near_misses.end() && i->score <= threshold; ++i)
	i->score = threshold + 1;

      scored_near_misses.sort();
    }
  }

  void Working::transfer() {

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
	 i != scored_near_misses.end() && c <= parms->limit
	   && ( i->score <= threshold || c <= 3 );
	 ++i, ++c) {
#    ifdef DEBUG_SUGGEST
      //COUT.printf("%p %p: ",  i->word, i->soundslike);
      COUT << i->word << '\t' << i->score 
           << '\t' << i->soundslike << "\n";
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
 	    near_misses_final->push_back(*dup_pair.first);
 	} while (i->repl_list->adv());
      } else {
        fix_case(i->word);
	dup_pair = duplicates_check.insert(i->word);
	if (dup_pair.second )
	  near_misses_final->push_back(*dup_pair.first);
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
  public:
    PosibErr<void> setup(SpellerImpl * m);
    //SuggestImpl(SpellerImpl * m, const SuggestParms & p)
    //  : speller_(m), parms_(p) 
    //{parms_.fill_distance_lookup(m->config(), m->lang());}
    PosibErr<void> set_mode(ParmString mode) {
      return parms_.set(mode, speller_);
    }
    double score(const char *base, const char *other) {
      //parms_.set_original_size(strlen(base));
      //Score s(&speller_->lang(),base,parms_);
      //string sl = speller_->lang().to_soundslike(other);
      //ScoreWordSound sws(other, sl.c_str());
      //s.score(sws);
      //return sws.score;
      return -1;
    }
    SuggestionList & suggest(const char * word);
  };
  
  PosibErr<void> SuggestImpl::setup(SpellerImpl * m)
  {
    speller_ = m;
    RET_ON_ERR(parms_.set(m->config()->retrieve("sug-mode"), speller_));
    if (m->config()->have("sug-typo-analysis"))
      parms_.use_typo_analysis = m->config()->retrieve_bool("sug-typo-analysis");
    if (m->config()->have("sug-repl-table"))
      parms_.use_repl_table = m->config()->retrieve_bool("sug-repl-table");
    
    StringList sl;
    m->config()->retrieve_list("sug-split-char", &sl);
    StringListEnumeration els = sl.elements_obj();
    const char * s;
    parms_.split_chars.clear();
    while ((s = els.next()) != 0) {
      parms_.split_chars.push_back(*s);
    }

    String keyboard = m->config()->retrieve("keyboard");
    if (keyboard == "none")
      parms_.use_typo_analysis = false;
    else
      RET_ON_ERR(aspeller::setup(parms_.ti, m->config(), &m->lang(), keyboard));

    return no_err;
  }

  SuggestionList & SuggestImpl::suggest(const char * word) { 
#   ifdef DEBUG_SUGGEST
    COUT << "=========== begin suggest " << word << " ===========\n";
#   endif
    parms_.set_original_word_size(strlen(word));
    suggestion_list.suggestions.resize(0);
    Working sug(speller_, &speller_->lang(),word,&parms_);
    sug.get_suggestions(suggestion_list.suggestions);
#   ifdef DEBUG_SUGGEST
    COUT << "^^^^^^^^^^^  end suggest " << word << "  ^^^^^^^^^^^\n";
#   endif
    return suggestion_list;
  }
  
}

namespace aspeller {
  PosibErr<Suggest *> new_default_suggest(SpellerImpl * m) {
    StackPtr<SuggestImpl> s(new SuggestImpl);
    RET_ON_ERR(s->setup(m));
    return s.release();
  }

  //Suggest * new_default_suggest(SpellerImpl * m, const SuggestParms & p) {
  //  return new aspeller_default_suggest::SuggestImpl(m,p);
  //}

  PosibErr<void> SuggestParms::set(ParmString mode, SpellerImpl * sp) {

    edit_distance_weights.del1 =  95;
    edit_distance_weights.del2 =  95;
    edit_distance_weights.swap =  90;
    edit_distance_weights.sub =  100;
    edit_distance_weights.similar = 10;
    edit_distance_weights.max = 100;
    edit_distance_weights.min =  90;

    normal_soundslike_weight = 50;
    small_word_soundslike_weight = 15;
    small_word_threshold = 4;

    soundslike_weight = normal_soundslike_weight;
    word_weight       = 100 - normal_soundslike_weight;

    split_chars = " -";

    skip = 2;
    limit = 100;
    span = 50;
    ngram_keep = 10;
    use_typo_analysis = true;
    use_repl_table = sp->have_repl;
    try_one_edit_word = true; // always a good idea, even when
                              // soundslike lookup is used
    check_after_one_edit_word = false;
    ngram_threshold = 2;
    if (mode == "ultra") {
      try_scan_1 = true;
      try_scan_2 = false;
      try_ngram = false;
    } else if (mode == "fast") {
      try_scan_1 = true;
      try_scan_2 = false;
      try_ngram = false;
    } else if (mode == "normal") {
      try_scan_1 = true;
      try_scan_2 = true;
      try_ngram = false;
    } else if (mode == "slow") {
      try_scan_1 = false;
      try_scan_2 = true;
      try_ngram = true;
      ngram_threshold = sp->have_soundslike ? 1 : 2;
    } else if (mode == "bad-spellers") {
      try_scan_1 = false;
      try_scan_2 = true;
      try_ngram = true;
      use_typo_analysis = false;
      normal_soundslike_weight = 55;
      small_word_threshold = 0;
      span = 125;
      limit = 1000;
      ngram_threshold = 1;
    } else {
      return make_err(bad_value, "sug-mode", mode, _("one of ultra, fast, normal, slow, or bad-spellers"));
    }
    if (!sp->have_soundslike) {
      // in this case try_scan_1 will not get better results than
      // try_one_edit_word
      if (try_scan_1) {
        check_after_one_edit_word = true;
        try_scan_1 = false;
      }
    }

    return no_err;
  }

    
  SuggestParms * SuggestParms::clone() const {
    return new SuggestParms(*this);
  }

  void SuggestParms::set_original_word_size(int size) {
    if (size <= small_word_threshold) {
      soundslike_weight = small_word_soundslike_weight;
    } else {
      soundslike_weight = normal_soundslike_weight;
    }
    word_weight = 100 - soundslike_weight;
  }
}
