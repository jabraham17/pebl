#!/usr/bin/env python3
import subprocess as sp
import re
import shutil
import os
import tarfile

def check_output(cmd):
    print(" ".join(cmd))
    return sp.check_output(cmd).decode("utf-8").strip().splitlines()

def check_call(cmd):
    print(" ".join(cmd))
    return sp.check_call(cmd)

def get_tags():
    output = check_output(["git", "tag", "--list"])
    return output

def get_last_tag_num():
    output = get_tags()
    nums = [0,]
    for l in output:
        if m:= re.match(r"^v(\d+)$", l):
            nums.append(int(m[1]))
    last= max(nums)
    return last

def get_last_tag():
    return f"v{get_last_tag_num()}"

def get_next_tag():
    return f"v{get_last_tag_num()+1}"


def build_compiler(build_dir, old_install_dir, new_install_dir):
    if os.path.exists(new_install_dir):
        shutil.rmtree(new_install_dir)

    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
    os.mkdir(build_dir)

    check_call(["cmake", "-S", ".", "-B", build_dir,"-G", "Ninja", f"-DCMAKE_INSTALL_PREFIX={new_install_dir}", "-DCMAKE_BUILD_TYPE=Release", "-DPEBL_SHARED_MODE=off"])
    check_call(["cmake", "--build", build_dir, "--target", "install", "--config", "Release"])

    shutil.rmtree(build_dir)

def package_compiler(name, source_dir):
    with tarfile.open(name, "w:gz") as tar:
        tar.add(source_dir, arcname=f"{os.path.basename(source_dir)}")

def commit_compiler(*files):
    check_call(["git", "add", *files])
    check_call(["git", "commit", "-mnew compiler binaries"])
    tag = get_next_tag()
    check_call(["git", "tag", tag])


build_dir = ".build-revision"
binaries_dir = "binaries"
old_install_dir = os.path.join(binaries_dir, "pebl")
new_install_dir = os.path.join(binaries_dir, "pebl-new")

# build the new compiler
build_compiler(build_dir, old_install_dir, new_install_dir)

# TODO: TEST NEW COMPILER
check_call(["./scripts/run_tests.sh", "test", "-D", f"INSTALL_DIR={os.path.join(os.getcwd(), new_install_dir)}"])

# remove the old and copy in the new
if os.path.exists(old_install_dir):
    shutil.rmtree(old_install_dir)
shutil.move(new_install_dir, old_install_dir)
install_dir = old_install_dir

# package the new compiler into a tar
new_tar = os.path.join(binaries_dir, f"{get_next_tag()}.tar.gz")
package_compiler(new_tar, install_dir)
old_tar = os.path.join(binaries_dir, f"{get_last_tag()}.tar.gz")
if os.path.exists(old_tar):
    os.remove(old_tar)

# put the new compiler under source control
commit_compiler(install_dir, new_tar, old_tar)
check_call(["git", "push", "origin", "--all"])
