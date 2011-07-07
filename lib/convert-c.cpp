/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "convert.hpp"
#include "error.hpp"
#include "posib_err.hpp"

namespace aspell {

class Config;

struct APIConv : public CanHaveError {
  Convert * conv;
  CharVector buf;
};

extern "C" CanHaveError * new_aspell_convert(Config * c, const char * from, const char * to, Normalize n, int filter)
{
  PosibErr<Convert *> ret = internal_new_convert(*c, from, to, true, n, false);
  if (ret.has_err()) {
    return new CanHaveError(ret.release_err());
  } else {
    APIConv * conv = new APIConv;
    conv->conv = ret.data;
    return conv;
  }
}

extern "C" APIConv * to_aspell_convert(CanHaveError * obj)
{
  return static_cast<APIConv *>(obj);
}

extern "C" void delete_aspell_convert(APIConv * ths)
{
  delete ths->conv;
  delete ths;
}

extern "C" int aspell_convert_needed(APIConv * ths)
{
  return (bool)ths->conv;
}

extern "C" const char * aspell_convert(APIConv * ths, const char * str)
{
  if (ths->conv) {
    ths->buf.clear();
    ths->conv->convert(str, -1, ths->buf);
    ths->conv->append_null(ths->buf);
    return ths->buf.data();
  } else {
    return str;
  }
}

}

