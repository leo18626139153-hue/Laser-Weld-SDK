#!/usr/bin/env python3
"""从 AI 镜像物化隔离工程，编译完整 SDK 并运行参数回归；不写原始源码。"""

import argparse
import datetime
import os
from pathlib import Path
import shutil
import subprocess
import sys


STUB_SOURCE = r'''#include "robotapi.h"
// 脚本编译测试不需要机械臂；任何实际调用均计数并返回失败。
int parameter_test_robot_calls = 0;
CRresult cr_set_boolRegValue(RobotHandle, unsigned int, unsigned int, BOOL*)
{ ++parameter_test_robot_calls; return CRresult::no_result; }
CRresult cr_set_int16RegValue(RobotHandle, unsigned int, unsigned int, int*)
{ ++parameter_test_robot_calls; return CRresult::no_result; }
CRresult cr_set_int32RegValue(RobotHandle, unsigned int, unsigned int, int*)
{ ++parameter_test_robot_calls; return CRresult::no_result; }
CRresult cr_get_currentTCPmsg(RobotHandle, TCPMsg*)
{ ++parameter_test_robot_calls; return CRresult::no_result; }
CRresult cr_TcpToFlangePose(double[ROB_AXIS_NUM], double[ROB_AXIS_NUM], double[ROB_AXIS_NUM])
{ ++parameter_test_robot_calls; return CRresult::no_result; }
CRresult cr_kineInverse(RobotHandle, double[ROB_AXIS_NUM], double[ROB_AXIS_NUM], double[ROB_AXIS_NUM])
{ ++parameter_test_robot_calls; return CRresult::no_result; }
'''


def run(command, cwd, env=None):
    """保留真实编译诊断；任何编译或断言失败都返回非零状态。"""
    print("RUN:", Path(str(command[0])).name, "目录:", cwd, flush=True)
    log_path = cwd / ("test.log" if str(command[0]).endswith("parameter_pipeline_test.exe")
                      else "compile.log")
    with log_path.open("wb") as log:
        result = subprocess.run([str(item) for item in command], cwd=str(cwd), env=env,
                                stdout=log, stderr=subprocess.STDOUT, timeout=300)
    data = log_path.read_bytes()
    try:
        output = data.decode("utf-8")
    except UnicodeDecodeError:
        output = data.decode("gb18030", errors="replace")
    for line in output.splitlines()[-35:]:
        print(line[:800], flush=True)
    if result.returncode:
        raise RuntimeError("进程返回 " + str(result.returncode) + "；完整诊断: " + str(log_path))


def materialize(mirror, target):
    """只转换可读 C/C++ 镜像后缀，不读取加密原始文件或覆盖既有构建。"""
    for directory in ("include", "src", "third_party"):
        for source in sorted((mirror / directory).rglob("*.md")):
            relative = source.relative_to(mirror)
            if source.name.endswith("-cpp.md"):
                name = source.name[:-7] + ".cpp"
            elif source.name.endswith("-h.md"):
                name = source.name[:-5] + ".h"
            else:
                continue
            output = target / relative.parent / name
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(source.read_bytes())


def compile_and_run(mirror, build_root, compiler, test_name, build_dll=False,
                    test_root=None, import_library=None):
    """所有业务头和实现均取指定镜像；测试文件及导入库来源分别明确记录。"""
    timestamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    target = build_root / ("parameter-pipeline-" + compiler + "-" + timestamp)
    target.mkdir(parents=True, exist_ok=False)
    materialize(mirror, target)
    test_source = target / "parameter_pipeline_test.cpp"
    test_input = (test_root or mirror) / "tests" / (test_name + "-cpp.md")
    print("SOURCE:", mirror, "TEST:", test_input, flush=True)
    test_source.write_bytes(test_input.read_bytes())
    stub_source = target / "robot_test_stubs.cpp"
    stub_source.write_text(STUB_SOURCE, encoding="utf-8")
    sources = sorted((target / "src").rglob("*.cpp"))
    sources += sorted((target / "include").rglob("*.cpp"))
    sources += [test_source, stub_source]
    includes = [target / "include" / "laserWeldSDK",
                target / "include" / "laserWeldSDK" / "common",
                target / "include" / "laserWeldSDK" / "brand",
                target / "third_party" / "cr_sdk" / "include"]
    defines = ["WIN32", "LASERWELDSDK_LIBRARY", "CRSDK_EXPORT"]
    executable = target / "parameter_pipeline_test.exe"

    if compiler == "msvc":
        cl = Path(shutil.which("cl") or "D:/VS/VC/bin/cl.exe")
        vcvars = cl.parent.parent / "vcvarsall.bat"
        if not cl.exists() or not vcvars.exists():
            raise RuntimeError("未找到 MSVC cl 或 vcvarsall.bat")
        # VS2015 默认选择最新 SDK，旧编译器可能缺少新版 UCRT 使用的内建函数。
        sdk_root = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)"))
        sdk_root = sdk_root / "Windows Kits" / "10" / "Include"
        sdk_versions = sorted(path.name for path in sdk_root.glob("10.*")
                              if (path / "ucrt" / "stdio.h").exists())
        print("MSVC 可用 Windows SDK:", sdk_versions, flush=True)
        selected_sdk = "10.0.10240.0" if "10.0.10240.0" in sdk_versions else (
            sdk_versions[0] if sdk_versions else "")
        sdk_argument = " " + selected_sdk if selected_sdk else ""
        arguments = ["/nologo", "/EHsc", "/W3", "/MDd", "/Od", "/utf-8",
                     "/Fe:" + str(executable)]
        arguments += ["/D" + value for value in defines]
        arguments += ["/I" + str(value) for value in includes]
        arguments += [str(value) for value in sources]
        # 有些旧 Kit 只安装了 UCRT；补充实际存在的系统库，不修改 Kit 或源码。
        sdk_base = sdk_root.parent
        system_libraries = sorted(sdk_base.glob("Lib/10.*/um/x64/kernel32.lib"))
        sdk81_library = sdk_base.parent / "8.1" / "Lib" / "winv6.3" / "um" / "x64" / "kernel32.lib"
        if sdk81_library.exists():
            system_libraries.insert(0, sdk81_library)
        if system_libraries:
            arguments += ["/link", "/LIBPATH:" + str(system_libraries[0].parent)]
        response = target / "compile.rsp"
        response.write_text(" ".join(subprocess.list2cmdline([arg])
                                    for arg in arguments), encoding="utf-8")
        batch = target / "compile.cmd"
        batch.write_text('@echo off\ncall "' + str(vcvars) +
                         '" amd64' + sdk_argument +
                         ' >nul\nif errorlevel 1 exit /b %errorlevel%\n' +
                         'cl @"' + str(response) +
                         '"\nexit /b %errorlevel%\n', encoding="utf-8")
        run(["cmd.exe", "/d", "/c", str(batch)], target)
        run([executable], target)
        if build_dll:
            # 真实 DLL 链接使用原始机器人导入库，不使用回归失败桩。
            if import_library is None:
                import_library = mirror.parent / "laserWeldSDK" / "third_party" / "cr_sdk" / "windows" / "libs" / "x64" / "cr_sdk.lib"
            if not import_library.exists():
                raise RuntimeError("真实 DLL 链接缺少机器人 SDK 导入库: " + str(import_library))
            dll_target = target / "runtime-dll"
            dll_target.mkdir()
            copied_library = dll_target / "cr_sdk.lib"
            shutil.copyfile(import_library, copied_library)
            dll = dll_target / "laserWeldSDK.dll"
            dll_arguments = [arg for arg in arguments
                             if arg not in ("/DCRSDK_EXPORT", str(test_source), str(stub_source))
                             and not arg.startswith("/Fe:")]
            dll_arguments = ["/MD" if arg == "/MDd" else "/O2" if arg == "/Od" else arg
                             for arg in dll_arguments]
            dll_arguments = ["/LD", "/Fe:" + str(dll)] + dll_arguments + [str(copied_library)]
            dll_response = dll_target / "compile.rsp"
            dll_response.write_text(" ".join(subprocess.list2cmdline([arg])
                                   for arg in dll_arguments), encoding="utf-8")
            dll_batch = dll_target / "compile.cmd"
            dll_batch.write_text(batch.read_text(encoding="utf-8").replace(str(response), str(dll_response)),
                                 encoding="utf-8")
            run(["cmd.exe", "/d", "/c", str(dll_batch)], dll_target)
            print("PASS: MSVC 完整运行库真实导入库链接:", dll, flush=True)
    else:
        gcc = Path(shutil.which("g++") or "D:/MinGW/bin/g++.exe")
        if not gcc.exists():
            raise RuntimeError("未找到 MinGW g++")
        arguments = [str(gcc), "-std=c++11", "-O0", "-g", "-pthread"]
        # 原日志工具按 MSVC 判断 Windows，MinGW 用等价时间函数适配进行隔离测试。
        if "localtime_r(" in (target / "src" / "utils.cpp").read_text(encoding="utf-8-sig"):
            compatibility = target / "mingw_test_compatibility.h"
            compatibility.write_text('#include <ctime>\n'
                                     'inline std::tm* localtime_r(const std::time_t* t, std::tm* out)\n'
                                     '{ return localtime_s(out, t) == 0 ? out : 0; }\n', encoding="utf-8")
            arguments += ["-include", str(compatibility)]
        arguments += ["-D" + value for value in defines]
        arguments += ["-I" + str(value) for value in includes]
        arguments += [str(value) for value in sources]
        arguments += ["-o", str(executable)]
        env = os.environ.copy()
        env["PATH"] = str(gcc.parent) + os.pathsep + env.get("PATH", "")
        run(arguments, target, env)
        run([executable], target, env)
    print("PASS:", compiler, "完整 SDK 参数回归；构建目录:", target, flush=True)
    return target


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", choices=("auto", "msvc", "mingw", "all"),
                        default="auto")
    parser.add_argument("--test", choices=("parameter_pipeline_test", "sup26ad_protocol_test"),
                        default="parameter_pipeline_test")
    parser.add_argument("--dll", action="store_true", help="MSVC 回归通过后用真实机器人导入库链接 DLL")
    parser.add_argument("--source", "--source-mirror", dest="source", type=Path,
                        help="指定可读 MD 源码快照，例如从原仓库 Git 内容导出的同步验证目录")
    parser.add_argument("--robot-sdk-library", type=Path,
                        help="指定真实机器人 SDK 导入库，仅用于 --dll")
    args = parser.parse_args()
    default_mirror = Path(__file__).resolve().parents[1]
    mirror = args.source.resolve() if args.source else default_mirror
    # 构建始终留在当前 SDK 工作区内；指定外部只读快照不会向其写文件。
    build_root = default_mirror.parent / ".refactor-work" / "build"
    for name in ("src/cl_api-cpp.md", "include/laserWeldSDK/cl_api-h.md"):
        if not (mirror / name).is_file():
            raise RuntimeError("指定源码目录缺少 MD 镜像文件: " + str(mirror / name))
    test_root = mirror if (mirror / "tests" / (args.test + "-cpp.md")).is_file() else default_mirror
    library = args.robot_sdk_library.resolve() if args.robot_sdk_library else (
        default_mirror.parent / "laserWeldSDK" / "third_party" / "cr_sdk" / "windows" / "libs" / "x64" / "cr_sdk.lib")
    if args.compiler == "all":
        compilers = ["msvc", "mingw"]
    elif args.compiler == "auto":
        compilers = ["msvc" if shutil.which("cl") else "mingw"]
    else:
        compilers = [args.compiler]
    for compiler in compilers:
        compile_and_run(mirror, build_root, compiler, args.test, args.dll and compiler == "msvc",
                        test_root, library)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        print("FAIL:", error, file=sys.stderr)
        sys.exit(1)
