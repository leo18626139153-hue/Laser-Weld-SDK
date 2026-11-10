#!/usr/bin/env python3
"""检查 SDK 自有 C++ 格式，避免第三方头文件和生成脚本被批量改写。"""

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


MIN_FORMATTER_MAJOR = 23
UTF8_BOM = b"\xef\xbb\xbf"
SOURCE_ROOTS = ("src", "include/laserWeldSDK", "tests")


def find_formatter(explicit):
    """显式配置优先，便于团队和 CI 使用同一套格式器。"""
    selected = explicit or os.environ.get("CLANG_FORMAT") or shutil.which("clang-format")
    if selected:
        return selected
    extensions = Path.home() / ".vscode" / "extensions"
    candidates = list(extensions.glob("ms-vscode.cpptools-*/LLVM/bin/clang-format.exe"))
    if candidates:
        return str(max(candidates, key=lambda item: item.stat().st_mtime))
    raise RuntimeError("未找到 clang-format；请使用 --clang-format 指定 clang-format 23 或更新版本")


def source_paths(root):
    return sorted(
        source
        for directory in SOURCE_ROOTS
        for source in (root / directory).rglob("*")
        if source.is_file() and source.suffix in (".cpp", ".h")
    )


def formatted_bytes(formatter, root, source, original):
    """BOM 由原文件决定，格式器只处理 UTF-8 正文和 CRLF 排版。"""
    has_bom = original.startswith(UTF8_BOM)
    body = original[len(UTF8_BOM):] if has_bom else original
    body.decode("utf-8")
    result = subprocess.run(
        [formatter, "--style=file:" + str(root / ".clang-format"), "--assume-filename=" + str(source)],
        input=body,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )
    formatted = result.stdout[len(UTF8_BOM):] if result.stdout.startswith(UTF8_BOM) else result.stdout
    return (UTF8_BOM if has_bom else b"") + formatted


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang-format", help="clang-format 可执行文件路径")
    parser.add_argument("--fix", action="store_true", help="格式化自有 C++ 文件并保留原 BOM")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    formatter = find_formatter(args.clang_format)
    version = subprocess.check_output([formatter, "--version"], text=True).strip()
    match = re.search(r"version\s+(\d+)", version)
    if match is None or int(match.group(1)) < MIN_FORMATTER_MAJOR:
        raise RuntimeError("需要 clang-format 23 或更新版本，当前为：" + version)
    changed = []
    pending = []
    sources = source_paths(root)
    for source in sources:
        original = source.read_bytes()
        formatted = formatted_bytes(formatter, root, source, original)
        if formatted != original:
            changed.append(source.relative_to(root))
            if args.fix:
                pending.append((source, formatted))
    # 所有文件均可读取和格式化后再写回，避免遇到加密文件时只更新一部分。
    for source, formatted in pending:
        source.write_bytes(formatted)
    for source in changed:
        print(("已格式化: " if args.fix else "格式不符: ") + source.as_posix())
    if changed and not args.fix:
        print("格式检查失败；使用 --fix 或编辑器按 .clang-format 整理后重试")
        return 1
    print(f"PASS: {len(sources)} 个自有 C++ 文件，{version}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, UnicodeError, RuntimeError, subprocess.CalledProcessError) as error:
        print("格式检查错误: " + str(error), file=sys.stderr)
        raise SystemExit(2)
