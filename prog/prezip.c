/*
 * Copyright (c) 2005
 * Kevin Atkinson
 * Jose Da Silva
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation.  Kevin Atkinson makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * Bug fixes and enhancements by Jose Da Silva, 2005.
 */

/*
 * Format:
 *   <data> ::= 0x02 <line>+ 0x1F 0xFF
 *   <line> ::= <prefix> <rest>*
 *   <prefix> ::= 0x00..0x1D | 0x1E 0xFF* 0x00..0xFE
 *   <rest> ::= 0x20..0xFF | <escape>
 *   <escape> ::= 0x1F 0x20..0x3F
 *
 * To decompress:
 *   Take the first PREFIX_LEN characters from the previous line
 *   and concatenate that with the rest, unescaping as necessary.
 *   The PREFIX_LEN is the sum of the characters in <prefix>.
 *   To unescape take the second character of <escape> and subtract 0x20.
 *   If the prefix length is computed before unescaping characters.
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#if defined(__CYGWIN__) || defined (_WIN32)

#  include <io.h>
#  include <fcntl.h>

#  define SETBIN(fno)  _setmode( _fileno( fno ), _O_BINARY )

#else

#  define SETBIN(fno)

#endif

#define HEAD "prezip, a prefix delta compressor. Version 0.2.0, 2005-02-02\n"

typedef struct Word {
  char * str;
  size_t alloc;
} Word;

/* Insure Space for needed character(s) plus a '\0' */
#define INSURE_SPACE(cur, p, need, go_error)\
  if (p + need - (cur)->str > (cur)->alloc) {\
    size_t tmp = (cur)->alloc*3/2;\
    if (tmp <= (cur)->alloc) goto go_error;\
    (cur)->alloc = tmp;\
    char * pos = (cur)->str;\
    pos = (char *)realloc((cur)->str, tmp);\
    if (pos == NULL) goto go_error;\
    p += (pos - (cur)->str);\
    (cur)->str = pos;\
  }

/* Advance through "prefix" until reached "rest" of line */
#define ADV(w, c, go_error) while (c--) {if (*w == 0) goto go_error; ++w;}

int get_word(FILE * in, char * w) {
  int bufsize = 255;
  register int c;

  while (c = getc(in), c <= 32 && c != EOF);
  if (c != EOF) {
    do {
      *w++ = (char)(c);
    } while (c = getc(in), c > 32 && c != EOF && --bufsize);
  }
  *w = '\0';
  ungetc(c, in);
  if (c == EOF) return 0; /* done */
  if (bufsize)  return 1; /* normal return */
  return 3;               /* error, word larger than 255 chars */
}

int compress(char LFonly, char zipMode) {
  Word  w1,w2;
  Word  * prev = &w1;
  Word  * cur  = &w2;
  char  * w, * p;
  int   c,l;
  int retVal = 0;

  if ((w1.str = (char *)malloc(256)) == NULL) return 6;
  if ((w2.str = (char *)malloc(256)) == NULL) {
    free(w1.str); return 6;
  }
  w1.str[0] = 0;
  w1.alloc = w2.alloc = 256;

  SETBIN (stdout);

  switch (zipMode) {
  case 2:
    /* compression version 0x02 == "prezip-bin -z" */
    if (putc(2, stdout) < 0) goto error_out_c;

    c = 0;
    while (c != EOF) {
      /* get next line */
      w = cur->str;
      while (c = getc(stdin), c != '\n' && c != EOF) {
        if (c >= 32) {
          INSURE_SPACE(cur, w, 1, error_mem_c);
          *w++ = c;
        } else {
          INSURE_SPACE(cur, w, 2, error_mem_c);
          *w++ = 31;
          *w++ = c + 32;
        }
      }

      /* Remove trailing Carriage Return from input wordlist */
      if (c == '\n' && LFonly && *(w - 1) == 45 && *(w - 2) == 31) {
        --w; --w;
      }

      *w = 0;
      p = prev->str;
      w = cur->str;

      /* get the length of the prefix */
      l = 0;
      while (*w == *p && *p != '\0') {++p; ++l; ++w;}

      /* prefix compress, and write word */
      if (l >= 30) {
        if (putc(30, stdout) < 0) goto error_out_c;
        l -= 30;
        while (l >= 255) {
          if (putc(255, stdout) < 0) goto error_out_c;
          l -= 255;
        }
      }
      if (putc(l, stdout) < 0) goto error_out_c;
      if (fputs(w, stdout) < 0) goto error_out_c;

      /* swap prev and next */
      {
        Word * tmp = cur;
        cur = prev;
        prev = tmp;
      }
    }

    if (putc(31, stdout) < 0) goto error_out_c;
    if (putc(255, stdout) < 0) goto error_out_c;
    break;

  case 1:
    /* compression version 0x01 == "word-list-compress c" */
    p = prev->str;
    w = cur->str;

    while ((retVal = get_word(stdin, w)) == 1) {

      /* get the length of the prefix */
      l = 0;
      while (*w == *p && *p != '\0') {++p; ++l; ++w;}

      if (l++ > 31) {
        if (putc('\0', stdout) < 0) goto error_out_c;
      }
      if (putc(l, stdout) < 0) goto error_out_c;
      if (fputs(w, stdout) < 0) goto error_out_c;

      /* swap prev and next */
      {
        Word * tmp = cur;
        cur = prev;
        prev = tmp;
        p = prev->str;
        w = cur->str;
      }
    }
    break;
  }

  while (0) {
    error_mem_c: retVal = 6; break;     /* memory alloc error */
    error_out_c: retVal = 5; break;     /* output data error  */
    /*           retVal = 3;               corrupt input      */
  }
  if (fflush(stdout) < 0 && retVal == 0) retVal = 5;

  free(w2.str);
  free(w1.str);

  return retVal;
}

int decompress(void) {
  Word cur;
  int c;
  int retVal = 0;
  char * w;
  unsigned char ch;

  if ((w = cur.str = (char *)malloc(257)) == NULL) return 6;
  cur.alloc = 257;

  SETBIN (stdin);

  c = getc(stdin);

  switch (c) {
  case 2:
    /* decompression version 0x02 == "prezip-bin -d" */
    retVal = 4;
    *w = '\0';
    /* get the length of the prefix */
    c = getc(stdin);
    while (c != EOF && retVal == 4) {
      w = cur.str;
      if (c == 30) {
        ADV(w, c, error_in_d);
        while ((c = getc(stdin)) == 255) ADV(w, c, error_in_d);
        if (c < 0) goto error_EOF_d;
      }
      ADV(w, c, error_in_d);

      /* get the rest of the line */
      while ((c = getc(stdin)) > 30) {
        INSURE_SPACE(&cur, w, 1, error_mem_d);
        *w++ = (char)c;
      }
      *w = '\0';

      /* output complete line (retVal=0 for last line) */
      for (w = cur.str; *w; w++) {
        if (*w != 31) {
          if (putc(*w, stdout) < 0) goto error_out_d;
        } else {
          ++w;
          ch = *w;
          if (32 <= ch && ch < 64) {
            if (putc(ch - 32, stdout) < 0) goto error_out_d;
          } else {
            if (ch == 255 && *++w == '\0') retVal = 0;
            else                           retVal = 3;
            break;
          }
        }
      }
      if (retVal)
        putc('\n', stdout);
    }
    break;

  case 1:
    /* decompression version 0x01 == "word-list-compress d" */
    {
      int last_max = 0;
      while (c != -1) {
        if (c == 0)
          c = getc(stdin);
        --c;
        if (c > last_max || c < 0) goto error_in_d;
        last_max = c;
        w = cur.str + c;
        while ((c = getc(stdin)) > 32) {
          INSURE_SPACE(&cur, w, 2, error_mem_d);
          *w++ = (char)c; last_max++;
        }
        *w = '\n'; *++w = '\0';
        if (fputs(cur.str, stdout) < 0) goto error_out_d;
      }
    }
    break;

    /* a place to hold all '-d' error codes in one spot */
    error_mem_d: retVal = 6; break; /* memory alloc     */
    error_out_d: retVal = 5; break; /* output data      */
  case EOF:
    error_EOF_d: retVal = 4; break; /* unexpected EOF   */
    error_in_d:  retVal = 3; break; /* corrupt input    */
  default:
    retVal = 2;                     /* unknown format   */
  }

  free (cur.str);

  return retVal;
}

int main (int argc, const char *argv[]) {
  int retVal = 1;               /* default, expect commandline error */
  int nextOption;               /* check all options on commandline  */
  int thisOption;               /* check rest of this option param.  */
  char LFonly = 0;              /* Remove CR from DOS type word-list */
  char doThis = 0;              /* prezip should do this: 'c' or 'd' */
  char zipMode = 2;             /* choice of prezip compression 1..2 */

  /* test all options on commandline, select 'doThis', stop if error */
  for (nextOption = 1; nextOption < argc; ++nextOption) {
    thisOption = 0;
    if (argv[nextOption][thisOption] == '-') ++thisOption;
    switch (argv[nextOption][thisOption]) {
    case 'h':
      if (doThis) goto PrezipUsage;
      doThis = 'h';
      retVal = 0;
      break;
    case 'v': case 'V':
      if (doThis) goto PrezipUsage;
      doThis = 'v';
      break;
    case 'd':
      if (doThis) goto PrezipUsage;
      doThis = 'd';
      break;
    case 'c':
      if (doThis) goto PrezipUsage;
      doThis = 'c';
      zipMode = 1;      /* word-list-compress_ion */
      break;
    case 'z':
      if (doThis) goto PrezipUsage;
      doThis = 'c';
      zipMode = 2;      /* prezip-bin compression */
      break;
    case 'l':
      LFonly = 1;       /* autodetect & remove CR */
      break;

    default:
      goto PrezipUsage;
    }
  }

  switch (doThis) {
  case 'c':
    retVal = compress(LFonly, zipMode);
    break;
  case 'd':
    retVal = decompress();
    break;

  case 'v':
    fputs(HEAD, stdout);
    retVal = 0;
    break;

  default:
   PrezipUsage:
    retVal = 1;
  case 'h':
    fprintf(stdout, "%s"
        "Usage:\n"
        "  %s [options] <filein >fileout\n"
        "Option:\n"
        "  -c = Compress (word-list-compress)\n"
        "  -z = Compress (prezip-bin)\n"
        "  -d = Decompress\n"
        "  -h = this Help message\n"
        "  -v = %s Version\n"
        "Extra option for use with -z:\n"
        "  -l = Autodetect EOL <CR><LF>, Remove <CR>\n"
        , HEAD, argv[0], argv[0]);
  }

  assert(retVal >= 0);
  if (retVal > 1)
    fputs("ERROR: ", stderr);
  switch (retVal) {
  case 2:
    fputs("Unknown Format\n", stderr); break;
  case 3:
    fputs("Corrupt Input\n", stderr); break;
  case 4:
    fputs("Unexpected EOF\n", stderr); break;
  case 5:
    fputs("Output Data Error\n", stderr); break;
  case 6:
    fputs("Memory Allocation Error\n", stderr);
  }
  return retVal;
}
