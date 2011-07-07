// This file is part of The New Aspell Copyright (C)
// 2002,2003,2004,2011 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find it at
// http://www.gnu.org/.

//
// NOTE: This program currently uses a very ugly mix of the internal
//       API and the external C interface.  The eventual goal is to
//       use only the external C++ interface, however, the external
//       C++ interface is currently incomplete.  The C interface is
//       used in some places because without the strings will not get
//       converted properly when the encoding is not the same as the
//       internal encoding used by Aspell.
// 

#include <ctype.h>
#include "settings.h"

#ifdef USE_LOCALE
# include <locale.h>
#endif

#ifdef HAVE_LANGINFO_CODESET
# include <langinfo.h>
#endif

#include "aspell.h"

#ifdef USE_FILE_INO
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
# include <fcntl.h>
#endif

#include "file_util.hpp"
#include "fstream.hpp"
#include "asc_ctype.hpp"
#include "check_funs.hpp"
#include "config.hpp"
#include "convert.hpp"
#include "checker.hpp"
#include "enumeration.hpp"
#include "errors.hpp"
#include "info.hpp"
#include "iostream.hpp"
#include "posib_err.hpp"
#include "speller.hpp"
#include "stack_ptr.hpp"
#include "string_enumeration.hpp"
#include "string_map.hpp"
#include "word_list.hpp"

#include "string_list.hpp"
#include "speller_impl.hpp"
#include "data.hpp"

#include "hash-t.hpp"
#include "hash_fun.hpp"

#include "gettext.h"

using namespace aspell;

// action functions declarations

void print_ver();
void print_help(bool verbose = false);
void config();

void check();
void pipe();
void convt();
void normlz();
void filter();
void list();
void dicts();
void modes();
void filters();

void clean();
void master();
void personal();
void repl();
void soundslike();
void munch();
void expand();
void combine();
void munch_list();
void dump_affix();

void print_error(ParmString msg)
{
  CERR.printf(_("Error: %s\n"), msg.str());
}

void print_error(ParmString msg, ParmString str)
{
  CERR.put(_("Error: "));
  CERR.printf(msg.str(), str.str());
  CERR.put('\n');
}

#define EXIT_ON_ERR(command) \
  do{PosibErrBase pe(command);\
  if(pe.has_err()){print_error(pe.get_err()->mesg); exit(1);}\
  } while(false)
#define EXIT_ON_ERR_SET(command, type, var)\
  type var;\
  do{PosibErr< type > pe(command);\
  if(pe.has_err()){print_error(pe.get_err()->mesg); exit(1);}\
  else {var=pe.data;}\
  } while(false)
#define BREAK_ON_ERR(command) \
  do{PosibErrBase pe(command);\
  if(pe.has_err()){print_error(pe.get_err()->mesg); break;}\
  } while(false)
#define BREAK_ON_ERR_SET(command, type, var)\
  type var;\
  do{PosibErr< type > pe(command);\
  if(pe.has_err()){print_error(pe.get_err()->mesg); break;}\
  else {var=pe.data;}\
  } while(false)


/////////////////////////////////////////////////////////
//
// Command line options functions and classes
// (including main)
//

typedef Vector<String> Args;
typedef Config         Options;
enum Action {do_create, do_merge, do_dump, do_test, do_other};

Args              args;
StackPtr<Options> options;
Action            action  = do_other;

struct PossibleOption {
  const char * name;
  char         abrv;
  int          num_arg;
  bool         is_command;
};

#define OPTION(name,abrv,num)         {name,abrv,num,false}
#define COMMAND(name,abrv,num)        {name,abrv,num,true}
#define ISPELL_COMP(abrv,num)         {"",abrv,num,false}

const PossibleOption possible_options[] = {
  OPTION("master",           'd', 1),
  OPTION("personal",         'p', 1),
  OPTION("ignore",           'W', 1),
  OPTION("lang",             'l', 1),
  OPTION("encoding",         'T', 1),
  OPTION("backup",           'b', 0),
  OPTION("dont-backup",      'x', 0),
  OPTION("run-together",     'C', 0),
  OPTION("dont-run-together",'B', 0),
  OPTION("guess",            'm', 0),
  OPTION("dont-guess",       'P', 0),
  
  COMMAND("usage",     '?',  0),
  COMMAND("help",      '\0', 0),
  COMMAND("version",   'v',  0),
  COMMAND("config",    '\0', 0),
  COMMAND("dicts",     '\0', 0),
  COMMAND("check",     'c',  0),
  COMMAND("pipe",      'a',  0),
  COMMAND("list",      '\0', 0),
  COMMAND("conv",      '\0', 2),
  COMMAND("norm",      '\0', 1),
  COMMAND("filter",    '\0', 0),
  COMMAND("soundslike",'\0', 0),
  COMMAND("munch",     '\0', 0),
  COMMAND("expand",    '\0', 0),
  COMMAND("combine",   '\0', 0),
  COMMAND("munch-list",'\0', 0),
  COMMAND("clean",     '\0', 0),
  COMMAND("filters",   '\0', 0),
  COMMAND("modes",     '\0', 0),

  COMMAND("dump",   '\0', 1),
  COMMAND("create", '\0', 1),
  COMMAND("merge",  '\0', 1),

  ISPELL_COMP('S',0), ISPELL_COMP('w',1),

  {"",'\0'}, {"",'\0'}
};

const PossibleOption * possible_options_end = possible_options + sizeof(possible_options)/sizeof(PossibleOption) - 2;

struct ModeAbrv {
  char abrv;
  const char * mode;
  const char * desc;
};
static const ModeAbrv mode_abrvs[] = {
  {'e', "mode=email", N_("enter Email mode.")},
  {'H', "mode=html",  N_("enter HTML mode.")},
  {'t', "mode=tex",   N_("enter TeX mode.")},
  {'n', "mode=nroff", N_("enter Nroff mode.")}
};

static const ModeAbrv *  mode_abrvs_end = mode_abrvs + 4;

const PossibleOption * find_option(char c) {
  const PossibleOption * i = possible_options;
  while (i != possible_options_end && i->abrv != c) 
    ++i;
  return i;
}

static inline bool str_equal(const char * begin, const char * end, 
                             const char * other) 
{
  while(begin != end && *begin == *other)
    ++begin, ++other;
  return (begin == end && *other == '\0');
}

static const PossibleOption * find_option(const char * begin, const char * end) {
  const PossibleOption * i = possible_options;
  while (i != possible_options_end 
         && !str_equal(begin, end, i->name))
    ++i;
  return i;
}

static const PossibleOption * find_option(const char * str) {
  const PossibleOption * i = possible_options;
  while (i != possible_options_end 
         && !strcmp(str, i->name) == 0)
    ++i;
  return i;
}

static void line_buffer() {
#if ! defined(WIN32) && ! defined(_WIN32)
  // set up stdin and stdout to be line buffered
  assert(setvbuf(stdin, 0, _IOLBF, 0) == 0); 
  assert(setvbuf(stdout, 0, _IOLBF, 0) == 0);
#endif
}

Conv dconv;
Conv uiconv;

int main (int argc, const char *argv[]) 
{
  options = new_config(); // this needs to be here becuase of a bug
                          // with static initlizers on Darwin.
#ifdef USE_LOCALE
  setlocale (LC_ALL, "");
#endif
  aspell_gettext_init();

  options->set_committed_state(false);

  if (argc == 1) {print_help(); return 0;}

  int i = 1;
  const PossibleOption * o;
  const char           * parm;

  //
  // process command line options by setting the appropriate options
  // in "options" and/or pushing non-options onto "argv"
  //
  PossibleOption other_opt = OPTION("",'\0',0);
  String option_name;
  while (i != argc) {
    if (argv[i][0] == '-') {
      bool have_parm = false;
      if (argv[i][1] == '-') {
        // a long arg
        const char * c = argv[i] + 2;
        while(*c != '=' && *c != '\0') ++c;
        o = find_option(argv[i] + 2, c);
        if (o == possible_options_end) {
          option_name.assign(argv[i] + 2, c - argv[i] - 2);
          other_opt.name    = option_name.c_str();
          other_opt.num_arg = -1;
          o = &other_opt;
        }
        if (*c == '=') {have_parm = true; ++c;}
        parm = c;
      } else {
        // a short arg
        const ModeAbrv * j = mode_abrvs;
        while (j != mode_abrvs_end && j->abrv != argv[i][1]) ++j;
        if (j == mode_abrvs_end) {
          o = find_option(argv[i][1]);
          if (argv[i][1] == 'v' && argv[i][2] == 'v')
            // Hack for -vv
            parm = argv[i] + 3;
          else
            parm = argv[i] + 2;
        } else { // mode option
          other_opt.name = "mode";
          other_opt.num_arg = 1;
          o = &other_opt;
          parm = j->mode + 5;
        }
        if (*parm) have_parm = true;
      }
      if (o == possible_options_end) {
        print_error(_("Invalid Option: %s"), argv[i]);
        return 1;
      }
      int num_parms;
      if (o->num_arg == 0) {
        num_parms = 0;
        if (parm[0] != '\0') {
          print_error(_(" does not take any parameters."), 
                      String(argv[i], parm - argv[i]));
          return 1;
        }
        i += 1;
      } else if (have_parm) {
        num_parms = 1;
        i += 1;
      } else if (i + 1 == argc || argv[i+1][0] == '-') {
        if (o->num_arg == -1) {
          num_parms = 0;
          i += 1;
        } else {
          print_error(_("You must specify a parameter for \"%s\"."), argv[i]);
          return 1;
        }
      } else {
        num_parms = o->num_arg;
        parm = argv[i + 1];
        i += 2;
      }
      if (o->is_command) {
        args.push_back(o->name);
        if (o->num_arg == 1)
          args.push_back(parm);
      } else if (o->name[0] != '\0') {
        Config::Entry * entry = new Config::Entry;
        entry->key = o->name;
        entry->value = parm;
        entry->need_conv = true;
        if (num_parms == -1) {
          entry->place_holder = args.size();
          args.push_back(parm);
        }
        options->set(entry);
      }
    } else {
      args.push_back(argv[i]);
      i += 1;
    }
  }

  options->read_in_settings();

  const char * codeset = 0;
#ifdef HAVE_LANGINFO_CODESET
  codeset = nl_langinfo(CODESET);
  if (ascii_encoding(*options, codeset)) codeset = 0;
#endif

// #ifdef USE_LOCALE
//   if (!options->have("encoding") && codeset)
//     EXIT_ON_ERR(options->replace("encoding", codeset));
// #endif

  Vector<int> to_remove;
  EXIT_ON_ERR(options->commit_all(&to_remove, codeset));
  for (int i = to_remove.size() - 1; i >= 0; --i) {
    args.erase(args.begin() + to_remove[i]);
  }

  if (args.empty()) {
    print_error(_("You must specify an action"));
    return 1;
  }

  String action_str = args.front();
  args.pop_front();
  const PossibleOption * action_opt = find_option(action_str.str());
  if (!action_opt->is_command) {
    print_error(_("Unknown Action: %s"),  action_str);
    return 1;
  } else if (action_opt->num_arg == 1 && args.empty()) {
    print_error(_("You must specify a parameter for \"%s\"."), action_str);
    return 1;
  } else if (action_opt->num_arg > (int)args.size()) {
    CERR.printf(_("Error: You must specify at least %d parameters for \"%s\".\n"), 
                action_opt->num_arg, action_str.str());
    return 1;
  }

  //
  // perform the requested action
  //
  if (action_str == "usage")
    print_help();
  else if (action_str == "help")
    print_help(true);
  else if (action_str == "version")
    print_ver();
  else if (action_str == "config")
    config();
  else if (action_str == "dicts")
    dicts();
  else if (action_str == "check")
    check();
  else if (action_str == "pipe")
    pipe();
  else if (action_str == "list")
    list();
  else if (action_str == "conv")
    convt();
  else if (action_str == "norm")
    normlz();
  else if (action_str == "filter")
    filter();
  else if (action_str == "soundslike")
    soundslike();
  else if (action_str == "munch")
    munch();
  else if (action_str == "expand")
    expand();
  else if (action_str == "combine")
    combine();
  else if (action_str == "munch-list")
    munch_list();
  else if (action_str == "clean")
    clean();
  else if (action_str == "filters")
    filters();
  else if (action_str == "modes")
    modes();
  else if (action_str == "dump")
    action = do_dump;
  else if (action_str == "create")
    action = do_create;
  else if (action_str == "merge")
    action = do_merge;
  else
    abort(); // this should not happen

  if (action != do_other) {
    if (args.empty()) {
      print_error(_("Unknown Action: %s"),  action_str);
      return 1;
    }
    String what_str = args.front();
    args.pop_front();
    if (what_str == "config")
      config();
    else if (what_str == "dicts")
      dicts();
    else if (what_str == "filters")
      filters();
    else if (what_str == "modes")
      modes();
    else if (what_str == "master")
      master();
    else if (what_str == "personal")
      personal();
    else if (what_str == "repl")
      repl();
    else if (what_str == "affix")
      dump_affix();
    else {
      print_error(_("Unknown Action: %s"),
                  String(action_str + " " + what_str));
      return 1;
    }
  }

  return 0;

}


/////////////////////////////////////////////////////////
//
// Action Functions
//
//

  
static SimpleConvert * setup_conv(const sp::LangImpl * lang,
                                  Config * config)
{
  if (config->retrieve("encoding") != "none") {
    PosibErr<SimpleConvert *> pe 
      = new_simple_convert_if_needed(*config,
                                     lang->charmap(),
                                     config->retrieve("encoding"),
                                     NormTo);
    if (pe.has_err()) {print_error(pe.get_err()->mesg); exit(1);}
    return pe.data;
  } else {
    return 0;
  }
}
 
static SimpleConvert * setup_conv(Config * config,
                                  const sp::LangImpl * lang)
{
  if (config->retrieve("encoding") != "none") {
    PosibErr<SimpleConvert *> pe 
      = new_simple_convert_if_needed(*config,
                                     config->retrieve("encoding"),
                                     lang->charmap(),
                                     NormFrom);
    if (pe.has_err()) {print_error(pe.get_err()->mesg); exit(1);}
    return pe.data;
  } else {
    return 0;
  }
}

void setup_display_conv(Config * config)
{
  const char * gettext_enc = 0;
  const char * env_enc = 0;
  String doc_enc = config->retrieve("encoding");
  String enc;
#ifdef ENABLE_NLS
  gettext_enc = bind_textdomain_codeset("aspell", 0);
  if (ascii_encoding(*options,gettext_enc)) gettext_enc = 0;
#endif
#ifdef HAVE_LANGINFO_CODESET
  env_enc = nl_langinfo(CODESET);
  if (ascii_encoding(*options, env_enc)) env_enc = 0;
#endif
  if (gettext_enc && env_enc && strcmp(gettext_enc,env_enc) != 0) 
  {
    fputs(("Error: bind_textdomain_codeset != nl_langinfo(CODESET)\n"), stderr);
    exit(-1);
  }
  if (gettext_enc)
    enc = gettext_enc;
  else if (env_enc)
    enc = env_enc;
  else
    enc = doc_enc;

  EXIT_ON_ERR(dconv.setup(*options, doc_enc, enc, NormNone));
  EXIT_ON_ERR(uiconv.setup(*options, enc, doc_enc, NormNone));
}

AspellSpeller * new_speller()
{
  AspellCanHaveError * ret 
    = new_aspell_speller(reinterpret_cast<AspellConfig *>(options.get()));
  if (aspell_error(ret)) {
    print_error(aspell_error_message(ret));
    exit(1);
  }
  return to_aspell_speller(ret);
}

AspellChecker * new_checker(AspellSpeller * sp)
{
  AspellCanHaveError * ret = new_aspell_checker(sp);
  if (aspell_error(ret)) {
    print_error(aspell_error_message(ret));
    exit(1);
  }
  return to_aspell_checker(ret);
}

AspellLanguage * new_language()
{
  AspellCanHaveError * ret 
    = new_aspell_language(reinterpret_cast<AspellConfig *>(options.get()));
  if (aspell_error(ret)) {
    print_error(aspell_error_message(ret));
    exit(1);
  }
  return to_aspell_language(ret);

}


///////////////////////////
//
// config
//

void config ()
{
  if (args.size() == 0) {
    load_all_filters(options);
    options->write_to_stream(COUT);
  } else {
    EXIT_ON_ERR_SET(options->retrieve_any(args[0]), String, value);
    COUT << value << "\n";
  }
}

///////////////////////////
//
// dicts
//

void dicts() 
{
  const DictInfoList * dlist = get_dict_info_list(options);

  StackPtr<DictInfoEnumeration> dels(dlist->elements());

  const DictInfo * entry;

  while ( (entry = dels->next()) != 0) 
    puts(entry->name);
}

///////////////////////////
//
// list available (filters/filter modes)
//

void list_available(PosibErr<StringPairEnumeration *> (*fun)(Config *))
{
  EXIT_ON_ERR_SET(fun(options), StringPairEnumeration *, els);
  StringPair sp;
  while (!els->at_end()) {
    sp = els->next();
    printf("%-14s %s\n", sp.first, gt_(sp.second));
  }
  delete els;
}

void filters()
{
  load_all_filters(options);
  list_available(available_filters);
}

void modes()
{
  list_available(available_filter_modes);
}

///////////////////////////
//
// pipe
//

// precond: strlen(str) > 0
char * trim_wspace (char * str)
{
  int last = strlen(str) - 1;
  while (asc_isspace(str[0])) {
    ++str;
    --last;
  }
  while (last > 0 && asc_isspace(str[last])) {
    --last;
  }
  str[last + 1] = '\0';
  return str;
}

bool get_word_pair(char * line, char * & w1, char * & w2)
{
  w2 = strchr(line, ',');
  if (!w2) {
    print_error(_("Invalid Input"));
    return false;
  }
  *w2 = '\0';
  ++w2;
  w1 = trim_wspace(line);
  w2 = trim_wspace(w2);
  return true;
}

void print_elements(const AspellWordList * wl) {
  AspellStringEnumeration * els = aspell_word_list_elements(wl);
  int count = 0;
  const char * w;
  String line;
  while ( (w = aspell_string_enumeration_next(els)) != 0 ) {
    ++count;
    line += w;
    line += ", ";
  }
  line.resize(line.size() - 2);
  COUT.printf("%u: %s\n", count, line.c_str());
}

#define BREAK_ON_SPELLER_ERR\
  do {if (aspell_speller_error(speller)) {\
    print_error(aspell_speller_error_message(speller)); break;\
  } } while (false)

void pipe() 
{
  line_buffer();

  bool terse_mode = false;
  bool do_time = options->retrieve_bool("time");
  bool suggest = options->retrieve_bool("suggest");
  bool include_guesses = options->retrieve_bool("guess");
  clock_t start,finish;

  if (!options->have("mode") && !options->have("filter")) {
    PosibErrBase err(options->replace("mode", "nroff"));
    if (err.has_err()) 
      CERR.printf(_("WARNING: Unable to enter Nroff mode: %s\n"),
                  err.get_err()->mesg);
  }

  start = clock();
  
  AspellSpeller * speller = new_speller();
  aspell::sp::SpellerImpl * real_speller = reinterpret_cast<aspell::sp::SpellerImpl *>(speller);
  Config * config = real_speller->config();
  MBLen mb_len;
  if (!config->retrieve_bool("byte-offsets")) 
    mb_len.setup(*config, config->retrieve("encoding"));
  if (do_time)
    COUT << _("Time to load word list: ")
         << (clock() - start)/(double)CLOCKS_PER_SEC << "\n";
  AspellChecker * checker = new_checker(speller);
  int c;
  const char * w;
  CharVector buf;
  char * line;
  char * line0;
  char * word;
  char * word2;
  int    ignore;
  PosibErrBase err;

  print_ver();

  for (;;) {
    buf.clear();
    fflush(stdout);
    while (c = getchar(), c != '\n' && c != EOF)
      buf.push_back(static_cast<char>(c));
    buf.push_back('\n'); // always add new line so strlen > 0
    buf.push_back('\0');
    line = buf.data();
    ignore = 0;
    switch (line[0]) {
    case '\n':
      if (c != EOF) continue;
      else          break;
    case '*':
      word = trim_wspace(line + 1);
      aspell_speller_add_to_personal(speller, word, -1);
      BREAK_ON_SPELLER_ERR;
      break;
    case '&':
      word = trim_wspace(line + 1);
      aspell_speller_add_lower_to_personal(speller, word, -1);
      BREAK_ON_SPELLER_ERR;
      break;
    case '@':
      word = trim_wspace(line + 1);
      aspell_speller_add_to_session(speller, word, -1);
      BREAK_ON_SPELLER_ERR;
      break;
    case '#':
      aspell_speller_save_all_word_lists(speller);
      BREAK_ON_SPELLER_ERR;
      break;
    case '+':
      word = trim_wspace(line + 1);
      err = config->replace("mode", word);
      if (err.get_err())
        config->replace("mode", "tex");
      goto reload_filters;
    case '-':
      // FIXME: config->remove("filter") doesn't work, is this even
      // the correct thing to do.  This is what I did in Aspell 0.60
      config->replace("mode", "nroff");
      goto reload_filters;
    case '~':
      word = trim_wspace(line + 1);
      BREAK_ON_ERR(config->replace("encoding", word));
      goto reload_filters;
    case '!':
      terse_mode = true;
      break;
    case '%':
      terse_mode = false;
      break;
    case '$':
      if (line[1] == '$') {
        switch(line[2]) {
        case 'r':
          switch(line[3]) {
          case 'a':
            if (get_word_pair(line + 4, word, word2))
              aspell_speller_store_replacement(speller, word, -1, word2, -1);
            break;
          }
          break;
        case 'c':
          switch (line[3]) {
          case 's':
            if (get_word_pair(line + 4, word, word2))
              BREAK_ON_ERR(err = config->replace(word, word2));
            if (strcmp(word,"suggest") == 0)
              suggest = config->retrieve_bool("suggest");
            else if (strcmp(word,"time") == 0)
              do_time = config->retrieve_bool("time");
            else if (strcmp(word,"guess") == 0)
              include_guesses = config->retrieve_bool("guess");
            break;
          case 'r':
            word = trim_wspace(line + 4);
            BREAK_ON_ERR_SET(config->retrieve_any(word), String, ret);
            COUT.printl(ret);
            break;
          }
          break;
        case 'p':
          switch (line[3]) {
          case 'p':
            print_elements(aspell_speller_personal_word_list(speller));
            break;
          case 's':
            print_elements(aspell_speller_session_word_list(speller));
            break;
          }
          break;
        case 'l':
          COUT.printl(config->retrieve("lang"));
          break;
        }
        break;
      } else {
        // continue on (no break)
      }
    case '^':
      ignore = 1;
    default:
      line0 = line;
      line += ignore;
      aspell_checker_process(checker, line, strlen(line), 0, 0);
      const AspellCheckerToken * token = 0;
      while (token = aspell_checker_next(checker), token) {
        if (token->correct) {
          if (!terse_mode) {
            const AspellCheckInfo * ci = aspell_speller_check_info(speller);
            COUT.put(ci->str);
            COUT.put('\n');
          }
        } else {
          word = line + token->begin.offset;
          unsigned word_len = token->end.offset - token->begin.offset;
          word[word_len] = '\0';
          String guesses;
          const AspellCheckInfo * ci = aspell_speller_check_info(speller);
          while (ci) {
            guesses << ", " << ci->str;
            ci = ci->next;
          }
          start = clock();
          const AspellWordList * suggestions = 0;
          if (suggest) 
            suggestions = aspell_speller_suggest(speller, word, -1);
          finish = clock();
          unsigned offset = mb_len(line0, token->begin.offset + ignore);
          if (suggestions && !aspell_word_list_empty(suggestions)) 
          {
            COUT.printf("& %s %u %u:", word, 
                        aspell_word_list_size(suggestions), offset);
            AspellStringEnumeration * els 
              = aspell_word_list_elements(suggestions);
            if (options->retrieve_bool("reverse")) {
              Vector<String> sugs;
              sugs.reserve(aspell_word_list_size(suggestions));
              while ( ( w = aspell_string_enumeration_next(els)) != 0)
                sugs.push_back(w);
              Vector<String>::reverse_iterator i = sugs.rbegin();
              while (true) {
                COUT.printf(" %s", i->c_str());
                ++i;
                if (i == sugs.rend()) break;
                COUT.put(',');
              }
            } else {
              while ( ( w = aspell_string_enumeration_next(els)) != 0) {
                COUT.printf(" %s%s", w, 
                            aspell_string_enumeration_at_end(els) ? "" : ",");
              }
            }
            delete_aspell_string_enumeration(els);
            if (include_guesses)
              COUT.put(guesses);
            COUT.put('\n');
          } else {
            if (guesses.empty())
              COUT.printf("# %s %u\n", word, offset);
            else
              COUT.printf("? %s 0 %u: %s\n", word, offset,
                          guesses.c_str() + 2);
          }
          if (do_time)
            COUT.printf(_("Suggestion Time: %f\n"), 
                        (finish-start)/(double)CLOCKS_PER_SEC);
        }
      }
      COUT.put('\n');
    }
    if (c == EOF) break;
    continue;
  reload_filters:
    BREAK_ON_ERR(reload_filters(real_speller));
    delete_aspell_checker(checker); // FIXME: Why?
    checker = new_checker(speller);
    continue;
  }

  delete_aspell_checker(checker);
  delete_aspell_speller(speller);
}

///////////////////////////
//
// check
//

enum UserChoice {None, Ignore, IgnoreAll, Replace, ReplaceAll, 
                 Add, AddLower, Exit, Abort};

struct Mapping {
  char primary[9];
  UserChoice reverse[256];
  void to_aspell();
  void to_ispell();
  char & operator[] (UserChoice c) {return primary[c];}
  UserChoice & operator[] (char c) 
    {return reverse[static_cast<unsigned char>(c)];}
};

void abort_check();

void setup_display_conv(Config *);

void check()
{
  String file_name;
  String new_name;
  FILE * in = 0;
  FILE * out = 0;
  Mapping mapping;
  bool changed = false;

  if (args.size() == 0) {
    print_error(_("You must specify a file name."));
    exit(-1);
  } else if (args.size() > 1) {
    print_error(_("Only one file name may be specified."));
    exit(-1);
  }
  
  file_name = args[0];
  new_name = file_name;
  new_name += ".new";

  in = fopen(file_name.c_str(), "r");
  if (!in) {
    print_error(_("Could not open the file \"%s\" for reading"), file_name);
    exit(-1);
  }
    
  if (!options->have("mode"))
    EXIT_ON_ERR(set_mode_from_extension(options, file_name));
    
  String m = options->retrieve("keymapping");
  if (m == "aspell")
    mapping.to_aspell();
  else if (m == "ispell")
    mapping.to_ispell();
  else {
    print_error(_("Invalid keymapping: %s"), m);
    exit(-1);
  }

#ifdef USE_FILE_INO
  {
    struct stat st;
    fstat(fileno(in), &st);
    int fd = open(new_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (fd >= 0) out = fdopen(fd, "w");
  }
#else
  out = fopen(new_name.c_str(), "w");
#endif
  if (!out) {
    print_error(_("Could not open the file \"%s\" for writing. File not saved."), file_name);
    exit(-1);
  }

  AspellSpeller * speller = new_speller();
  AspellChecker * checker = new_checker(speller);

  state = new CheckerString(checker, speller,in,out,64);
 
  word_choices = new Choices;

  menu_choices = new Choices;
  menu_choices->push_back(Choice(mapping[Ignore],     _("Ignore")));
  menu_choices->push_back(Choice(mapping[IgnoreAll],  _("Ignore all")));
  menu_choices->push_back(Choice(mapping[Replace],    _("Replace")));
  menu_choices->push_back(Choice(mapping[ReplaceAll], _("Replace all")));
  menu_choices->push_back(Choice(mapping[Add],        _("Add")));
  menu_choices->push_back(Choice(mapping[AddLower],   _("Add Lower")));
  menu_choices->push_back(Choice(mapping[Abort],      _("Abort")));
  menu_choices->push_back(Choice(mapping[Exit],       _("Exit")));

  String word0, new_word;
  Vector<String> sug_con;
  StackPtr<StringMap> replace_list(new_string_map());
  const char * w;

  begin_check();

  while (state->next_misspelling()) {

    char * word = state->get_real_word(word0);

    //
    // check if it is in the replace list
    //

    if ((w = replace_list->lookup(word)) != 0) {
      state->replace(w);
      continue;
    }

    //
    // print the line with the misspelled word highlighted;
    //

    display_misspelled_word();

    //
    // print the suggestions and menu choices
    //

    const AspellWordList * suggestions = aspell_speller_suggest(speller, word, -1);
    AspellStringEnumeration * els = aspell_word_list_elements(suggestions);
    sug_con.resize(0);
    while (sug_con.size() != 10 
           && (w = aspell_string_enumeration_next(els)) != 0)
      sug_con.push_back(w);
    delete_aspell_string_enumeration(els);

    // disable suspend
    unsigned int suggestions_size = sug_con.size();
    unsigned int suggestions_mid = suggestions_size / 2;
    if (suggestions_size % 2) suggestions_mid++; // if odd
    word_choices->resize(0);
    for (unsigned int j = 0; j != suggestions_mid; ++j) {
      word_choices->push_back(Choice('0' + j+1, sug_con[j]));
      if (j + suggestions_mid != suggestions_size) 
        word_choices
          ->push_back(Choice(j+suggestions_mid+1 == 10 
                             ? '0' 
                             : '0' + j+suggestions_mid+1,
                             sug_con[j+suggestions_mid]));
    }
    //enable suspend
    display_menu();

  choice_prompt:

    prompt("? ");

  choice_loop:

    //
    // Handle the users choice
    //

    int choice;
    get_choice(choice);
      
    if (choice == '0') choice = '9' + 1;
    
    switch (mapping[choice]) {
    case Exit:
      goto exit_loop;
    case Abort: {
      prompt(_("Are you sure you want to abort (y/n)? "));
      get_choice(choice);
      /* TRANSLATORS: The user may input any of these characters to say "yes".
         MUST ONLY CONSIST OF ASCII CHARACTERS. */
      const char * yes_characters = _("Yy");
      if (strchr(yes_characters, choice) != 0)
        goto abort_loop;
      goto choice_prompt;
    }
    case Ignore:
      break;
    case IgnoreAll:
      aspell_speller_add_to_session(speller, word, -1);
      break;
    case Add:
      aspell_speller_add_to_personal(speller, word, -1);
      break;
    case AddLower:
      aspell_speller_add_lower_to_personal(speller, word, -1);
      break;
    case Replace:
    case ReplaceAll:
      // the string new_word is in the encoding of the document
      prompt(_("With: "));
      get_line(new_word);
      if (new_word.size() == 0)
        goto choice_prompt;
      if (new_word[0] >= '1' && new_word[0] < (char)suggestions_size + '1')
        new_word = sug_con[new_word[0]-'1'];
      state->replace(new_word);
      changed = true;
      if (mapping[choice] == ReplaceAll && (strcmp(word,new_word.str()) != 0))
        replace_list->replace(word, new_word);
      break;
    default:
      // the replasments are in the encoding of the document
      if (choice >= '1' && choice < (char)suggestions_size + '1') { 
        state->replace(sug_con[choice-'1']);
        changed = true;
      } else {
        error(_("Sorry that is an invalid choice!"));
        goto choice_loop;
      }
    }
  }
exit_loop:
  {
    aspell_speller_save_all_word_lists(speller);
    state.del(); // to close the file handles
    delete_aspell_checker(checker);
    delete_aspell_speller(speller);

    if (changed) {

      bool keep_backup = options->retrieve_bool("backup");
      if (keep_backup) {
        String backup_name = file_name;
        backup_name += ".bak";
        rename_file(file_name, backup_name);
      }
      rename_file(new_name, file_name);

    } else {

      remove_file(new_name);

    }
    
    //end_check();
    
    return;
  }
abort_loop:
  {
    state.del(); // to close the file handles
    delete_aspell_checker(checker);
    delete_aspell_speller(speller);

    remove_file(new_name);

    return;
  }
}

#define U (unsigned char)

void Mapping::to_aspell() 
{
  memset(this, 0, sizeof(Mapping));
  primary[Ignore    ] = 'i';
  reverse[U'i'] = Ignore;
  reverse[U' '] = Ignore;
  reverse[U'\n'] = Ignore;

  primary[IgnoreAll ] = 'I';
  reverse[U'I'] = IgnoreAll;

  primary[Replace   ] = 'r';
  reverse[U'r'] = Replace;

  primary[ReplaceAll] = 'R';
  reverse[U'R'] = ReplaceAll;

  primary[Add       ] = 'a';
  reverse[U'A'] = Add;
  reverse[U'a'] = Add;

  primary[AddLower  ] = 'l';
  reverse[U'L'] = AddLower;
  reverse[U'l'] = AddLower;

  primary[Abort     ] = 'b';
  reverse[U'b'] = Abort;
  reverse[U'B'] = Abort;
  reverse[control('c')] = Abort;

  primary[Exit      ] = 'x';
  reverse[U'x'] = Exit;
  reverse[U'X'] = Exit;
}

void Mapping::to_ispell() 
{
  memset(this, 0, sizeof(Mapping));
  primary[Ignore    ] = ' ';
  reverse[U' '] = Ignore;
  reverse[U'\n'] = Ignore;

  primary[IgnoreAll ] = 'A';
  reverse[U'A'] = IgnoreAll;
  reverse[U'a'] = IgnoreAll;

  primary[Replace   ] = 'R';
  reverse[U'R'] = ReplaceAll;
  reverse[U'r'] = Replace;

  primary[ReplaceAll] = 'E';
  reverse[U'E'] = ReplaceAll;
  reverse[U'e'] = Replace;

  primary[Add       ] = 'I';
  reverse[U'I'] = Add;
  reverse[U'i'] = Add;

  primary[AddLower  ] = 'U';
  reverse[U'U'] = AddLower;
  reverse[U'u'] = AddLower;

  primary[Abort     ] = 'Q';
  reverse[U'Q'] = Abort;
  reverse[U'q'] = Abort;
  reverse[control('c')] = Abort;

  primary[Exit      ] = 'X';
  reverse[U'X'] = Exit;
  reverse[U'x'] = Exit;
}
#undef U

///////////////////////////
//
// list
//

void list()
{
  AspellSpeller * speller = new_speller();
  AspellChecker * checker = new_checker(speller);

  state = new CheckerString(checker, speller,stdin,0,64);

  String word;
 
  while (state->next_misspelling()) {

    state->get_real_word(word);
    COUT.printl(word);

  }
  
  state.del(); // to close the file handles
  delete_aspell_speller(speller);
}

///////////////////////////
//
// convt
//

void convt()
{
  Conv conv;
  String buf1, buf2;
  const char * from = fix_encoding_str(args[0], buf1);
  const char * to   = fix_encoding_str(args[1], buf2);
  Normalize norm = NormNone;
  if (strcmp(from, "utf-8") == 0 && strcmp(to, "utf-8") != 0)
    norm = NormFrom;
  else if (strcmp(from, "utf-8") != 0 && strcmp(to, "utf-8") == 0)
    norm = NormTo;
  if (args.size() > 2) {
    for (String::iterator i = args[2].begin(); i != args[2].end(); ++i)
      *i = asc_tolower(*i);
    options->replace("normalize", "true");
    if (args[2] == "none")
      options->replace("normalize", "false");
    else if (args[2] == "internal")
      options->replace("norm-strict", "false");
    else if (args[2] == "strict")
      options->replace("norm-strict", "true");
    else
      EXIT_ON_ERR(options->replace("norm-form", args[2]));
  }
  EXIT_ON_ERR(conv.setup(*options, args[0], args[1], norm));
  String line;
  while (CIN.getline(line))
    COUT.printl(conv(line));
}

void normlz()
{
  options->replace("normalize", "true");
  const char * from = args.size() < 3 ? "utf-8" : args[0].str();
  const char * to   = args.size() < 3 ? "utf-8" : args[2].str();
  const char * intr = args.size() < 3 ? args[0].str() : args[1].str();
  String * form = (args.size() == 2   ? &args[1] 
                   : args.size() == 4 ? &args[3]
                   : 0);
  Normalize decode_norm = NormTo;
  if (form) {
    for (String::iterator i = form->begin(); i != form->end(); ++i)
      *i = asc_tolower(*i);
    if (*form == "internal") {
      options->replace("norm-strict", "false");
      decode_norm = NormNone;
    } else if (*form == "strict") {
      options->replace("norm-strict", "true");
      decode_norm = NormNone;
    }
    if (decode_norm == NormTo) EXIT_ON_ERR(options->replace("norm-form", *form));
  }
  Conv encode,decode;
  EXIT_ON_ERR(encode.setup(*options, from, intr, NormFrom));
  EXIT_ON_ERR(decode.setup(*options, intr, to, decode_norm));
  String line;
  while (CIN.getline(line))
    COUT.printl(decode(encode(line)));
}

///////////////////////////
//
// filter
//

void filter()
{
  //assert(setvbuf(stdin, 0, _IOLBF, 0) == 0);
  //assert(setvbuf(stdout, 0, _IOLBF, 0) == 0);
  CERR << _("Sorry \"filter\" is currently unimplemented.\n");
  exit(3);
}


///////////////////////////
//
// print_ver
//

void print_ver () {
  printf("@(#) International Ispell Version 3.1.20 " 
         "(but really Aspell %s)\n", aspell_version_string());
}

///////////////////////////////////////////////////////////////////////
//
// These functions use implementation details of the default speller
// module
//

class IstreamEnumeration : public StringEnumeration {
  FStream * in;
  String data;
public:
  IstreamEnumeration(FStream & i) : in(&i) {}
  StringEnumeration * clone() const {
    return new IstreamEnumeration(*this);
  }
  void assign (const StringEnumeration * other) {
    *this = *static_cast<const IstreamEnumeration *>(other);
  }
  Value next() {
    if (!in->getline(data)) return 0;
    else return data.c_str();
  }
  bool at_end() const {return *in;}
};

///////////////////////////
//
// clean
//

void clean()
{
  using namespace aspell::sp;

  bool strict = args.size() != 0 && args[0] == "strict";
  
  Config * config = options;

  CachePtr<LangImpl> lang;
  find_language(*config);
  PosibErr<LangImpl *> res = new_lang_impl(*config);
  if (res.has_err()) {print_error(res.get_err()->mesg); exit(1);}
  lang.reset(res.data);
  IstreamEnumeration in(CIN);
  WordListIterator wl_itr(&in, lang, &CERR);
  config->replace("validate-words", "true");
  config->replace("validate-affixes", "true");
  if (!strict)
    config->replace("clean-words", "true");
  config->replace("clean-affixes", "true");
  config->replace("skip-invalid-words", "true");
  wl_itr.init(*config);
  Conv oconv, oconv2;
  if (config->have("encoding")) {
    EXIT_ON_ERR(oconv.setup(*config, lang->charmap(), config->retrieve("encoding"), NormTo));
    oconv2.setup(*config, lang->charmap(), config->retrieve("encoding"), NormTo);
  } else {
    EXIT_ON_ERR(oconv.setup(*config, lang->charmap(), lang->data_encoding(), NormTo));
    oconv2.setup(*config, lang->charmap(), lang->data_encoding(), NormTo);
  }
  while (wl_itr.adv()) {
    if (*wl_itr->aff.str) 
      COUT.printf("%s/%s\n", oconv(wl_itr->word), oconv2(wl_itr->aff));
    else
      COUT.printl(oconv(wl_itr->word));
  }
}

///////////////////////////
//
// master
//

void dump (aspell::sp::Dict * lws, Convert * conv) 
{
  using namespace aspell::sp;

  switch (lws->basic_type) {
  case Dict::basic_dict:
    {
      Dictionary * ws = static_cast<Dictionary *>(lws);
      StackPtr<WordEntryEnumeration> els(ws->detailed_elements());
      WordEntry * wi;
      while (wi = els->next(), wi) {
        wi->write(COUT,*ws->lang(), conv);
        COUT << '\n';
      }
    }
    break;
  case Dict::multi_dict:
    {
      StackPtr<DictsEnumeration> els(lws->dictionaries());
      Dict * ws;
      while (ws = els->next(), ws) 
        dump (ws, conv);
    }
    break;
  default:
    abort();
  }
}

void dump (AspellSpeller * speller, 
           const AspellWordList * (* get_word_list)(AspellSpeller *))
{
  const AspellWordList * wl = get_word_list(speller);
  if (!wl) {print_error(aspell_speller_error_message(speller)); return;}
  AspellStringEnumeration * els = aspell_word_list_elements(wl);
  const char * w;
  while ( (w = aspell_string_enumeration_next(els)) != 0 ) {
    COUT.printl(w);
  }
}

void master () {
  using namespace aspell::sp;

  if (args.size() != 0) {
    options->replace("master", args[0].c_str());
  }

  Config * config = options;

  if (action == do_create) {
    
    find_language(*config);
    EXIT_ON_ERR(create_default_readonly_dict
                (new IstreamEnumeration(CIN),
                 *config));

  } else if (action == do_merge) {
    
    print_error(_("Can't merge a master word list yet. Sorry."));
    exit (1);
    
  } else if (action == do_dump) {

    AspellSpeller * speller = new_speller();
    dump(speller, aspell_speller_main_word_list);
    delete_aspell_speller(speller);
  }
}

///////////////////////////
//
// personal
//

void personal () {
  using namespace aspell::sp;

  if (args.size() != 0) {
    EXIT_ON_ERR(options->replace("personal", args[0]));
  }
  options->replace("module", "aspeller");
  if (action == do_create || action == do_merge) {
    CERR << _("Sorry \"create/merge personal\" is currently unimplemented.\n");
    exit(3);

    // FIXME
#if 0
    StackPtr<Speller> speller(new_speller(options));

    if (action == do_create) {
      if (file_exists(speller->config()->retrieve("personal-path"))) {
        print_error(_("Sorry I won't overwrite \"%s\""), 
                    speller->config()->retrieve("personal-path"));
        exit (1);
      }
      speller->personal_word_list().data->clear();
    }

    String word;
    while (CIN >> word) 
      speller->add_to_personal(word);

    speller->save_all_word_lists();
#endif

  } else { // action == do_dump

    AspellSpeller * speller = new_speller();
    dump(speller, aspell_speller_personal_word_list);
    delete_aspell_speller(speller);

  }
}

///////////////////////////
//
// repl
//

void repl() {
  using namespace aspell::sp;

  if (args.size() != 0) {
    options->replace("repl", args[0].c_str());
  }

  if (action == do_create || action == do_merge) {

    CERR << _("Sorry \"create/merge repl\" is currently unimplemented.\n");
    exit(3);

    // FIXME
#if 0
    SpellerImpl speller(options);

    if (action == do_create) {
      if (file_exists(speller->config()->retrieve("repl-path"))) {
        print_error(_("Sorry I won't overwrite \"%s\""),
                    speller->config()->retrieve("repl-path"));
        exit (1);
      }
      speller->personal_repl().clear();
    }
    
    try {
      String word,repl;

      while (true) {
        get_word_pair(word,repl,':');
        EXIT_ON_ERR(speller->store_repl(word,repl,false));
      }

    } catch (bad_cin) {}

    EXIT_ON_ERR(speller->personal_repl().synchronize());

#endif

  } else if (action == do_dump) {

    // FIXME: This is currently broken

    ReplacementDict * repl = new_default_replacement_dict();
    repl->load(options->retrieve("repl-path"), *options);
    StackPtr<WordEntryEnumeration> els(repl->detailed_elements());
    
    WordEntry * rl = 0;
    WordEntry words;
    Conv conv(setup_conv(repl->lang(), options));
    while ((rl = els->next())) {
      repl->repl_lookup(*rl, words);
      do {
        COUT << conv(rl->word) << ": " << conv(words.word) << "\n";
      } while (words.adv());
    }
    delete repl;
  }
}

//////////////////////////
//
// soundslike
//

void soundslike() {
  AspellLanguage * lang = new_language();
  String word;
  line_buffer();
  while (CIN.getline(word)) {
    const char * sl = aspell_language_to_soundslike(lang, word.str(), word.size());
    printf("%s\t%s\n", word.str(), sl);
  }
  delete_aspell_language(lang);
}

//////////////////////////
//
// munch
//

int put_word_munch(void *, const AspellWord * w)
{
  COUT << ' ' << w->str;
  return true;
}

void munch() 
{
  AspellLanguage * lang = new_language();
  String word;
  line_buffer();
  while (CIN.getline(word)) {
    COUT << word;
    aspell_language_munch(lang, word.str(), word.size(), put_word_munch, 0);
    COUT << '\n';
  }
  delete_aspell_language(lang);
}

//////////////////////////
//
// expand
//

int put_word_expand_1(void * d, const AspellWord * w)
{
  bool & prev = *static_cast<bool *>(d);
  if (prev) COUT << ' ';
  COUT << w->str;
  prev = true;
  return true;
}

int put_word_expand_2(void * d, const AspellWord * w)
{
  const char * str = static_cast<const char *>(d);
  COUT << str << ' ' << w->str << '\n';
  return true;
}

void expand() 
{
  int level = 1;
  if (args.size() > 0)
    level = atoi(args[0].c_str()); //FIXME: More verbose
  int limit = INT_MAX;
  if (args.size() > 1)
    limit = atoi(args[1].c_str());
  AspellLanguage * lang = new_language();
  String word;
  line_buffer();
  while (CIN.getline(word)) {
    if (level <= 2) {
      if (level == 2) COUT << word << ' ';
      bool prev = false;
      aspell_language_expand(lang, word.str(), word.size(), 
                             put_word_expand_1, &prev, 
                             limit);
      COUT << '\n';
    } else if (level >= 3) {
      aspell_language_expand(lang, word.str(), word.size(), 
                             put_word_expand_2, const_cast<char *>(word.str()), 
                             limit);
    }
  }
  delete_aspell_language(lang);
}

//////////////////////////
//
// combine
//

static void combine_aff(String & aff, const char * app)
{
  for (; *app; ++app) {
    if (!memchr(aff.c_str(),*app,aff.size()))
      aff.push_back(*app);
  }
}

static void print_wordaff(const String & base, const String & affs, Conv & oconv)
{
  if (base.empty()) return;
  COUT << oconv(base);
  if (affs.empty())
    COUT << '\n';
  else
    COUT.printf("/%s\n", oconv(affs));
}

static bool lower_equal(sp::LangImpl * l, ParmString a, ParmString b)
{
  if (a.size() != b.size()) return false;
  if (l->to_lower(a[0]) != l->to_lower(b[0])) return false;
  return memcmp(a + 1, b + 1, a.size() - 1) == 0;
}

void combine()
{
  using namespace sp;
  CachePtr<LangImpl> lang;
  find_language(*options);
  PosibErr<LangImpl *> res = new_lang_impl(*options);
  if (res.has_err()) {print_error(res.get_err()->mesg); exit(1);}
  lang.reset(res.data);
  Conv iconv(setup_conv(options, lang));
  Conv oconv(setup_conv(lang, options));
  String word;
  String base;
  String affs;
  line_buffer();
  while (CIN.getline(word)) {
    word = iconv(word);

    CharVector buf; buf.append(word.c_str(), word.size() + 1);
    char * w = buf.data();
    char * af = strchr(w, '/');
    size_t s;
    if (af != 0) {
      s = af - w;
      *af++ = '\0';
    } else {
      s = strlen(w);
      af = w + s;
    }

    if (lower_equal(lang, base, w)) {
      if (lang->is_lower(base.str())) {
        combine_aff(affs, af);
      } else {
        base = w;
        combine_aff(affs, af);
      }
    } else {
      print_wordaff(base, affs, oconv);
      base = w;
      affs = af;
    }

  }
  print_wordaff(base, affs, oconv);
}

//////////////////////////
//
// munch list
//

int get_word_ml(void * d, AspellWord * w) 
{
  String * str = static_cast<String *>(d);
  bool res = CIN.getline(*str);
  if (!res) return false;
  w->str = str->str();
  w->len = str->size();
  return true;
}

int put_word_ml(void *, const AspellWord * w)
{
  COUT << w->str << '\n';
  return true;
}

void munch_list()
{
  String word;
  AspellMunchListParms * parms = new_aspell_munch_list_parms();

  for (unsigned i = 0; i < args.size(); ++i) {
    if (args[i] == "simple")      ; // FIXME: Warning
    else if (args[i] == "single") parms->multi = false;
    else if (args[i] == "multi")  parms->multi = true;
    else if (args[i] == "keep")   parms->simplify = false;
    else 
    {
      print_error(_("\"%s\" is not a valid flag for the \"munch-list\" command."),
                  args[i]);
      exit(1);
    }
  }

  AspellLanguage * lang = new_language();
  aspell_munch_list(lang, get_word_ml, &word, put_word_ml, 0, parms);
  delete_aspell_munch_list_parms(parms);
  delete_aspell_language(lang);

}

//////////////////////////
//
// dump affix
//

void dump_affix()
{
  FStream in;
  EXIT_ON_ERR(aspell::sp::open_affix_file(*options, in));
  
  String line;
  while (in.getline(line))
    COUT << line << '\n';
}



///////////////////////////////////////////////////////////////////////


///////////////////////////
//
// print_help
//

void print_help_line(char abrv, char dont_abrv, const char * name, 
                     KeyInfoType type, const char * desc, bool no_dont = false) 
{
  String command;
  if (abrv != '\0') {
    command += '-';
    command += abrv;
    if (dont_abrv != '\0') {
      command += '|';
      command += '-';
      command += dont_abrv;
    }
    command += ',';
  }
  command += "--";
  if (type == KeyInfoBool && !no_dont) command += "[dont-]";
  if (type == KeyInfoList) command += "add|rem-";
  command += name;
  if (type == KeyInfoString || type == KeyInfoList) 
    command += "=<str>";
  if (type == KeyInfoInt)
    command += "=<int>";
  const char * tdesc = _(desc);
  printf("  %-27s %s\n", command.c_str(), tdesc); // FIXME: consider word wrapping
}

namespace aspell {
  PosibErr<ConfigModule *> get_dynamic_filter(Config * config, ParmStr value);
}

static const char * usage_text[] = 
{
  /* TRANSLATORS: These should all be formated to fit in 80 column or
     less */
  N_("Usage: aspell [options] <command>"),
  N_("<command> is one of:"),
  N_("  -?|usage         display a brief usage message"),
  N_("  help             display a detailed help message"),
  N_("  -c|check <file>  to check a file"),
  N_("  -a|pipe          \"ispell -a\" compatibility mode"),
  N_("  [dump] config    dumps the current configuration to stdout"),
  N_("  config <key>     prints the current value of an option"),
  N_("  [dump] dicts | filters | modes"),
  N_("    lists available dictionaries / filters / filter modes"),
  N_("[options] is any of the following:")
};
static const unsigned usage_text_size = sizeof(usage_text)/sizeof(const char *);

static const char * help_text[] = 
{
  usage_text[0],
  "",
  usage_text[1],
  usage_text[2],
  usage_text[3],
  usage_text[4],
  usage_text[5],
  N_("  list             produce a list of misspelled words from standard input"),
  usage_text[6],
  usage_text[7],
  N_("  soundslike       returns the sounds like equivalent for each word entered"),
  N_("  munch            generate possible root words and affixes"),
  N_("  expand [1-4]     expands affix flags"),
  N_("  clean [strict]   cleans a word list so that every line is a valid word"),
  //N_("  filter           passes standard input through filters"),
  N_("  -v|version       prints a version line"),
  N_("  munch-list [simple] [single|multi] [keep]"),
  N_("    reduce the size of a word list via affix compression"),
  N_("  conv <from> <to> [<norm-form>]"),
  N_("    converts from one encoding to another"),
  N_("  norm (<norm-map> | <from> <norm-map> <to>) [<norm-form>]"),
  N_("    perform Unicode normalization"),
  usage_text[8],
  usage_text[9],
  N_("  dump|create|merge master|personal|repl [<name>]"),
  N_("    dumps, creates or merges a master, personal, or replacement dictionary."),
  "",
  /* TRANSLATORS: "none", "internal" and "strict" are literal values
     and should not be translated. */
  N_("  <norm-form>      normalization form to use, either none, internal, or strict"),
  "",
  usage_text[10],
  ""
};
static const unsigned help_text_size = sizeof(help_text)/sizeof(const char *);

void print_help (bool verbose) {
  load_all_filters(options);
  if (verbose) {
    printf(_("\n"
             "Aspell %s.  Copyright 2000-2011 by Kevin Atkinson.\n"
             "\n"), aspell_version_string());
    for (unsigned i = 0; i < help_text_size; ++i)
      puts(gt_(help_text[i]));
  } else {
    for (unsigned i = 0; i < usage_text_size; ++i)
      puts(gt_(usage_text[i]));
  }
  StackPtr<KeyInfoEnumeration> els(options->possible_elements(true,false));
  const KeyInfo * k;
  while (k = els->next(), k) {
    if (k->desc == 0 || k->flags & KEYINFO_HIDDEN) continue;
    if (!verbose && !(k->flags & KEYINFO_COMMON)) continue;
    const PossibleOption * o = find_option(k->name);
    const char * name = k->name;
    print_help_line(o->abrv, 
                    strncmp((o+1)->name, "dont-", 5) == 0 ? (o+1)->abrv : '\0',
                    name, k->type, k->desc);
    if (verbose && strcmp(name, "mode") == 0) {
      for (const ModeAbrv * j = mode_abrvs;
           j != mode_abrvs_end;
           ++j)
      {
        print_help_line(j->abrv, '\0', j->mode, KeyInfoBool, j->desc, true);
      }
    }
  }

  if (verbose) {
    //
    putchar('\n');
    putchar('\n');
    puts(
      _("Available Dictionaries:\n"
        "    Dictionaries can be selected directly via the \"-d\" or \"master\"\n"
        "    option.  They can also be selected indirectly via the \"lang\",\n"
        "    \"variety\", and \"size\" options.\n"));
    
    const DictInfoList * dlist = get_dict_info_list(options);
    
    StackPtr<DictInfoEnumeration> dels(dlist->elements());
    
    const DictInfo * entry;
    
    while ( (entry = dels->next()) != 0) 
    {
      printf("  %s\n", entry->name);
    }


    //
    putchar('\n');
    putchar('\n');
    fputs(
      _("Available Filters (and associated options):\n"
        "    Filters can be added or removed via the \"filter\" option.\n"),
      stdout);
    for (Vector<ConfigModule>::const_iterator m = options->filter_modules.begin();
         m != options->filter_modules.end();
         ++m)
    {
      printf(_("\n  %s filter: %s\n"), m->name, gt_(m->desc));
      for (k = m->begin; k != m->end; ++k) {
        const PossibleOption * o = find_option(k->name);
        const char * name = k->name;
        const KeyInfo * ok = options->keyinfo(name + 2);
        if (k == ok) name += 2;
        print_help_line(o->abrv, 
                        strncmp((o+1)->name, "dont-", 5) == 0 ? (o+1)->abrv : '\0',
                        name, k->type, k->desc);
      }
    }

    //
    putchar('\n');
    putchar('\n');
    puts(  
      /* TRANSLATORS: This should be formated to fit in 80 column or less */
      _("Available Filter Modes:\n"
        "    Filter Modes are reconfigured combinations of filters optimized for\n"
        "    files of a specific type. A mode is selected via the \"mode\" option.\n"
        "    This will happen implicitly if Aspell is able to identify the file\n"
        "    type from the extension, and possibility the contents, of the file.\n"));
    
    EXIT_ON_ERR_SET(available_filter_modes(options), StringPairEnumeration *, els);
    StringPair sp;
    while (!els->at_end()) {
      sp = els->next();
      printf("  %-14s %s\n", sp.first, gt_(sp.second));
    }
    delete els;
  }
}

