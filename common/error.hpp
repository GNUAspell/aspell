/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_ERROR__HPP
#define ASPELL_ERROR__HPP

namespace aspell {

struct ErrorInfo;

struct Error {
  // NOTE: Also used by the C API.  Do not modify the data members with
  //   out also modifying "mk-src.in" as they must be layed out in the same
  //   fashion as the AspellError struct.

  const char * mesg; // expected to be allocated with malloc
  const ErrorInfo * err;

  Error() : mesg(0), err(0) {}

  Error(const Error &);
  Error & operator=(const Error &);
  ~Error();
  
  bool is_a(const ErrorInfo * e) const;
};

struct ErrorInfo {
  // NOTE: Also used by the C API.  Do not modify the data members with
  //   out also modifying "mk-src.in" as they must be layed out in the same
  //   fashion as the AspellErrorInfo struct.
  const ErrorInfo * isa;
  const char * mesg;
  unsigned int num_parms;
  const char * parms[3];
};


}

#endif /* ASPELL_ERROR__HPP */
