import argparse as ap
from typing import Any, Dict, List, Optional
import utils
import paths
import os
import optimization


def validate_args(args: ap.Namespace) -> bool:
    for f in args.files:
        if not paths.is_pebl_source(f) and not paths.is_obj_file(f):
            utils.error(f"unknown file extension for '{f}'")

    if args.compile and len(args.files) > 1:
        utils.error("cannot specify multiple files with '--compile'")

    if args.human_readable and not args.compile:
        utils.error("cannot specify '--asm' without '--compile'")

    return True


def set_defaults(args: ap.Namespace) -> ap.Namespace:
    if not args.output:
        if args.compile:
            base = paths.getpathbase(os.path.basename(args.files[0]))
            if args.human_readable:
                args.output = f"{base}.ll"
            else:
                args.output = f"{base}.o"
        else:
            args.output = "a.out"

    return args


def parse_args(raw_args: List[str]) -> ap.Namespace:
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
    AP.add_argument("-c", "--compile", default=False, action="store_true")

    AP.add_argument(
        "-S",
        "--asm",
        dest="human_readable",
        default=False,
        action="store_true",
        help="output llvm bitcode",
    )
    AP.add_argument('-g', '--debug', default=False, action='store_true')

    class OptAction(ap.Action):
        @classmethod
        def valid_option_str(cls):
            return "{" + ", ".join([str(o) for o in optimization.Choices]) + "}"

        def __init__(self, *args: Any, **kwargs: Dict[str, Any]):
            kwargs["default"] = optimization.Choices[0]
            super().__init__(*args, **kwargs)

        def __call__(
            self,
            parser: ap.ArgumentParser,
            namespace: ap.Namespace,
            values: str,
            option_string: Optional[str] = None,
        ):
            opt = None
            for o in optimization.Choices:
                if o.name == values:
                    opt = o
                    break
            if opt:
                setattr(namespace, self.dest, opt)
            else:
                raise ap.ArgumentError(
                    self,
                    f"'{values}' is not a valid option - possible options are {OptAction.valid_option_str()}",
                )

    AP.add_argument(
        "--opt",
        "-opt",
        type=str,
        action=OptAction,
        help=f"which set of optimizations to apply - possible choices are {OptAction.valid_option_str()}",
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
        "--jobs",
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
        "--peblc",
        default=None,
        help=SUPPRESS("directly pass the peblc compiler to use"),
    )
    AP.add_argument(
        "--linker",
        default=None,
        help=SUPPRESS("directly pass the linker compiler to use"),
    )
    AP.add_argument(
        "--path",
        dest="paths",
        action="append",
        type=str,
        help=SUPPRESS("specify a path"),
    )
    AP.add_argument(
        "--temp-dir-name", default=None, help=SUPPRESS("temp directory name to use")
    )
    AP.add_argument(
        "--keep-temp-dir", default=False, action="store_true", help=SUPPRESS("")
    )

    args = AP.parse_args(raw_args + (["-h"] if internal_args.help_hidden else []))
    return args
