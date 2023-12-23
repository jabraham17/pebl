import sys
import subprocess as sp
from typing import Tuple

class UnhandledError(Exception):
    pass


global verbose
verbose = 0


def log(*args, **kwargs):
    verbose_level = kwargs.pop("verbose_level", 1)
    if verbose_level <= verbose:
        print(*args, **kwargs)


def warning(*args, **kwargs):
    kwargs["file"] = sys.stderr
    print("warning: ", *args, **kwargs)


def error(*args, **kwargs):
    kwargs["file"] = sys.stderr
    print("error:", *args, **kwargs)
    raise UnhandledError()


def execute_process(*cmd: str) -> Tuple[int, str]:
    log(" ".join(cmd))
    cp = sp.run(cmd, stdout=sp.PIPE, stderr=sp.STDOUT, encoding="utf-8")
    return (cp.returncode, cp.stdout)


def execute_compiler(*cmd: str) -> None:
    ret, stdout = execute_process(*cmd)
    if ret == 0:
        if stdout:
            warning(stdout)
    else:
        error(f"'{' '.join(cmd)}' failed\n" + stdout if stdout else "")
