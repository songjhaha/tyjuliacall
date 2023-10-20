#!/usr/bin/env python
from __future__ import annotations
from functools import cache
from pmakefile import * # type: ignore
from subprocess import call
import typing
import os

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
 
@recipe(name='sjh/juliasetup')# 调用 Git 的 clone 命令，将存储库克隆到 'sjh/juliasetup' 子目录中。
def clone_call_repo():
    call([
        'git', 'clone',
        r'ssh://git@github.com:Suzhou-Tongyuan/tyjuliacall.git',
        'sjh/juliasetup' #克隆代码的目标目录
    ])

def call_library_build(platform: Platform, arch: Architecure):
    """
    The build process is demonstrated by this famous blog post:
        https://andrewkelley.me/post/zig-cc-powerful-drop-in-replacement-gcc-clang.html
    """

    zig = found_zig()
    log(f'[Build TyJuliaCALL] building with:')

    old = os.getcwd()
    try:
        dll_path = to_dll_path("tyjuliacall", platform)#生成一个动态链接库（DLL）的文件路径，存储在 dll_path 变量中。
        target = f"binaries/{platform}-{arch}/{dll_path}"#构建一个目标文件的路径，存储在 target 变量中。这个路径通常用于存放构建后的二进制文件。
        Path(target).parent.mkdir(parents=True, exist_ok=True)


        script_dir = os.path.dirname(__file__)  # 获取脚本所在的目录
        source_code_path = os.path.join(script_dir, "library.cpp")  # 计算源代码文件的路径
        include_path = os.path.join(script_dir, "tyjuliacall/libjuliacall/include")  # 计算 include 文件夹的路径

        code = call([
            zig, 'c++', '-target', compute_zig_target(platform, arch),
            "-fPIC", "-shared",
            source_code_path, '-I', include_path,
            "-o", target,
        ])

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
        recipe('sjh/juliasetup')(_julia_call_build)


make()
