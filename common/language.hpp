/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_LANGUAGE__HPP
#define ASPELL_LANGUAGE__HPP

#include "can_have_error.hpp"
#include "clone_ptr.hpp"
#include "language_types.hpp"
#include "simple_string.hpp"
#include "convert.hpp"
#include "check_list.hpp"

namespace aspell {

struct MunchListParms;
class StringEnumeration;
class WordList;
class ObjStack;

struct WordAff
{
  SimpleString word;
  const unsigned char * aff;
  WordAff * next;
};

class LangBase {
public:
  virtual const char * name() const = 0;
  virtual const char * charmap() const = 0;
  
  virtual CasePattern case_pattern(ParmStr str) const = 0;
  virtual char * fix_case(CasePattern cs, char * str) const = 0;
  virtual char * to_soundslike(char * str) const = 0;

  virtual void munch(ParmStr word, GuessInfo * cl, bool cross = true) const = 0;
  virtual WordAff * expand(ParmStr word, ParmStr aff,
                           ObjStack & buf, int limit = INT_MAX) const  = 0;
  virtual ~LangBase() {}
};

// the Config class WILL be modified for the moment.  But it WILL NOT
// take ownership of it.
PosibErr<LangBase *> new_language(Config * c);

}

#endif /* ASPELL_LANGUAGE__HPP */
