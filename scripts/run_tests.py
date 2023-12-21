import glob
import json
import os
import re
import sys
from typing import Any, Callable, Dict, List, Tuple
import random
import string
import subprocess as sp
import shlex
import argparse as ap
import difflib

Result = Tuple[bool, str]


def warn(*args: Any, **kwargs: Any):
    kwargs["file"] = sys.stderr
    print("Warning: ", *args, **kwargs)


def error(*args: Any, **kwargs: Any):
    kwargs["file"] = sys.stderr
    print("Warning: ", *args, **kwargs)
    exit(1)


def static_vars(**kwargs: Any):
    def wrapper(func: Callable[[Any], Any]):
        for k in kwargs:
            setattr(func, k, kwargs.get(k))
        return func

    return wrapper


@static_vars(generated=set())
def gen_temp_name(len: int = 8, suffix: str = ".tmp", prefix: str = ""):
    alphabet = string.ascii_letters + string.digits
    while True:
        rand_part = "".join(random.choices(alphabet, k=len))
        name = f"{prefix}{rand_part}{suffix}"
        gen = getattr(gen_temp_name, "generated")
        if name not in gen:
            gen.add(name)
            setattr(gen_temp_name, "generated", gen)
            return name


def load_json_config(filename: str) -> Any:
    with open(filename, "r") as f:
        return json.load(f)


def load_yaml_config(filename: str) -> Any:
    try:
        import yaml

        with open(filename, "r") as f:
            return yaml.load(f, yaml.SafeLoader)

    except ModuleNotFoundError:
        error("could not find 'pyyaml', install with 'python3 -m pip install pyyaml'")


config_file_loaders = {
    "json": load_json_config,
    "yml": load_yaml_config,
    "yaml": load_yaml_config,
}


def load_config_file(filename: str) -> Any:
    _, ext = os.path.splitext(filename)
    ext = ext[1:] if ext.startswith(".") else ext

    if ext not in config_file_loaders:
        return None
    return config_file_loaders[ext](filename)


def is_config_filename(filename: str) -> bool:
    _, ext = os.path.splitext(filename)
    ext = ext[1:] if ext.startswith(".") else ext
    return ext in config_file_loaders


def readlines(filename: str) -> List[str]:
    with open(filename, "r") as fp:
        return fp.readlines()
    return []


def argsplit(args: str) -> List[str]:
    if sys.platform == "win32":
        return [args]
    else:
        return shlex.split(args)

class TestSuite:
    def __init__(
        self,
        path: str,
        extra_variables: Dict[str, str] = dict(),
        print_commands: bool = False,
    ):
        self.path: str = path
        self.config: Dict[Any, Any] = load_config_file(path)
        self.print_commands = print_commands

        # TODO: validate config

        for var, value in extra_variables.items():
            self.set_variable(var, value)

    def set_variable(self, varname: str, value: str):
        variables = self.config.get("variables", dict())
        variables[varname] = value
        self.config["variables"] = variables

    def has_variable(self, varname: str):
        return varname in self.config.get("variables", dict())

    def get_variable(self, varname: str) -> str:
        variables = self.config.get("variables", dict())
        v = variables.get(varname, None)
        return v

    def _replace_variables(self, s: str, extra_vars: Dict[str, str] = dict()):
        # replace variables
        def replace(m: re.Match) -> str:
            var = m[1]
            if var in extra_vars:
                return extra_vars.get(var, "")
            elif self.has_variable(var):
                v = self.get_variable(m[1])
                return self._replace_variables(v, extra_vars)
            else:
                # no vars, dont replace
                return m[0]

        s = re.sub(r"\$\{([a-zA-Z_0-9]+)\}", replace, s)
        s = s.replace("/", os.path.sep)
        # s = re.sub(r"\\{1}", "\\\\", s)
        return s

    def _run_single_test(self, idx: int, file: str, test_config: Dict) -> Result:
        extra_args = {"FILE": file}
        basename, ext = os.path.splitext(file)
        ext = ext[1:] if ext.startswith(".") else ext
        if ext != "pebl":
            warn(f"'{file}' does not have extension 'pebl'")
        if not os.path.exists(file):
            warn(f"'{file}' does not exist")

        comp_cmd = test_config.get("compile-cmd", None)
        if comp_cmd:
            comp_cmd = self._replace_variables(comp_cmd, extra_args)
            comp_cmd = argsplit(comp_cmd)
        exec_cmd = test_config.get("exec-cmd", None)
        if exec_cmd:
            exec_cmd = self._replace_variables(exec_cmd, extra_args)
            exec_cmd = argsplit(exec_cmd)
        good_file = test_config.get("good-file", None)
        if good_file:
            good_file = self._replace_variables(good_file, extra_args)

        if not good_file:
            good_file = f"{basename}.good"

        if not os.path.exists(good_file):
            warn(f"'{good_file}' does not exist")

        if not comp_cmd and not exec_cmd:
            return (False, "nothing to do")

        if self.print_commands:
            print(f"Running test {idx} for '{file}' from '{self.path}'")

        print(os.getcwd())

        outfilename = f"{basename}.{idx}.tmp.out"
        if os.path.exists(outfilename):
            os.remove(outfilename)
        with open(outfilename, "a") as outfile:
            if comp_cmd:
                if self.print_commands:
                    print("  " + " ".join(comp_cmd + [f"&>{outfilename}"]))
                cp = sp.run(comp_cmd, stdout=outfile, stderr=outfile)
                outfile.flush()

            if exec_cmd:
                if self.print_commands:
                    print("  " + " ".join(exec_cmd + [f"&>{outfilename}"]))
                cp = sp.run(exec_cmd, stdout=outfile, stderr=outfile)
                outfile.flush()


        outfile_lines = readlines(outfilename)
        goodfile_lines = readlines(good_file)
        diffres = list(difflib.context_diff(outfile_lines, goodfile_lines, fromfile=outfilename, tofile=good_file))
        if len(diffres) != 0:
            return (False, f"failed to match good file - '{' '.join(diffres)}'")

        # if we reach this point, its a presumed success
        os.remove(outfilename)

        return (True, "")

    def _run_test_file(self, file: str, test_configs: List[Dict]) -> List[Result]:
        results = []
        for idx, config in enumerate(test_configs):
            results.append(self._run_single_test(idx, file, config))
        return results

    def run_tests(self) -> List[Result]:
        # enter test dir
        pwd = os.getcwd()
        d = os.path.dirname(os.path.abspath(self.path))
        os.chdir(d)

        results = []
        tests = self.config.get("tests", [])
        for t in tests:
            file = t.get("file", None)
            configs = t.get("configs", [])
            if file:
                results.extend(self._run_test_file(file, configs))
            else:
                warn("missing 'file' in test config")

        os.chdir(pwd)

        return results


def main(raw_args: List[str]) -> int:
    a = ap.ArgumentParser()
    a.add_argument("paths", nargs="*")
    a.add_argument("-v", "--verbose", action="store_true", default=False)
    a.add_argument("-p", "--print-commands", action="store_true", default=False)
    args = a.parse_args(raw_args)

    extra_variables = {
        "ROOT": os.path.dirname(os.path.abspath(__file__)),
        "BIN_DIR": "${ROOT}/build/bin",
        "EXT": ""
    }
    if sys.platform == "win32":
        extra_variables["EXT"] = ".exe"

    files = []
    for p in args.paths:
        if os.path.isfile(p):
            if is_config_filename(p):
                files.append(p)
            else:
                warn(
                    f"unknown file ext for '{p}', expecting {{{', '.join([str(k) for k in config_file_loaders.keys()])}}}"
                )
        elif os.path.isdir(p):
            for k in config_file_loaders.keys():
                glob_path = os.path.join(f"{p}", "**", f"*.{k}")
                files.extend(glob.glob(glob_path, recursive=True))

    for f in files:
        ts = TestSuite(f, extra_variables, print_commands=args.print_commands)
        results = ts.run_tests()
        n_tests = len(results)
        n_passed = sum([1 for r in results if r[0]])

        width = 80
        header = f"Ran test from '{f}', {n_passed}/{n_tests} tests passed"
        print("=" * width)
        print(f"| {header}{' '*(width-len(header)-4)} |")
        print("=" * width)
        if args.verbose:
            for r in results:
                if not r[0]:
                    print(f"Failed: {r[1]}")

    return 1


if __name__ == "__main__":
    exit(main(sys.argv[1:]))
