"""Remove default Godot icons embedded in Windows export templates."""
import ctypes
from ctypes import wintypes


RT_ICON = 3
RT_GROUP_ICON = 14
# RESOURCE_DATA_NOT_FOUND, RESOURCE_TYPE_NOT_FOUND
NO_RESOURCES = {1812, 1813}


def api(name, result, *arguments):
    function = getattr(ctypes.WinDLL("kernel32", use_last_error=True), name)
    function.restype = result
    function.argtypes = arguments
    return function


def require(result):
    if not result:
        raise ctypes.WinError(ctypes.get_last_error())
    return result


def find_icons(path):
    ptr = ctypes.c_void_p
    name_callback = ctypes.WINFUNCTYPE(wintypes.BOOL, ptr, ptr, ptr, ctypes.c_ssize_t)
    lang_callback = ctypes.WINFUNCTYPE(wintypes.BOOL, ptr, ptr, ptr, wintypes.WORD, ctypes.c_ssize_t)
    names = api("EnumResourceNamesW", wintypes.BOOL, ptr, ptr, name_callback, ctypes.c_ssize_t)
    languages = api("EnumResourceLanguagesW", wintypes.BOOL, ptr, ptr, ptr, lang_callback, ctypes.c_ssize_t)
    load = api("LoadLibraryExW", ptr, wintypes.LPCWSTR, ptr, wintypes.DWORD)
    close = api("FreeLibrary", wintypes.BOOL, ptr)

    handle = require(load(str(path), None, 2))  # LOAD_LIBRARY_AS_DATAFILE
    entries = []
    try:
        for kind in (RT_ICON, RT_GROUP_ICON):
            resource_names = []

            @name_callback
            def add_name(_module, _kind, name, _data):
                # A resource name can be an ordinal or a string (including "").
                resource_names.append(ctypes.wstring_at(name) if name and name > 65535 else name)
                return True

            if not names(handle, kind, add_name, 0) and ctypes.get_last_error() not in NO_RESOURCES:
                raise ctypes.WinError(ctypes.get_last_error())
            for name in resource_names:
                @lang_callback
                def add_language(_module, _kind, _name, language, _data):
                    entries.append((kind, name, language))
                    return True

                key = ctypes.c_wchar_p(name) if isinstance(name, str) else name
                require(languages(handle, kind, key, add_language, 0))
    finally:
        require(close(handle))
    return entries


def strip_icons(path):
    entries = find_icons(path)
    if not entries:
        return
    ptr = ctypes.c_void_p
    begin = api("BeginUpdateResourceW", ptr, wintypes.LPCWSTR, wintypes.BOOL)
    update = api("UpdateResourceW", wintypes.BOOL, ptr, ptr, ptr, wintypes.WORD, ptr, wintypes.DWORD)
    end = api("EndUpdateResourceW", wintypes.BOOL, ptr, wintypes.BOOL)
    handle = require(begin(str(path), False))
    try:
        for kind, name, language in entries:
            key = ctypes.c_wchar_p(name) if isinstance(name, str) else name
            require(update(handle, kind, key, language, None, 0))
    except BaseException:
        end(handle, True)  # Discard pending changes.
        raise
    require(end(handle, False))
    if find_icons(path):
        raise ValueError(f"Default template icon resources remain in {path.name}")
