// This file is part of The New Aspell
// Copyright (C) 2019 by Kevin Atkinson  and
// Copyright (C) 2025 by Igor Támara
// under the GNU LGPL license version 2.0 or 2.1.  You should
// have received a copy of the LGPL license along with this
// library if you did not you can find it at http://www.gnu.org/.

#include "settings.h"

#include "asc_ctype.hpp"
#include "config.hpp"
#include "filter_char.hpp"
#include "indiv_filter.hpp"
#include "iostream.hpp"

// #define DEBUG_FILTER
/*
 * Include the path of the directory that holds the compiled filter
 * In an invocation like :
 * inst/bin/aspell --add-filter-path=inst/lib/aspell-0.60/
 * --data-dir=/usr/lib/aspell -c po/es.po To reuse the dictionaries, we need to
 * pass the 32 bit compatibility option in Debian
 * ./configure --enable-maintainer-mode --disable-shared
 * --disable-pspell-compatibility   --enable-w-all-error   --prefix="`pwd`/inst"
 * CFLAGS='-g -O' CXXFLAGS='-g -O' --enable-32-bit-hash-fun && bear -- make
 * seergdb is an option for debugger
 */
using namespace acommon;

namespace {

enum actionState { source = 0, translation = 1, other = 2 };

class PoFilter : public IndividualFilter {
  int hide_to_char(FilterChar *, char, FilterChar *);
  int find_char(FilterChar *, char, FilterChar *);
  int hide_all(FilterChar *, FilterChar *);
  int initial_whitespace(FilterChar *, FilterChar *);
  int sanitize_portion(FilterChar *, FilterChar *);
  void maintainer(FilterChar *&, FilterChar *&);
  void translator(FilterChar *&, FilterChar *&);

public:
  virtual PosibErr<bool> setup(Config *);
  virtual void reset(void);
  void process(FilterChar *&, FilterChar *&);
};

bool in_header = false;
bool header_processed = false;
bool in_translation = false;
bool maintainer_mode = false;
actionState current_action = other;

PosibErr<bool> PoFilter::setup(Config *config) {
  name_ = "po-filter";
  order_num_ = 0.80;

  maintainer_mode = config->retrieve_bool("f-po-maintainer-mode");
  return true;
}

void PoFilter::maintainer(FilterChar *&str, FilterChar *&end) {

  for (FilterChar *cur = str; cur < end;) {
    if (*cur == 'm' && *(cur + 1) == 's' && *(cur + 2) == 'g' &&
        *(cur + 3) == 'i' && *(cur + 4) == 'd') {
      current_action = source;
      cur += hide_to_char(cur, '"', end);
      if (cur >= end)
        break;
      sanitize_portion(cur, end - 2);
      cur += find_char(cur, '\n', end);
      if (*(cur - 2) == '"')
        *(cur - 2) = ' ';
    } else {
      if (current_action == source) {
        cur += initial_whitespace(cur, end);
        if (*cur == '"') {
          // This is a multiline msgid...
          *cur = ' ';
          sanitize_portion(cur + 1, end - 1);
          if (*(end - 1) == '"')
            *(end - 1) = ' ';
          cur = end;
          break;
        } else {
          current_action = other;
        }
      }
      cur += hide_to_char(cur, '\n', end);
    }
  }
}

void PoFilter::translator(FilterChar *&str, FilterChar *&end) {
  for (FilterChar *cur = str; cur < end;) {
    if (*cur == 'm' && *(cur + 1) == 's' && *(cur + 2) == 'g' &&
        cur + 6 < end) {
      cur += hide_all(cur, cur + 3);
      current_action = other;
      if (*cur == 'i' && *(cur + 1) == 'd' && header_processed == false) {
        current_action = source;
        in_translation = false;
        cur += hide_to_char(cur, '"', end);
        if (cur >= end)
          break;
        in_header = *cur == '"';
#ifdef DEBUG_FILTER
        if (in_header) {
          CERR.printf(" H");
        } else {
          CERR.printf(" h");
        }
#endif
        cur += hide_to_char(cur, '\n', end);
        if (cur >= end)
          break;
      } else if (*cur == 's' && *(cur + 1) == 't' && *(cur + 2) == 'r') {
        current_action = translation;
        if (in_header) {
#ifdef DEBUG_FILTER
          CERR.printf(" H");
#endif
          in_translation = false;
          cur += hide_all(cur, end);
          break;
        } else {
          in_translation = true;
        }
        cur += hide_to_char(cur, '"', end);
        if (cur >= end)
          break;
#ifdef DEBUG_FILTER
        if (end > cur + 1) {
          CERR.printf(" I -----------------------");
        }
#endif
        // the chunk between cur and end is the translation
        // and can be processed to avoid escape characters
        // and ignore formatters or the like for a given language
        sanitize_portion(cur, end - 2);
        cur += find_char(cur, '\n', end);
        if (*(cur - 2) == '"')
          *(cur - 2) = ' ';
      } else { // A malformed file is not reviewed
        in_translation = false;
        cur += hide_all(cur, end);
      }
    } else {
      if (in_translation) {
        cur += initial_whitespace(cur, end);
        if (*cur == '"') {
          // This is a multiline translation
#ifdef DEBUG_FILTER
          CERR.printf(" M -----------------------");
#endif
          *cur = ' ';
          sanitize_portion(cur + 1, end - 1);
          if (*(end - 1) == '"')
            *(end - 1) = ' ';
          cur = end;
          break;
        }
#ifdef DEBUG_FILTER
        CERR.printf(" h");
#endif
        in_translation = false;
        // The line does not start with a quote, then hiding
        cur += hide_all(cur, end);
      } else {
        if (current_action == source) {
          in_header = false;
        }
        in_translation = false;
        cur += hide_all(cur, end);
#ifdef DEBUG_FILTER
        CERR.printf(" h");
#endif
      }
    }
  } // end of for
}

void PoFilter::process(FilterChar *&str, FilterChar *&end) {
#ifdef DEBUG_FILTER
  CERR.printf("\np %lu:", end - str);
  FilterChar *tmp = str;
  int limit = 20, i = 0;
  while (tmp < end - 1 && i < limit) {
    CERR.printf("%c", (char)*tmp);
    tmp++;
    i++;
  }
#endif
  if (maintainer_mode) {
    maintainer(str, end);
  } else {
    translator(str, end);
  }
}

int PoFilter::hide_all(FilterChar *begin, FilterChar *end) {
  // We will hide everything, nothing to work in here
  FilterChar *current = begin;

  while (current < end) {
    *current = ' ';
    current++;
  }
  return current - begin;
}

int PoFilter::hide_to_char(FilterChar *begin, char limiter, FilterChar *end) {
  // This line needs no spell checking, we go out if the endline is
  // reached, and do no clear the endline, otherwise we clear
  FilterChar *current = begin;
  while (current < end) {
    if (*current == '\n') {
      current++;
      break;
    } else if (*current == limiter) {
      *current = ' ';
      current++;
      break;
    }
    *current = ' ';
    current++;
  }
  return current - begin;
}

int PoFilter::find_char(FilterChar *begin, char limiter, FilterChar *end) {
  // We find the limiter and return the position of the next char
  // we are limited also by the line
  FilterChar *current = begin;
  while (current < end) {
    if (*current == limiter || *current == '\n') {
      current++;
      break;
    }
    current++;
  }
  return current - begin;
}

int PoFilter::initial_whitespace(FilterChar *begin, FilterChar *end) {
  // We find the limiter and return the position of the next char
  FilterChar *current = begin;
  while (current < end && asc_isspace(*current))
    current++;
  return current - begin;
}

int PoFilter::sanitize_portion(FilterChar *begin, FilterChar *end) {
  // We remove the escape characters like \n and friends
  FilterChar *current = begin;
  while (current < end) {
    if (*current == '\\') {
      *current = ' ';
      current++;
      if (current < end) {
        *current = ' ';
        current++;
      } else {
        break;
      }
    }
    current++;
  }
  return current - begin;
}

void PoFilter::reset(void) {
  in_header = false;
  header_processed = false;
  maintainer_mode = false;
}
} // namespace

C_EXPORT
IndividualFilter *new_aspell_po_filter() { return new PoFilter; }
