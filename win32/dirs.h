#ifndef dirs_h
#define dirs_h

#ifdef WIN32_USE_EXECUTABLE_DIR
# define DATA_DIR "<prefix/data-subdir>"
# define CONF_DIR "<prefix>"
# define DICT_DIR "<prefix/dict-subdir>"
# define PREFIX "!prefix"
#else
# define DATA_DIR "aspell-win32/data"
# define CONF_DIR "aspell-win32"
# define DICT_DIR "dicts"
# define PREFIX "aspell-win32"
#endif

#endif
