/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "convert.hpp"
#include "error.hpp"
#include "language.hpp"
#include "language-c.hpp"
#include "language_types.hpp"
#include "mutable_string.hpp"
#include "posib_err.hpp"
#include "string_enumeration.hpp"
#include "word_list.hpp"
#include "lang_impl.hpp"

namespace aspell {

class CanHaveError;
class Config;
struct Error;
class Language;
struct MunchListParms;

extern "C" CanHaveError * new_aspell_language(Config * config)
{
  PosibErr<const LangBase *> ret = new_language(config);
  if (ret.has_err()) return new CanHaveError(ret.release_err());

  StackPtr<Language> lang(new Language(ret));

  const char * sys_enc = lang->real->charmap();
  String user_enc = config->retrieve("encoding");
  if (user_enc == "none") user_enc = sys_enc;

  PosibErr<FullConvert *> conv;
  conv = new_full_convert(*config, user_enc, sys_enc, NormFrom);
  if (conv.has_err()) return new CanHaveError(conv.release_err());
  lang->to_internal_.reset(conv);
  conv = new_full_convert(*config, sys_enc, user_enc, NormTo);
  if (conv.has_err()) return new CanHaveError(conv.release_err());
  lang->from_internal_.reset(conv);

  return lang.release();
}

extern "C" int aspell_language_munch(Language * ths, 
                                     const char * str, int str_size,
                                     PutWordCallback * put_string, void * ps_data)
{
  ths->temp_str_0.clear();
  ths->to_internal_->convert(str, str_size, ths->temp_str_0);
  ths->real->munch(ths->temp_str_0, &ths->gi);
  String & buf = ths->temp_str_1;
  for (const IntrCheckInfo * ci = ths->gi.head; ci; ci = ci->next) 
  {
    buf.clear();
    ths->temp_str_0.clear();
    buf << ci->word;
    if (ci->pre_flag || ci->suf_flag) {
      buf << '/';
      if (ci->pre_flag) buf << ci->pre_flag;
      if (ci->suf_flag) buf << ci->suf_flag;
    }
    ths->from_internal_->convert(buf.str(), buf.size(), ths->temp_str_0);
    Word w;
    w.str = ths->temp_str_0.str();
    w.len = ths->temp_str_0.size();
    bool res = put_string(ps_data, &w);
    if (!res) break;
  }
  return 0;
}

extern "C" const WordList * aspell_language_expand(Language * ths, 
                                                   const char * str, int str_size, 
                                                   PutWordCallback * put_string, void * ps_data,
                                                   int limit)
{
  ths->temp_str_0.clear();
  ths->to_internal_->convert(str, str_size, ths->temp_str_0);
  char * w = ths->temp_str_0.mstr();
  char * af = strchr(w, '/');
  size_t s;
  if (af != 0) {
    s = af - w;
    *af++ = '\0';
  } else {
    s = ths->temp_str_0.size();
    af = w + s;
  }
  ths->gi.buf.reset();
  WordAff * exp_list = ths->real->expand(w, af, ths->gi.buf, limit);
  String & buf = ths->temp_str_1;
  for (WordAff * p = exp_list; p; p = p->next) {
    buf.clear();
    ths->temp_str_0.clear();
    buf << p->word;
    if (limit < INT_MAX && p->aff[0]) buf << '/' << (const char *)p->aff;
    ths->from_internal_->convert(buf.str(), buf.size(), ths->temp_str_0);
    Word w;
    w.str = ths->temp_str_0.str();
    w.len = ths->temp_str_0.size();
    bool res = put_string(ps_data, &w);
    if (!res) break;
  }
  
  return 0;
}


Language::~Language() 
{
}

}

