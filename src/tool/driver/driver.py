#!/usr/bin/env python3
import os
import sys
import tempfile
from typing import Any, List, Optional, Tuple
import argparse as ap
import subprocess as sp
import glob
import shutil
import multiprocessing as mp
from concurrent.futures import ProcessPoolExecutor, Executor, ThreadPoolExecutor
from functools import partial

if sys.version_info[0] < 3 or sys.version_info[1] < 8:
    print("python version 8 or higher is required")
    exit(1)


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


def find_path_tool(
    name: str, search_names: Optional[List[str]] = None
) -> Optional[str]:
    """find executables in PATH"""
    PATH = os.environ.get("PATH", None)
    if not PATH:
        return None

    if search_names is None:
        search_names = [name]

    for p in PATH.split(os.pathsep):
        log(f"searching ('{p}') for '{name}'", verbose_level=2)
        for n in search_names:
            toolpath = os.path.join(p, n)
            if os.path.exists(toolpath):
                log(f"found '{toolpath}'", verbose_level=2)
                return toolpath
    return None


def find_local_tool(
    name: str, search_names: Optional[List[str]] = None
) -> Optional[str]:
    """find executables in same directory as this script"""
    script_dir = os.path.dirname(__file__)

    if search_names is None:
        search_names = [name]

    for n in search_names:
        toolpath = os.path.join(script_dir, n)
        if os.path.exists(toolpath):
            log(f"found '{toolpath}'", verbose_level=2)
            return toolpath
    return None


def find_llvm_tool(
    toolname: str, LLVM_INSTALL: Optional[str] = None, LLVM_VERSION: int = 17
) -> Optional[str]:
    """finds an llvm tool like 'llc'"""
    if LLVM_INSTALL:
        log(f"searching LLVM path ('{LLVM_INSTALL}') for '{toolname}'", verbose_level=2)
        toolpath = os.path.join(LLVM_INSTALL, "bin", toolname)
        if os.path.exists(toolpath):
            log(f"found '{toolpath}'", verbose_level=2)
            return toolpath

    # search for both 'toolname' and 'toolname-versionNum', prefer version num specifc
    return find_path_tool(toolname, [f"{toolname}-{LLVM_VERSION}", toolname])

def is_obj_file(path: str) -> bool:
    ext = os.path.splitext(path)[1]
    return ext == ".o"

def is_pebl_source(path: str) -> bool:
    ext = os.path.splitext(path)[1]
    return ext == ".pebl"

def compile_pebl_source(
    source_file: str, compiler: str, opt: str, llc: str, temp_dir: str
) -> str:
    """
    compile a pebl source file to an object file.
    returns the compiled object file path
    """
    if not is_pebl_source(source_file):
        error(f"unknown file extension for '{source_file}'")
    basename = os.path.splitext(os.path.basename(source_file))[0]

    asm_filename = os.path.join(temp_dir, f"{basename}.ll")
    asm_opt_filename = os.path.join(temp_dir, f"{basename}-opt.ll")
    object_filename = os.path.join(temp_dir, f"{basename}.o")

    execute_compiler(compiler, source_file, "-output", asm_filename)
    execute_compiler(opt, asm_filename, "-o", asm_opt_filename, "-S", "-passes=mem2reg")
    execute_compiler(
        llc,
        asm_opt_filename,
        "-o",
        object_filename,
        "-relocation-model=pic",
        "-filetype=obj",
    )

    return object_filename


def build_pebl_files(
    pool: Executor, sources: List[str], compiler: str, opt: str, llc: str, temp_dir: str
) -> List[str]:
    """
    compile all the source files
    """
    objects = pool.map(
        partial(
            compile_pebl_source, compiler=compiler, opt=opt, llc=llc, temp_dir=temp_dir
        ),
        sources,
    )

    return list(objects)


def build_standard_library(
    pool: Executor,
    std_library: str,
    compiler: str,
    opt: str,
    llc: str,
    ar: str,
    temp_dir: str,
) -> str:
    """
    compile the standard library into an archive
    returns the compiled object file path
    """
    sources = glob.glob(os.path.join(std_library, "**", "*.pebl"), recursive=True)
    objects = pool.map(
        partial(
            compile_pebl_source, compiler=compiler, opt=opt, llc=llc, temp_dir=temp_dir
        ),
        sources,
    )

    archive = os.path.join(temp_dir, "std-pebl.a")
    execute_compiler(ar, "rcs", archive, *objects)

    return archive


def compile_c_source(source_file: str, compiler: str, temp_dir: str) -> str:
    """
    compile a c source file to an object file.
    returns the compiled object file path
    """
    ext = os.path.splitext(source_file)[1]
    if ext != ".c":
        error(f"unknown file extension {ext}")
    basename = os.path.splitext(os.path.basename(source_file))[0]

    object_filename = os.path.join(temp_dir, f"{basename}.o")

    execute_compiler(compiler, "-c", source_file, "-o", object_filename)

    return object_filename


def build_runtime(
    pool: Executor, runtime: str, compiler: str, ar: str, temp_dir: str
) -> str:
    """
    compile the runtime into an archive
    returns the compiled object file path
    """
    sources = glob.glob(os.path.join(runtime, "**", "*.c"), recursive=True)
    objects = pool.map(
        partial(compile_c_source, compiler=compiler, temp_dir=temp_dir), sources
    )

    archive = os.path.join(temp_dir, "runtime.a")
    execute_compiler(ar, "rcs", archive, *objects)

    return archive


def link_executable(objects: List[str], linker: str, outfile: str):
    """execute the linker and get an executable"""
    execute_compiler(linker, "-o", outfile, *objects)
    return outfile


def main(raw_args: List[str]) -> int:
    # hack to allow a `--help-hidden`
    AP = ap.ArgumentParser(add_help=False)
    AP.add_argument(
        "--help-hidden", action="store_true", default=False, help=ap.SUPPRESS
    )
    internal_args, _ = AP.parse_known_args(raw_args)
    del AP

    AP = ap.ArgumentParser(formatter_class=ap.ArgumentDefaultsHelpFormatter)
    AP.add_argument("files", nargs="+", type=str, help="pebl files to compile")
    AP.add_argument("-o", "--output", default=None)
    AP.add_argument("-c", "--just-compile", default=False, action='store_true')
    AP.add_argument(
        "--llvm-install", default=None, help="llvm install path to search for tools"
    )
    AP.add_argument(
        "-v",
        "--verbose",
        action="count",
        default=0,
        help="run in verbose mode, specify multiple times for increasing verbosity",
    )
    AP.add_argument(
        "-j",
        "--num-processes",
        type=int,
        default=4,
        help="number of parallel processes to execute",
    )
    AP.add_argument(
        "--help-hidden", action="store_true", default=False, help=ap.SUPPRESS
    )

    SUPPRESS = lambda x: x if internal_args.help_hidden else ap.SUPPRESS
    # internal debug flags
    AP.add_argument(
        "--pebl-compiler",
        default=None,
        help=SUPPRESS("directly pass the peblc compiler to use"),
    )
    AP.add_argument(
        "--c-compiler",
        default=None,
        help=SUPPRESS("directly pass the c compiler to use"),
    )
    AP.add_argument(
        "--runtime",
        default="runtime",
        help=SUPPRESS("specify the runtime library path"),
    )
    AP.add_argument(
        "--standard-library",
        default="library",
        help=SUPPRESS("specify the standard library path"),
    )
    AP.add_argument(
        "--temp-dir-name", default=None, help=SUPPRESS("temp directory name to use")
    )
    AP.add_argument(
        "--keep-temp-dir", default=False, action="store_true", help=SUPPRESS("")
    )

    args = AP.parse_args(raw_args + (["-h"] if internal_args.help_hidden else []))

    global verbose
    verbose = args.verbose

    opt = ""
    if opt_ := find_llvm_tool("opt", LLVM_INSTALL=args.llvm_install):
        opt = opt_
    else:
        error("could not find 'opt'")
    llc = ""
    if llc_ := find_llvm_tool("llc", LLVM_INSTALL=args.llvm_install):
        llc = llc_
    else:
        error("could not find 'llc'")
    ar = ""
    if ar_ := find_llvm_tool("llvm-ar", LLVM_INSTALL=args.llvm_install):
        ar = ar_
    elif ar_ := find_path_tool("ar"):
        ar = ar_
    else:
        error("could not find 'ar'")

    pebl_compiler = args.pebl_compiler
    if pebl_compiler is None:
        if pc_ := find_local_tool("peblc"):
            pebl_compiler = pc_
        elif pc_ := find_path_tool("peblc"):
            pebl_compiler = pc_
        else:
            error(f"could not find pebl compiler")

    c_compiler = args.c_compiler
    if c_compiler is None:
        if cc_ := find_path_tool("cc", ["clang", "gcc", "cc"]):
            c_compiler = cc_
        else:
            error(f"could not find c compiler")

    linker = ""
    if ld_ := find_path_tool("ld", ["clang", "gcc"]):
        linker = ld_
    else:
        error(f"could not find linker")

    runtime = args.runtime
    if not os.path.exists(runtime):
        error(f"{runtime} does not exist")

    std_library = args.standard_library
    if not os.path.exists(std_library):
        error(f"{std_library} does not exist")

    temp_dir = args.temp_dir_name
    if temp_dir is None:
        temp_dir = tempfile.mkdtemp()
    else:
        if os.path.exists(temp_dir):
            shutil.rmtree(temp_dir)
        os.makedirs(temp_dir)

    files = args.files
    just_compile = args.just_compile
    if just_compile and len(files) > 1:
        error("cannot specify multiple files with '-c'")
    outfile = args.output
    if outfile is None:
        if just_compile:
            outfile = f"{os.path.splitext(os.path.basename(files[0]))[0]}.o"
        else:
            outfile = 'a.out'
    num_processes = args.num_processes

    def pool_init(v: bool):
        global verbose
        verbose = v

    if just_compile:
        obj = compile_pebl_source(files[0], pebl_compiler, opt, llc, temp_dir)
        shutil.copyfile(obj, outfile)
    else:
        with ProcessPoolExecutor(
            max_workers=num_processes, initializer=pool_init, initargs=(args.verbose,)
        ) as pool:
            object_files = []
            source_files = []
            for f in files:
                if is_pebl_source(f):
                    source_files.append(f)
                elif is_obj_file(f):
                    object_files.append(f)
                else:
                    warning(f"ignoring '{f}': unknown file type")
            object_files += build_pebl_files(pool, source_files, pebl_compiler, opt, llc, temp_dir)
            std_lib_a = build_standard_library(
                pool, std_library, pebl_compiler, opt, llc, ar, temp_dir
            )
            runtime_a = build_runtime(pool, runtime, c_compiler, ar, temp_dir)

            objects = object_files + [std_lib_a, runtime_a]
            link_executable(objects, linker, outfile)

    # cleanup
    if not args.keep_temp_dir and os.path.exists(temp_dir):
        shutil.rmtree(temp_dir)

    return 0


if __name__ == "__main__":
    try:
        exit(main(sys.argv[1:]))
    except UnhandledError:
        exit(1)
