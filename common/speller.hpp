// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

// Aspell implementation header file.
// Applications that just use the Aspell library should not include 
// these files as they are subject to change.
// Aspell Modules MUST include some of the implementation files and
// spell checkers MAY include some of these files.
// If ANY of the implementation files are included you also link with
// libaspell-impl to protect you from changes in these files.

#ifndef ASPELL_SPELLER__HPP
#define ASPELL_SPELLER__HPP

#include "can_have_error.hpp"
#include "copy_ptr.hpp"
#include "clone_ptr.hpp"
#include "mutable_string.hpp"
#include "posib_err.hpp"
#include "parm_string.hpp"
#include "char_vector.hpp"
#include "check_info.hpp"

namespace aspell {

  typedef void * SpellerLtHandle;

  class Config;
  class WordList;
  class FullConvert;
  class Filter;
  class DocumentChecker;
  class Checker;

  struct IntrCheckInfo {
    mutable CheckInfo ext; // Stuff that is used by the C interface
    mutable String str;    // buffer for use by the above
    const IntrCheckInfo * next;
    ParmString word; // generally the root

    const char * pre_add;
    const char * inner_suf_add;
    const char * outer_suf_add;

    void get_suf(char * buf) const; 
    // puts combined suffix in buf, len must be at least suf_add_len,
    // doesn't null terminate

    short pre_strip_len;
    short pre_add_len;

    short suf_strip_len;
    short suf_add_len;

    short pre_flag;
    short suf_flag;
    short guess;
    short compound;

    short inner_suf_strip_len;
    short inner_suf_add_len;

    short outer_suf_strip_len;
    short outer_suf_add_len;

    void clear();
  };

  class Speller : public CanHaveError
  {
  private:
    SpellerLtHandle lt_handle_;
    Speller(const Speller &);
    Speller & operator= (const Speller &);
  public:
    String temp_str_0;
    String temp_str_1;
    ClonePtr<FullConvert> to_internal_;
    ClonePtr<FullConvert> from_internal_;
  protected:
    CopyPtr<Config> config_;
    Speller(SpellerLtHandle h);
  public:
    SpellerLtHandle lt_handle() const {return lt_handle_;}

    Config * config() {return config_;}
    const Config * config() const {return config_;}

    // utility functions

    virtual char * to_lower(char *) = 0;

    // the setup class will take over for config
    virtual PosibErr<void> setup(Config *) = 0;

    virtual Checker * new_checker() = 0;

    ////////////////////////////////////////////////////////////////
    // 
    // Strings from this point on are expected to be in the 
    // encoding specified by config->retrieve("encoding")
    //

    virtual PosibErr<bool> check(MutableString) = 0;

    // this function return information about the last word checked
    // The "ext" part of the struct is not filled in.  For that
    // use check_info()
    virtual const IntrCheckInfo * intr_check_info() = 0;

    virtual PosibErr<void> add_to_personal(MutableString) = 0;
    virtual PosibErr<void> add_to_session (MutableString) = 0;

    PosibErr<void> add_lower_to_personal(MutableString str) {
      return add_to_personal(to_lower(str));
    }
    
    PosibErr<void> add_lower_to_session(MutableString str) {
      return add_to_session(to_lower(str));
    }
    
    // because the word lists may potently have to convert from one
    // encoding to another the pointer returned by the enumeration is only
    // valid to the next call.

    virtual PosibErr<const WordList *> personal_word_list() const = 0;
    virtual PosibErr<const WordList *> session_word_list () const = 0;
    virtual PosibErr<const WordList *> main_word_list () const = 0;
  
    virtual PosibErr<void> save_all_word_lists() = 0;
  
    virtual PosibErr<void> clear_session() = 0;

    virtual PosibErr<const WordList *> suggest(MutableString) = 0;
    // return null on error
    // the word list returned by suggest is only valid until the next
    // call to suggest
  
    virtual PosibErr<void> store_replacement(MutableString, 
					     MutableString) = 0;

    virtual ~Speller();

    // Reload the conversion filters.  Bit of a hack, I hope to find a
    // better way
    virtual PosibErr<void> reload_conv() = 0;

  };


  // This function is current a hack to reload the filters in the
  // speller class.  I hope to eventually find a better way.
  PosibErr<void> reload_filters(Speller * m);


  PosibErr<Speller *> new_speller(Config * c);

}

#endif
