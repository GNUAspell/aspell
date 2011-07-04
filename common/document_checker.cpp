/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "document_checker.hpp"
#include "tokenizer.hpp"
#include "convert.hpp"
#include "speller.hpp"
#include "config.hpp"

namespace acommon {

  DocumentChecker::DocumentChecker() 
    : status_fun_(0), speller_(0) {}
  DocumentChecker::~DocumentChecker() 
  {
  }

  PosibErr<void> DocumentChecker
  ::setup(Tokenizer * tokenizer, Speller * speller, Filter * filter)
  {
    tokenizer_.reset(tokenizer);
    filter_.reset(filter);
    speller_ = speller;
    conv_ = speller->to_internal_;
    return no_err;
  }

  void DocumentChecker::set_status_fun(void (* sf)(void *, Token, int), 
				       void * d)
  {
    status_fun_ = sf;
    status_fun_data_ = d;
  }

  void DocumentChecker::reset()
  {
    if (filter_)
      filter_->reset();
  }

  void DocumentChecker::process(const char * str, int size)
  {
    proc_str_.clear();
    conv_->decode(str, size, proc_str_);
    proc_str_.append(0);
    FilterChar * begin = proc_str_.pbegin();
    FilterChar * end   = proc_str_.pend() - 1;
    if (filter_)
      filter_->process(begin, end);
    tokenizer_->reset(begin, end);
  }

  Token DocumentChecker::next_misspelling()
  {
    bool correct;
    Token tok;
    do {
      if (!tokenizer_->advance()) {
	tok.offset = proc_str_.size();
	tok.len = 0;
	return tok;
      }
      correct = speller_->check(MutableString(tokenizer_->word.data(),
					      tokenizer_->word.size() - 1));
      tok.len  = tokenizer_->end_pos - tokenizer_->begin_pos;
      tok.offset = tokenizer_->begin_pos;
      if (status_fun_)
	(*status_fun_)(status_fun_data_, tok, correct);
    } while (correct);
    return tok;
  }

}

