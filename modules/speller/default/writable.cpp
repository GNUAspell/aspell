// This file is part of The New Aspell
// Copyright (C) 2000,2011 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include <time.h>

#include "file_util.hpp"
#include "hash-t.hpp"
#include "data.hpp"
#include "data_util.hpp"
#include "enumeration.hpp"
#include "errors.hpp"
#include "fstream.hpp"
#include "lang_impl.hpp"
#include "getdata.hpp"

namespace {

//////////////////////////////////////////////////////////////////////
//
// WritableBase
//

using namespace std;
using namespace aspell::sp;
using namespace aspell;

typedef const char * Str;
typedef unsigned char byte;

struct Hash {
  InsensitiveHash<> f;
  Hash(const LangImpl * l) : f(l) {}
  size_t operator() (Str s) const {
    return f(s);
  }
};

struct Equal {
  InsensitiveEqual f;
  Equal(const LangImpl * l) : f(l) {}
  bool operator() (Str a, Str b) const {
    return f(a, b);
  }
};

//
// Store word data.
//  The word_ is variable length and must be last
//
struct WordRec 
{
 byte word_info_;
 byte size_; //The size of word_
 char word_[1];
 const char * key() const { return word_; }
 const WordRec * word_rec() const { return this; }
} ;

// This class stores word data in WordRec objects.
typedef Vector<WordRec *> WordVec;

//
// Store spelling replacement, a misspelled word
// and a list of correctly spelled replacments.
// WordRec is variable length and must be last.
//
struct WordReplRec 
{
  WordVec correct;
  WordRec misspelled; // this must be the last element in the structure
  const char * key() const { return misspelled.word_; }
  const WordRec * word_rec() const { return &misspelled; }
};

//
// Hash table params for a writable dictionary word record pointer
//
template <typename HashValue>
struct LookupParams
{
  typedef HashValue Value;
  typedef const char * Key;

  enum { is_multi = 1 };
  LookupParams(const Hash & h, const Equal & e) : hash(h), equal(e) {}
  Hash  hash;
  Equal equal;
  Str key (const HashValue & v) const { return v->key(); }
};

void write_n_escape(FStream & o, const char * str) {
  while (*str != '\0') {
    if (*str == '\n') o << "\\n";
    else if (*str == '\r') o << "\\r";
    else if (*str == '\\') o << "\\\\";
    else o << *str;
    ++str;
  }
}

static inline char f_getc(FStream & in) {
  int c = in.get();
  return c == EOF ? '\0' : (char)c;
}
  
bool getline_n_unescape(FStream & in, String & str, char delem) {
  str.clear();
  char c = f_getc(in);
  if (!c) return false;
  while (c && c != delem) {
    if (c == '\\') {
      c = f_getc(in);
      if (c == 'n') str.append('\n');
      else if (c == 'r') str.append('\r');
      else if (c == '\\') str.append('\\');
      else {str.append('\\'); continue;}
    } else {
      str.append(c);
    }
    c = f_getc(in);
  }
  return true;
}

bool getline_n_unescape(FStream & in, DataPair & d, String & buf)
{
  if (!getline_n_unescape(in, buf, '\n')) return false;
  d.value.str  = buf.mstr();
  d.value.size = buf.size();
  return true;
}

class WritableBase : public Dictionary {
protected:
  String suffix;
  String compatibility_suffix;
    
  time_t cur_file_date;
  
  String compatibility_file_name;
    
  WritableBase(BasicType t, const char * n, const char * s, const char * cs)
    : Dictionary(t,n),
      suffix(s), compatibility_suffix(cs),
      personal_no_hint(false), personal_sort(false),
      use_soundslike(true) {fast_lookup = true;}
  virtual ~WritableBase() {}
  
  virtual PosibErr<void> save(FStream &, ParmString) = 0;
  virtual PosibErr<void> merge(FStream &, ParmString, Config * = 0) = 0;
    
  PosibErr<void> save2(FStream &, ParmString);
  PosibErr<void> update(FStream &, ParmString);
  PosibErr<void> save(bool do_update);
  PosibErr<void> update_file_date_info(FStream &);
  PosibErr<void> load(ParmString, Config &, DictList *, SpellerImpl *);
  PosibErr<void> merge(ParmString);
  PosibErr<void> save_as(ParmString);

  String file_encoding;
  ConvObj iconv;
  ConvObj oconv;
  PosibErr<void> set_file_encoding(ParmString, Config & c);

  PosibErr<void> synchronize() {return save(true);}
  PosibErr<void> save_noupdate() {return save(false);}

  template <typename InputIterator>
  void save_words(FStream&, InputIterator, InputIterator);

  bool personal_no_hint;
  bool personal_sort;

  bool use_soundslike;
  ObjStack             buffer;
 
};

PosibErr<void> WritableBase::update_file_date_info(FStream & f) {
  RET_ON_ERR(update_file_info(f));
  cur_file_date = get_modification_time(f);
  return no_err;
}
  
PosibErr<void> WritableBase::load(ParmString f0, Config & config,
                                  DictList *, SpellerImpl *)
{
  set_file_name(f0);
  const String f = file_name();
  FStream in;

  personal_no_hint = config.retrieve_bool("personal-no-hint");
  personal_sort = config.retrieve_bool("personal-sort");

  if (file_exists(f)) {
      
    RET_ON_ERR(open_file_readlock(in, f));
    if (in.peek() == EOF) return make_err(cant_read_file,f); 
    // ^^ FIXME 
    RET_ON_ERR(merge(in, f, &config));
      
  } else if (f.substr(f.size()-suffix.size(),suffix.size()) 
             == suffix) {
      
    compatibility_file_name = f.substr(0,f.size() - suffix.size());
    compatibility_file_name += compatibility_suffix;
      
    {
      PosibErr<void> pe = open_file_readlock(in, compatibility_file_name);
      if (pe.has_err()) {compatibility_file_name = ""; return pe;}
    } {
      PosibErr<void> pe = merge(in, compatibility_file_name, &config);
      if (pe.has_err()) {compatibility_file_name = ""; return pe;}
    }
      
  } else {
      
    return make_err(cant_read_file,f);
      
  }

  return update_file_date_info(in);
}

PosibErr<void> WritableBase::merge(ParmString f0) {
  FStream in;
  Dict::FileName fn(f0);
  RET_ON_ERR(open_file_readlock(in, fn.path));
  RET_ON_ERR(merge(in, fn.path));
  return no_err;
}

PosibErr<void> WritableBase::update(FStream & in, ParmString fn) {
  typedef PosibErr<void> Ret;
  {
    Ret pe = merge(in, fn);
    if (pe.has_err() && compatibility_file_name.empty()) return pe;
  } {
    Ret pe = update_file_date_info(in);
    if (pe.has_err() && compatibility_file_name.empty()) return pe;
  }
  return no_err;
}
    
PosibErr<void> WritableBase::save2(FStream & out, ParmString fn) {
  truncate_file(out, fn);
      
  RET_ON_ERR(save(out,fn));

  out.flush();

  return no_err;
}

PosibErr<void> WritableBase::save_as(ParmString fn) {
  compatibility_file_name = "";
  set_file_name(fn);
  FStream inout;
  RET_ON_ERR(open_file_writelock(inout, file_name()));
  RET_ON_ERR(save2(inout, file_name()));
  RET_ON_ERR(update_file_date_info(inout));
  return no_err;
}

PosibErr<void> WritableBase::save(bool do_update) {
  FStream inout;
  RET_ON_ERR_SET(open_file_writelock(inout, file_name()),
                 bool, prev_existed);

  if (do_update
      && prev_existed 
      && get_modification_time(inout) > cur_file_date)
    RET_ON_ERR(update(inout, file_name()));

  RET_ON_ERR(save2(inout, file_name()));
  RET_ON_ERR(update_file_date_info(inout));
    
  if (compatibility_file_name.size() != 0) {
    remove_file(compatibility_file_name.c_str());
    compatibility_file_name = "";
  }

  return no_err;
}

PosibErr<void> WritableBase::set_file_encoding(ParmString enc, Config & c)
{
  if (enc == file_encoding) return no_err;
  if (enc == "") enc = lang()->charmap();
  RET_ON_ERR(iconv.setup(c, enc, lang()->charmap(), NormFrom));
  RET_ON_ERR(oconv.setup(c, lang()->charmap(), enc, NormTo));
  if (iconv || oconv) 
    file_encoding = enc;
  else
    file_encoding = "";
  return no_err;
}


/////////////////////////////////////////////////////////////////////
// 
//  Common Stuff
//

// Store WordRec data in a WordEntry object.
static inline void set_word(WordEntry & res, const WordRec * w)
{
  res.word      = w->word_;
  res.word_size = w->size_;
  res.word_info = w->word_info_;
  res.aff       = "";
}

// Store soundslike data in a WordEntry object.
static inline void set_sl(WordEntry & res, Str w)
{
  res.word      = w;
  res.word_size = strlen(w);
}

// An enumeration of soundslike objects.
// It passes each soundslike to the caller as a WordEntry object.
template<typename Value>
struct SoundslikeElements : public SoundslikeEnumeration {

  typedef typename Value::const_iterator Itr;

  Itr i;
  Itr end;

  WordEntry d;

  SoundslikeElements(Itr i0, Itr end0) : i(i0), end(end0) {
    d.what = WordEntry::Soundslike;
  }

  // Step to the next soundlike record
  // Store in intr[0] the SoundslikeLookup value field which is a
  // Vector of WordRec or a Vector of WordReplRec.
  WordEntry * next(int) {
    if (i == end) return 0;
    set_sl(d, i->first);
    d.intr[0] = (void *)(&i->second);
    ++i;
    return &d;
  }
};

// An enumeration of word objects.
// It passes each word to the caller as a WordEntry object.
template<typename Value>
struct CleanElements : public SoundslikeEnumeration {

  typedef typename Value::const_iterator Itr;

  Itr i;
  Itr end;

  WordEntry d;

  CleanElements(Itr i0, Itr end0) : i(i0), end(end0) {
    d.what = WordEntry::Word;
  }

  WordEntry * next(int) {
    if (i == end) return 0;
    set_word(d, (*i)->word_rec());
    ++i;
    return &d;
  }
};

//
template <typename Lookup>
struct ElementsParms {
  typedef WordEntry *                Value;
  typedef typename Lookup::const_iterator Iterator;
  Iterator end_;
  WordEntry data;
  ElementsParms(Iterator e) : end_(e) {}
  bool endf(Iterator i) const {return i==end_;}
  Value deref(Iterator i) {set_word(data, (*i)->word_rec()); return &data;}
  static Value end_state() {return 0;}
};

static void soundslike_next(WordEntry * w)
{
  const WordRec *const * &i  = (const WordRec *const *&)(w->intr[0]);
  const WordRec *const * end = (const WordRec *const *) (w->intr[1]);
  set_word(*w, (*i)->word_rec());
  ++i;
  if (i == end) w->adv_ = 0;
}

static void sl_init( WordRec *const * i,
						   WordRec *const * end, WordEntry & o)
{
  set_word(o, (*i)->word_rec());
  ++i;
  if (i != end) {
    o.intr[0] = (void *)i;
    o.intr[1] = (void *)end;
    o.adv_ = soundslike_next;
  } else {
    o.intr[0] = 0;
  }
}
static void soundslike_next_repl(WordEntry * w)
{
  const WordReplRec *const * &i  = (const WordReplRec *const *&)(w->intr[0]);
  const WordReplRec *const * end = (const WordReplRec *const *) (w->intr[1]);
  set_word(*w, (*i)->word_rec());
  ++i;
  if (i == end) w->adv_ = 0;
}

static void sl_init( WordReplRec *const * i,
						   WordReplRec *const * end, WordEntry & o)
{
  set_word(o, (*i)->word_rec());
  ++i;
  if (i != end) {
    o.intr[0] = (void *)i;
    o.intr[1] = (void *)end;
    o.adv_ = soundslike_next_repl;
  } else {
    o.intr[0] = 0;
  }
}
/////////////////////////////////////////////////////////////////////
// 
//  WritableDict
//

class WritableDict : public WritableBase
{
public:
  // The key is the soundslike string, the value is the vector of words
  typedef WordVec LookupVec;
  typedef hash_map<Str,LookupVec> SoundslikeLookup;
  typedef HashTable<LookupParams<WordRec *> > WordLookup;

public: // but don't use
  PosibErr<void> save(FStream &, ParmString);
  PosibErr<void> merge(FStream &, ParmString, Config * config);

public:

  WritableDict() : WritableBase(basic_dict, "WritableDict", ".pws", ".per") {}

  Size   size()     const;
  bool   empty()    const;
  PosibErr<void> clear();
  
  PosibErr<void> add(ParmString w) {return Dictionary::add(w);}
  PosibErr<void> add(ParmString w, ParmString s);

  bool lookup(ParmString word, const SensitiveCompare *, WordEntry &) const;

  bool clean_lookup(const char * sondslike, WordEntry &) const;

  bool soundslike_lookup(const WordEntry & soundslike, WordEntry &) const;
  bool soundslike_lookup(ParmString soundslike, WordEntry &) const;

  WordEntryEnumeration * detailed_elements() const;

  SoundslikeEnumeration * soundslike_elements() const;
  void set_lang_hook(Config & c) {
    set_file_encoding(lang()->data_encoding(), c);
    word_lookup.reset(new WordLookup(10,LookupParams<WordRec *>(
		 Hash(lang()), Equal(lang()))));
    use_soundslike = lang()->have_soundslike();
  }
protected:
  StackPtr<WordLookup> word_lookup;
  SoundslikeLookup     soundslike_lookup_;

};

WritableDict::Size WritableDict::size() const 
{
  return word_lookup->size();
}

bool WritableDict::empty() const 
{
  return word_lookup->empty();
}

PosibErr<void> WritableDict::clear() 
{
  word_lookup->clear();
  soundslike_lookup_.clear();
  buffer.reset();
  return no_err;
}

bool WritableDict::lookup(ParmString word, const SensitiveCompare * c,
                          WordEntry & o) const
{
  o.clear();
  pair<WordLookup::iterator, WordLookup::iterator> p(word_lookup->equal_range(word));
  while (p.first != p.second) {
    WordRec * w= *p.first;
    if ((*c)(word,w->key())) {
      o.what = WordEntry::Word;
      set_word(o, w);
      return true;
    }
    ++p.first;
  }
  return false;
}

bool WritableDict::clean_lookup(const char * sl, WordEntry & o) const
{
  o.clear();
  pair<WordLookup::iterator, WordLookup::iterator> p(word_lookup->equal_range(sl));
  if (p.first == p.second) return false; // empty
  o.what = WordEntry::Word;
  WordRec *rec = *p.first;
  set_word(o, rec);
  return true;
  // FIXME: Deal with multiple entries
}  

bool WritableDict::soundslike_lookup(const WordEntry & word, WordEntry & o) const 
{
  if (use_soundslike) {

    const LookupVec * tmp 
      = (const LookupVec *)(word.intr[0]);
    o.clear();

    o.what = WordEntry::Word;
    sl_init(tmp->pbegin(),tmp->pend(), o);

  } else {
      
    o.what = WordEntry::Word;
    o.word = word.word;
    o.word_size = word.word_size;
    o.word_info = word.word_info;
    o.aff  = "";
    
  }
  return true;
}

bool WritableDict::soundslike_lookup(ParmString word, WordEntry & o) const 
{
  if (use_soundslike) {

    o.clear();
    SoundslikeLookup::const_iterator i = soundslike_lookup_.find(word);
    if (i == soundslike_lookup_.end()) {
      return false;
    } else {
      o.what = WordEntry::Word;
      LookupVec *v = &(i->second);
      sl_init(v->pbegin(), v->pend(), o);
      return true;
    }
  
  } else {

    return WritableDict::clean_lookup(word, o);

  }
}

SoundslikeEnumeration * WritableDict::soundslike_elements() const
{
  if (use_soundslike)
    return new SoundslikeElements<SoundslikeLookup>(soundslike_lookup_.begin(), 
                                                    soundslike_lookup_.end());
  else
    return new CleanElements<WordLookup>(word_lookup->begin(),
                                         word_lookup->end());
}

WritableDict::Enum * WritableDict::detailed_elements() const
{
  typedef ElementsParms<WordLookup> WordElements;
  return new MakeEnumeration<WordElements>
    (word_lookup->begin(),WordElements(word_lookup->end()));
}

//
// Add a word and soundlike to the dictionary.
// we allocate a buffers with these implied structure:
//
PosibErr<void> WritableDict::add(ParmString w, ParmString s)
{
  RET_ON_ERR(check_if_valid(*lang(),w));
  SensitiveCompare c(lang());
  WordEntry we;
  if (WritableDict::lookup(w,&c,we)) return no_err;
  WordRec *rec = static_cast<WordRec *>(buffer.alloc(sizeof(WordRec) + w.size()));
  rec->word_info_ = lang()->get_word_info(w);
  rec->size_ = w.size();
  memcpy(rec->word_, w.str(), w.size() + 1);
  word_lookup->insert(rec);
  if (use_soundslike) {
    char *soundslike = static_cast<char *>(buffer.alloc(s.size() +1));
    memcpy(soundslike,s.str(), s.size() + 1);
    soundslike_lookup_[soundslike].push_back(rec);
  }
  return no_err;
}

PosibErr<void> WritableDict::merge(FStream & in, 
                                   ParmString file_name, 
                                   Config * config)
{
  typedef PosibErr<void> Ret;
  unsigned int ver;

  String buf;
  DataPair dp;

  if (!getline(in, dp, buf))
    make_err(bad_file_format, file_name);
  
  split(dp);
  if (dp.key == "personal_wl")
    ver = 10;
  else if (dp.key == "personal_ws-1.1")
    ver = 11;
  else 
    return make_err(bad_file_format, file_name);
  
  split(dp);
  {
    Ret pe = set_check_lang(dp.key, *config);
    if (pe.has_err())
      return pe.with_file(file_name);
  }

  split(dp); // count not used at the moment

  split(dp);
  if (dp.key.size > 0)
    set_file_encoding(dp.key, *config);
  else
    set_file_encoding("", *config);
  
  ConvP conv(iconv);
  while (getline_n_unescape(in, dp, buf)) {
    if (ver == 10)
      split(dp);
    else
      dp.key = dp.value;
    Ret pe = add(conv(dp.key));
    if (pe.has_err()) {
      clear(); //fixme: the output error does not indicate which key was not supported.
      return pe.with_file(file_name);
    }
  }
  return no_err;
}

template <typename InputIterator>
inline void WritableBase::save_words(FStream& out, InputIterator i, InputIterator e)
{
  ConvP conv(oconv);
  for (;i != e; ++i) {
    write_n_escape(out, conv((*i)->key()));
    out << '\n';
  }
}

// return true if r1 < r2
inline bool compare_word_rec(WordRec const* r1, WordRec const* r2)
{
  return strcmp(r1->key(), r2->key()) < 0;
}

PosibErr<void> WritableDict::save(FStream & out, ParmString file_name)
{
  int size = personal_no_hint ? 0 : word_lookup->size();

  out.printf("personal_ws-1.1 %s %i %s\n",
             lang_name(), size, file_encoding.c_str());

  if (personal_sort) {
    // WordVec sorted_words(word_lookup->begin(), word_lookup->end());
    // WordVec doesn't support the iterator copy constructor
    WordVec sorted_words;
    sorted_words.assign(word_lookup->begin(), word_lookup->end());
    // NOTE: std::sort is likely an overkill here
    std::sort(sorted_words.begin(), sorted_words.end(), compare_word_rec);
    save_words(out, sorted_words.begin(), sorted_words.end());
  } else {
    save_words(out, word_lookup->begin(), word_lookup->end());
  }

  return no_err;
}

/////////////////////////////////////////////////////////////////////
// 
//  WritableReplList
//

class WritableReplDict : public WritableBase
{
public:
  //The key is the soundslike string, the value is the vector of words
  typedef Vector<WordReplRec *> LookupVec;
  typedef hash_map<Str,LookupVec> SoundslikeLookup;
  typedef HashTable<LookupParams<WordReplRec *> > WordLookup;

  WritableReplDict(const WritableReplDict&);
  WritableReplDict& operator=(const WritableReplDict&);

public:
  WritableReplDict() : WritableBase(replacement_dict, "WritableReplDict", ".prepl",".rpl") 
  {
    fast_lookup = true;
  }
  ~WritableReplDict();

  Size   size()     const;
  bool   empty()    const;
  PosibErr<void> clear();

  bool lookup(ParmString word, const SensitiveCompare * c,
                              WordEntry & o, WordVec *&vec) const;

  bool clean_lookup(ParmString sondslike, WordEntry &) const;

  bool soundslike_lookup(const WordEntry &, WordEntry &) const;
  bool soundslike_lookup(ParmString, WordEntry &) const;

  bool repl_lookup(const WordEntry &, WordEntry &) const;
  bool repl_lookup(ParmString, WordEntry &) const;
      
  WordEntryEnumeration * detailed_elements() const;
  SoundslikeEnumeration * soundslike_elements() const;
      
  PosibErr<void> add_repl(ParmString mis, ParmString cor) {
    return Dictionary::add_repl(mis,cor);}
  PosibErr<void> add_repl(ParmString mis, ParmString cor, ParmString s);

  void set_lang_hook(Config & c) {
    set_file_encoding(lang()->data_encoding(), c);
    word_lookup.reset(new WordLookup(10,LookupParams<WordReplRec *>( Hash(lang()), Equal(lang()))));
    use_soundslike = lang()->have_soundslike();
  }
private:
  PosibErr<void> save(FStream &, ParmString );
  PosibErr<void> merge(FStream &, ParmString , Config * config);
  StackPtr<WordLookup>   word_lookup;
  SoundslikeLookup soundslike_lookup_;
};

WritableReplDict::Size WritableReplDict::size() const 
{
  return word_lookup->size();
}

bool WritableReplDict::empty() const 
{
  return word_lookup->empty();
}

PosibErr<void> WritableReplDict::clear() 
{
  word_lookup->clear();
  soundslike_lookup_.clear();
  buffer.reset();
  return no_err;
}
    
//
// Lookup param word in the hash table
// For each record found,
//   if we have the right word
//    Set the output word entry to:
//      what = Misspelled
//      word = word
//      intr[0] = WordLookup record.
// Notice that only the last records data is stored.
// We could loop through the records and then store the values we want.
//
bool WritableReplDict::lookup(ParmString word, const SensitiveCompare * c,
                              WordEntry & o, WordVec *&vec) const
{
  o.clear();
  pair<WordLookup::iterator, WordLookup::iterator> p(word_lookup->equal_range(word));
  while (p.first != p.second) {
    const WordRec *w = (*p.first)->word_rec();
    if ((*c)(word,w->key())) {
      o.what = WordEntry::Misspelled;
      set_word(o, w);
      WordReplRec *repl = *p.first;
      vec = &(repl->correct);
      o.intr[0] = (void *)*p.first;
      return true;
    }
    ++p.first;
  }
  return false;
}

bool WritableReplDict::clean_lookup(ParmString sl, WordEntry & o) const
{
  o.clear();
  pair<WordLookup::iterator, WordLookup::iterator> p(word_lookup->equal_range(sl));
  if (p.first == p.second) return false;
  o.what = WordEntry::Misspelled;
  WordReplRec *repl = *p.first;
  set_word(o, repl->word_rec());
  o.intr[0] = (void *)repl;
  return true;
  // FIXME: Deal with multiple entries
}  

bool WritableReplDict::soundslike_lookup(const WordEntry & word, WordEntry & o) const 
{
  if (use_soundslike) {
    const LookupVec * tmp = (const LookupVec *)(word.intr[0]);
    o.clear();
    o.what = WordEntry::Misspelled;
    sl_init(tmp->pbegin(),tmp->pend(), o);
  } else {
    o.what = WordEntry::Misspelled;
    o.word = word.word;
    o.word_size = word.word_size;
    o.aff = "";
  }
  return true;
}

bool WritableReplDict::soundslike_lookup(ParmString soundslike, WordEntry & o) const
{
  if (use_soundslike) {
    o.clear();
    SoundslikeLookup::const_iterator i = soundslike_lookup_.find(soundslike);
    if (i == soundslike_lookup_.end()) {
      return false;
    } else {
      o.what = WordEntry::Misspelled;
      LookupVec *v = &(i->second);
      sl_init(v->pbegin(),v->pend(), o);
      return true;
    }
  } else {
    return WritableReplDict::clean_lookup(soundslike, o);
  }
}

//
// Return begin, end of all soundlike records.
//
SoundslikeEnumeration * WritableReplDict::soundslike_elements() const
{
  if (use_soundslike)
    return new SoundslikeElements<SoundslikeLookup>(soundslike_lookup_.begin(), 
                                                    soundslike_lookup_.end());
  else
    return new CleanElements<WordLookup>(word_lookup->begin(),
                                         word_lookup->end());
}

WritableReplDict::Enum * WritableReplDict::detailed_elements() const {
  typedef ElementsParms<WordLookup> WordElements;
  return new MakeEnumeration<WordElements>
    (word_lookup->begin(),WordElements(word_lookup->end()));
}

static void repl_next(WordEntry * w)
{
  const WordRec *const *& i  = (const WordRec *const *&)(w->intr[0]);
  const WordRec *const * end = (const WordRec *const *) (w->intr[1]);
  set_word(*w, *i);
  ++i;
  if (i == end) w->adv_ = 0;
}

static void repl_init(const WordRec *const * i,
							 const WordRec *const * end, WordEntry & o)
{
  o.what = WordEntry::Word;
  set_word(o, *i);
  ++i;
  if (i != end) {
    o.intr[0] = (void *)i;
    o.intr[1] = (void *)end;
    o.adv_ = repl_next;
  } else {
    o.intr[0] = 0;
  }
}
  
bool WritableReplDict::repl_lookup(const WordEntry & w, WordEntry & o) const 
{
  WordVec * repls;
  if (w.intr[0] && !w.intr[1]) { // the intr are not for the sl iter
    WordReplRec * r = static_cast<WordReplRec *>(w.intr[0]);
    repls = &(r->correct);
    if (!repls) return false;
  } else {
    SensitiveCompare c(lang()); // FIXME: This is not exactly right
    WordEntry tmp;
    WritableReplDict::lookup(w.word, &c, tmp, repls);
    if (!repls) return false;
  }
  o.clear();
  repl_init(repls->pbegin(),repls->pend(), o);
  return true;
}

bool WritableReplDict::repl_lookup(ParmString word, WordEntry & o) const 
{
  WordEntry w;
  w.word = word;
  return WritableReplDict::repl_lookup(w, o);
}

//
// Add a replacement pair (misspelled, correction) and a soundslike.
// we allocate a buffers with these implied structure:
//
PosibErr<void> WritableReplDict::add_repl(ParmString mis, ParmString cor, ParmString sl) 
{
  SensitiveCompare cmp(lang()); // FIXME: I don't think this is completely correct
  WordEntry we;

  pair<WordLookup::iterator, WordLookup::iterator> p0(word_lookup->equal_range(mis));
  WordLookup::iterator p = p0.first;

  for (; p != p0.second && !cmp(mis,(*p)->key()); ++p);


  WordReplRec *repl;
  if (p == p0.second) {
    // mis not found, make a WordReplRec and insert it.
    void *buff = buffer.alloc(sizeof(WordReplRec) + mis.size());
    repl = new (buff) WordReplRec;
    repl->misspelled.word_info_ = lang()->get_word_info(mis);
    repl->misspelled.size_ = mis.size();
    memcpy(repl->misspelled.word_, mis.str(), mis.size() + 1);
    word_lookup->insert(repl);
  } else {
    repl = *p;
  }
  WordVec & v = repl->correct;

  for (WordVec::iterator i = v.begin(); i != v.end(); ++i)
    if (cmp(cor, (*i)->key())) return no_err; // found
  WordRec * rec = static_cast<WordRec *>(buffer.alloc(sizeof(WordRec) + cor.size()));
  rec->word_info_ = lang()->get_word_info(cor);
  rec->size_ = cor.size();
  memcpy(rec->word_, cor.str(), cor.size() + 1);
  v.push_back(rec);

  if (use_soundslike) {
    // Allocate space for the soundslike string, save the Word/Replace record
    char * soundslike = static_cast<char *>(buffer.alloc(sl.size() +1));
    memcpy(soundslike, sl.str(), sl.size() + 1);
    soundslike_lookup_[soundslike].push_back(repl);
  }

  return no_err;
}

PosibErr<void> WritableReplDict::save (FStream & out, ParmString file_name) 
{
  out.printf("personal_repl-1.1 %s 0 %s\n", lang_name(), file_encoding.c_str());
  
  WordLookup::iterator i = word_lookup->begin();
  WordLookup::iterator e = word_lookup->end();

  ConvP conv1(oconv);
  ConvP conv2(oconv);
  
  for (;i != e; ++i) 
  {
    WordReplRec *repl = (*i);
    WordVec & v = repl->correct;
    for (WordVec::iterator j = v.begin(); j != v.end(); ++j)
    {
      write_n_escape(out, conv1(repl->key()));
      out << ' ';
      write_n_escape(out, conv2((*j)->key()));
      out << '\n';
    }
  }
  return no_err;
}

PosibErr<void> WritableReplDict::merge(FStream & in,
                                       ParmString file_name, 
                                       Config * config)
{
  typedef PosibErr<void> Ret;
  unsigned int version;
  unsigned int num_words, num_repls;

  String buf;
  DataPair dp;

  if (!getline(in, dp, buf))
    make_err(bad_file_format, file_name);

  split(dp);
  if (dp.key == "personal_repl")
    version = 10;
  else if (dp.key == "personal_repl-1.1") 
    version = 11;
  else
    return make_err(bad_file_format, file_name);

  split(dp);
  {
    Ret pe = set_check_lang(dp.key, *config);
    if (pe.has_err())
      return pe.with_file(file_name);
  }

  unsigned int num_soundslikes = 0;
  if (version == 10) {
    split(dp);
    num_soundslikes = atoi(dp.key);
  }

  split(dp); // not used at the moment

  split(dp);
  if (dp.key.size > 0)
    set_file_encoding(dp.key, *config);
  else
    set_file_encoding("", *config);

  if (version == 11) {

    ConvP conv1(iconv);
    ConvP conv2(iconv);
    for (;;) {
      bool res = getline_n_unescape(in, buf, '\n');
      if (!res) break;
      char * mis = buf.mstr();
      char * repl = strchr(mis, ' ');
      if (!repl) continue; // bad line, ignore
      *repl = '\0'; // split string
      ++repl;
      if (!repl[0]) continue; // empty repl, ignore
      WritableReplDict::add_repl(conv1(mis), conv2(repl));
    }
    
  } else {
    
    String mis, sound, repl;
    unsigned int h,i,j;
    for (h=0; h != num_soundslikes; ++h) {
      in >> sound >> num_words;
      for (i = 0; i != num_words; ++i) {
        in >> mis >> num_repls;
        in.ignore(); // ignore space
        for (j = 0; j != num_repls; ++j) {
          in.getline(repl, ',');
          WritableReplDict::add_repl(mis, repl);
        }
      }
    }

  }
  return no_err;
}

WritableReplDict::~WritableReplDict()
{
  WordLookup::iterator i = word_lookup->begin();
  WordLookup::iterator e = word_lookup->end();
  
  for (;i != e; ++i) {
    WordReplRec *repl = (*i);
    if (repl)
      repl->~WordReplRec();
  }
}

}

namespace aspell { namespace sp {

  Dictionary * new_default_writable_dict() {
    return new WritableDict();
  }

  Dictionary * new_default_replacement_dict() {
    return new WritableReplDict();
  }

} }
