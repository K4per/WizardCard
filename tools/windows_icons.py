"""Check the Windows player's embedded icon against the original pre-graybox export."""
import ctypes
from ctypes import wintypes
import hashlib
import struct

# Godot 4.7.2 exports six PNG renditions of wizardcard-app-v1.png, sorted by width.
# SHA-256 of the concatenated RT_ICON images in the original 2.6.4 player.
ORIGINAL_ICON_DIGEST = "3c435a05a8a3368c125cd028d0e2271a5c70ae0c6786e032d83399d50cf0a028"
ICON_SIZES = [16, 32, 48, 64, 128, 256]


def verify_application_icon(path):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    ptr = ctypes.c_void_p

    def api(name, result, *args):
        function = getattr(kernel, name)
        function.argtypes = args
        function.restype = result
        return function

    def require(result):
        if not result:
            raise ctypes.WinError(ctypes.get_last_error())
        return result

    handle = require(api("LoadLibraryExW", ptr, wintypes.LPCWSTR, ptr, wintypes.DWORD)(str(path), None, 2))
    try:
        def read_resource(kind, name):
            resource = require(api("FindResourceW", ptr, ptr, ptr, ptr)(handle, name, kind))
            size = require(api("SizeofResource", wintypes.DWORD, ptr, ptr)(handle, resource))
            loaded = require(api("LoadResource", ptr, ptr, ptr)(handle, resource))
            return ctypes.string_at(require(api("LockResource", ptr, ptr)(loaded)), size)

        groups = []
        callback = ctypes.WINFUNCTYPE(wintypes.BOOL, ptr, ptr, ptr, ctypes.c_ssize_t)

        @callback
        def collect(_handle, _kind, name, _unused):
            groups.append(ctypes.wstring_at(name) if name and name > 65535 else name)
            return True

        enumerate_groups = api("EnumResourceNamesW", wintypes.BOOL, ptr, ptr, callback, ctypes.c_ssize_t)
        if not enumerate_groups(handle, 14, collect, 0) or len(groups) != 1:
            raise ValueError(f"Expected the original WizardCard icon group in {path.name}")
        name = ctypes.c_wchar_p(groups[0]) if isinstance(groups[0], str) else groups[0]
        group = read_resource(14, name)
        if len(group) != 6 + 14 * len(ICON_SIZES) or struct.unpack_from("<HHH", group) != (0, 1, len(ICON_SIZES)):
            raise ValueError(f"Unexpected WizardCard icon group in {path.name}")
        images = {}
        for width, height, _, _, _, _, size, ordinal in struct.iter_unpack("<BBBBHHIH", group[6:]):
            image = read_resource(3, ordinal)
            if height != width or len(image) != size:
                raise ValueError(f"Malformed WizardCard icon in {path.name}")
            images[width or 256] = image
        if sorted(images) != ICON_SIZES or hashlib.sha256(b"".join(images[s] for s in ICON_SIZES)).hexdigest() != ORIGINAL_ICON_DIGEST:
            raise ValueError(f"Expected the original WizardCard application icon in {path.name}")
        return ICON_SIZES.copy()
    finally:
        require(api("FreeLibrary", wintypes.BOOL, ptr)(handle))
