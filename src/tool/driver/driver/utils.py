import sys
import subprocess as sp
from typing import Optional, Tuple
import os
import signal


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


def _errorcode_tostring(code: int) -> Optional[str]:
    if code < 0:
        try:
            return signal.strsignal(-code)
        except ValueError:
            return "unknown error"
    else:
        return None


def execute_process(*cmd: str) -> Tuple[int, str]:
    log(" ".join(cmd))
    cp = sp.run(cmd, stdout=sp.PIPE, stderr=sp.STDOUT, encoding="utf-8")
    output = cp.stdout
    if extra := _errorcode_tostring(cp.returncode):
        output += ("\n" if output else "") + extra
    return (cp.returncode, output)


def execute_compiler(*cmd: str) -> None:
    ret, stdout = execute_process(*cmd)
    if ret == 0:
        if stdout:
            warning(stdout)
    else:
        error(f"'{' '.join(cmd)}' failed\n" + stdout if stdout else "")
