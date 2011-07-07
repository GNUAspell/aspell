
#include "file_util.hpp"
#include "fstream.hpp"
#include "config.hpp"
#include "data.hpp"
#include "getdata.hpp"
#include "string.hpp"
#include "parm_string.hpp"
#include "errors.hpp"
#include "vector.hpp"

#include "gettext.h"

namespace {

  using namespace aspell;
  using namespace aspell::sp;

  typedef Vector<Dict *> Wss;

  class MultiDictImpl : public Dictionary
  {
  public:
    MultiDictImpl() : Dictionary(multi_dict, "MultiDictImpl") {}
    PosibErr<void> load(ParmString, Config &, DictList *, SpellerImpl *);
    DictsEnumeration * dictionaries() const;

    StringEnumeration * elements() const;
    Enum * detailed_elements() const;
    Size   size()     const;
    bool   empty()    const;
    
  private:
    Wss wss;
  };

  PosibErr<void> MultiDictImpl::load(ParmString fn, 
                                     Config & config, 
                                     DictList * new_dicts,
                                     SpellerImpl * speller)
  {
    String dir = figure_out_dir("",fn);
    FStream in;
    RET_ON_ERR(in.open(fn, "r"));
    set_file_name(fn);
    String buf; DataPair d;
    while(getdata_pair(in, d, buf)) 
    {
      if (d.key == "add") {

        RET_ON_ERR_SET(add_data_set(d.value, config, new_dicts, speller, dir), Dict *, res);
        RET_ON_ERR(set_check_lang(res->lang()->name(), config));
	wss.push_back(res);

      } else {
	
	return make_err(unknown_key, d.key).with_file(fn, d.line_num);

      }
    }

    if (wss.empty()) {
      return make_err(bad_file_format, fn, 
                      _("There must be at least one \"add\" line."));
    }

    return no_err;
  }

  struct Parms
  {
    typedef Dict * Value;
    typedef Wss::const_iterator Iterator;
    Iterator end;
    Parms(Iterator e) : end(e) {}
    bool endf(Iterator i)   const {return i == end;}
    Value end_state()       const {return 0;}
    Value deref(Iterator i) const {return *i;}
  };

  DictsEnumeration * MultiDictImpl::dictionaries() const
  {
    return new MakeEnumeration<Parms>(wss.begin(), wss.end());
  }

  template <class Parms>
  class EnumImpl : public Parms::Enum {
    Wss::const_iterator i;
    Wss::const_iterator end;
    ClonePtr<typename Parms::Enum> els;
  public:
    typedef typename Parms::Enum        Base;
    typedef typename Parms::Enum::Value Value;

    EnumImpl(Wss::const_iterator i0, Wss::const_iterator end0)
      : i(i0), end(end0) {if (i != end) els.reset(Parms::elements(*i));}
    
    bool at_end() const {
      return i == end;
    }
    Value next() {
      if (i == end) return 0;
    loop:
      Value str = els->next();
      if (str) return str;
      ++i; 
      if (i == end) return 0; 
      els.reset(Parms::elements(*i));
      goto loop;
    }
    Base * clone() const {
      return new EnumImpl(*this);
    }
    void assign(const Base * other) {
      *this = *static_cast<const EnumImpl *>(other);
    }
  };

  struct EnumParms {
    typedef StringEnumeration Enum;
    static StringEnumeration * elements(const Dict * d) {return d->elements();}
  };

  StringEnumeration * MultiDictImpl::elements() const
  {
    return new EnumImpl<EnumParms>(wss.begin(), wss.end());
  }

  struct DetailedEnumParms {
    typedef MultiDictImpl::Enum Enum;
    static MultiDictImpl::Enum * elements(const Dict * d) {return d->detailed_elements();}
  };

  MultiDictImpl::Enum * MultiDictImpl::detailed_elements() const
  {
    return new EnumImpl<DetailedEnumParms>(wss.begin(), wss.end());
  }

  MultiDictImpl::Size MultiDictImpl::size() const
  {
    Size s = 0;
    Wss::const_iterator i = wss.begin(), end = wss.end();
    for (; i != end; ++i)
      s += (*i)->size();
    return s;
  }

  bool MultiDictImpl::empty() const
  {
    bool emp = true;
    Wss::const_iterator i = wss.begin(), end = wss.end();
    for (; i != end; ++i)
      emp = emp && (*i)->empty();
    return emp;
  }

}

namespace aspell { namespace sp {

  MultiDict * new_default_multi_dict() 
  {
    return new MultiDictImpl();
  }

} }
