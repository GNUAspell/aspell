
#include "gettext.h"

#if ENABLE_NLS

#include "lock.hpp"

static aspell::Mutex lock;

static bool did_init = false;

extern "C" void aspell_gettext_init()
{
  {
    aspell::Lock l(&lock);
    if (did_init) return;
    did_init = true;
  }
  bindtextdomain("aspell", LOCALEDIR);
}

#else

extern "C" void aspell_gettext_init()
{
}

#endif
