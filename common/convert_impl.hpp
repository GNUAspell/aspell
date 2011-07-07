// This file is part of The New Aspell
// Copyright (C) 2004 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_CONVERT_IMPL__HPP
#define ASPELL_CONVERT_IMPL__HPP

#include <math.h>

#include "convert.hpp"
#include "objstack.hpp"
#include "vararray.hpp"

namespace aspell {

  typedef unsigned char  byte;
  typedef unsigned char  Uni8;
  typedef unsigned short Uni16;
  typedef unsigned int   Uni32;

  //////////////////////////////////////////////////////////////////////
  //
  // NormLookup
  //

  template <class T>
  struct NormTable
  {
    static const unsigned struct_size;
    unsigned mask;
    unsigned height;
    unsigned width;
    unsigned size;
    T * end;
    T data[1]; // hack for data[]
  };

  template <class T>
  const unsigned NormTable<T>::struct_size = sizeof(NormTable<T>) - sizeof(T);

  template <class T, class From>
  struct NormLookupRet
  {
    const typename T::To * to;
    const From * last;
    NormLookupRet(const typename T::To * t, const From * l) 
      : to(t), last(l) {}
  };

  template <class T, class From>
  static inline NormLookupRet<T,From> norm_lookup(const NormTable<T> * d, 
                                                  const From * s, const From * stop,
                                                  const typename T::To * def,
                                                  const From * prev) 
  {
  loop:
    if (s != stop) {
      const T * i = d->data + (static_cast<typename T::From>(*s) & d->mask);
      for (;;) {
        if (i->from == static_cast<typename T::From>(*s)) {
          if (i->sub_table) {
            // really tail recursion
            if (i->to[1] != T::to_non_char) {def = i->to; prev = s;}
            d = (const NormTable<T> *)(i->sub_table);
            s++;
            goto loop;
          } else {
            return NormLookupRet<T,From>(i->to, s);
          }
        } else {
          i += d->height;
          if (i >= d->end) break;
        }
      }
    }
    return NormLookupRet<T,From>(def, prev);
  }

  template <class T>
  void free_norm_table(NormTable<T> * d)
  {
    for (T * cur = d->data; cur != d->end; ++cur) {
      if (cur->sub_table)
        free_norm_table<T>(static_cast<NormTable<T> *>(cur->sub_table));
    }
    free(d);
  }

  //////////////////////////////////////////////////////////////////////
  //
  // create norm table
  //

# define SANITY(check) \
    if (!(check)) return sanity_fail(__FILE__, FUNC, __LINE__, #check)
# define CREATE_NORM_TABLE(T, in, res) \
    do { PosibErr<NormTable<T> *> pe( create_norm_table<T>(in) );\
         if (pe.has_err()) return PosibErrBase(pe); \
         res = pe.data; } while(false)
  PosibErrBase sanity_fail(const char * file, const char * func, 
                           unsigned line, const char * check_str);

  struct Tally 
  {
    int size;
    Uni32 mask;
    int max;
    int * data;
    Tally(int s, int * d) : size(s), mask(s - 1), max(0), data(d) {
      memset(data, 0, sizeof(int)*size);
    }
    void add(Uni32 chr) {
      Uni32 p = chr & mask;
      data[p]++;
      if (data[p] > max) max = data[p];
    }
  };

  template <class T, class TableInput>
  static PosibErr< NormTable<T> * > create_norm_table(TableInput & in)
  {
    const char FUNC[] = "create_norm_table";
    RET_ON_ERR(in.init());
    int size = in.size;
    VARARRAY(T, d, size);
    memset(d, 0, sizeof(T) * size);
    int sz = 1 << (unsigned)floor(log(size <= 1 ? 1.0 : size - 1)/log(2.0));
    VARARRAY(int, tally0_d, sz);   Tally tally0(sz,   tally0_d);
    VARARRAY(int, tally1_d, sz*2); Tally tally1(sz*2, tally1_d);
    VARARRAY(int, tally2_d, sz*4); Tally tally2(sz*4, tally2_d);
    in.cur = d;
    for (;;) {
      RET_ON_ERR_SET(in.get_next(), bool, res);
      if (!res) break;
      tally0.add(in.cur->from);
      tally1.add(in.cur->from);
      tally2.add(in.cur->from);
      if (in.have_sub_table) {
        TableInput next_in(in);
        in.get_sub_table(next_in);
        CREATE_NORM_TABLE(T, next_in, in.cur->sub_table);
      }
      ++in.cur;
    }
    SANITY(in.cur - d == size);
    Tally * which = &tally0;
    if (which->max > tally1.max) which = &tally1;
    if (which->max > tally2.max) which = &tally2;
    NormTable<T> * final = (NormTable<T> *)calloc(1, NormTable<T>::struct_size + 
                                                  sizeof(T) * which->size * which->max);
    memset(final, 0, NormTable<T>::struct_size + sizeof(T) * which->size * which->max);
    final->mask = which->size - 1;
    final->height = which->size;
    final->width = which->max;
    final->end = final->data + which->size * which->max;
    final->size = size;
    for (T * cur = d; cur != d + size; ++cur) {
      T * dest = final->data + (cur->from & final->mask);
      while (dest->from != 0) dest += final->height;
      *dest = *cur;
      if (dest->from == 0) dest->from = T::from_non_char;
    }
    for (T * dest = final->data; dest < final->end; dest += final->height) {
      if (dest->from == 0 || (dest->from == T::from_non_char && dest->to[0] == 0)) {
        dest->from = T::from_non_char;
        dest->set_to_to_non_char();
      }
    }
    return final;
  }

}

#endif
