// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ACOMMON_FILTER__HPP
#define ACOMMON_FILTER__HPP

#include <assert.h>

#include "string.hpp"
#include "posib_err.hpp"
#include "filter_char.hpp"

namespace aspell {

  class Config;

  class FilterHandle {
  public:
    FilterHandle() : handle(0) {}
    ~FilterHandle();
    void * release() {
      void * tmp = handle;
      handle = NULL;
      return tmp;
    }
    operator bool() {return handle != NULL;}
    void * get() {return handle;}

    // The direct interface usually when new_filter ... functions are coded
    // manually
    FilterHandle & operator= (void * h) {
      assert(handle == NULL);
      handle = h; 
      return *this;
    }
    // only valid when handle == 0
    FilterHandle(const FilterHandle & other) {
      assert(other.handle == NULL);
      handle = NULL;
    }
      
  private:
    void operator = (const FilterHandle &);
    void * handle;
  };  

  class IndividualFilter {
    friend class ConversionFilter;
    friend class NormalFilter;
  public:

    // sets up the filter 
    //
    // any options effecting this filter should start with the filter
    // name followed by a dash
    // should return true if the filter should be used false otherwise
    // (in which case it will be deleted)
    virtual PosibErr<bool> setup(Config *) = 0;

    // reset the internal state of the filter
    //
    // should be called whenever a new document is being filtered
    virtual void reset() = 0;

    // process the string
    //
    // The filter may either modify the string passed in or use its
    // own buffer for the output.  If the string uses its own buffer
    //  start and stop should be set to the beginning and one past the
    // end of its buffer respectfully.
    //
    // The string passed in should only be split on white space
    // characters.  Furthermore, between calls to reset, each string
    // should be passed in exactly once and in the order they appeared
    // in the document.  Passing in stings out of order, skipping
    // strings or passing them in more than once may lead to undefined
    // results.
    //
    // The following properties are guaranteed to be true and must
    // stay true:
    //   *stop  == '\0';
    //   strlen == stop - start;
    // this way it is always safe to look one character ahead.
    //
    virtual void process(FilterChar * & start, FilterChar * & stop) = 0;

    virtual ~IndividualFilter() {}

    const char * name() const {return name_.str();}
    const char * base_name() const {return base_name_.str();}
    double order_num() const {return order_num_;}

    enum What {Encoder, Filter, Decoder};
    What what() const {return what_;}

    FilterHandle handle;

  protected:
    // set the name and type of filter, after called base_name, name,
    // and what will be defined
    void set_order_num(double on) {order_num_ = on;}

  private:
    // IndividualFilter should not be inherited from except for 
    // by NormalFilter and ConversionFilter
    IndividualFilter();

    String base_name_;      // must consist of 'a-z|0-9'
    String name_;
    double order_num_; // between 0 and 1 exclusive
    What what_;        // an encoder, filter, or decoder?
  };

  class NormalFilter : public IndividualFilter {
  protected:
    NormalFilter() {}
    void set_name(ParmStr name);
  };

  // "ConversionFilter::process" can not have any state.  However it IS
  // allowed to use an internal buffer, therefore "process" is still not
  // thread safe or const
  class ConversionFilter : public IndividualFilter {
  protected:
    ConversionFilter() {}
    void set_name(ParmStr name, What);
    
  };

}

#endif
