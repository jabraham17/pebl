from typing import List, Union, Any


class Passes:
    def __init__(self, name: str, passes: List[Union[str, Any]]):
        self.name = name
        self.passes: List[str] = []
        for p in passes:
            if isinstance(p, Passes):
                self.passes.extend(p.passes)
            else:
                self.passes.append(p)

    def __str__(self) -> str:
        return str(self.name)

    def __eq__(self, other: str):
        if isinstance(other, str):
            return self.name == other
        return False


OptNone = Passes("none", [])
OptBasic = Passes("basic", ["mem2reg"])
OptFull = Passes("full", ["default<O3>"])

Choices = [OptNone, OptBasic, OptFull]
