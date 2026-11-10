#!/usr/bin/env python3
"""针对指定 Windows x64 SDK DLL 运行公共接口和生成 Lua 的离线回归。"""

import argparse
import locale
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def msvc_environment(args, work):
    script = work / "environment.cmd"
    script.write_text('@echo off\ncall "%~1" amd64 %~2 >nul\nif errorlevel 1 exit /b %errorlevel%\nset\n', encoding="ascii")
    result = subprocess.run(["cmd.exe", "/d", "/c", str(script), str(args.vcvars), args.windows_sdk],
                            check=True, stdout=subprocess.PIPE)
    encoding = locale.getpreferredencoding(False)
    environment = dict(line.split("=", 1) for line in result.stdout.decode(encoding, errors="replace").splitlines()
                       if "=" in line and not line.startswith("="))
    # VS2015 兼容的 UCRT 可以与完整的 Windows SDK 系统头和系统库一起使用。
    kits = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Windows Kits" / "10"
    headers = kits / "Include" / args.ucrt / "ucrt"
    libraries = kits / "Lib" / args.ucrt / "ucrt" / "x64"
    if headers.is_dir() and libraries.is_dir():
        environment["INCLUDE"] = str(headers) + os.pathsep + environment.get("INCLUDE", "")
        environment["LIB"] = str(libraries) + os.pathsep + environment.get("LIB", "")
    path = [args.sdk_library_dir, args.robot_sdk_dir / "libs" / "x64"]
    original_path = environment.pop("Path", environment.get("PATH", ""))
    environment["PATH"] = os.pathsep.join(str(part) for part in path) + os.pathsep + original_path
    return environment


def read_test(tests, *names):
    for name in names:
        path = tests / name
        if path.is_file():
            return path.read_text(encoding="utf-8-sig")
    raise RuntimeError(f"测试源不存在 {tests / names[0]}")


def lua_literal(value):
    delimiter = "="
    while f"]{delimiter}]" in value:
        delimiter += "="
    return f"[{delimiter}[\n{value}]{delimiter}]"


def run(args):
    tests = Path(__file__).resolve().parent
    header_dir = args.sdk_source / "include" / "laserWeldSDK"
    import_library = args.sdk_library_dir / "laserWeldSDK.lib"
    for path in (header_dir / "cl_api.h", args.robot_sdk_dir / "include" / "robotapi.h",
                 import_library, args.sdk_library_dir / "laserWeldSDK.dll", args.vcvars, args.lua):
        if not path.is_file():
            raise RuntimeError(f"找不到所需文件 {path}")
    with tempfile.TemporaryDirectory(prefix="trm-sdk-test-") as directory:
        work = Path(directory)
        environment = msvc_environment(args, work)
        test_source = work / "trm_control_test.cpp"
        test_source.write_text(read_test(tests, "trm_control_test.cpp", "trm_control_test-cpp.md"), encoding="utf-8")
        test_executable = work / "trm_control_test.exe"
        compiler = shutil.which("cl.exe", path=environment["PATH"])
        if compiler is None:
            raise RuntimeError("MSVC 环境中没有 cl.exe")
        subprocess.run([compiler, "/nologo", "/EHsc", "/MD", "/utf-8", "/DWIN32", "/I" + str(header_dir),
                        "/I" + str(args.robot_sdk_dir / "include"), str(test_source), str(import_library),
                        "/Fe:" + str(test_executable)], cwd=work, env=environment, check=True)
        subprocess.run([str(test_executable)], cwd=work, env=environment, check=True)

        # 从标准输入送入夹具和测试，避免本机对 .lua 文件的透明加密影响解释器读取。
        fixtures = ("trm_program.lua", "trm_apply.lua", "trm_manual.lua")
        encoded = [f'["{name}"]={lua_literal((work / name).read_text(encoding="utf-8"))}' for name in fixtures]
        harness = read_test(tests, "trm_runtime_test.lua", "trm_runtime_test-lua.md")
        source = "local fixtures={" + ",".join(encoded) + "}\n"
        source += "local function loadfile(path) return loadstring(fixtures[path], path) end\n"
        subprocess.run([str(args.lua), "-"], input=(source + harness).encode("utf-8"), cwd=work, check=True)


def main():
    root = Path(__file__).resolve().parents[1]
    default_source = root if (root / "include/laserWeldSDK/cl_api.h").is_file() else root.parent / "laserWeldSDK"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-source", type=Path, default=default_source, help="与所测 DLL 配套的 SDK 源码目录")
    parser.add_argument("--sdk-library-dir", type=Path, required=True, help="SDK DLL 与导入库目录")
    parser.add_argument("--robot-sdk-dir", type=Path, required=True, help="构建使用的 cr_sdk 目录")
    parser.add_argument("--lua", type=Path, default=Path(shutil.which("lua") or "D:/lua/5.1/lua.exe"), help="Lua 5.1 解释器")
    parser.add_argument("--vcvars", type=Path, default=Path("D:/VS/VC/vcvarsall.bat"), help="VS2015 环境脚本")
    parser.add_argument("--windows-sdk", default="10.0.19041.0", help="VC 环境的完整 Windows SDK 版本")
    parser.add_argument("--ucrt", default="10.0.10240.0", help="兼容 VS2015 的 UCRT 版本")
    args = parser.parse_args()
    for key, value in vars(args).items():
        if isinstance(value, Path):
            setattr(args, key, value.resolve())
    try:
        run(args)
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"特尔码离线回归失败 {error}\n")


if __name__ == "__main__":
    main()
