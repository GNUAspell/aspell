// This file is part of The New Aspell
// Copyright (C) 2004 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "settings.h"

#include "convert_filter.hpp"

using namespace aspell;

C_EXPORT IndividualFilter * new_aspell_genconv_decoder() 
{
  GenConvFilterParms parms("genconv");
  return new_convert_filter_decoder(parms);
}
C_EXPORT IndividualFilter * new_aspell_genconv_encoder() 
{
  GenConvFilterParms parms("genconv");
  return new_convert_filter_encoder(parms);
}
