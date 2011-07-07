// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_CONVERT__HPP
#define ASPELL_CONVERT__HPP

#include "string.hpp"
#include "posib_err.hpp"
#include "char_vector.hpp"
#include "filter_char.hpp"
#include "filter_char_vector.hpp"
#include "stack_ptr.hpp"
#include "filter.hpp"
#include "cache.hpp"

namespace aspell {

  class OStream;
  class Config;

  struct ConvBase : public Cacheable {
    typedef const Config CacheConfig;
    typedef const char * CacheKey;
    String key;
    bool cache_key_eq(const char * l) const  {return key == l;}
    ConvBase() {}
  private:
    ConvBase(const ConvBase &);
    void operator=(const ConvBase &);
  };

  struct Decode : public ConvBase {
    virtual PosibErr<void> init(ParmStr code, const Config &) {return no_err;}
    virtual void decode(const char * in, int size,
			FilterCharVector & out) const = 0;
    virtual PosibErr<void> decode_ec(const char * in, int size,
                                     FilterCharVector & out, ParmStr orig) const = 0;
    static PosibErr<Decode *> get_new(const String &, const Config *);
    virtual ~Decode() {}
  };
  struct Encode : public ConvBase {
    // null characters should be treated like any other character
    // by the encoder.
    virtual PosibErr<void> init(ParmStr, const Config &) {return no_err;}
    virtual void encode(const FilterChar * in, const FilterChar * stop, 
                        CharVector & out) const = 0;
    virtual PosibErr<void> encode_ec(const FilterChar * in, const FilterChar * stop, 
                                     CharVector & out, ParmStr orig) const = 0;
    virtual void encode(const FilterChar * in, const FilterChar * stop, 
                        FilterCharVector & buf) const = 0;

    static PosibErr<Encode *> get_new(const String &, const Config *);
    virtual ~Encode() {}
  };
  struct DirectConv { // convert directly from in_code to out_code.
    // should not take ownership of decode and encode.
    // decode and encode guaranteed to stick around for the life
    // of the object.
    virtual PosibErr<void> init(const Decode *, const Encode *, 
				const Config &) {return no_err;}
    virtual void convert(const char * in, int size, 
			 CharVector & out) const = 0;
    virtual PosibErr<void> convert_ec(const char * in, int size, 
                                      CharVector & out, ParmStr orig) const = 0;
    virtual ~DirectConv() {}
  };
  template <class T> struct NormTable;
  struct FromUniNormEntry;
  struct ToUniNormEntry;
  struct NormTables : public Cacheable {
    typedef const Config CacheConfig;
    typedef const char * CacheKey;
    String key;
    bool cache_key_eq(const char * l) const  {return key == l;}
    static PosibErr<NormTables *> get_new(const String &, const Config *);
    NormTable<FromUniNormEntry> * internal;
    NormTable<FromUniNormEntry> * strict_d;
    NormTable<FromUniNormEntry> * strict;
    struct ToUniTable {
      String name;
      NormTable<ToUniNormEntry> * data;
      NormTable<ToUniNormEntry> * ptr;
      ToUniTable() : data(0), ptr(0) {}
    };
    typedef Vector<ToUniTable> ToUni;
    Vector<ToUniTable> to_uni;
    ~NormTables();
  };

  typedef FilterCharVector ConvertBuffer;
  struct Encoding;

  class Convert {
  protected:
    String in_code_;
    CachePtr<Decode> decode_c;
    StackPtr<Decode> decode_s;
    Decode * decode_;
    String out_code_;
    CachePtr<Encode> encode_c;
    StackPtr<Encode> encode_s;
    Encode * encode_;
    CachePtr<NormTables> norm_tables_;
    StackPtr<DirectConv> conv_;

    static const unsigned int null_len_ = 4; // POSIB FIXME: Be more precise

    Convert(const Convert &);
    void operator=(const Convert &);

  public:
    Convert() {}
    virtual ~Convert();

    enum InitError {NoError = 0, UnknownDecoder, UnknownEncoder, OtherError};

    struct InitRet {
      InitError error;
      PosibErr<void> error_obj;
      InitRet() : error(NoError) {}
    };

    InitRet init(const Config &, ParmStr in, ParmStr out);
    InitRet init_norm_to(const Config &, ParmStr in, ParmStr out, 
                         ParmStr norm_form);
    InitRet init_norm_from(const Config &, ParmStr in, ParmStr out);

    const char * in_code()  const {return in_code_.str();}
    const char * out_code() const {return out_code_.str();}

    void append_null(CharVector & out) const
    {
      const char nul[4] = {0,0,0,0}; // 4 should be enough
      out.write(nul, null_len_);
    }

    unsigned int null_len() const {return null_len_;}
  
    // these filters will generally not translate null characters
    // if you need a null character at the end, add it yourself
    // with append_null

    void decode(const char * in, int size, FilterCharVector & out) const 
      {decode_->decode(in,size,out);}
    
    void encode(const FilterChar * in, const FilterChar * stop, 
		CharVector & out) const
      {encode_->encode(in,stop,out);}

    void encode(const FilterChar * in, const FilterChar * stop, 
                FilterCharVector & buf) const
      {encode_->encode(in,stop,buf);}

    // convert has the potential to use internal buffers and
    // is therefore not const.  It is also not thread safe
    // and I have no intention to make it thus.

    virtual void convert(const char * in, int size, CharVector & out) = 0;

  protected:

    // does NOT pass it through filters
    // DOES NOT use an internal state
    void simple_convert(const char * in, int size, CharVector & out, ConvertBuffer & buf) const
    {
      if (conv_) {
	conv_->convert(in,size,out);
      } else {
        buf.clear();
        decode_->decode(in, size, buf);
        encode_->encode(buf.pbegin(), buf.pend(), out);
      }
    }

    // does NOT pass it through filters
    // DOES NOT use an internal state
    PosibErr<void> simple_convert_ec(const char * in, int size, CharVector & out, 
                                     ConvertBuffer & buf, ParmStr orig) const
    {
      if (conv_) {
	RET_ON_ERR(conv_->convert_ec(in,size,out, orig));
      } else {
        buf.clear();
        RET_ON_ERR(decode_->decode_ec(in, size, buf, orig));
        RET_ON_ERR(encode_->encode_ec(buf.pbegin(), buf.pend(), 
                                      out, orig));
      }
      return no_err;
    }
  };

  class SimpleConvert : public Convert
  {
  public:
    // does NOT pass it through filters
    // DOES NOT use an internal state
    void convert(const char * in, int size, CharVector & out, ConvertBuffer & buf) const
    {
      simple_convert(in, size, out, buf);
    }

    // does NOT pass it through filters
    // DOES NOT use an internal state
    PosibErr<void> convert_ec(const char * in, int size, CharVector & out, 
                              ConvertBuffer & buf, ParmStr orig) const
    {
      return simple_convert_ec(in, size, out, buf, orig);
    }

    void convert(const char * in, int size, CharVector & out) 
    {
      simple_convert(in, size, out, buf_);
    }
  private:
    ConvertBuffer buf_;    
  };

  class FullConvert : public Convert
  {
  private:

    ConvertBuffer buf_;

    // This filter is used when the convert method is called.  It must
    // be set up by an external entity as this class does not set up
    // this class in any way.
    Filter filter_;

    void add_filter_codes();

  public:
    
    PosibErr<void> add_filters(Config * c, 
                               bool use_encoder, 
                               bool use_filter, 
                               bool use_decoder) 
    {
      RET_ON_ERR(setup_filter(filter_, c, use_encoder, use_filter, use_decoder));
      add_filter_codes();
      return no_err;
    }
    void add_filter(IndividualFilter * f) {
      filter_.add_filter(f);
      add_filter_codes();
    }

    Filter * shallow_copy_filter() {return filter_.shallow_copy();}
      
    // convert has the potential to use internal buffers and
    // is therefore not const.  It is also not thread safe
    // and I have no intention to make it thus.

    void convert(const char * in, int size, CharVector & out) {
      if (filter_.empty()) {
        simple_convert(in,size,out,buf_);
      } else {
        generic_convert(in,size,out);
      }
    }

    void generic_convert(const char * in, int size, CharVector & out);

    void filter(FilterChar * & start, FilterChar * & stop)
      {if (!filter_.empty()) filter_.process(start, stop);}

  };

  //bool operator== (const Convert & rhs, const Convert & lhs);

  // Enc MAY NOT be part of buf
  const char * fix_encoding_str(ParmStr enc, String & buf);

  // Follows any alias files
  // currently only follows a single level
  // it is ok if enc and buf are part of the same string.
  const char * resolve_alias(const Config & c, ParmStr enc, String & buf);

  void get_base_enc(String & res, ParmStr enc);

  // also returns true if the encoding is unknown
  bool ascii_encoding(const Config & c, ParmStr enc0);

  enum Normalize {NormNone, NormFrom, NormTo};

  PosibErr<Convert *> internal_new_convert(const Config & c, 
                                           ParmString in, ParmString out,
                                           bool if_needed,
                                           Normalize n,
                                           bool simple,
                                           Convert::InitRet = Convert::InitRet());
  
  static inline PosibErr<SimpleConvert *> 
  new_simple_convert(const Config & c, ParmStr in, ParmStr out, Normalize n)
  {
    PosibErr<Convert *> pe = internal_new_convert(c,in,out,false,n,true);
    if (pe.has_err()) return (PosibErrBase &)pe;
    return (SimpleConvert *)pe.data;
  }
  
  static inline PosibErr<SimpleConvert *> 
  new_simple_convert_if_needed(const Config & c, ParmStr in, ParmStr out, Normalize n)
  {
    PosibErr<Convert *> pe = internal_new_convert(c,in,out,true,n,true);
    if (pe.has_err()) return (PosibErrBase &)pe;
    return (SimpleConvert *)pe.data;
  }

  static inline PosibErr<FullConvert *> 
  new_full_convert(const Config & c, ParmStr in, ParmStr out, Normalize n)
  {
    PosibErr<Convert *> pe = internal_new_convert(c,in,out,false,n,false);
    if (pe.has_err()) return (PosibErrBase &)pe;
    return (FullConvert *)pe.data;
  }
  
  static inline PosibErr<FullConvert *> 
  new_full_convert_if_needed(const Config & c, ParmStr in, ParmStr out, Normalize n)
  {
    PosibErr<Convert *> pe = internal_new_convert(c,in,out,true,n,false);
    if (pe.has_err()) return (PosibErrBase &)pe;
    return (FullConvert *)pe.data;
  }

  struct ConvObj {
    SimpleConvert * ptr;
    ConvObj(SimpleConvert * c = 0) : ptr(c) {}
    ~ConvObj() {delete ptr;}
    PosibErr<void> setup(const Config & c, ParmStr from, ParmStr to, Normalize norm)
    {
      delete ptr;
      ptr = 0;
      PosibErr<SimpleConvert *> pe = new_simple_convert_if_needed(c, from, to, norm);
      if (pe.has_err()) return pe;
      ptr = pe.data;
      return no_err;
    }
    operator const SimpleConvert * () const {return ptr;}
  private:
    ConvObj(const ConvObj &);
    void operator=(const ConvObj &);
  };

  struct ConvP {
    const SimpleConvert * conv;
    ConvertBuffer buf0;
    CharVector buf;
    operator bool() const {return conv;}
    ConvP(const SimpleConvert * c = 0) : conv(c) {}
    ConvP(const ConvObj & c) : conv(c.ptr) {}
    ConvP(const ConvP & c) : conv(c.conv) {}
    void operator=(const ConvP & c) { conv = c.conv; }
    PosibErr<void> setup(const Config & c, ParmStr from, ParmStr to, 
                         Normalize norm)
    {
      delete conv;
      conv = 0;
      PosibErr<SimpleConvert *> pe = new_simple_convert_if_needed(c, from, to, norm);
      if (pe.has_err()) return pe;
      conv = pe.data;
      return no_err;
    }
    char * operator() (char * str, size_t sz)
    {
      if (conv) {
        buf.clear();
        conv->convert(str, sz, buf, buf0);
        return buf.mstr();
      } else {
        return str;
      }
    }
    const char * operator() (const char * str, size_t sz)
    {
      if (conv) {
        buf.clear();
        conv->convert(str, sz, buf, buf0);
        return buf.str();
      } else {
        return str;
      }
    }
    char * operator() (MutableString str)
    {
      return operator()(str.str, str.size);
    }
    char * operator() (char * str)
    {
      if (conv) {
        buf.clear();
        conv->convert(str, -1, buf, buf0);
        return buf.mstr();
      } else {
        return str;
      }
    }
    const char * operator() (ParmStr str)
    {
      if (conv) {
        buf.clear();
        conv->convert(str, -1, buf, buf0);
        return buf.mstr();
      } else {
        return str;
      }
    }
    char * operator() (char c)
    {
      buf.clear();
      if (conv) {
        char str[2] = {c, 0};
        conv->convert(str, 1, buf, buf0);
      } else {
        buf.append(c);
      }
      return buf.mstr();
    }
  };

  struct Conv : public ConvP
  {
    ConvObj conv_obj;
    Conv(SimpleConvert * c = 0) : ConvP(c), conv_obj(c) {}
    PosibErr<void> setup(const Config & c, ParmStr from, ParmStr to, Normalize norm)
    {
      RET_ON_ERR(conv_obj.setup(c,from,to,norm));
      conv = conv_obj.ptr;
      return no_err;
    }
  };

  struct ConvECP {
    const SimpleConvert * conv;
    ConvertBuffer buf0;
    CharVector buf;
    operator bool() const {return conv;}
    ConvECP(const SimpleConvert * c = 0) : conv(c) {}
    ConvECP(const ConvObj & c) : conv(c.ptr) {}
    ConvECP(const ConvECP & c) : conv(c.conv) {}
    void operator=(const ConvECP & c) { conv = c.conv; }
    PosibErr<void> setup(const Config & c, ParmStr from, ParmStr to, Normalize norm)
    {
      delete conv;
      conv = 0;
      PosibErr<SimpleConvert *> pe = new_simple_convert_if_needed(c, from, to, norm);
      if (pe.has_err()) return pe;
      conv = pe.data;
      return no_err;
    }
    PosibErr<char *> operator() (char * str, size_t sz)
    {
      if (conv) {
        buf.clear();
        RET_ON_ERR(conv->convert_ec(str, sz, buf, buf0, str));
        return buf.mstr();
      } else {
        return str;
      }
    }
    PosibErr<char *> operator() (MutableString str)
    {
      return operator()(str.str, str.size);
    }
    PosibErr<char *> operator() (char * str)
    {
      if (conv) {
        buf.clear();
        RET_ON_ERR(conv->convert_ec(str, -1, buf, buf0, str));
        return buf.mstr();
      } else {
        return str;
      }
    }

    PosibErr<const char *> operator() (ParmStr str)
    {
      if (conv) {
        buf.clear();
        RET_ON_ERR(conv->convert_ec(str, -1, buf, buf0, str));
        return buf.mstr();
      } else {
        return str.str();
      }
    }
    PosibErr<const char *> operator() (char c)
    {
      char buf2[2] = {c, 0};
      return operator()(ParmString(buf2,1));
    }
  };

  struct ConvEC : public ConvECP
  {
    ConvObj conv_obj;
    ConvEC(SimpleConvert * c = 0) : ConvECP(c), conv_obj(c) {}
    PosibErr<void> setup(const Config & c, ParmStr from, ParmStr to, Normalize norm)
    {
      RET_ON_ERR(conv_obj.setup(c,from,to,norm));
      conv = conv_obj.ptr;
      return no_err;
    }
  };

  // DOES NOT take ownership of the conversion filter
  struct FullConv {
    FullConvert * conv;
    CharVector buf;
    operator bool() const {return conv;}
    FullConv(FullConvert * c = 0) : conv(c) {}
  private:
    FullConv(const FullConv & c) : conv(c.conv) {}
    void operator=(const FullConv & c) { conv = c.conv; }
  public:
    void reset(FullConvert * c = 0) {conv = c;}
    char * operator() (char * str, size_t sz)
    {
      if (conv) {
        buf.clear();
        conv->convert(str, sz, buf);
        return buf.mstr();
      } else {
        return str;
      }
    }
    const char * operator() (const char * str, size_t sz)
    {
      if (conv) {
        buf.clear();
        conv->convert(str, sz, buf);
        return buf.str();
      } else {
        return str;
      }
    }
    char * operator() (MutableString str)
    {
      return operator()(str.str, str.size);
    }
    char * operator() (char * str)
    {
      if (conv) {
        buf.clear();
        conv->convert(str, -1, buf);
        return buf.mstr();
      } else {
        return str;
      }
    }
    const char * operator() (ParmStr str)
    {
      if (conv) {
        buf.clear();
        conv->convert(str, -1, buf);
        return buf.mstr();
      } else {
        return str;
      }
    }
    char * operator() (char c)
    {
      buf.clear();
      if (conv) {
        char str[2] = {c, 0};
        conv->convert(str, 1, buf);
      } else {
        buf.append(c);
      }
      return buf.mstr();
    }
  };

  struct MBLen 
  {
    enum Encoding {Other, UTF8, UCS2, UCS4} encoding;
    MBLen() : encoding(Other) {}
    PosibErr<void> setup(const Config &, ParmStr enc);
    unsigned operator()(const char * str, const char * stop);
    unsigned operator()(const char * str, unsigned byte_size) {
      return operator()(str, str + byte_size);}
  };

}

#endif
