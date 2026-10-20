#!/usr/bin/env python3
"""编译隔离 SDK，执行其实际生成的 Lua；不连接机械臂、不写原始源码。"""

import argparse
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import sys


def main():
    """复用完整 SDK 编译流程，并在无设备的 Lua 调度器内验证时序。"""
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", choices=("mingw", "msvc", "all"), default="mingw")
    parser.add_argument("--source", type=Path, help="指定可读 MD 镜像；默认当前镜像")
    parser.add_argument("--lua", type=Path, help="指定已安装的 Lua 解释器；不自动安装")
    args = parser.parse_args()
    current_mirror = Path(__file__).resolve().parents[1]
    mirror = args.source.resolve() if args.source else current_mirror
    lua = args.lua or Path(shutil.which("lua") or "D:/lua/5.1/lua.exe")
    if not lua.is_file():
        raise RuntimeError("未找到 Lua；请用 --lua 指定本地解释器")
    helpers = runpy.run_path(str(current_mirror / "tests" / "run_parameter_pipeline_test.py.md"))
    compilers = ("mingw", "msvc") if args.compiler == "all" else (args.compiler,)
    for compiler in compilers:
        target = helpers["compile_and_run"](
            mirror, current_mirror.parent / ".refactor-work" / "build", compiler,
            "sequence_control_test", test_root=current_mirror)
        harness = current_mirror / "tests" / "sequence_runtime_test.lua.md"
        command = [str(lua), str(harness), str(target)]
        environment = os.environ.copy()
        environment["PATH"] = str(lua.parent) + os.pathsep + environment.get("PATH", "")
        print("RUN: 实际生成 Lua 的协程调度回归，目录:", target, flush=True)
        completed = subprocess.run(command, cwd=str(target), env=environment,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   timeout=120)
        output = completed.stdout.decode("utf-8", errors="replace").replace("\r\n", "\n")
        log_path = target / "sequence-runtime.log"
        log_path.write_text(output, encoding="utf-8")
        print(output, end="", flush=True)
        if completed.returncode:
            raise RuntimeError("Lua 行为回归失败；完整日志: " + str(log_path))
        print("PASS: C++ 生成与 Lua 执行均通过:", target, flush=True)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        print("FAIL:", error, file=sys.stderr)
        sys.exit(1)
