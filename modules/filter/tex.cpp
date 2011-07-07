// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "asc_ctype.hpp"
#include "config.hpp"
#include "string.hpp"
#include "indiv_filter.hpp"
#include "mutable_container.hpp"
#include "string_map.hpp"
#include "clone_ptr.hpp"
#include "vector.hpp"
#include "errors.hpp"
#include "convert_filter.hpp"

//#define FILTER_PROGRESS_CONTROL "tex-filter-debug.log"
//#include "filter_debug.hpp"
//#include <stdio.h>
//#include <cstdio>
#include "filter_char_vector.hpp"
#include "string_list.hpp"
#include "string_enumeration.hpp"

#include "gettext.h"

namespace {

  using namespace aspell;

  class TexFilter : public NormalFilter 
  {
  private:
    enum InWhat {Name, Opt, Parm, Other, Swallow};
    struct Command {
      InWhat in_what;
      String name;
      const char * do_check;
      Command() {}
      Command(InWhat w) : in_what(w), do_check("P") {}
    };

    bool in_comment;
    bool prev_backslash;
    Vector<Command> stack;

    class Commands : public StringMap {
    public:
      PosibErr<bool> add(ParmStr to_add);
      PosibErr<bool> remove(ParmStr to_rem);
    };
    
    Commands commands;
    bool check_comments;
    
    inline void push_command(InWhat);
    inline void pop_command();

    bool end_option(char u, char l);

    inline bool process_char(FilterChar::Chr c);
    
  public:
    PosibErr<bool> setup(Config *);
    void reset();
    void process(FilterChar * &, FilterChar * &);
  };

  //
  //
  //

  inline void TexFilter::push_command(InWhat w) {
    stack.push_back(Command(w));
  }

  inline void TexFilter::pop_command() {
    stack.pop_back();
    if (stack.empty())
      push_command(Parm);
  }

  //
  //
  //

  PosibErr<bool> TexFilter::setup(Config * opts) 
  {
    set_name("tex");
    set_order_num(0.35);

    commands.clear();
    opts->retrieve_list("f-tex-command", &commands);
    
    check_comments = opts->retrieve_bool("f-tex-check-comments");

    reset();
    return true;
  }
  
  void TexFilter::reset() 
  {
    in_comment = false;
    prev_backslash = false;
    stack.resize(0);
    push_command(Parm);
  }

#  define top stack.back()

  // yes this should be inlined, it is only called once
  inline bool TexFilter::process_char(FilterChar::Chr c) 
  {
    // deal with comments
    if (c == '%' && !prev_backslash) in_comment = true;
    if (in_comment && c == '\n')     in_comment = false;

    prev_backslash = false;

    if (in_comment)                  return !check_comments;

    if (top.in_what == Name) {
      if (asc_isalpha(c)) {

	top.name += c;
	return true;

      } else {

	if (top.name.empty() && (c == '@')) {
	  top.name += c;
	  return true;
	}
	  
	top.in_what = Other;

	if (top.name.empty()) {
	  top.name.clear();
	  top.name += c;
	  top.do_check = commands.lookup(top.name.c_str());
	  if (top.do_check == 0) top.do_check = "";
	  return !asc_isspace(c);
	}

	top.do_check = commands.lookup(top.name.c_str());
	if (top.do_check == 0) top.do_check = "";

	if (asc_isspace(c)) { // swallow extra spaces
	  top.in_what = Swallow;
	  return true;
	} else if (c == '*') { // ignore * at end of commands
	  return true;
	}
	
	// continue o...
      }

    } else if (top.in_what == Swallow) {

      if (asc_isspace(c))
	return true;
      else
	top.in_what = Other;
    }

    if (c == '{')
      while (*top.do_check == 'O' || *top.do_check == 'o') 
	++top.do_check;

    if (*top.do_check == '\0')
      pop_command();

    if (c == '{') {

      if (top.in_what == Parm || top.in_what == Opt || top.do_check == '\0')
	push_command(Parm);

      top.in_what = Parm;
      return true;
    }

    if (top.in_what == Other) {

      if (c == '[') {

	top.in_what = Opt;
	return true;

      } else if (asc_isspace(c)) {

	return true;

      } else {
	
	pop_command();

      }

    } 

    if (c == '\\') {
      prev_backslash = true;
      push_command(Name);
      return true;
    }

    if (top.in_what == Parm) {

      if (c == '}')
	return end_option('P','p');
      else
	return *top.do_check == 'p';

    } else if (top.in_what == Opt) {

      if (c == ']')
	return end_option('O', 'o');
      else
	return *top.do_check == 'o';

    }

    return false;
  }

  void TexFilter::process(FilterChar * & str, FilterChar * & stop)
  {
    FilterChar * cur = str;

    while (cur != stop) {
      if (process_char(*cur))
	*cur = ' ';
      ++cur;
    }
  }

  bool TexFilter::end_option(char u, char l) {
    top.in_what = Other;
    if (*top.do_check == u || *top.do_check == l)
      ++top.do_check;
    return true;
  }

  //
  // TexFilter::Commands
  //

  PosibErr<bool> TexFilter::Commands::add(ParmStr value) {
    int p1 = 0;
    while (!asc_isspace(value[p1])) {
      if (value[p1] == '\0') 
	return make_err(bad_value, value,"",
                        _("a string of 'o','O','p',or 'P'"));
      ++p1;
    }
    int p2 = p1 + 1;
    while (asc_isspace(value[p2])) {
      if (value[p2] == '\0') 
	return make_err(bad_value, value,"",
                        _("a string of 'o','O','p',or 'P'"));
      ++p2;
    }
    String t1; t1.assign(value,p1);
    String t2; t2.assign(value+p2);
    return StringMap::replace(t1, t2);
  }
  
  PosibErr<bool> TexFilter::Commands::remove(ParmStr value) {
    int p1 = 0;
    while (!asc_isspace(value[p1]) && value[p1] != '\0') ++p1;
    String temp; temp.assign(value,p1);
    return StringMap::remove(temp);
  }
}

C_EXPORT IndividualFilter * new_aspell_tex_filter() 
{
  return new TexFilter;
}

C_EXPORT IndividualFilter * new_aspell_tex_decoder() 
{
  GenConvFilterParms parms("tex");
  parms.file = "tex";
  return new_convert_filter_decoder(parms);
}

C_EXPORT IndividualFilter * new_aspell_tex_encoder() 
{
  GenConvFilterParms parms("tex");
  parms.file = "tex";
  return new_convert_filter_encoder(parms);
}
