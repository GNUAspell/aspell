// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include "speller.hpp"
#include "checker.hpp"
#include "stack_ptr.hpp"
#include "convert.hpp"

namespace aspell {

  PosibErr<Checker *> 
  new_checker(Speller * speller)
  {
    StackPtr<Checker> checker(speller->new_checker());
    StackPtr<Filter> filter(speller->to_internal_->shallow_copy_filter());
    setup_filter(*filter, speller->config(), true, true, false);
    checker->set_filter(filter.release());
    checker->reset();
    return checker.release();
  }

}
