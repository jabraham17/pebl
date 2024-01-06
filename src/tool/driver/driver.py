#!/usr/bin/env python3
import enum
import os
import sys
import tempfile
from typing import Any, Callable, Dict, List, Optional, Tuple
import argparse as ap
import subprocess as sp
import glob
import shutil
import multiprocessing as mp
from functools import partial
from dataclasses import dataclass, field
from concurrent.futures import Executor

if sys.version_info[0] < 3 or sys.version_info[1] < 8:
    print("python version 8 or higher is required")
    exit(1)

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "driver")))
import utils
import paths
import mp
import arguments
from shims import override
import optimization


@dataclass
class TempDirectory:
    path: str

    def exists(self) -> bool:
        return os.path.exists(self.path)

    def remove(self):
        shutil.rmtree(self.path)

    def makedirs(self):
        os.makedirs(self.path)

    def get_file(self, basename: str, prefix: str = "", suffix: str = "") -> str:
        return os.path.join(self.path, f"{prefix}{basename}{suffix}")


@dataclass
class Executable:
    path: str
    arguments: List[str] = field(default_factory=list)

    def get_cmd(self, *extra_args: str) -> List[str]:
        cmd = [self.path] + self.arguments + list(extra_args)
        return cmd

    def execute(self, *extra_args: str) -> None:
        cmd = self.get_cmd(*extra_args)
        ret, stdout = utils.execute_process(*cmd)
        if ret == 0:
            if stdout:
                utils.warning(stdout)
        else:
            utils.error(f"'{' '.join(cmd)}' failed\n" + stdout if stdout else "")


@dataclass
class LLVMIrOptimizer(Executable):
    passes: List[str] = field(default_factory=list)

    @override
    def get_cmd(self, *extra_args: str) -> List[str]:
        cmd = super().get_cmd(*extra_args)
        if len(self.passes) > 0:
            pass_cmd = "-passes=" + ",".join(self.passes)
            cmd.append(pass_cmd)
        return cmd


@dataclass
class Toolchain:
    """a collection of programs to compile code"""

    # executables
    pebl_compiler: Optional[Executable] = None
    llvm_ir_assembler: Optional[Executable] = None
    llvm_ir_optimizer: Optional[Executable] = None
    linker: Optional[Executable] = None
    archiver: Optional[Executable] = None


@dataclass
class Libraries:
    """collection of pebl libraries"""

    runtime: str
    stdlib: str
    startup: str


class StopAfter(enum.Enum):
    COMPILE = enum.auto()
    OPTIMIZE = enum.auto()


def build_file(
    file: str,
    toolchain: Toolchain,
    temp_dir: TempDirectory,
    stop_after: Optional[StopAfter] = None,
    outfile: Optional[str] = None,
) -> str:
    assert (
        toolchain.pebl_compiler != None
        and toolchain.llvm_ir_optimizer != None
        and toolchain.llvm_ir_assembler != None
    )
    assert toolchain.linker != None

    basename = paths.getpathbase(os.path.basename(file))

    # compile file to ir
    ifile = file
    should_stop_after = stop_after == StopAfter.COMPILE
    ofile = (
        outfile
        if should_stop_after and outfile
        else temp_dir.get_file(basename, suffix=".ll")
    )
    toolchain.pebl_compiler.execute(ifile, "-output", ofile)
    if should_stop_after:
        return ofile

    # optimize ir
    ifile = ofile
    should_stop_after = stop_after == StopAfter.OPTIMIZE
    ofile = (
        outfile
        if should_stop_after and outfile
        else temp_dir.get_file(basename, suffix="-opt.ll")
    )
    toolchain.llvm_ir_optimizer.execute(ifile, "-o", ofile, "-S")
    if should_stop_after:
        return ofile

    # assemble ir to obj
    ifile = ofile
    ofile = outfile if outfile else temp_dir.get_file(basename, suffix=".o")
    toolchain.llvm_ir_assembler.execute(
        ifile,
        "-o",
        ofile,
        "-relocation-model=pic",
        "-filetype=obj",
    )
    return ofile


def build_executable(
    pool: Executor,
    files: List[str],
    toolchain: Toolchain,
    libs: Libraries,
    temp_dir: TempDirectory,
    outfile: str,
):
    assert toolchain.linker != None
    pebl_files = []
    obj_files = []
    for f in files:
        if paths.is_pebl_source(f):
            pebl_files.append(f)
        elif paths.is_obj_file(f):
            obj_files.append(f)
        else:
            utils.error(f"unknown file extension for '{f}'")

    compiled_obj_files = pool.map(
        partial(build_file, toolchain=toolchain, temp_dir=temp_dir), pebl_files
    )
    objects = obj_files + list(compiled_obj_files)
    toolchain.linker.execute(
        "-o", outfile, *objects, libs.stdlib, libs.runtime, libs.startup
    )


def main(raw_args: List[str]) -> int:
    args = arguments.parse_args(raw_args)
    utils.verbose = args.verbose
    arguments.validate_args(args)
    args = arguments.set_defaults(args)

    #
    # collect the toolchain
    #
    toolchain = Toolchain()

    def wrap_executable(name: str, path: Optional[str]) -> Executable:
        path_ = ""
        if path is not None:
            path_ = path
        else:
            utils.error(f"could not find '{name}'")
        return Executable(path_)

    def get_llvm_names(name: str, LLVM_VERSION=17) -> List[str]:
        return [f"{name}-{LLVM_VERSION}", name]

    def search_path_for_llvm(name: str) -> Optional[str]:
        return paths.search_path(
            name, search_names=get_llvm_names(name), extra_paths=args.paths
        )

    toolchain.llvm_ir_optimizer = wrap_executable("opt", search_path_for_llvm("opt"))
    toolchain.llvm_ir_assembler = wrap_executable("llc", search_path_for_llvm("llc"))
    toolchain.archiver = wrap_executable(
        "ar",
        paths.search_path(
            "ar",
            search_names=get_llvm_names("llvm-ar") + ["ar"],
            extra_paths=args.paths,
        ),
    )

    toolchain.pebl_compiler = wrap_executable(
        "peblc", paths.search_path("peblc", extra_paths=args.paths, default=args.peblc)
    )
    toolchain.linker = wrap_executable(
        "linker",
        paths.search_path(
            "ld",
            search_names=["clang", "gcc"],
            extra_paths=args.paths,
            default=args.linker,
        ),
    )

    # 
    # add flags to peblc
    # 
    if args.debug:
        toolchain.pebl_compiler.arguments.append("-g")

    #
    # build the opt pipeline
    #
    opt_path = toolchain.llvm_ir_optimizer.path
    toolchain.llvm_ir_optimizer = LLVMIrOptimizer(opt_path, passes=args.opt.passes)

    #
    # find the libraries
    #
    def find_library(name: str) -> str:
        if path := paths.search_path(name, extra_paths=args.paths):
            return path
        else:
            utils.error(f"could not find '{name}'")
        return ""

    libraries = Libraries(
        find_library("libpebl_runtime.a"),
        find_library("libpebl_stdlib.a"),
        find_library("libpebl_start.a"),
    )

    #
    # build tempdir
    #

    if args.temp_dir_name is None:
        temp_dir = TempDirectory(tempfile.mkdtemp())
    else:
        temp_dir = TempDirectory(args.temp_dir_name)
        if temp_dir.exists():
            temp_dir.remove()
        temp_dir.makedirs()

    if args.compile:
        stop_after = None
        if args.human_readable:
            stop_after = (
                StopAfter.COMPILE
                if args.opt == optimization.OptNone
                else StopAfter.OPTIMIZE
            )

        file = args.files[0]
        build_file(file, toolchain, temp_dir, stop_after, args.output)
    else:
        with mp.get_pool(args.jobs) as pool:
            build_executable(
                pool, args.files, toolchain, libraries, temp_dir, args.output
            )
    #
    # cleanup
    #
    if not args.keep_temp_dir and temp_dir.exists():
        temp_dir.remove()

    return 0


if __name__ == "__main__":
    try:
        exit(main(sys.argv[1:]))
    except utils.UnhandledError:
        exit(1)
