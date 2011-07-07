#include "speller_impl.hpp"
#include "lang_impl.hpp"
#include "checker.hpp"

namespace aspell { namespace sp {

  class CheckerImpl : public Checker 
  {
  public:
    CheckerImpl(SpellerImpl *);
    void i_reset(Segment * seg);
    void i_recheck(Segment * seg);
    const CheckerToken * next();
    
    String word;
    SegmentIterator prev_;
    SegmentIterator cur_;
    SegmentIterator next_;
    SpellerImpl * speller;
    const LangImpl * lang;
    
    inline bool is_word(FilterChar::Chr c) {return lang->is_alpha(c);}
    inline bool is_begin(FilterChar::Chr c) {return lang->special(c).begin;}
    inline bool is_middle(FilterChar::Chr c) {return lang->special(c).middle;}
    inline bool is_end(FilterChar::Chr c) {return lang->special(c).end;}
  };

  CheckerImpl::CheckerImpl(SpellerImpl * sp)
  {
    init(sp);
    speller = sp;
    lang = &speller->lang();
  }

  void CheckerImpl::i_reset(Segment * seg)
  {
    cur_ = seg;
  }

  void CheckerImpl::i_recheck(Segment * seg) 
  {
    CheckerImpl::i_reset(seg);
  }

#define ADV do {prev_ = cur_; cur_ = next_; next_.adv(this);} while (false)

  const CheckerToken * CheckerImpl::next() 
  {
    // get "cur_" in a consistent state
    if (cur_.off_end()) {
      // becuase the underlying segment list may have grown 
      // there may be more data even if cur_.off_end(), however
      // !prev_.off_end() and advancing prev_ will _now_ point
      // to the next character
      assert(!prev_.off_end());
      cur_ = prev_;
      cur_.adv(this);
      if (cur_.off_end()) return 0; // truly no more data left
    } else {
      // if !cur_.off_end() than cur_ needs to be initialized so it
      // will point to the first non empty segment
      cur_.init(this);
    }
    // clear "prev_" so that it doesn't point to anything
    prev_.clear(); 
    // we cannot rely on the value of "next_" being correct from last
    // time so reinitialize it
    next_ = cur_;
    next_.adv(this); 

    //
    word.clear();

    // skip spaces (non-word characters)
    while (!cur_.off_end() &&
	   !(is_word(*cur_)
	     || (is_begin(*cur_) && is_word(*next_)))) 
      ADV;

    if (cur_.off_end()) return 0;

    token.begin.which = cur_.seg->which;
    token.begin.offset = cur_.offset;
    token.b.seg = cur_.seg;
    token.b.pos = cur_.pos;

    if (is_begin(*cur_))
    {
      word.append(*cur_);
      ADV;
    }

    while (is_word(*cur_) || 
	   (is_middle(*cur_) && is_word(*prev_) && is_word(*next_)))
    {
      word.append(*cur_);
      ADV;
    }

    if (is_end(*cur_))
    {
      word.append(*cur_);
      ADV;
    }

    token.end.which = cur_.seg->which;
    token.end.offset = cur_.offset;
    token.e.seg = cur_.seg;
    token.e.pos = cur_.pos;

    token.correct = speller->check(word).data;

    free_segments(0, prev_.seg);

    //fprintf(stderr, ">>%s\n", word.str());

    return &token;
  }

  Checker * SpellerImpl::new_checker() {
    return new CheckerImpl(this);
  }

} }
