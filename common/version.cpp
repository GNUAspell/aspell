#include "settings.h"

extern "C" const char * aspell_version_string() {
#ifdef NDEBUG
  return VERSION " NDEBUG";
#endif
  return VERSION;
}
