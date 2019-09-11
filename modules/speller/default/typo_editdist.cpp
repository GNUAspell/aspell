
#include <cstring>
#include <stdint.h>
#include <algorithm>

#include "vararray.hpp"
#include "typo_editdist.hpp"
#include "config.hpp"
#include "language.hpp"
#include "file_data_util.hpp"
#include "getdata.hpp"
#include "cache-t.hpp"
#include "asc_ctype.hpp"

// typo_edit_distance is implemented using a straight forward dynamic
// programming algorithm with out any special tricks.  Its space
// usage AND running time is tightly asymptotically bounded by
// strlen(a)*strlen(b)

typedef unsigned char uchar;

namespace aspeller {

  using namespace std;

  namespace {

    struct CombinedEdit {
      typedef char Op;
      uint16_t total;
      uint8_t prev_i;
      uint8_t prev_j;
      Op op;
      uint8_t dist[3];
    };
    
    inline bool operator < (CombinedEdit x, CombinedEdit y) {
      return x.total < y.total;
    }
    
    inline CombinedEdit combine(Matrix<CombinedEdit> & e, int i, int j, CombinedEdit::Op op,
                                uint8_t dist0 = 0, uint8_t dist1 = 0, uint8_t dist2 = 0) {
      CombinedEdit res;
      res.total = e(i,j).total + dist0 + dist1 + dist2;
      res.prev_i = i;
      res.prev_j = j;
      res.op = op;
      res.dist[0] = dist0;
      res.dist[1] = dist1;
      res.dist[2] = dist2;
      return res;
    }
  }

  short typo_edit_distance(NormalizedString word0, 
			   NormalizedString target0,
			   const TypoEditDistanceInfo & w,
                           Vector<IndexedEdit> * edits) 
  {
    int word_size   = word0.size + 1;
    int target_size = target0.size + 1;
    const uchar * word   = word0.data;
    const uchar * target = target0.data;
    VARARRAY(CombinedEdit, e_d, word_size * target_size);
    Matrix<CombinedEdit> e(word_size,target_size, e_d);
    e(0,0).total = 0;
    e(0,0).prev_i = 255;
    e(0,0).prev_j = 255;
    e(0,0).op = '\0';
    e(0,0).dist[0] = 0;
    e(0,0).dist[1] = 0;
    e(0,0).dist[2] = 0;
    for (int j = 1; j != target_size; ++j)
      e(0,j) = combine(e, 0, j-1, 'i', w.missing);
    --word;
    --target;
    CombinedEdit te;
    for (int i = 1; i != word_size; ++i) {
      e(i,0) = combine(e, i-1, 0, 'd', w.extra_dis2);
      for (int j = 1; j != target_size; ++j) {

	if (word[i] == target[j]) {

	  e(i,j) = combine(e, i-1, j-1, 'r', 0);

	} else {
	  
	  te = e(i,j) = combine(e, i-1, j-1, 'r', w.repl(word[i],target[j]));
	  
	  if (i != 1) {
	    te =  combine(e, i-1, j, 'd', w.extra(word[i-1], target[j]));
	    if (te < e(i,j)) e(i,j) = te;
	    te = combine(e, i-2, j-1, 'D', w.extra(word[i-1], target[j]), w.repl(word[i], target[j]));
            if (te < e(i,j)) e(i,j) = te;
	  } else {
	    te =  combine(e, i-1,j, 'd', w.extra_dis2);
	    if (te < e(i,j)) e(i,j) = te;
	  }

	  te = combine(e, i, j-1, 'i', w.missing);
	  if (te < e(i,j)) e(i,j) = te;

	  //swap
	  if (i != 1 && j != 1) {
            te = combine(e, i-2, j-2, 's',
                         w.swap, w.repl(word[i], target[j-1]), w.repl(word[i-1], target[j]));
	      if (te < e(i,j)) e(i,j) = te;
          }
	}
      } 
    }
    if (edits) {
      // reverse pointers
      uint8_t i = word_size-1, j = target_size-1;
      uint8_t next_i = 255;
      uint8_t next_j = 255;
      while (i != 255 && j != 255) {
        CombinedEdit & edit = e(i,j);
        uint8_t prev_i = edit.prev_i;
        uint8_t prev_j = edit.prev_j;
        edit.prev_i = next_i;
        edit.prev_j = next_j;
        next_i = i;
        next_j = j;
        i = prev_i;
        j = prev_j;
      }
      // prev now means next
      edits->clear();
      i = e(0,0).prev_i;
      j = e(0,0).prev_j;
      while (i != 255 && j != 255) {
        CombinedEdit & edit = e(i,j);
        --i;
        --j;
        switch (edit.op) {
        case 'r':
          edits->push_back(IndexedEdit(i,j,'r',edit.dist[0]));
          break;
        case 'd':
          edits->push_back(IndexedEdit(i,j+1,'d',edit.dist[0]));
          break;
        case 'D':
          edits->push_back(IndexedEdit(i-1,j,'d',edit.dist[0]));
          edits->push_back(IndexedEdit(i,j,'r',edit.dist[1]));
          break;
        case 'i':
          edits->push_back(IndexedEdit(i+1,j,'i',edit.dist[0]));
          break;
        case 's':
          edits->push_back(IndexedEdit(i-1,j,'r',edit.dist[2]));
          edits->push_back(IndexedEdit(i,j-1,'r',edit.dist[1]));
          edits->push_back(IndexedEdit(i,j,'s',edit.dist[0]));
          break;
        default:
          abort();
        }
        i = edit.prev_i;
        j = edit.prev_j;
      }
    }
    return e(word_size-1,target_size-1).total;
  }

  // void transform_target(const Vector<Edit> & edits, ParmStr target, String & out) {
  //   out.clear();
  //   for (Vector<Edit>::const_iterator itr = edits.begin(), e = edits.end(); itr != e; ++itr) {
  //     Edit edit = *itr;
  //     int j = edit.j;
  //     switch (edit.op) {
  //     case 'r':
  //       out.push_back(target[j]);
  //       break;
  //     case 'd':
  //       // noop
  //       break;
  //     case 'i':
  //       out.push_back(target[j]);
  //       break;
  //     case 's':
  //       // nothing more to do
  //       break;
  //     default:
  //       abort();
  //     }
  //   }
  // }
  
  Edit Edits::operator[](int i) {
    Edit e = (*orig)[i];
    switch (e.op) {
    case 'r':
      if (e.cost == 0)
        e.op = 'a';
      // fall though
    case 'i':
      e.chr = target[e.j];
      break;
    default:
      break;
    }
    return e;
  }

  void Edit::fmt(OStream & out) const {
    out << op;
    if (op == 'r' || op == 'i')
      out << '(' << chr << ')';
    if (cost != 0)
      out << " +" << static_cast<unsigned>(cost);
  }

  void apply_edits(Edits & edits, ParmStr word, String & out) {
    out.clear();
    for (int k = 0, sz = edits.size(); k != sz; ++k) {
      Edit edit = edits[k];
      int i = edit.i;
      int j = edit.j;
      switch (edit.op) {
      case 'a':
        out.push_back(word[i]);
        break;
      case 'r':
        out.push_back(edit.chr);
        break;
      case 'd':
        // noop
        break;
      case 'i':
        out.push_back(edit.chr);
        break;
      case 's':
        std::swap(out.end()[-2],out.end()[-1]);
        break;
      default:
        abort();
      }
    }
  }
  
  void apply_edits(Edits & edits, String & word) {
    String::iterator p = word.begin();
    for (int k = 0, sz = edits.size(); k != sz; ++k) {
      Edit edit = edits[k];
      int j = edit.j;
      switch (edit.op) {
      case 'a':
        ++p;
        break;
      case 'r':
        *p = edit.chr;
        ++p;
        break;
      case 'd':
        p = word.erase(p);
        break;
      case 'i':
        p = word.insert(p, edit.chr) + 1;
        break;
      case 's':
        std::swap(p[-2],p[-1]);
        break;
      default:
        abort();
      }
    }    
  }

  // void apply_edits_two_pass(const Vector<Edit> & edits, String & word, ParmStr target) {
  //   for (Vector<Edit>::const_iterator itr = edits.begin(), e = edits.end(); itr != e; ++itr) {
  //     Edit edit = *itr;
  //     if (edit.op == 'r' && edit.cost != 0) {
  //       word[edit.i] = target[edit.j];
  //     }
  //   }
  //   String::iterator word;
  //   for (Vector<Edit>::const_iterator itr = edits.begin(), e = edits.end(); itr != e; ++itr) {
  //     Edit edit = *itr;
  //     if (edit.op == 'r' && edit.cost != 0) {
  //       switch (edit.op) {
  //       case 'd':
          
  //       }
  //     }
  //   }
  // }
    
  static GlobalCache<TypoEditDistanceInfo> typo_edit_dist_info_cache("keyboard");

  PosibErr<void> setup(CachePtr<const TypoEditDistanceInfo> & res,
                       const Config * c, const Language * l, ParmString kb)
  {
    PosibErr<TypoEditDistanceInfo *> pe = get_cache_data(&typo_edit_dist_info_cache, c, l, kb);
    if (pe.has_err()) return pe;
    res.reset(pe.data);
    return no_err;
  }

  struct CharPair {
    char d[2];
    CharPair(char a, char b) {d[0] = a; d[1] = b;}
  };

  void TypoEditDistanceInfo::set_max() {
    if (missing > max) max = missing;
    if (swap    > max) max = swap;
    if (repl_dis1 > max) max = repl_dis1;
    if (repl_dis2 > max) max = repl_dis2;
    if (extra_dis1 > max) max = extra_dis1;
    if (extra_dis2 > max) max = extra_dis2;
  }

  PosibErr<TypoEditDistanceInfo *> 
  TypoEditDistanceInfo::get_new(const char * kb, const Config * cfg, const Language * l)
  {
    ConvEC iconv;
    RET_ON_ERR(iconv.setup(*cfg, "utf-8", l->charmap(), NormFrom));
    
    Vector<CharPair> data;
    
    char to_stripped[256];
    for (int i = 0; i <= 255; ++i)
      to_stripped[i] = l->to_stripped(i);
    
    if (strcmp(kb, "none") != 0) {
      FStream in;
      String file, dir1, dir2;
      fill_data_dir(cfg, dir1, dir2);
      find_file(file, dir1, dir2, kb, ".kbd");
      RET_ON_ERR(in.open(file.c_str(), "r"));
      
      String buf;
      DataPair d;
      while (getdata_pair(in, d, buf)) {
        if (d.key == "key") {
          PosibErr<char *> pe(iconv(d.value.str, d.value.size));
          if (pe.has_err()) 
            return pe.with_file(file, d.line_num);
          char * v = pe.data;
          char base = *v;
          while (*v) {
            to_stripped[(uchar)*v] = base;
            ++v;
            while (asc_isspace(*v)) ++v;
          }
        } else {
          PosibErr<char *> pe(iconv(d.key.str, d.key.size));
          if (pe.has_err()) 
            return pe.with_file(file, d.line_num);
          char * v = pe.data;
          if (strlen(v) != 2) 
            return make_err(invalid_string, d.key.str).with_file(file, d.line_num);
          to_stripped[(uchar)v[0]] = v[0];
          to_stripped[(uchar)v[1]] = v[1];
          data.push_back(CharPair(v[0], v[1]));
        }
      }
    }

    TypoEditDistanceInfo * w = new TypoEditDistanceInfo();
    w->keyboard = kb;

    memset(w->to_normalized_, 0, sizeof(w->to_normalized_));

    int c = 1;
    for (int i = 0; i <= 255; ++i) {
      if (l->is_alpha(i)) {
	if (w->to_normalized_[(uchar)to_stripped[i]] == 0) {
	  w->to_normalized_[i] = c;
	  w->to_normalized_[(uchar)to_stripped[i]] = c;
	  ++c;
	} else {
	  w->to_normalized_[i] = w->to_normalized_[(uchar)to_stripped[i]];
	}
      }
    }
    for (int i = 0; i != 256; ++i) {
      if (w->to_normalized_[i]==0) w->to_normalized_[i] = c;
    }
    w->max_normalized = c;
    
    c = w->max_normalized + 1;
    int cc = c * c;
    w->data = (uint8_t *)malloc(cc * 2 * sizeof(short));
    w->repl .init(c, c, w->data);
    w->extra.init(c, c, w->data + cc);
    
    for (int i = 0; i != c; ++i) {
      for (int j = 0; j != c; ++j) {
        w->repl (i,j) = w->repl_dis2;
        w->extra(i,j) = w->extra_dis2;
      }
    }
    
    for (unsigned i = 0; i != data.size(); ++i) {
      const char * d = data[i].d;
      w->repl (w->to_normalized(d[0]),
               w->to_normalized(d[1])) = w->repl_dis1;
      w->repl (w->to_normalized(d[1]),
               w->to_normalized(d[0])) = w->repl_dis1;
      w->extra(w->to_normalized(d[0]),
               w->to_normalized(d[1])) = w->extra_dis1;
      w->extra(w->to_normalized(d[1]),
               w->to_normalized(d[0])) = w->extra_dis1;
    }
    
    for (int i = 0; i != c; ++i) {
      w->repl(i,i) = 0;
      w->extra(i,i) = w->extra_dis1;
    }

    return w;
  }
  

}
