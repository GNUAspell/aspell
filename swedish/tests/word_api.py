"""Direct libaspell word checking, isolated from personal dictionaries."""
import ctypes as C
import ctypes.util
from pathlib import Path


def aspell_library():
    library = ctypes.util.find_library('aspell')
    if library:
        return library
    for path in ('/opt/homebrew/lib/libaspell.dylib',
                 '/usr/local/lib/libaspell.dylib'):
        if Path(path).is_file():
            return path
    return None

def bind(lib, name, args, result):
    function = getattr(lib, name)
    function.argtypes, function.restype = args, result
    return function


class Aspell:
    def __init__(self, root, home):
        library = aspell_library()
        if not library:
            raise RuntimeError('libaspell not found')
        self.lib = C.CDLL(library)
        config = bind(self.lib, 'new_aspell_config', [], C.c_void_p)()
        replace = bind(self.lib, 'aspell_config_replace', [C.c_void_p, C.c_char_p, C.c_char_p], C.c_int)
        create = bind(self.lib, 'new_aspell_speller', [C.c_void_p], C.c_void_p)
        error_number = bind(self.lib, 'aspell_error_number', [C.c_void_p], C.c_uint)
        error_message = bind(self.lib, 'aspell_error_message', [C.c_void_p], C.c_char_p)
        cast = bind(self.lib, 'to_aspell_speller', [C.c_void_p], C.c_void_p)
        delete_config = bind(self.lib, 'delete_aspell_config', [C.c_void_p], None)
        self.check = bind(self.lib, 'aspell_speller_check', [C.c_void_p, C.c_char_p, C.c_int], C.c_int)
        self.error_message = bind(self.lib, 'aspell_speller_error_message', [C.c_void_p], C.c_char_p)
        self.destroy = bind(self.lib, 'delete_aspell_speller', [C.c_void_p], None)
        settings = {'lang': 'sv', 'encoding': 'utf-8', 'home-dir': str(home),
                    'conf': '/dev/null', 'per-conf': '/dev/null',
                    'personal': 'empty.pws', 'repl': 'empty.prepl',
                    'run-together': 'false', 'ignore-case': 'false', 'ignore-accents': 'false'}
        if root is not None:
            settings.update({'local-data-dir': str(root), 'dict-dir': str(root), 'master': 'sv.rws'})
        try:
            for key, value in settings.items():
                if not replace(config, key.encode(), value.encode()):
                    raise RuntimeError(f'Unsupported Aspell setting: {key}')
            possible_error = create(config)
        finally:
            delete_config(config)
        if error_number(possible_error):
            message = error_message(possible_error).decode()
            bind(self.lib, 'delete_aspell_can_have_error', [C.c_void_p], None)(possible_error)
            raise RuntimeError(message)
        self.handle = cast(possible_error)

    def spell(self, word):
        encoded = word.encode('utf-8')
        result = self.check(self.handle, encoded, len(encoded))
        if result < 0:
            raise RuntimeError(self.error_message(self.handle).decode())
        return bool(result)

    def close(self):
        self.destroy(self.handle)
