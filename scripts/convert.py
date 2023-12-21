import json
import sys
from typing import Any, Dict, List
import yaml
import os
import argparse as ap

def load_yaml(filename: str) -> Any:
    with open(filename, "r") as f:
        return yaml.load(f, yaml.SafeLoader)

class MyYamlDumper(yaml.Dumper):
    def __init__(self, *args, **kwargs):
        kwargs["sort_keys"] = False
        super().__init__(*args, **kwargs)

def to_yaml(d: Any, filename: str):
    with open(filename, "w") as f:
        return yaml.dump(d, f, Dumper=MyYamlDumper)

def load_json(filename: str) -> Any:
    with open(filename, "r") as f:
        return json.load(f)

def to_json(d: Any, filename: str):
    with open(filename, "w") as f:
        return json.dump(d, f, indent=2)

format_map = {
    "yml": (load_yaml, to_yaml),
    "yaml": (load_yaml, to_yaml),
    "json": (load_json, to_json)
}

def convert(file: str, new_format: str="yaml"):
    base, ext = os.path.splitext(file)
    ext = ext[1:] if ext.startswith(".") else ext
    new_file = f"{base}.{new_format}"

    if ext not in format_map: raise ValueError(f"unknown format '{ext}'")
    (load, _) = format_map[ext]

    if new_format not in format_map: raise ValueError(f"unknown format '{new_format}'")
    (_, to) = format_map[new_format]


    d = load(file)
    to(d, new_file)

def main(raw_args: List[str]) -> int:
    a = ap.ArgumentParser()
    a.add_argument("files", nargs="*", type=str)
    a.add_argument("-format", type=str, default="yaml", choices=list(format_map.keys()))
    args = a.parse_args(raw_args)

    for f in args.files:
        convert(f, new_format=args.format)

    return 0

    
if __name__ == "__main__":
    exit(main(sys.argv[1:]))
