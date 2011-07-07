// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <algorithm>

#include <assert.h>
#include <string.h>
#include <math.h>

#include "settings.h"
#include "asc_ctype.hpp"
#include "convert_impl.hpp"
#include "fstream.hpp"
#include "getdata.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "stack_ptr.hpp"
#include "cache.hpp"
#include "file_util.hpp"
#include "file_data_util.hpp"
#include "objstack.hpp"
#include "convert_filter.hpp"

#include "indiv_filter.hpp"

#include "iostream.hpp"

#include "gettext.h"

//If the max macro was defined, undefine it. We use max as a field name.
#ifdef max
#undef max
#endif
namespace aspell {

  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////
  //
  // Lookups
  //
  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////

  //////////////////////////////////////////////////////////////////////
  //
  // ToUniLookup
  //

  class ToUniLookup 
  {
    Uni32 data[256];
    static const Uni32 npos = (Uni32)(-1);
  public:
    void reset();
    Uni32 operator[] (char key) const {return data[(unsigned char)key];}
    bool have(char key) const {return data[(unsigned char)key] != npos;}
    bool insert(char key, Uni32 value);
  };

  void ToUniLookup::reset() 
  {
    for (int i = 0; i != 256; ++i)
      data[i] = npos;
  }

  bool ToUniLookup::insert(char key, Uni32 value)
  {
    if (data[(unsigned char)key] != npos) 
      return false;
    data[(unsigned char)key] = value;
    return true;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // FromUniLookup
  //

  // Assumes that the maximum number of items in the table is 256
  // Also assumes (unsigned char)i == i % 256

  // Based on the iso-8859-* character sets it is very fast, almost all
  // lookups involving no more than 2 comparisons.
  // NO looks ups involded more than 3 compassions.
  // Also, no division (or modules) is done whatsoever.


  struct UniItem {
    Uni32 key;
    char  value;
  };

  class FromUniLookup 
  {
  private:
    static const Uni32 npos = (Uni32)(-1);
    UniItem * overflow_end;
  
    UniItem data[256*4];

    UniItem overflow[256]; // you can never be too careful;
  
  public:
    FromUniLookup() {}
    void reset();
    inline char operator() (Uni32 key, char unknown = '?') const;
    bool insert(Uni32 key, char value);
  };

  void FromUniLookup::reset()
  {
    for (unsigned i = 0; i != 256*4; ++i)
      data[i].key = npos;
    overflow_end = overflow;
  }

  inline char FromUniLookup::operator() (Uni32 k, char unknown) const
  {
    const UniItem * i = data + (unsigned char)k * 4;

    if (i->key == k) return i->value;
    ++i;
    if (i->key == k) return i->value;
    ++i;
    if (i->key == k) return i->value;
    ++i;
    if (i->key == k) return i->value;
  
    if (i->key == npos) return unknown;
  
    for(i = overflow; i != overflow_end; ++i)
      if (i->key == k) return i->value;

    return unknown;
  }

  bool FromUniLookup::insert(Uni32 k, char v) 
  {
    UniItem * i = data + (unsigned char)k * 4;
    UniItem * e = i + 4;
    while (i != e && i->key != npos) {
      if (i->key == k)
        return false;
      ++i;
    }
    if (i == e) {
      for(i = overflow; i != overflow_end; ++i)
        if (i->key == k) return false;
    }
    i->key = k;
    i->value = v;
    return true;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // CharLookup
  //

  class CharLookup 
  {
  private:
    int data[256];
  public:
    void reset();
    char operator[] (char key) const {return data[(unsigned char)key];}
    bool insert(char key, char value);
  };

  void CharLookup::reset() {
    for (int i = 0; i != 256; ++i) 
      data[i] = -1;
  }

  bool CharLookup::insert(char key, char value) 
  {
    if (data[(unsigned char)key] != -1)
      return false;
    data[(unsigned char)key] = value;
    return true;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // NormLookup Parms
  //

  template <typename T>
  struct AppendToFixed
  {
    T * d;
    unsigned i;
    AppendToFixed(T * d0, void *)
      : d(d0), i(0) {}
    void operator() (unsigned c) {
      assert(i < d->max_to);
      d->to[i] = static_cast<typename T::To>(c);
      assert(c == static_cast<Uni32>(d->to[i]));
      ++i;
    }
    void finish() {}
  };
  
#if 0 // currently unused

  template <typename T>
  struct AppendToDynamic
  {
    T * d;
    unsigned i;
    ObjStack * buf;
    AppendToDynamic(T * d0, ObjStack * buf)
      : d(d0), i(0) {tmp->abort_temp();}
    void operator() (unsigned c) {
      d = buf->resize_temp((i+2)*sizeof(typename T::To));
      d->to[i] = static_cast<typename T::To>(c);
      assert(c == static_cast<Uni32>(d->to[i]));
      ++i;
    }
    void finish() {
      d->to[i] = static_cast<typename T::To>(0);
      tmp->commit_temp();
    }
  };

#endif

  struct FromUniNormEntry
  {
    typedef Uni32 From;
    Uni32 from;
    typedef byte To;
    typedef AppendToFixed<FromUniNormEntry> AppendTo;
    byte  to[4];
    static const From from_non_char = (From)(-1);
    static const To   to_non_char   = 0x10;
    void set_to_to_non_char() {to[0] = to_non_char;} 
    static const unsigned max_to = 4;
    void * sub_table;
  } 
#ifdef __GNUC__    
    __attribute__ ((aligned (16)))
#endif
  ;

  struct ToUniNormEntry
  {
    typedef byte From;
    byte from;
    typedef Uni16 To;
    typedef AppendToFixed<ToUniNormEntry> AppendTo;
    Uni16 to[3];
    static const From from_non_char = 0x10;
    static const To   to_non_char   = 0x10;
    void set_to_to_non_char() {to[0] = to_non_char;}
    static const unsigned max_to = 3;
    void * sub_table;
  } 
#ifdef __GNUC__    
    __attribute__ ((aligned (16)))
#endif
  ;

  //////////////////////////////////////////////////////////////////////
  //
  // read in char data
  //

  PosibErr<void> read_in_char_data (const Config & config,
                                    ParmStr encoding,
                                    ToUniLookup & to,
                                    FromUniLookup & from)
  {
    to.reset();
    from.reset();
    
    String dir1,dir2,file_name;
    fill_data_dir(&config, dir1, dir2);
    find_file(file_name,dir1,dir2,encoding,".cset");

    FStream data;
    PosibErrBase err = data.open(file_name, "r");
    if (err.get_err()) { 
      char mesg[300];
      snprintf(mesg, 300, _("This could also mean that the file \"%s\" could not be opened for reading or does not exist."),
               file_name.c_str());
      return make_err(unknown_encoding, encoding, mesg);
    }
    unsigned chr;
    Uni32 uni;
    String line;
    char * p;
    do {
      p = get_nb_line(data, line);
    } while (*p != '/');
    for (chr = 0; chr != 256; ++chr) {
      p = get_nb_line(data, line);
      if (strtoul(p, 0, 16) != chr)
        return make_err(bad_file_format, file_name);
      uni = strtoul(p + 3, 0, 16);
      to.insert(chr, uni);
      from.insert(uni, chr);
    }
  
    return no_err;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // read in norm tables
  //

  template <class T>
  struct TableFromIStream {
  private:
    IStream & in;
    String  & buf;
    ObjStack * os;
    void operator=(const TableFromIStream &);
  public:
    TableFromIStream(IStream & in0, String & buf0, ObjStack * os0 = 0) 
      : in(in0), buf(buf0), os(os0) {}
    TableFromIStream(const TableFromIStream & other) 
      : in(other.in), buf(other.buf), os(other.os) {}
    int size; // only valid after init is called
    // "cur" and "have_sub_table" will be updated on each call to get_next()
    T * cur;
    bool have_sub_table; // if a sub_table exits get_sub_table MUST be called
    PosibErr<void> init() // sets up the table for input and sets size
    {
      const char FUNC[] = "TableFromIStream::init";
      const char * p = get_nb_line(in, buf);
      SANITY(*p == 'N');
      ++p;
      size = strtoul(p, (char **)&p, 10);
      return no_err;
    }
    PosibErr<bool> get_next() // fills in next entry pointed to by
                              // cur and sets have_sub_table
    {
      const char FUNC[] = "TableFromIStream::get_next";
      char * p = get_nb_line(in, buf);
      if (*p == '.') return false;
      Uni32 f = strtoul(p, (char **)&p, 16);
      cur->from = static_cast<typename T::From>(f);
      SANITY(f == cur->from);
      ++p;
      SANITY(*p == '>');
      ++p;
      SANITY(*p == ' ');
      ++p;
      {
        typename T::AppendTo append_to(cur, os);
        if (*p != '-') {
          for (;;) {
            const char * q = p;
            Uni32 t = strtoul(p, (char **)&p, 16);
            if (q == p) break;
            append_to(t);
          } 
        } else {
          append_to(0);
          append_to(T::to_non_char);
        }
        append_to.finish();
      }
      if (*p == ' ') ++p;
      if (*p == '/') have_sub_table = true;
      else           have_sub_table = false;
      return true;
    }      
    void get_sub_table(TableFromIStream & d) {}
  };

  static PosibErr<void> init_norm_tables(FStream & in, NormTables * d) 
  {
    const char FUNC[] = "init_norm_tables";
    String l;
    get_nb_line(in, l);
    remove_comments(l);
    
    SANITY (l == "INTERNAL");
    get_nb_line(in, l);
    remove_comments(l);
    SANITY (l == "/");
    { 
      TableFromIStream<FromUniNormEntry> tin(in, l);
      CREATE_NORM_TABLE(FromUniNormEntry, tin, d->internal);
    }
    get_nb_line(in, l);
    remove_comments(l);
    SANITY (l == "STRICT");
    char * p = get_nb_line(in, l);
    remove_comments(l);
    if (l == "/") {
      TableFromIStream<FromUniNormEntry> tin(in, l);
      CREATE_NORM_TABLE(FromUniNormEntry, tin, d->strict_d);
      d->strict = d->strict_d;
    } else {
      SANITY(*p == '=');
      ++p; ++p;
      SANITY(strcmp(p, "INTERNAL") == 0);
      d->strict = d->internal;
    }
    while (get_nb_line(in, l)) {
      remove_comments(l);
      d->to_uni.push_back(NormTables::ToUniTable());
      NormTables::ToUniTable & e = d->to_uni.back();
      e.name.resize(l.size());
      for (unsigned i = 0; i != l.size(); ++i)
        e.name[i] = asc_tolower(l[i]);
      char * p = get_nb_line(in, l);
      remove_comments(l);
      if (l == "/") {
        TableFromIStream<ToUniNormEntry> tin(in, l);
        CREATE_NORM_TABLE(ToUniNormEntry, tin, e.data);
        e.ptr = e.data;
      } else {
        SANITY(*p == '=');
        ++p; ++p;
        for (char * q = p; *q; ++q) *q = asc_tolower(*q);
        Vector<NormTables::ToUniTable>::iterator i = d->to_uni.begin();
        while (i->name != p && i != d->to_uni.end()) ++i;
        SANITY(i != d->to_uni.end());
        e.ptr = i->ptr;
        get_nb_line(in, l);
      }
    }  
    return no_err;
  }

  PosibErr<NormTables *> NormTables::get_new(const String & encoding, 
                                             const Config * config)
  {
    String dir1,dir2,file_name;
    fill_data_dir(config, dir1, dir2);
    find_file(file_name,dir1,dir2,encoding,".cmap");
    
    FStream in;
    PosibErrBase err = in.open(file_name, "r");
    if (err.get_err()) { 
      char mesg[300];
      snprintf(mesg, 300, _("This could also mean that the file \"%s\" could not be opened for reading or does not exist."),
               file_name.c_str());
      return make_err(unknown_encoding, encoding, mesg); // FIXME
    }

    NormTables * d = new NormTables;
    d->key = encoding;
    err = init_norm_tables(in, d);
    if (err.has_err()) {
      return make_err(bad_file_format, file_name, err.get_err()->mesg);
    }

    return d;
  }

  NormTables::~NormTables()
  {
    free_norm_table<FromUniNormEntry>(internal);
    if (strict_d)
      free_norm_table<FromUniNormEntry>(strict_d);
    for (unsigned i = 0; i != to_uni.size(); ++i) {
      if (to_uni[i].data)
        free_norm_table<ToUniNormEntry>(to_uni[i].data);
    }
  }

  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////
  //
  //  Convert
  //
  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////

#if 0

  bool operator== (const Convert & rhs, const Convert & lhs)
  {
    return strcmp(rhs.in_code(), lhs.in_code()) == 0
      && strcmp(rhs.out_code(), lhs.out_code()) == 0;
  }

#endif

  void FullConvert::add_filter_codes()
  {
    const char * slash = strchr(in_code_.str(), '/');
    if (slash) in_code_.resize(slash - in_code_.str());
    for (Filter::Iterator i = filter_.begin(); i != filter_.end(); ++i) {
      if ((*i)->what() == IndividualFilter::Decoder) {
        in_code_ += '/';
        in_code_ += (*i)->base_name();
      }
    }

    slash = strchr(out_code_.str(), '/');
    if (slash) out_code_.resize(slash - out_code_.str());
    for (Filter::Iterator i = filter_.begin(); i != filter_.end(); ++i) {
      if ((*i)->what() == IndividualFilter::Encoder) {
        out_code_ += '/';
        out_code_ += (*i)->base_name();
      }
    }

    //printf("%p: %s >> %s\n", this, in_code_.str(), out_code_.str());
  }

  //////////////////////////////////////////////////////////////////////
  //
  // Trivial Conversion
  //

  template <typename Chr>
  struct DecodeDirect : public Decode 
  {
    void decode(const char * in0, int size, FilterCharVector & out) const {
      const Chr * in = reinterpret_cast<const Chr *>(in0);
      if (size == -1) {
        for (;*in; ++in)
          out.append(*in);
      } else {
        const Chr * stop = reinterpret_cast<const Chr *>(in0 +size);
        for (;in != stop; ++in)
          out.append(*in);
      }
    }
    PosibErr<void> decode_ec(const char * in0, int size, 
                             FilterCharVector & out, ParmStr) const {
      DecodeDirect::decode(in0, size, out);
      return no_err;
    }
  };

  template <typename Chr>
  struct EncodeDirect : public Encode
  {
    void encode(const FilterChar * in, const FilterChar * stop, 
                CharVector & out) const {
      for (; in != stop; ++in) {
        Chr c = in->chr;
        if (c != in->chr) c = '?';
        out.append(&c, sizeof(Chr));
      }
    }
    PosibErr<void> encode_ec(const FilterChar * in, const FilterChar * stop, 
                             CharVector & out, ParmStr orig) const {
      for (; in != stop; ++in) {
        Chr c = in->chr;
        if (c != in->chr) {
          char m[70];
          snprintf(m, 70, _("The Unicode code point U+%04X is unsupported."), in->chr);
          return make_err(invalid_string, orig, m);
        }
        out.append(&c, sizeof(Chr));
      }
      return no_err;
    }
    void encode(const FilterChar * b, const FilterChar * e, 
                FilterCharVector & out) const {
      out.append(b, e - b);
    }
  };

  template <typename Chr>
  struct ConvDirect : public DirectConv
  {
    void convert(const char * in0, int size, CharVector & out) const {
      if (size == -1) {
        const Chr * in = reinterpret_cast<const Chr *>(in0);
        for (;*in != 0; ++in)
          out.append(in, sizeof(Chr));
      } else {
        out.append(in0, size);
      }
    }
    PosibErr<void> convert_ec(const char * in0, int size, 
                              CharVector & out, ParmStr) const {
      ConvDirect::convert(in0, size, out);
      return no_err;
    }
  };

  //////////////////////////////////////////////////////////////////////
  //
  //  Lookup Conversion
  //

  struct DecodeLookup : public Decode 
  {
    ToUniLookup lookup;
    PosibErr<void> init(ParmStr code, const Config & c) {
      FromUniLookup unused;
      return read_in_char_data(c, code, lookup, unused);
    }
    void decode(const char * in, int size, FilterCharVector & out) const {
      if (size == -1) {
        for (;*in; ++in)
          out.append(lookup[*in]);
      } else {
        const char * stop = in + size;
        for (;in != stop; ++in)
          out.append(lookup[*in]);
      }
    }
    PosibErr<void> decode_ec(const char * in, int size, 
                             FilterCharVector & out, ParmStr) const {
      DecodeLookup::decode(in, size, out);
      return no_err;
    }
  };

  struct DecodeNormLookup : public Decode 
  {
    typedef ToUniNormEntry E;
    NormTable<E> * data;
    DecodeNormLookup(NormTable<E> * d) : data(d) {}
    // must be null terminated
    // FIXME: Why must it be null terminated?
    void decode(const char * in, int size, FilterCharVector & out) const {
      const char * stop = in + size; // will word even if size -1
      while (in != stop) {
        if (*in == 0) {
          if (size == -1) break;
          out.append(0);
          ++in;
        } else {
          NormLookupRet<E,char> ret =
            norm_lookup<E,char>(data, in, stop, 0, in);
          for (unsigned i = 0; ret.to[i] && i < E::max_to; ++i)
            out.append(ret.to[i]);
          in = ret.last + 1;
        }
      }
    }
    PosibErr<void> decode_ec(const char * in, int size, 
                             FilterCharVector & out, ParmStr) const {
      DecodeNormLookup::decode(in, size, out);
      return no_err;
    }
  };

  struct EncodeLookup : public Encode 
  {
    FromUniLookup lookup;
    PosibErr<void> init(ParmStr code, const Config & c) 
      {ToUniLookup unused;
      return read_in_char_data(c, code, unused, lookup);}
    void encode(const FilterChar * in, const FilterChar * stop, 
                CharVector & out) const {
      for (; in != stop; ++in) {
        out.append(lookup(*in));
      }
    }
    PosibErr<void> encode_ec(const FilterChar * in, const FilterChar * stop, 
                             CharVector & out, ParmStr orig) const {
      for (; in != stop; ++in) {
        char c = lookup(*in, '\0');
        if (c == '\0' && in->chr != 0) {
          char m[70];
          snprintf(m, 70, _("The Unicode code point U+%04X is unsupported."), in->chr);
          return make_err(invalid_string, orig, m);
        }
        out.append(c);
      }
      return no_err;
    }
    void encode(const FilterChar * in, const FilterChar * stop,
                FilterCharVector & out) const {
      for (; in != stop; ++in)
        out.append(lookup(*in));
    }
  };

  struct EncodeNormLookup : public Encode 
  {
    typedef FromUniNormEntry E;
    NormTable<E> * data;
    EncodeNormLookup(NormTable<E> * d) : data(d) {}
    // *stop must equal 0
    void encode(const FilterChar * in, const FilterChar * stop, 
                CharVector & out) const {
      while (in < stop) {
        if (*in == 0) {
          out.append('\0');
          ++in;
        } else {
          NormLookupRet<E,FilterChar> ret = norm_lookup<E,FilterChar>
            (data, in, stop, (const byte *)"?", in);
          for (unsigned i = 0; i < E::max_to && ret.to[i]; ++i)
            out.append(ret.to[i]);
          in = ret.last + 1;
        }
      }
    }
    PosibErr<void> encode_ec(const FilterChar * in, const FilterChar * stop, 
                             CharVector & out, ParmStr orig) const {
      while (in < stop) {
        if (*in == 0) {
          out.append('\0');
          ++in;
        } else {
          NormLookupRet<E,FilterChar> ret = 
            norm_lookup<E,FilterChar>(data, in, stop, 0, in);
          if (ret.to == 0) {
            char m[70];
            snprintf(m, 70, _("The Unicode code point U+%04X is unsupported."), in->chr);
            return make_err(invalid_string, orig, m);
          }
          for (unsigned i = 0; i < E::max_to && ret.to[i]; ++i)
            out.append(ret.to[i]);
          in = ret.last + 1;
        }
      }
      return no_err;
    }
    void encode(const FilterChar * in, const FilterChar * stop,
                FilterCharVector & out) const {
      while (in < stop) {
        if (*in == 0) {
          out.append(FilterChar(0));
          ++in;
        } else {
          NormLookupRet<E,FilterChar> ret = norm_lookup<E>(data, in, stop, (const byte *)"?", in);
          const FilterChar * end = ret.last + 1;
          unsigned width = 0;
          for (; in != end; ++in) width += in->width;
          out.append(FilterChar(ret.to[0], width));
          for (unsigned i = 1; i < E::max_to && ret.to[i]; ++i) {
            out.append(FilterChar(ret.to[i],0));
          }
        }
      }
    }
  };

  //////////////////////////////////////////////////////////////////////
  //
  //  UTF8
  //
  
#define get_check_next \
  if (in == stop) goto error;          \
  c = *in;                             \
  if ((c & 0xC0) != 0x80) goto error;  \
  ++in;                                \
  u <<= 6;                             \
  u |= c & 0x3F;                       \
  ++w;

  static inline FilterChar from_utf8 (const char * & in, const char * stop, 
                                      Uni32 err_char = '?')
  {
    Uni32 u = (Uni32)(-1);
    FilterChar::Width w = 1;

    // the first char is guaranteed not to be off the end
    char c = *in;
    ++in;

    while (in != stop && (c & 0xC0) == 0x80) {c = *in; ++in; ++w;}
    if ((c & 0x80) == 0x00) { // 1-byte wide
      u = c;
    } else if ((c & 0xE0) == 0xC0) { // 2-byte wide
      u  = c & 0x1F;
      get_check_next;
    } else if ((c & 0xF0) == 0xE0) { // 3-byte wide
      u  = c & 0x0F;
      get_check_next;
      get_check_next;
    } else if ((c & 0xF8) == 0xF0) { // 4-byte wide
      u  = c & 0x07;
      get_check_next;
      get_check_next;
      get_check_next;
    } else {
      goto error;
    }

    return FilterChar(u, w);
  error:
    return FilterChar(err_char, w);
  }

  static inline void to_utf8 (FilterChar in, CharVector & out)
  {
    FilterChar::Chr c = in;
    
    if (c < 0x80) {
      out.append(c);
    }
    else if (c < 0x800) {
      out.append(0xC0 | (c>>6));
      out.append(0x80 | (c & 0x3F));
    }
    else if (c < 0x10000) {
      out.append(0xE0 | (c>>12));
      out.append(0x80 | (c>>6 & 0x3F));
      out.append(0x80 | (c & 0x3F));
    }
    else if (c < 0x200000) {
      out.append(0xF0 | (c>>18));
      out.append(0x80 | (c>>12 & 0x3F));
      out.append(0x80 | (c>>6 & 0x3F));
      out.append(0x80 | (c & 0x3F));
    }
  }
  
  struct DecodeUtf8 : public Decode 
  {
    ToUniLookup lookup;
    void decode(const char * in, int size, FilterCharVector & out) const {
      const char * stop = in + size; // this is OK even if size == -1
      while (in != stop && *in) {
        out.append(from_utf8(in, stop));
      }
    }
    PosibErr<void> decode_ec(const char * in, int size, 
                             FilterCharVector & out, ParmStr orig) const {
      const char * begin = in;
      const char * stop = in + size; // this is OK even if size == -1
      while (in != stop && *in) {
        FilterChar c = from_utf8(in, stop, (Uni32)-1);
        if (c == (Uni32)-1) {
          char m[70];
          snprintf(m, 70, _("Invalid UTF-8 sequence at position %ld."), (long)(in - begin));
          return make_err(invalid_string, orig, m);
        }
        out.append(c);
      }
      return no_err;
    }
  };

  struct EncodeUtf8 : public Encode 
  {
    FromUniLookup lookup;
    void encode(const FilterChar * in, const FilterChar * stop, 
                CharVector & out) const {
      for (; in != stop; ++in) {
        to_utf8(*in, out);
      }
    }
    PosibErr<void> encode_ec(const FilterChar * in, const FilterChar * stop, 
                             CharVector & out, ParmStr) const {
      for (; in != stop; ++in) {
        to_utf8(*in, out);
      }
      return no_err;
    }
    void encode(const FilterChar * in, const FilterChar * stop, 
                FilterCharVector & out) const {
      abort();
    }
  };

  //////////////////////////////////////////////////////////////////////
  //
  // Cache
  //

  static GlobalCache<Decode> decode_cache("decode");
  static GlobalCache<Encode> encode_cache("encode");
  static GlobalCache<NormTables> norm_tables_cache("norm_tables");
  
  //////////////////////////////////////////////////////////////////////
  //
  // new_aspell_convert
  //

  void FullConvert::generic_convert(const char * in, int size, CharVector & out)
  {
    buf_.clear();
    decode(in, size, buf_);
    FilterChar * start = buf_.pbegin();
    FilterChar * stop = buf_.pend();
    filter(start, stop);
    encode(start, stop, out);
  }
  
  const char * fix_encoding_str(ParmStr enc, String & buf)
  {
    const char * i   = strrchr(enc, '.');
    const char * end = enc + enc.size();
    if (i) i++; 
    else   i = enc;
    buf.clear();
    buf.reserve(end - i);
    for (; i != end; ++i)
      buf.push_back(asc_tolower(*i));

    if (strncmp(buf.c_str(), "iso8859", 7) == 0)
      buf.insert(buf.begin() + 3, '-'); // For backwards compatibility

    if (buf == "ascii" || buf == "ansi_x3.4-1968")
      return "iso-8859-1";
    else if (buf == "utf8")
      return "utf-8";
    else if (buf == "machine unsigned 16" || buf == "utf-16")
      return "ucs-2";
    else if (buf == "machine unsigned 32" || buf == "utf-32")
      return "ucs-4";
    else
      return buf.c_str();
  }

  const char * resolve_alias(const Config & c, ParmStr enc, String & buf)
  {
    String dir1 ,dir2,file_name;
    fill_data_dir(&c, dir1, dir2);
    find_file(file_name,dir1,dir2,enc, ".calias");
    FStream f;
    PosibErrBase err = f.open(file_name, "r");
    if (err.get_err()) return enc.str();
    char * p = get_nb_line(f, buf);
    remove_comments(p);
    unescape(p);
    return p;
  }

  void get_base_enc(String & res, ParmStr enc)
  {
    size_t l = strcspn(enc, "|:/");
    res.assign(enc, l);
  }

  struct Encoding {
    String base;
    String norm_form; 
    Vector<String> layers; // such as "tex", "texinfo", etc
  };

  bool operator== (const Encoding & x, const Encoding & y)
  {
    if (x.base != y.base) return false;
    if (x.norm_form != y.norm_form) return false;
    if (x.layers != y.layers) return false;
    return true;
  }

  PosibErr<void> decode_encoding_string(const Config & c,
                                        ParmStr str, Encoding & enc)
  {
    String tmp;

    const char * slash = strchr(str, '/');
    const char * colon = strchr(str, ':');

    if (colon && (!slash || slash > colon)) {
      enc.base.assign(str, colon);
      if (!slash)
        enc.norm_form.assign(colon + 1);
      else
        enc.norm_form.assign(colon + 1, slash);
    } else if (slash) {
      enc.base.assign(str, slash);
    } else {
      enc.base.assign(str);
    }

    const char * s = fix_encoding_str(enc.base, tmp);
    enc.base = resolve_alias(c, s, tmp);
    
    const char * layers = 0;
    if (slash) layers = slash + 1;
       
    if (enc.norm_form.empty()) {
      if (c.retrieve_bool("normalize").data || c.retrieve_bool("norm-required").data)
        enc.norm_form = c.retrieve("norm-form");
      else
        enc.norm_form = "none";
    }
    if (enc.norm_form == "none" && c.retrieve_bool("norm-required").data)
      enc.norm_form = "nfc";

    // push "layers" encoding on extra list
    if (layers) {
      do {
        slash = strchr(layers,'/');
        if (slash) tmp.assign(layers, slash);
        else       tmp.assign(layers);
        enc.layers.push_back(resolve_alias(c, tmp.str(), tmp));
        layers = slash + 1;
      } while (slash);
    }
    return no_err;
  }

  bool ascii_encoding(const Config & c, ParmStr enc0)
  {
    if (enc0.empty()) return true;
    if (enc0 == "ANSI_X3.4-1968" 
        || enc0 == "ASCII" || enc0 == "ascii") return true;
    String buf;
    const char * enc = fix_encoding_str(enc0, buf);
    if (strcmp(enc, "utf-8") == 0 
        || strcmp(enc, "ucs-2") == 0 
        || strcmp(enc, "ucs-4") == 0) return false;
    String dir1,dir2,file_name;
    fill_data_dir(&c, dir1, dir2);
    file_name << dir1 << enc << ".cset";
    if (file_exists(file_name)) return false;
    if (dir1 == dir2) return true;
    file_name.clear();
    file_name << dir2 << enc << ".cset";
    return !file_exists(file_name);
  }

  static PosibErr<bool> add_conv_filters(const Config & c, FullConvert * & fc,
                                         Vector<String> & els,
                                         bool decoder, bool prev_err)
  {
    Vector<String>::const_iterator i;
    for (i = els.begin(); i != els.end(); ++i) 
    {
      const char * name0 = i->str();
      const char * colon = strchr(name0, ':');

      String name;
      name += "l-";
      name += c.retrieve("lang").data;
      name += "-";
      unsigned pre_len = name.size();
      if (colon) name.append(name0, colon);
      else       name.append(name0);

      GenConvFilterParms p(name);
      p.file = p.name;
      p.form = colon ? colon + 1 : "multi";
      StackPtr<IndividualFilter> f(new_convert_filter(decoder, p));
      PosibErr<bool> pe = f->setup((Config *)&c); // FIXME: Cast shold not
                                                  // be necessary
      if (pe.has_err(cant_read_file)) {
        name.erase(0, pre_len);
        p.name = p.file = name;
        f.del();
        f = new_convert_filter(decoder, p);
        pe = f->setup((Config *)&c);
      }

      if (prev_err && pe.has_err(cant_read_file)) {return false;}
      else if (pe.has_err()) return pe;
      if (fc == 0) return make_err(not_simple_encoding, name);

      if (pe.data) fc->add_filter(f.release());
    }
    return true;
  }

  PosibErr<Convert *> internal_new_convert(const Config & c,
                                           ParmString in_s, ParmString out_s,
                                           bool if_needed,
                                           Normalize norm,
                                           bool simple,
                                           Convert::InitRet prev_err)
  {
    Encoding in, out;
    RET_ON_ERR(decode_encoding_string(c, in_s,  in));
    RET_ON_ERR(decode_encoding_string(c, out_s, out));
    if      (in.base.empty())  in.base = out.base;
    else if (out.base.empty()) out.base = in.base;
    if (in.base.empty() /* implies out_e.base.empty() */)
      in.base = out.base = "iso-8859-1";

    if (if_needed && in == out) return 0;

    if (simple) {
      if (! in.layers.empty()) return make_err(not_simple_encoding, in_s );
      if (!out.layers.empty()) return make_err(not_simple_encoding, out_s);
    }

    StackPtr<Convert> conv;
    if (simple) conv = new SimpleConvert;
    else        conv = new FullConvert;

    Convert::InitRet ret;
    if (norm == NormNone) {
      ret = conv->init(c, in.base, out.base);
    } else if (norm == NormFrom) {
      if (in.norm_form == "none")
        ret = conv->init(c, in.base, out.base);
      else
        ret = conv->init_norm_from(c, in.base, out.base);
    } else if (norm == NormTo) {
      if (out.norm_form == "none")
        ret = conv->init(c, in.base, out.base);
      else
        ret = conv->init_norm_to(c, in.base, out.base, out.norm_form);
    }

    if (ret.error != Convert::NoError) 
    {
      if (ret.error == prev_err.error) {
        return 0;
      } else if (!prev_err.error && ret.error == Convert::UnknownDecoder)  {
        String tmp = "/";
        tmp += in_s;
        PosibErr<Convert *> pe = internal_new_convert(c, tmp, out_s, if_needed, norm, simple, ret);
        if (pe.has_err() || pe.data) {ret.error_obj.ignore_err(); return pe;}
        return ret.error_obj;
      } else if (!prev_err.error && ret.error == Convert::UnknownEncoder)  {
        String tmp = "/";
        tmp += out_s;
        PosibErr<Convert *> pe = internal_new_convert(c, in_s, tmp, if_needed, norm, simple, ret);
        if (pe.has_err() || pe.data) {ret.error_obj.ignore_err(); return pe;}
        return ret.error_obj;
      } else {
        return ret.error_obj;
      };
    }

    FullConvert * fc = (FullConvert *)(simple ? 0 : conv.get());
    
    {
      PosibErr<bool> pe = add_conv_filters(c, fc, in.layers, true,
                                           prev_err.error == Convert::UnknownDecoder);
      if (pe.has_err()) return (PosibErrBase &)pe;
      if (!pe.data) return 0;
    } {
      PosibErr<bool> pe = add_conv_filters(c, fc, out.layers, false,
                                           prev_err.error == Convert::UnknownEncoder);
      if (pe.has_err()) return (PosibErrBase &)pe;
      if (!pe.data) return 0;
    }
     
    //printf("%s => %s\n", conv->in_code(),  conv->out_code());

    return conv.release();
  }

  PosibErr<Decode *> Decode::get_new(const String & key, const Config * c)
  {
    StackPtr<Decode> ptr;
    if (key == "iso-8859-1")
      ptr.reset(new DecodeDirect<Uni8>);
    else if (key == "ucs-2")
      ptr.reset(new DecodeDirect<Uni16>);
    else if (key == "ucs-4")
      ptr.reset(new DecodeDirect<Uni32>);
    else if (key == "utf-8")
      ptr.reset(new DecodeUtf8);
    else
      ptr.reset(new DecodeLookup);
    RET_ON_ERR(ptr->init(key, *c));
    ptr->key = key;
    return ptr.release();
  }

  PosibErr<Encode *> Encode::get_new(const String & key, const Config * c)
  {
    StackPtr<Encode> ptr;
    if (key == "iso-8859-1")
      ptr.reset(new EncodeDirect<Uni8>);
    else if (key == "ucs-2")
      ptr.reset(new EncodeDirect<Uni16>);
    else if (key == "ucs-4")
      ptr.reset(new EncodeDirect<Uni32>);
    else if (key == "utf-8")
      ptr.reset(new EncodeUtf8);
    else
      ptr.reset(new EncodeLookup);
    RET_ON_ERR(ptr->init(key, *c));
    ptr->key = key;
    return ptr.release();
  }

  Convert::~Convert() {}

#define FAILED_decode  ret.error = UnknownDecoder
#define FAILED_encode  ret.error = UnknownEncoder
#define FAILED_neither ret.error = OtherError

#define INIT_RET_ON_ERR(what, command) \
  do {\
    PosibErrBase pe(command);\
    if (pe.has_err()) {\
      InitRet ret;\
      if (pe.prvw_err()->is_a(unknown_encoding)) FAILED_##what;\
      else                                       ret.error = OtherError;\
      ret.error_obj = PosibErrBase(pe);\
      return ret;\
    }\
  } while(false)\

#define INIT_NO_ERR InitRet();

  Convert::InitRet Convert::init(const Config & c, ParmStr in, ParmStr out)
  {
    INIT_RET_ON_ERR(decode, setup(decode_c, &decode_cache, &c, in));
    decode_ = decode_c.get();
    in_code_ = decode_->key;
    INIT_RET_ON_ERR(encode, setup(encode_c, &encode_cache, &c, out));
    encode_ = encode_c.get();
    out_code_ = encode_->key;

    conv_ = 0;
    if (in == out) {
      if (in == "ucs-2") {
        conv_ = new ConvDirect<Uni16>;
      } else if (in == "ucs-4") {
        conv_ = new ConvDirect<Uni32>;
      } else {
        conv_ = new ConvDirect<char>;
      }
    }

    if (conv_)
      INIT_RET_ON_ERR(neither, conv_->init(decode_, encode_, c));

    return INIT_NO_ERR;
  }

  
  Convert::InitRet Convert::init_norm_from(const Config & c, ParmStr in, ParmStr out)
  {
    INIT_RET_ON_ERR(encode, setup(norm_tables_, &norm_tables_cache, &c, out));

    INIT_RET_ON_ERR(decode, setup(decode_c, &decode_cache, &c, in));
    decode_ = decode_c.get();
    in_code_ = decode_->key;

    if (c.retrieve_bool("norm-strict")) {
      encode_s = new EncodeNormLookup(norm_tables_->strict);
      encode_ = encode_s;
      encode_->key = out;
      out_code_ = out;
      in_code_ += "|strict";
    } else {
      encode_s = new EncodeNormLookup(norm_tables_->internal);
      encode_ = encode_s;
      encode_->key = out;
      out_code_ = out;
      in_code_ += "|norm";
    }
    conv_ = 0;

    return INIT_NO_ERR;
  }

  Convert::InitRet Convert::init_norm_to(const Config & c, ParmStr in, ParmStr out,
                                      ParmStr norm_form)
  {
    INIT_RET_ON_ERR(decode, setup(norm_tables_, &norm_tables_cache, &c, in));

    INIT_RET_ON_ERR(encode, setup(encode_c, &encode_cache, &c, out));
    encode_ = encode_c.get();
    out_code_ = encode_->key;

    NormTables::ToUni::const_iterator i = norm_tables_->to_uni.begin();
    for (; i != norm_tables_->to_uni.end() && i->name != norm_form; ++i);
    assert(i != norm_tables_->to_uni.end());

    decode_s = new DecodeNormLookup(i->ptr);
    decode_ = decode_s;
    decode_->key = in;
    in_code_ = decode_->key;
    out_code_ += ':';
    out_code_ += i->name;

    conv_ = 0;

    return INIT_NO_ERR;
  }

  PosibErr<void> MBLen::setup(const Config &, ParmStr enc0)
  {
    String buf;
    const char * enc = fix_encoding_str(enc0,buf);
    if      (strcmp(enc, "utf-8") == 0) encoding = UTF8;
    else if (strcmp(enc, "ucs-2") == 0) encoding = UCS2;
    else if (strcmp(enc, "ucs-4") == 0) encoding = UCS4;
    else                                encoding = Other;
    return no_err;
  }

  unsigned MBLen::operator()(const char * str, const char * stop)
  {
    unsigned size = 0;
    switch (encoding) {
    case Other: 
      return stop - str;
    case UTF8:
      for (; str != stop; ++str) {
        if ((*str & 0x80) == 0 || (*str & 0xC0) == 0xC0) ++size;
      }
      return size;
    case UCS2:
      return (stop - str)/2;
    case UCS4:
      return (stop - str)/4;
    }
    return 0;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // error checking utility function
  //
  PosibErrBase sanity_fail(const char * file, const char * func, 
                           unsigned line, const char * check_str) 
  {
    char mesg[500];
    snprintf(mesg, 500, "%s:%d: %s: Assertion \"%s\" failed.",
             file,  line, func, check_str);
    return make_err(bad_input_error, mesg);
  }
}
