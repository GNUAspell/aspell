// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_INFO__HPP
#define ASPELL_INFO__HPP

#include "info_types.hpp"
#include "posib_err.hpp"

namespace aspell {

  typedef int c_boolean;

  class Config;
  struct DictInfo;
  class DictInfoEnumeration;
  class DictInfoList;
  struct ModuleInfo;
  class ModuleInfoEnumeration;
  class ModuleInfoList;
  class StringList;
  struct StringListImpl;
  class FStream;
  class StringMap;

  PosibErr<void> get_dict_file_name(const DictInfo *, 
				    String & main_wl, String & flags);

  struct MDInfoListAll;
  
  struct ModuleInfoNode;
  
  class ModuleInfoList {
  public:
    ModuleInfoList() : size_(0), head_(0) {}
    void clear();
    PosibErr<void> fill(MDInfoListAll &, Config *);
    bool empty() const;
    unsigned int size() const;
    ModuleInfoEnumeration * elements() const;
    PosibErr<void> proc_info(MDInfoListAll &,
			     Config *,
			     const char * name,
			     unsigned int name_size,
			     IStream &);
    ModuleInfoNode * find(const char * to_find, 
			  unsigned int to_find_len);
  public: // but don't use
    unsigned int size_;
    ModuleInfoNode * head_;
  };
  
  const ModuleInfoList * get_module_info_list(Config *);

  struct DictInfoNode;

  class DictInfoList {
  public:
    DictInfoList() : size_(0), head_(0) {}
    void clear();
    PosibErr<void> fill(MDInfoListAll &, Config *);
    bool empty() const;
    unsigned int size() const;
    DictInfoEnumeration * elements() const;
    PosibErr<void> proc_file(MDInfoListAll &,
			     Config *,
			     const char * dir,
			     const char * name,
			     unsigned int name_size,
			     const ModuleInfo *);
  public: // but don't use
    unsigned int size_;
    DictInfoNode * head_;
  };

  const DictInfoList * get_dict_info_list(Config *);

  const StringMap * get_dict_aliases(Config *);

  class ModuleInfoEnumeration {
  public:
    typedef const ModuleInfo * Value;

    const ModuleInfoNode * node_;
    ModuleInfoEnumeration(const ModuleInfoNode * n) : node_(n) {}

    bool at_end() const;
    const ModuleInfo * next();
    ModuleInfoEnumeration * clone() const;
    void assign(const ModuleInfoEnumeration * other);
    ModuleInfoEnumeration() : node_(0) {}
    virtual ~ModuleInfoEnumeration() {}
  };

  struct DictInfoNode;

  class DictInfoEnumeration {
  public:
    const DictInfoNode * node_;
    DictInfoEnumeration(const DictInfoNode * n) : node_(n) {}

    typedef const DictInfo * Value;

    bool at_end() const;
    const DictInfo * next();
    DictInfoEnumeration * clone() const;
    void assign(const DictInfoEnumeration * other);
    DictInfoEnumeration() : node_(0) {}
    virtual ~DictInfoEnumeration() {}
  };




}

#endif /* ASPELL_INFO__HPP */
