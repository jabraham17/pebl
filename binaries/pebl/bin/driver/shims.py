import sys

# add a dummy @override for previous versions
if sys.version_info[1] < 12:

    def override(func):
        return func

else:
    import typing

    def override(func):
        return typing.override(func)
