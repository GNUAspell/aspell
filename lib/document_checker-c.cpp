/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "checker.hpp"
#include "error.hpp"

namespace aspell {

class CanHaveError;
struct Error;
class Speller;

struct Token {
  unsigned int offset;
  unsigned int len;
};

typedef Checker DocumentChecker;

extern "C" void delete_aspell_document_checker(DocumentChecker * ths)
{
  delete ths;
}

extern "C" unsigned int aspell_document_checker_error_number(const DocumentChecker * ths)
{
  return ths->err_ == 0 ? 0 : 1;
}

extern "C" const char * aspell_document_checker_error_message(const DocumentChecker * ths)
{
  return ths->err_ ? ths->err_->mesg : "";
}

extern "C" const Error * aspell_document_checker_error(const DocumentChecker * ths)
{
  return ths->err_;
}

extern "C" CanHaveError * new_aspell_document_checker(Speller * speller)
{
  PosibErr<Checker *> ret = new_checker(speller);
  if (ret.has_err()) {
    return new CanHaveError(ret.release_err());
  } else {
    return ret.data;
  }
}

extern "C" DocumentChecker * to_aspell_document_checker(CanHaveError * obj)
{
  return static_cast<DocumentChecker *>(obj);
}

extern "C" void aspell_document_checker_reset(DocumentChecker * ths)
{
  ths->reset();
}

extern "C" void aspell_document_checker_process(DocumentChecker * ths, const char * str, int size)
{
  ths->process(str, size);
}

extern "C" Token aspell_document_checker_next_misspelling(DocumentChecker * ths)
{
  const CheckerToken * ctok = ths->next_misspelling();
  Token tok;
  if (ctok) {
    tok.offset = ctok->begin.offset;
    tok.len    = ctok->end.offset - ctok->begin.offset;
  } else {
    tok.offset = 0;
    tok.len = 0;
  }
  return tok;
}



}

