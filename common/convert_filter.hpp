// This file is part of The New Aspell
// Copyright (C) 2004 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_CONVERT_FILTER__HPP
#define ASPELL_CONVERT_FILTER__HPP

#include "indiv_filter.hpp"
#include "parm_string.hpp"

namespace aspell {

  struct GenConvFilterParms {
    String name;
    String file; // if empty than the file will be retrieved via the
                 // "f-<name>-file" config keyword
    String form;  // if empty "f-<name>-form" config keyword will be used
    double order_num; // if not set an appropriate order number will be used
    GenConvFilterParms(ParmStr n) : name(n), order_num(-1) {}
  };

  IndividualFilter * new_convert_filter(bool decoder, // otherwise encoder
                                        const GenConvFilterParms & parms);

  static inline IndividualFilter * 
  new_convert_filter_decoder(const GenConvFilterParms & parms)
  {
    return new_convert_filter(true, parms);
  }

  static inline IndividualFilter *
  new_convert_filter_encoder(const GenConvFilterParms & parms)
  {
    return new_convert_filter(false, parms);
  }

}


#endif
