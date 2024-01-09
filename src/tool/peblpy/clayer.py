import ctypes as ct
import os
from typing import Optional, List


def search_path(
    name: str,
    extra_paths: Optional[List[str]] = None,
) -> Optional[str]:
    """search paths for a given file"""

    paths_to_search = []

    if extra_paths:
        paths_to_search.extend(extra_paths)

    if PATH := os.environ.get("PATH", None):
        paths_to_search.extend(PATH.split(os.pathsep))

    # run search
    for p in paths_to_search:
        path = os.path.join(p, name)
        if os.path.exists(path):
            return path

    return None


libname = "libpeblc.so"
libpath = search_path(libname, extra_paths=["build-release/lib/compiler"])
if not libpath:
    print(f"could not find '{libname}'")
    exit(1)
lib = ct.cdll.LoadLibrary(libpath)
