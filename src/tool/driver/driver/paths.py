from typing import Optional, List
import utils
import os


def search_path(
    name: str,
    search_names: Optional[List[str]] = None,
    extra_paths: Optional[List[str]] = None,
    default: Optional[str] = None,
) -> Optional[str]:
    """search paths for a given file"""

    if default is not None:
        return default

    if search_names is None:
        search_names = [name]

    # 1. search user paths
    # 2. search the local lib paths
    # 3. search PATH
    paths_to_search = []

    if extra_paths:
        paths_to_search.extend(extra_paths)

    # in an install, this is pebl/bin/driver
    # if so, there is a pebl/lib/, which should be seached
    # NOTE: should be sperate, as `normpath` may fail if `lib` doesn't exist
    script_dir = os.path.dirname(__file__)
    pebl_dir = os.path.normpath(os.path.join(script_dir, os.pardir, os.pardir))
    lib_dir = os.path.join(pebl_dir, "lib")
    if os.path.exists(lib_dir):
        paths_to_search.append(lib_dir)
    bin_dir = os.path.join(pebl_dir, "bin")
    if os.path.exists(bin_dir):
        paths_to_search.append(bin_dir)

    if PATH := os.environ.get("PATH", None):
        paths_to_search.extend(PATH.split(os.pathsep))

    # run search
    for p in paths_to_search:
        utils.log(f"searching ('{p}') for '{name}'", verbose_level=2)
        for n in search_names:
            name = os.path.join(p, n)
            if os.path.exists(name):
                utils.log(f"found '{name}'", verbose_level=2)
                return name

    return None


def getpathext(path: str) -> str:
    return os.path.splitext(path)[1]


def getpathbase(path: str) -> str:
    return os.path.splitext(path)[0]


def is_obj_file(path: str) -> bool:
    return getpathext(path) == ".o"


def is_llvmir_file(path: str) -> bool:
    return getpathext(path) == ".ll"


def is_c_source(path: str) -> bool:
    return getpathext(path) == ".c"


def is_pebl_source(path: str) -> bool:
    return getpathext(path) == ".pebl"
