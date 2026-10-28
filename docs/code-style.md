# SDK 代码风格

SDK 自有 C++ 代码统一使用 `.clang-format`，编辑器通用设置使用 `.editorconfig`。格式检查覆盖 `src`、`include/laserWeldSDK` 和 `tests`，包括放在公开头目录中的实现文件。

1. 使用 4 个空格缩进，禁用 Tab；保留既有 UTF-8 BOM 和 CRLF。
2. 左大括号与函数、类型声明或控制语句同行。
3. 行宽为 160。长条件和长参数列表按逻辑分组换行；不要为了对齐而拆开可容纳的简单语句。
4. 控制语句始终使用花括号，包括单语句分支和循环。
5. 周期、超时、容量、单位转换、协议帧长度和功能码使用有含义的常量。优先复用已有定义，类型边界使用标准库。协议描述表中的地址、范围和缩放值已有字段语义，可保留原值以便逐项核对手册；数学公式的基本系数和循环起始值无需另造名称。
6. 中文注释解释限制、原因和边界，不复述函数调用或保留“本次修复”等历史记录。公共 API 的参数、单位、所有权和返回值说明继续保留。
7. 声明、赋值和调用能在行宽内表达时保持单行。例如：

```cpp
std::unordered_map<int, std::pair<InstTypeLaserSdk, int>>::const_iterator it = instListData.listLocToTypeID.find(instLoc);
if (it == instListData.listLocToTypeID.end()) {
    return false;
}
```

Lua 字符串和生成脚本是 SDK 对外输出，不随 C++ 格式整理改变。超长字符串需要拆分时只使用 C++ 相邻字符串拼接，并核对生成结果一致。第三方库、第三方头文件和构建产物不在格式检查范围内。

## 检查与格式化

使用 clang-format 23 或更新版本及 Python 3：

```text
python scripts/check_code_style.py --clang-format /path/to/clang-format
python scripts/check_code_style.py --clang-format /path/to/clang-format --fix
git diff --check
```

检查器也支持 `CLANG_FORMAT` 环境变量和 PATH 中的 `clang-format`。Windows 下会查找已安装的 VS Code C++ 扩展附带的格式器。检查失败时返回非零状态；`--fix` 仅格式化上述自有 C++ 文件，并恢复各文件原有 BOM。

自动工具只能检查排版和控制语句花括号。常量命名、注释原因、公共接口兼容及协议语义仍需代码审查。调整字符串、协议常量或控制语句后，应运行参数、协议及时序回归；格式整理不代表完成设备验收。
