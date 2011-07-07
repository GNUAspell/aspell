// This file is part of The New Aspell
// Copyright (C) 2002,2011 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <assert.h>

#include "checker_string.hpp"
#include "speller.hpp"
#include "checker.hpp"
#include "asc_ctype.hpp"
#include "convert.hpp"

using namespace aspell;

extern Conv dconv;
extern Conv uiconv;

static int get_line(FILE * in, CheckerString::Line & d)
{
  d.clear();
  int i;
  while ((i = getc(in)), i != EOF)
  {
    d.real.push_back(static_cast<char>(i));
    if (i == '\n') break;
  }
  if (dconv.conv) {
    dconv.conv->convert(d.real.str(), d.real.size(), d.buf, dconv.buf0);
    d.disp.str = d.buf.str();
    d.disp.size = d.buf.size();
  } else {
    d.disp.str = d.real.str();
    d.disp.size = d.real.size();
  }
  return d.real.size();
}

CheckerString::CheckerString(AspellChecker * checker,
                             AspellSpeller * speller,
			     FILE * in, FILE * out, 
			     int num_lines)
  : in_(in), out_(out), checker_(checker), speller_(speller)
{
  lines_.reserve(num_lines + 1);
  for (; num_lines > 0; --num_lines)
  {
    lines_.resize(lines_.size() + 1);
    int s = get_line(in_, lines_.back());
    if (s == 0) break;
  }
  if (lines_.back().real.size() != 0)
    lines_.resize(lines_.size() + 1);

  end_ = lines_.pend() - 1;
  cur_line_ = lines_.pbegin();

  aspell_checker_set_more_data_callback(checker_, checker_callback, this);
  aspell_checker_process(checker_, cur_line_->real.data(), cur_line_->real.size(), 0, cur_line_);
}

CheckerString::~CheckerString()
{
  if (out_)
    for (cur_line_ = first_line(); !off_end(cur_line_); next_line(cur_line_))
    {
      fwrite(cur_line_->real.data(), cur_line_->real.size(), 1, out_);
      cur_line_->clear();
    }
  if (in_ != stdin)
    fclose(in_);
  if (out_ && out_ != stdout && out_ != stdout)
    fclose(out_);
}

bool CheckerString::read_next_line()
{
  if (feof(in_)) return false;
  Line * next = end_;
  inc(next);
  if (next == cur_line_) return false;
  int s = get_line(in_, *end_);
  if (s == 0) return false;
  end_ = next;
  if (out_ && end_->real.size() > 0)
    fwrite(end_->real.data(), end_->real.size(), 1, out_);
  end_->clear();
  return true;
}

void CheckerString::checker_callback(void * d, void * w)
{
  CheckerString * cs = static_cast<CheckerString *>(d);
  Line * cur = static_cast<Line *>(w);
  cs->next_line(cur);
  if (cs->off_end(cur)) return;
  aspell_checker_process(cs->checker_, cur->real.data(), cur->real.size(), 0, cur);
}

bool CheckerString::next_misspelling()
{
  const AspellCheckerToken * tok = aspell_checker_next_misspelling(checker_);
  if (!tok) return false;
  cur_line_ = static_cast<Line *>(tok->begin.which);
  real_word_begin_ = cur_line_->real.begin() + tok->begin.offset;
  real_word_size_  = tok->end.offset - tok->begin.offset;
  fix_display_str();
  return true;
}

void CheckerString::replace(ParmString repl)
{
  assert(real_word_size_ > 0);
  int offset = real_word_begin_ - cur_line_->real.begin();
  aspell_speller_store_replacement(speller_, &*real_word_begin_, real_word_size_,
				   repl.str(), repl.size());
  cur_line_->real.replace(real_word_begin_, real_word_begin_ + real_word_size_,
                          repl.str(), repl.str() + repl.size());
  aspell_checker_replace(checker_, repl.str(), repl.size());
  real_word_begin_ = cur_line_->real.begin() + offset;
  real_word_size_ = repl.size();
  fix_display_str();
}

void CheckerString::fix_display_str()
{
  if (dconv.conv) {
    cur_line_->buf.clear();
    int s = real_word_begin_ - cur_line_->real.begin();
    if (s > 0) dconv.conv->convert(cur_line_->real.data(), s, 
                                   cur_line_->buf, dconv.buf0);
    int off = cur_line_->buf.size();
    dconv.conv->convert(real_word_begin_, real_word_size_, 
                        cur_line_->buf, dconv.buf0);
    disp_word_size_ = cur_line_->buf.size() - off;
    s = cur_line_->real.end() - (real_word_begin_ + real_word_size_);
    if (s > 0) dconv.conv->convert(cur_line_->real.data() + (cur_line_->real.size() - s), 
                                  s, cur_line_->buf, dconv.buf0);
    cur_line_->disp.str = cur_line_->buf.str();
    cur_line_->disp.size = cur_line_->buf.size();
    disp_word_begin_ = cur_line_->buf.data() + off;
  } else {
    cur_line_->disp.str  = cur_line_->real.str();
    cur_line_->disp.size = cur_line_->real.size();
    disp_word_size_  = real_word_size_;
    disp_word_begin_ = real_word_begin_;
  }
}

