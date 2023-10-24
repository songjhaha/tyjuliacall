#!/usr/bin/env python
from __future__ import annotations
from functools import cache
from pmakefile import * # type: ignore
from subprocess import call
import typing
import os
import sysconfig
import pathlib
import find_libpython

phony([
    'juliacall-windows-x64',
    'juliacall-windows-aarch64',
    'juliacall-linux-x64',
    'juliacall-linux-aarch64',
    'juliacall-macos-x64',
    'juliacall-macos-aarch64',
])

HOST_CC = 'zig cc'

if typing.TYPE_CHECKING:
    Platform = typing.Literal['windows', 'linux', 'macos']
    Architecure = typing.Literal['x64', 'aarch64']

def to_dll_path(base: str, platform: Platform):
    #根据不同的操作系统生成正确的文件路径。
    if platform == 'windows':
        return f'{base}.dll'
    elif platform == 'linux':
        return f'{base}.so'
    elif platform == 'macos':
        return f'{base}.dylib'
    else:
        raise ValueError(f'unknown platform {platform!r}')

def compute_zig_target(platform: Platform, arch: Architecure):
    if platform == 'windows':
        if arch == 'x64':
            return 'x86_64-windows-gnu'
        elif arch == 'aarch64':
            return 'aarch64-windows-gnu'
        else:
            log(f'unknown arch {arch!r}', 'error')
            exit(1)
    elif platform == 'linux':
        if arch == 'x64':
            return 'x86_64-linux-gnu'
        elif arch == 'aarch64':
            return 'aarch64-linux-gnu'
        else:
            log(f'unknown arch {arch!r}', 'error')
            exit(1)
    elif platform == 'macos':
        if arch == 'x64':
            return 'x86_64-macos-none'
        elif arch == 'aarch64':
            return 'aarch64-macos-none'
        else:
            log(f'unknown arch {arch!r}', 'error')
            exit(1)
    else:
        log(f'unknown platform {platform!r}', 'error')
        exit(1)

@cache
def found_zig():
    zig = shutil.which('zig')
    if not zig:
        log('zig not found', 'error')
        exit(1)
    return zig

@recipe(name='deps/capi')
def f():
    pass

def call_library_build(platform: Platform, arch: Architecure):
    """
    The build process is demonstrated by this famous blog post:
        https://andrewkelley.me/post/zig-cc-powerful-drop-in-replacement-gcc-clang.html
    """

    zig = found_zig()
    log(f'[Build TyJuliaCALL] building with:')

    old = os.getcwd()
    try:
        dll_path = to_dll_path("libjuliacall", platform)#生成一个动态链接库（DLL）的文件路径，存储在 dll_path 变量中。
        target = f"binaries/{platform}-{arch}/{dll_path}"#构建一个目标文件的路径，存储在 target 变量中。这个路径通常用于存放构建后的二进制文件。
        Path(target).parent.mkdir(parents=True, exist_ok=True)


        script_dir = pathlib.Path(__file__).parent  # 获取脚本所在的目录
        source_code_path = script_dir.joinpath("libjuliacall", "juliacall.cpp").absolute().as_posix() # 计算源代码文件的路径
        include_path1 = script_dir.joinpath("libjuliacall", "include").absolute().as_posix() # 计算 include 文件夹的路径
        include_path2 = sysconfig.get_paths()["include"]
        include_path2 = pathlib.Path(include_path2).absolute().as_posix() # 计算 python include 文件夹的路径

        libpython_files= find_libpython.find_libpython()
        libpython_files = pathlib.Path(libpython_files).parent.absolute().as_posix()

        cmd = [
            zig, 'c++', '-target', compute_zig_target(platform, arch),
            "-fPIC", "-shared",
            source_code_path, '-I', include_path1,'-I', include_path2,
            "-o", target,
            "-L", libpython_files,  # libpython路径
            '-lc',
            "-lpython3"  # 连接libpython库
        ]
        print(cmd)
        code = call(cmd)

        if code != 0:
            log('[Build TyJuliaCALL] failed', 'error')
            exit(1)
        log('[Build TyJuliaCALL] TyJuliaCALL built finished', 'ok')
    finally:
        os.chdir(old)
    log(f'[Build TyJuliaCALL] finished', 'ok')

# build matrix
for platform in ('windows', 'linux', 'macos'):
    for arch in ('x64', 'aarch64'):
        def _julia_call_build(platform: Platform=platform, arch: Architecure=arch):
            call_library_build(platform, arch)
        _julia_call_build.__name__ = f'juliacall_{platform}_{arch}'
        _julia_call_build.__doc__ = f"build tyjuliacall shared library for {platform}-{arch}"
        recipe('deps/capi')(_julia_call_build)

make()
