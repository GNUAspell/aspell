
#include "language.hpp"
#include "language-c.hpp"
#include "hash-t.hpp"

namespace {

using namespace aspell;

//
// munch list (complete version)
//
//
// This version will produce a smaller list than the simple version.
// It is very close to the optimum result. 
//

/* TODO:
 *
 * Have some sort in incremental mode which will get good results
 * without having to completely expand the word list first.
 * 
 * The idea is to have two word lists, the first one is the previously
 * compressed list, and the second one is the list of words to run
 * munch-list on.  First check that words in the second list or not
 * already covered by the first list.  If not than ... to be
 * determined.  Will go something like this, when checking for legal
 * root words and expansions check both list.  If any of the root
 * words or expansions are in the first list put them in the same
 * disjoint set of the original word, but don't expand entries from the
 * first list yet...
 *
 */

//
// Hash table to store the words
//

struct CML_Entry {
  const char * word;
  char * aff;
  CML_Entry * parent;
  CML_Entry * next;
  int rank;
  CML_Entry(const char * w = 0) : word(w), aff(0), parent(0), next(0), rank(0) {}
};

struct CML_Parms {
  typedef CML_Entry Value;
  typedef const char * Key;
  static const bool is_multi = true;
  aspell::hash<const char *> hash;
  bool equal(Key x, Key y) {return strcmp(x,y) == 0;}
  Key key(const Value & v) {return v.word;}
};

typedef HashTable<CML_Parms> CML_Table;

//
// add an affix to a word but keep the prefixes and suffixes separate
//

static void add_affix(CML_Table::iterator b, char aff, bool prefix)
{
  char * p = b->aff;
  int s = 3;
  if (p) {
    while (*p) {
      if (*p == aff) return; 
      ++p;
    }
    s = (p - b->aff) + 2;
  }
  char * tmp = (char *)malloc(s);
  p = b->aff;
  char * q = tmp;
  if (p) {while (*p != '/') *q++ = *p++;}
  if (prefix) *q++ = aff;
  *q++ = '/';
  if (p) {p++; while (*p != '\0') *q++ = *p++;}
  if (!prefix) *q++ = aff;
  *q++ = '\0';
  assert(q - tmp == s);
  if (b->aff) free(b->aff);
  b->aff = tmp;
}

//
// Standard disjoint set algo with union by rank and path compression
//

static void link(CML_Entry * x, CML_Entry * y)
{
  if (x == y) return;
  if (x->rank > y->rank) {
    y->parent = x;
  } else {
    x->parent = y;
    if (x->rank == y->rank) y->rank++;
  }
}

static CML_Entry * find_set (CML_Entry * x) 
{
  if (x->parent)
    return x->parent = find_set(x->parent);
  else
    return x;
}

//
// Stuff to manage prefix-suffix combinations
//

struct PreSuf {
  String pre;
  String suf;
  String & get(int i) {return i == 0 ? pre : suf;}
  const String & get(int i) const {return i == 0 ? pre : suf;}
  PreSuf() : next(0) {}
  PreSuf * next;
};

class PreSufList {
public:
  PreSuf * head;
  PreSufList() : head(0) {}
  void add(PreSuf * to_add) {
    to_add->next = head;
    head = to_add;
  }
  void clear() {
    while (head) {
      PreSuf * tmp = head;
      head = head->next;
      delete tmp;
    }
  }
  void transfer(PreSufList & other) {
    clear();
    head = other.head;
    other.head = 0;
  }
  ~PreSufList() {
    clear();
  }
};


// Example of usage:
//   combine(in, res, 0)
//   Pre:  in =  [(ab, c) (ab, d) (c, de) (c, ef)]
//   Post: res = [(ab, cd), (c, def)]
static void combine(const PreSufList & in, PreSufList & res, int which)
{
  const PreSuf * i = in.head;
  while (i) { {
    const String & s = i->get(which);
    for (const PreSuf * j = in.head; j != i; j = j->next) {
      if (j->get(which) == s) goto cont;
    }
    PreSuf * tmp = new PreSuf;
    tmp->pre = i->pre;
    tmp->suf = i->suf;
    String & b = tmp->get(!which);
    for (const PreSuf * j = i->next; j; j = j->next) {
      if (j->get(which) != s) continue;
      const String & a = j->get(!which);
      for (String::const_iterator x = a.begin(); x != a.end(); ++x) {
        if (memchr(b.data(), *x, b.size())) continue;
        b += *x;
      }
    }
    res.add(tmp);
  } cont:
    i = i->next;
  }
}

//
// Stuff used when pruning the list of base words
//

struct Expansion {
  const char * word;
  char * aff; // modifying this will modify the affix entry in the hash table
  std::vector<bool> exp;
  std::vector<bool> orig_exp;
};

// static void dump(const Vector<Expansion *> & working, 
//                  const Vector<CML_Table::iterator> & entries)
// {
//   for (unsigned i = 0; i != working.size(); ++i) {
//     if (!working[i]) continue;
//     CERR.printf("%s/%s ", working[i]->word, working[i]->aff);
//     for (unsigned j = 0; j != working[i]->exp.size(); ++j) {
//       if (working[i]->exp[j])
//         CERR.printf("%s ", entries[j]->word);
//     }
//     CERR.put('\n');
//   }
//   CERR.put('\n');
// }

// standard set algorithms on a bit vector

static bool subset(const std::vector<bool> & smaller, 
                   const std::vector<bool> & larger)
{
  assert(smaller.size() == larger.size());
  unsigned s = larger.size();
  for (unsigned i = 0; i != s; ++i) {
    if (smaller[i] && !larger[i]) return false;
  }
  return true;
}

static void merge(std::vector<bool> & x, const std::vector<bool> & y)
{
  assert(x.size() == y.size());
  unsigned s = x.size();
  for (unsigned i = 0; i != s; ++i) {
    if (y[i]) x[i] = true;
  }
}

static void purge(std::vector<bool> & x, const std::vector<bool> & y)
{
  assert(x.size() == y.size());
  unsigned s = x.size();
  for (unsigned i = 0; i != s; ++i) {
    if (y[i]) x[i] = false;
  }
}

static inline unsigned count(const std::vector<bool> & x) {
  unsigned c = 0;
  for (unsigned i = 0; i != x.size(); ++i) {
    if (x[i]) ++c;
  }
  return c;
}

// 

struct WorkingLt {
  bool operator() (Expansion * x, Expansion * y) {

    // LARGEST number of expansions
    unsigned x_s = count(x->exp);
    unsigned y_s = count(y->exp);
    if (x_s != y_s) return x_s > y_s;

    // SMALLEST base word
    x_s = strlen(x->word);
    y_s = strlen(y->word);
    if (x_s != y_s) return x_s < y_s;

    // LARGEST affix string
    x_s = strlen(x->aff);
    y_s = strlen(y->aff);
    if (x_s != y_s) return x_s > y_s; 

    // 
    int cmp = strcmp(x->word, y->word);
    if (cmp != 0) return cmp < 0;

    //
    cmp = strcmp(x->aff, y->aff);
    return cmp < 0;
  }
};

//
// Finally the function that does the real work
//

void munch_list_complete(Language * lang,
                         GetWordCallback * get_string, void * gs_data,
                         PutWordCallback * put_string, void * ps_data,
                         bool multi, bool simplify)
{
  FullConv oconv(lang->from_internal_);
  String buf;
  ObjStack exp_buf;
  WordAff * exp_list;
  GuessInfo gi;
  CML_Table table;
  ObjStack table_buf;
  Word word;

  // add words to dictionary
  while (get_string(gs_data, &word)) {
    buf.clear();
    lang->to_internal_->convert(word.str, word.len, buf);
    char * w = buf.mstr();
    char * af = strchr(w, '/');
    size_t s;
    if (af != 0) {
      s = af - w;
      *af++ = '\0';
    } else {
      s = strlen(w);
      af = w + s;
    }
    exp_buf.reset();
    exp_list = lang->real->expand(w, af, exp_buf);
    for (WordAff * q = exp_list; q; q = q->next) {
      if (!table.have(q->word)) // since it is a multi hash table
        table.insert(CML_Entry(table_buf.dup(q->word))).first;
    }
  }

  // Now try to munch each word in the dictionary.  This will also
  // group the base words into disjoint sets based on there expansion.
  // For example the words:
  //   clean cleaning cleans cleaned dog dogs
  // would be grouped into two disjoint sets:
  //   1) clean cleaning cleans cleaned
  //   2) dog dogs
  // Each of the disjoint sets can then be processed independently
  CML_Table::iterator p = table.begin();
  CML_Table::iterator end = table.end();
  String flags;
  for (; p != end; ++p) 
  {
    lang->real->munch(p->word, &gi, false);
    const IntrCheckInfo * ci = gi.head;
    while (ci)
    { {
      // check if the base word is in the dictionary
      CML_Table::iterator b = table.find(ci->word);
      if (b == table.end()) goto cont;

      // check if all the words once expanded are in the dictionary
      char flags[2];
      assert(!(ci->pre_flag && ci->suf_flag));
      if      (ci->pre_flag != 0) flags[0] = ci->pre_flag;
      else if (ci->suf_flag != 0) flags[0] = ci->suf_flag;
      flags[1] = '\0';
      exp_buf.reset();
      exp_list = lang->real->expand(ci->word, flags, exp_buf);
      for (WordAff * q = exp_list; q; q = q->next) {
        if (!table.have(q->word)) goto cont;
      }

      // all the expansions are in the dictionary now add the affix to
      // the base word and figure out which disjoint set it belongs to
      add_affix(b, flags[0], ci->pre_flag != 0);
      CML_Entry * bs = find_set(&*b);
      for (WordAff * q = exp_list; q; q = q->next) {
        CML_Table::iterator w = table.find(q->word);
        assert(b != table.end());
        CML_Entry * ws = find_set(&*w);
        link(bs,ws);
      }

    } cont:
      ci = ci->next;
    }
  }

  // If a base word has both prefixes and suffixes try to combine them.
  // This can lead to multiple entries for the same base word.  If "multi"
  // is true than include all the entries.  Otherwise, only include the
  // one with the largest number of expansions.  This is a greedy choice
  // that may not be optimal, but is close to it.
  p = table.begin();
  String pre,suf;
  CML_Entry * extras = 0;
  for (; p != end; ++p) 
  {
    pre.clear(); suf.clear();
    if (!p->aff) continue;
    char * s = p->aff;
    while (*s != '/') pre += *s++;
    ++s;
    while (*s != '\0') suf += *s++;
    if (pre.empty()) {

      strcpy(p->aff, suf.str());

    } else if (suf.empty()) {

      strcpy(p->aff, pre.str());

    } else {

      // Try all possible combinations and keep the ones which expand
      // to legal words.

      PreSufList cross,tmp1,tmp2;
      PreSuf * ps = 0;

      for (String::iterator pi = pre.begin(); pi != pre.end(); ++pi) {
        String::iterator si = suf.begin();
        while (si != suf.end()) { {
          char flags[3] = {*pi, *si, '\0'};
          exp_buf.reset();
          exp_list = lang->real->expand(p->word, flags, exp_buf);
          for (WordAff * q = exp_list; q; q = q->next) {
            if (!table.have(q->word)) goto cont2;
          }
          ps = new PreSuf;
          ps->pre += *pi;
          ps->suf += *si;
          cross.add(ps);
        } cont2:
          ++si;
        }
      }

      // Now combine the legal cross pairs with other ones when
      // possible.

      // final res = [ (pre, []) ([],suf),
      //               (cross | combine first | combine second)
      //               (cross | combine second | combine first)
      //             | combine first
      //             | combine second
      //
      // combine first [(ab, c) (ab, d) (c, de) (c, ef)]
      //   =  [(ab, cd), (c, def)]
      
      combine(cross, tmp1, 0); 
      combine(tmp1,  tmp2, 1);
      tmp1.clear();
      
      combine(cross, tmp1, 1);
      combine(tmp1,  tmp2, 0);
      tmp1.clear();

      cross.clear();

      ps = new PreSuf;
      ps->pre = pre;
      tmp2.add(ps);
      ps = new PreSuf;
      ps->suf = suf;
      tmp2.add(ps);

      combine(tmp2, tmp1, 0);
      combine(tmp1, cross, 1);

      if (multi) {

        // It is OK to have multiple entries with the same base word
        // so use them all.

        ps = cross.head;
        assert(ps);
        memcpy(p->aff, ps->pre.data(), ps->pre.size());
        memcpy(p->aff + ps->pre.size(), ps->suf.str(), ps->suf.size() + 1);
        
        ps = ps->next;
        CML_Entry * bs = find_set(&*p);
        for (; ps; ps = ps->next) {
          
          CML_Entry * tmp = new CML_Entry;
          tmp->word = p->word;
          tmp->aff = (char *)malloc(ps->pre.size() + ps->suf.size() + 1);
          memcpy(tmp->aff, ps->pre.data(), ps->pre.size());
          memcpy(tmp->aff + ps->pre.size(), ps->suf.str(), ps->suf.size() + 1);
          
          tmp->parent = bs;
          
          tmp->next = extras;
          extras = tmp;
        }

      } else {

        // chose the one which has the largest number of expansions

        int max_exp = 0;
        PreSuf * best = 0;
        String flags;

        for (ps = cross.head; ps; ps = ps->next) {
          flags  = ps->pre;
          flags += ps->suf;
          exp_buf.reset();
          exp_list = lang->real->expand(p->word, flags, exp_buf);
          int c = 0;
          for (WordAff * q = exp_list; q; q = q->next) ++c;
          if (c > max_exp) {max_exp = c; best = ps;}
        }

        memcpy(p->aff, best->pre.data(), best->pre.size());
        memcpy(p->aff + best->pre.size(), best->suf.str(), best->suf.size() + 1);
      }
    }
  }

  while (extras) {
    CML_Entry * tmp = extras;
    extras = extras->next;
    tmp->next = 0;
    table.insert(*tmp);
    delete tmp;
  }

  // Create a linked list for each disjoint set
  p = table.begin();
  for (; p != end; ++p) 
  {
    p->rank = -1;
    CML_Entry * bs = find_set(&*p);
    if (bs != &*p) {
      p->next = bs->next;
      bs->next = &*p;
    } 
  }

  // Now process each disjoint set independently
  p = table.begin();
  for (; p != end; ++p) 
  {
    if (p->parent) continue;

    Vector<CML_Table::iterator> entries;
    Vector<Expansion> expansions;
    Vector<Expansion *> to_keep;
    std::vector<bool> to_keep_exp;
    Vector<Expansion *> working;
    Vector<unsigned> to_remove;

    // First assign numbers to each unique word.  The rank field is
    // no longer used so use it to store the number.
    for (CML_Entry * q = &*p; q; q = q->next) {
      CML_Table::iterator e = table.find(q->word);
      if (e->rank == -1) {
        e->rank = entries.size();
        q->rank = entries.size();
        entries.push_back(e);
      } else {
        q->rank = e->rank;
      }
      if (q->aff) {
        Expansion tmp;
        tmp.word = q->word;
        tmp.aff  = q->aff;
        expansions.push_back(tmp);
      }
    }

    to_keep_exp.resize(entries.size());

    // Store the expansion of each base word in a bit vector and
    // add it to the working set
    for (Vector<Expansion>::iterator q = expansions.begin(); 
         q != expansions.end(); 
         ++q)
    {
      q->exp.resize(entries.size());
      exp_buf.reset();
      exp_list = lang->real->expand(q->word, q->aff, exp_buf);
      for (WordAff * i = exp_list; i; i = i->next) {
        CML_Table::iterator e = table.find(i->word);
        assert(0 <= e->rank && e->rank < (int)entries.size());
        q->exp[e->rank] = true;
      }
      q->orig_exp = q->exp;
      working.push_back(&*q);
    }
    
    unsigned prev_working_size = INT_MAX;

    // This loop will repeat until the working set is empty.  This
    // will produce optimum results in most cases.  Non optimum
    // results may be possible if step (4) is necessary, but in
    // practice this step is rarly necessary.
    do {
      prev_working_size = working.size();

      // Sort the list based on WorkingLt.  This is necessary every
      // time since the expansion list can change.
      std::sort(working.begin(), working.end(), WorkingLt());

      // (1) Eliminate any elements which are a subset of others
      for (unsigned i = 0; i != working.size(); ++i) {
        if (!working[i]) continue;
        for (unsigned j = i + 1; j != working.size(); ++j) {
          if (!working[j]) continue;
          if (subset(working[j]->exp, working[i]->exp)) {
            working[j] = 0;
          }
        }
      }

      // (2) Move any elements which expand to unique entree 
      // into the to_keep list
      to_remove.clear();
      for (unsigned i = 0; i != entries.size(); ++i) {
        int n = -1;
        for (unsigned j = 0; j != working.size(); ++j) {
          if (working[j] && working[j]->exp[i]) {
            if (n == -1) n = j;
            else         n = -2;
          }
        }
        if (n >= 0) to_remove.push_back(n);
      }
      for (unsigned i = 0; i != to_remove.size(); ++i) {
        unsigned n = to_remove[i];
        if (!working[n]) continue;
        to_keep.push_back(working[n]);
        merge(to_keep_exp, working[n]->exp);
        working[n] = 0;
      }

      // (3) Eliminate any elements which are a subset of all the
      // elements in the to_keep list
      for (unsigned i = 0; i != working.size(); ++i) {
        if (working[i] && subset(working[i]->exp, to_keep_exp)) {
          working[i] = 0;
        }
      }

      // Compact the working list
      {
        int i = 0, j = 0;
        while (j != (int)working.size()) {
          if (working[j]) {
            working[i] = working[j];
            ++i;
          }
          ++j;
        }
        working.resize(i);
      }

      // (4) If none of the entries in working have been removed via
      // the above methods then make a greedy choice and move the
      // first element into the to_keep list.
      if (working.size() > 0 && working.size() == prev_working_size)
      {
        to_keep.push_back(working[0]);
        //CERR.printf("Making greedy choice! Chosing %s/%s.\n",
        //            working[0]->word, working[0]->aff);
        merge(to_keep_exp, working[0]->exp);
        working.erase(working.begin(), working.begin() + 1);
      }

      // (5) Trim the expansion list for any elements left in the
      // working set by removing the expansions that already exist in
      // the to_keep list
      for (unsigned i = 0; i != working.size(); ++i) {
        purge(working[i]->exp, to_keep_exp);
      }

    } while (working.size() > 0);

    if (simplify) {

      // Remove unnecessary flags.  A flag is unnecessary if it does
      // does not expand to any new words, that is words that are not
      // already covered by an earlier entries in the list.

      for (unsigned i = 0; i != to_keep.size(); ++i) {
        to_keep[i]->exp = to_keep[i]->orig_exp;
      }
     
      std::sort(to_keep.begin(), to_keep.end(), WorkingLt());

      std::vector<bool> tally(entries.size());
      std::vector<bool> backup(entries.size());
      std::vector<bool> working(entries.size());
      String flags;
      
      for (unsigned i = 0; i != to_keep.size(); ++i) {

        backup = tally;

        merge(tally, to_keep[i]->exp);

        String flags_to_keep = to_keep[i]->aff;
        bool something_changed;
        do {
          something_changed = false;
          for (unsigned j = 0; j != flags_to_keep.size(); ++j) {
            flags.assign(flags_to_keep.data(), j);
            flags.append(flags_to_keep.data(j+1), 
                         flags_to_keep.size() - (j+1));
            working = backup;
            exp_buf.reset();
            exp_list = lang->real->expand(to_keep[i]->word, flags, exp_buf);
            for (WordAff * q = exp_list; q; q = q->next) {
              CML_Table::iterator e = table.find(q->word);
              working[e->rank] = true;
            }
            if (working == tally) {
              flags_to_keep = flags;
              something_changed = true;
              break;
            }
          }
        } while (something_changed);

        if (flags_to_keep != to_keep[i]->aff) {
          memcpy(to_keep[i]->aff, flags_to_keep.str(), flags_to_keep.size() + 1);
        }
      }
      
    }

    // Finally print the resulting list

    for (unsigned i = 0; i != to_keep.size(); ++i) {
      buf.clear();
      lang->from_internal_->convert(to_keep[i]->word, -1, buf);
      if (to_keep[i]->aff[0]) {
        lang->from_internal_->convert("/", 1, buf);
        lang->from_internal_->convert(to_keep[i]->aff, -1, buf);
      }
      word.str = buf.str();
      word.len = buf.size();
      bool res = put_string(ps_data, &word);
      if (!res) goto quit;
    }
    for (unsigned i = 0; i != to_keep_exp.size(); ++i) {
      if (!to_keep_exp[i]) {
        assert(!entries[i]->aff);
        buf.clear();
        lang->from_internal_->convert(entries[i]->word, -1, buf);
        word.str = buf.str();
        word.len = buf.size();
        bool res = put_string(ps_data, &word);
        if (!res) goto quit;
      }
    }
  }

quit:
  p = table.begin();
  for (; p != end; ++p) 
  {
    if (p->aff) free(p->aff);
    p->aff = 0;
  }
}

}

namespace acommon {

extern "C" int aspell_munch_list(Language * ths, 
                                 GetWordCallback * in_cb, 
                                 void * in_cb_data, 
                                 PutWordCallback * out_cb, 
                                 void * out_cb_data, 
                                 MunchListParms * parms)
{
  munch_list_complete(ths, in_cb, in_cb_data, out_cb, out_cb_data, 
                      parms->multi, parms->simplify);
  return 0;
}

}
