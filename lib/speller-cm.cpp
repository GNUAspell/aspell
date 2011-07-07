/* This file is part of The New Aspell
 * Copyright (C) 2005 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "convert.hpp"
#include "speller.hpp"

namespace aspell {

extern "C" const CheckInfo * aspell_speller_check_info(Speller * ths)
{
  const IntrCheckInfo * ci = ths->intr_check_info();
  if (ci == 0) return 0;
  //aspeller::CasePattern casep 
  //  = real_speller->lang().case_pattern(cword);
  String & buf = ths->temp_str_1;
  const IntrCheckInfo * p = ci;
  while (p) {
    buf.clear();
    if (p->guess) {
      if (p->pre_add && p->pre_add[0])
        buf.append(p->pre_add, p->pre_add_len).append('+');
      buf.append(p->word);
      if (p->pre_strip_len > 0) 
        buf.append('-').append(p->word.str(), p->pre_strip_len);
      if (p->suf_strip_len > 0) 
        buf.append('-').append(p->word.str() + p->word.size() - p->suf_strip_len, 
                               p->suf_strip_len);
      if (p->suf_add_len > 0) {
        buf.append('+');
        p->get_suf(buf.data(buf.alloc(p->suf_add_len)));
      }
      // FIXME: real_speller->lang().fix_case(casep, buf.data(), buf.data());
    } else if (p->compound) {
      buf.append('-');
    } else if (p->pre_flag || p->suf_flag) {
      buf.append("+ ");
      buf.append(p->word);
    } else {
      buf.append('*');
    }
    ths->from_internal_->convert(buf.str(), buf.size(), p->str);
    p->ext.next = p->next ? &p->next->ext : 0;
    p->ext.str = p->str.str();
    p->ext.str_len = p->str.size();
    p->ext.guess = p->guess;
    p = p->next;
  }
  return &ci->ext;
}


}

