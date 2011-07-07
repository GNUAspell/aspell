// This file is part of The New Aspell
// Copyright (C) 2004 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#ifndef __aspell_check_list__
#define __aspell_check_list__

#include "objstack.hpp"
#include "speller.hpp"

namespace aspell {

  struct GuessInfo
  {
    int num;
    IntrCheckInfo * head;
    GuessInfo() : num(0), head(0) {}
    void reset() { buf.reset(); num = 0; head = 0; }
    IntrCheckInfo * add() {
      num++;
      IntrCheckInfo * tmp = (IntrCheckInfo *)buf.alloc_top(sizeof(IntrCheckInfo));
      new(tmp) IntrCheckInfo();
      tmp->clear();
      tmp->next = head;
      head = tmp;
      head->guess = true;
      return head;
    }
    void * alloc(unsigned s) {return buf.alloc_bottom(s);}
    char * dup(ParmString str) {return buf.dup(str);}
  public: // but don't use
    ObjStack buf;
  };


}

#endif
