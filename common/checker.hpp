/* This file is part of The New Aspell
 * Copyright (C) 2001, 2005 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_DOCUMENT_CHECKER__HPP
#define ASPELL_DOCUMENT_CHECKER__HPP

#include "checker_types.hpp"
#include "filter.hpp"
#include "char_vector.hpp"
#include "copy_ptr.hpp"
#include "can_have_error.hpp"
#include "filter_char.hpp"
#include "filter_char_vector.hpp"

namespace aspell {

  class Config;
  class Speller;
  class FullConvert;

  struct SegmentData : public FilterCharVector {
    mutable int refcount;
    SegmentData() : refcount(0) {}
  };

  class SegmentDataPtr
  {
    SegmentData * ptr;
  public:
    void del() {
      if (!ptr) return;
      ptr->refcount--;
      if (ptr->refcount == 0) delete ptr;
      ptr = 0;
    }
    void assign(SegmentData * p) {
      del();
      ptr = p;
      if (ptr) ptr->refcount++;
    }

    SegmentDataPtr() : ptr(0) {}
    SegmentDataPtr(const SegmentDataPtr & other)
      : ptr(other.ptr) {if (ptr) ptr->refcount++;}
    SegmentDataPtr(SegmentData * p)
      : ptr(p) {if (ptr) ptr->refcount++;}
    ~SegmentDataPtr() {del();}
    SegmentDataPtr & operator= (const SegmentDataPtr & other) 
      {assign(other.ptr); return *this;}
    SegmentDataPtr & operator= (SegmentData * p) 
      {assign(p); return *this;}

    SegmentData & operator*  () const {return *ptr;}
    SegmentData * operator-> () const {return ptr;}
    SegmentData * get()         const {return ptr;}
    operator SegmentData * ()   const {return ptr;}
  };

  struct Segment
  {
    // Each segment is expected to have the null character ('\0') as
    // the last character of the string ie *(end-1) == '\0';
    const FilterChar * begin;
    const FilterChar * end;
    Segment * prev;
    Segment * next;
    
    void * which;
    unsigned id;     // uniq id for the orignal string
    unsigned offset; // offset from the orignal string
    unsigned ignore; // don't spellchecker up to ignore
    
    SegmentDataPtr data;
    Segment() : begin(0), end(0), prev(0), next(0), 
                which(0), id(0), offset(0), ignore(0) {}
  };

  class Checker : public CanHaveError {
    friend struct SegmentIterator;
  protected:
    struct IToken : public CheckerToken
    {
      struct Pos {
        Segment * seg;
        const FilterChar * pos;
      };
      Pos b; // begin
      Pos e; // end
    };
  
    virtual void i_reset(Segment *) = 0;
    
    virtual void i_recheck(Segment *) = 0;
    
    IToken token;

    void free_segments(Segment * f = 0, Segment * l = 0); 
    // free all segments between f and l, but not including f and l
    // if f and l is null than free all segments
    // if f is null than free all segments up to l
    // if f is non-null than l must also be non-null
    
    // this needs to be called by the derived class in the constructor
    void init(Speller * speller);

    void need_more(Segment * seg) // seg = last segment on list
      {if (more_data_callback_) more_data_callback_(more_data_callback_data_, seg->which);}

  public:

    Checker();
    virtual ~Checker();
    void reset();
    // Should be called to reset the state when starting a new
    // document.

    void process(const char * str, unsigned size, unsigned ignore = 0,
                 void * which = 0);
    // Process the current string and add it to the queue of strings
    // to check.  The string can be as long as you want it to be (even
    // the whole document) not be smaller than a whitespace separated
    // token and should not split a non-whitspace delimited token in
    // two.  For example:
    //   OK:  "hello ", "world"
    //   NOT: "hel", "lo world"
    //   NOT: "hello w", "orld"
    //   OK:  "http://www.google.com"
    //   NOT: "http://", "www.google.com"
    // An imaginary whitespace character is inserted after each
    // segment, thus,
    //   "hel", "lo world"
    // will be interrupted as "hel lo world".
    //
    // The "which" is a genertic pointer which can be used to
    // keep track of which string the current word belongs to.
    //
    // A refrence to the orignal string is NOT kept, therefore you
    // are free to delete or modify the original string.  However 
    // if the more_data_callback is defined you might want to keep
    // the string around ...

    void add_separator();
    // Add a separator after the last processed string, to separate
    // one word from another.  This means more than simply inserting
    // a whitespace character in the case when the word can have a
    // space in it

    void replace(const char * str, unsigned size); 
    // after a word is corrected, as restarting is not
    // always an option when statefull filters are
    // involved
    
    virtual const CheckerToken * next() = 0; 
    // get next word, returns null if more data is needed

    const CheckerToken * next_misspelling() {
      const CheckerToken * tok;
      while (tok = next(), tok && tok->correct);
      return tok;
    }

    const CheckerToken * cur() const {return &token;}
    
    //bool can_reset_mid(); // true if no statefull filters are involved
    //                      // and thus can be reset mid document

    void set_filter(Filter * f) // will take ownership of filter
      {filter_.reset(f);}

    void set_more_data_callback(void (*c)(void *, void *), void * d) 
      {more_data_callback_ = c; more_data_callback_data_ = d;}
    // sets the callback that is called when more data is needed.  The
    // callback function is expected to add more data with the
    // "process" method.  The first paramter will of the callback
    // function is the callback specific data "d", the second
    // paramter is the "which" pointer of the last string used.
    // If the callback is not set or fails to add more data
    // than the "next" method will return null.

    void set_string_freed_callback(void (*c)(void *, void *), void * d) 
      {string_freed_callback_ = c; string_freed_callback_data_ = d;}
    // sets the callback that is called when a string is freed. The
    // first paramter will of the callback function is the callback
    // specific data "d", the second paramter is the "which" pointer 
    // assocated with the freed sting.

    bool span_strings() {return span_strings_;}
    void set_span_strings(bool v) {span_strings_ = v;}
    // if a word can span multiple strings in the case when word has a
    // space in it or the like.  This defaults to false.

    // Should I add these?
    //PosibErr<void> add_to_personal();
    //PosibErr<void> add_lower_to_personal();
    //PosibErr<void> add_to_session();
    //PosibErr<void> add_lower_to_session();
    //PosibErr<const WordList *> suggest();

  private:
    Checker(const Checker &);
    void operator= (const Checker &);

    Segment * fill_segment(Segment * seg, 
                           const char * str, unsigned size, 
                           Filter * filter);

    void (* more_data_callback_)(void *, void *);
    void * more_data_callback_data_;

    void (* string_freed_callback_)(void *, void *);
    void * string_freed_callback_data_;

    FullConvert * conv_;
    CopyPtr<Filter> filter_;
    FilterCharVector proc_str_;
    unsigned last_id;
    bool span_strings_;
    Segment * first;
    Segment * last;
  };

  struct SegmentIterator {
    static const FilterChar empty_str[1];
    Segment    * seg;
    const FilterChar * pos;
    unsigned        offset;
    SegmentIterator()
      : seg(0), pos(empty_str), offset(0) {}
    void clear()
      {seg = 0; pos = empty_str; offset = 0;}
    SegmentIterator(Segment * s) 
      : seg(s), pos(s->begin), offset(s->offset) {}
    void operator= (Segment * s) 
      {seg = s; pos = s->begin; offset = s->offset;}
    const FilterChar & operator*() const {return *pos;}
    bool off_end() const {return seg == 0;}
    void init(Checker * c)
      {if (seg && pos == seg->end) adv_seg(c);}
    bool adv(Checker * c) {
      if (!seg) return false;
      offset += pos->width;
      pos++;
      if (pos == seg->end) return adv_seg(c);
      return true;
    }
    bool adv_seg(Checker * c);
  };
  
  PosibErr<Checker *> new_checker(Speller *);

}

#endif /* ASPELL_DOCUMENT_CHECKER__HPP */
