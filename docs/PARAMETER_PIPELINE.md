# 激光焊参数配置接入

本次参数编译链将参数语义、类型与缩放规则、寄存器绑定、Lua 输出分开。上位机先配置机器人控制器的 RS485、Modbus 主从站和板卡物理地址映射；SDK 使用配置完成后的本地参数寄存器地址。SDK 的新参数接口没有 COM、站号、波特率和 CRC 字段，也不创建或连接从站。

## 接口与默认行为

- `parameterdefine.h`：稳定参数编号、描述、读写绑定、物理值及诊断结构。各结构的 `structSize` 必须填 `sizeof(本结构)`。
- `cl_parameters_configuration_get`：获取句柄当前配置；两个数组都传空指针可查询数量。数组容量不足时返回 `BUFFER_TOO_SMALL_LASER_SDK`，更新需求数量且不写两个数组。
- `cl_parameters_configuration_set`：校验后深复制安装配置。失败保留原配置；不同焊机句柄配置分别保存。
- `cl_parameters_configuration_reset`：恢复该设备的默认参数配置。
- `cl_parameters_script_get`：仅编译本次传入的参数值，不发送报文、不写机器资源。脚本指针为空时查询所需字节数，包含结尾零字符；容量不足时不写输出。
- 原 `cl_craft_inst_devices_params_script_get`：继续转换工艺并生成参数脚本，显式安装的参数配置也对该入口生效。工艺参数脚本不再同时开启出光。

未安装自定义配置时，宝辰鑫工艺参数保留本地槽位 60–83，SUP26AD 保留 60–86；旧超强保留自由通讯参数输出，并从 `MachineResource` 读取原地址。旧超强的地址零有效，其他必要地址需由调用方完整填写。安装自定义配置后三种设备统一生成 `write_int16_register` 本地映射写入。切换板卡品牌会清理上一型号的参数配置。

欧斯睿普继续沿用原 SDK 既有的旧超强兼容别名，本次没有新增独立厂商协议。参数结构为当前版本的固定布局，`structSize` 用于入参大小检查，不能据此承诺未来扩展数组元素的二进制兼容；布局变更需要新接口或版本约定。

旧超强独立 `cl_get_arc_start_script` 在原基线中将气阀 49、送丝 50 误列入激光头参数，但激光头渲染器没有对应条目，返回 `PARAMETER_ERROR_LASER_SDK`。该遗留地址分类问题本轮仅记录；完整指令与参数链的验证不表示此旧接口已修复。空 CO2 片段的 Lua 行边界回归直接调用同一通讯渲染器验证。

参数编号不等于地址。只更改寄存器地址时修改绑定；新增不同类型或单位的参数时同时提供描述。整数、浮点编码规则为 `raw = value * scale + offset`。支持 U16、I16、U32、I32、F32，三十二位类型占两个连续的十六位寄存器；`wordOrder` 规定高字/低字顺序，寄存器内不交换字节。整数舍入可选四舍五入、向零截断或必须精确表示。I16 在 Lua 中保留负数。

`LASER_PARAMETER_INVALID_ADDRESS_SDK` 表示方向未绑定，不能用零表示缺失。只读参数不允许显式写入；缺少写绑定、重复编号、同方向地址重叠、非有限数、范围与编码溢出均返回错误。物理值范围先校验，编码前再检查原始类型范围。上位机还需按实际控制器校验可用本地槽位和 Modbus 映射，SDK 的十六位地址范围不代表控制器拥有全部这些槽位。

## 修改一个参数地址

下面的步骤在创建焊机句柄后执行。实际项目应检查每一步返回码，并显示 `LaserParameterDiagnosticSdk` 的参数编号和诊断。修改后上位机的主从站映射也必须使用相同本地槽位。

```cpp
unsigned descriptorCount = 0;
unsigned bindingCount = 0;
cl_parameters_configuration_get(handle, nullptr, &descriptorCount,
                                nullptr, &bindingCount);
std::vector<LaserParameterDescriptorSdk> descriptors(descriptorCount);
std::vector<LaserParameterBindingSdk> bindings(bindingCount);
cl_parameters_configuration_get(handle, descriptors.data(), &descriptorCount,
                                bindings.data(), &bindingCount);
for (std::size_t i = 0; i < bindings.size(); ++i)
{
    if (bindings[i].parameterId == LASER_PARAM_WELD_POWER_SDK)
        bindings[i].writeAddress = 300; // 上位机已将本地 300 映射到板卡功率地址
}
LaserParameterDiagnosticSdk diagnostic = {};
diagnostic.structSize = sizeof(diagnostic);
cl_parameters_configuration_set(handle, descriptors.data(), descriptorCount,
                                bindings.data(), bindingCount, &diagnostic);
// 后续原工艺脚本接口或新参数值接口都使用此句柄的配置。
```

设备工艺转换保留品牌语义：宝辰鑫开关光使用瓦数，超强开关光使用焊接功率百分比，宽度和偏移采用各自描述的比例。自定义配置的范围由描述决定，必要工艺参数至少包含可写的焊接功率、占空比和扫描宽度。可选只读参数从工艺写入中跳过；新参数值接口仍拒绝显式写只读参数。工艺转换本身的功率比例、焊速与缓降距离等关联条件仍校验。

## 回归验证

在同时包含 `laserWeldSDK/` 与 `laserWeldSDKForAI/` 的外层工作区目录执行：

```text
python laserWeldSDKForAI/tests/run_parameter_pipeline_test.py.md --compiler msvc
python laserWeldSDKForAI/tests/run_parameter_pipeline_test.py.md --compiler msvc --dll
python laserWeldSDKForAI/tests/run_parameter_pipeline_test.py.md --compiler mingw
python laserWeldSDKForAI/tests/run_parameter_pipeline_test.py.md --source .refactor-work/synced-source-20261013 --compiler msvc --dll
python laserWeldSDKForAI/tests/run_parameter_pipeline_test.py.md --source .refactor-work/synced-source-20261013 --compiler mingw
```

2026-10-13 已用同步后的原工程可信快照验证：MSVC 2015 x64 与 MinGW 13.2 C++11 分别通过 9 组场景、1505 项检查，参数编译期间机器人 SDK 调用为零。MSVC 另以真实 `cr_sdk.lib` 链接 Release DLL，核验 4 个新参数接口导出，并确认 `loadLuaFunctionScriptOut(brand)` 与 `(brand, bool)` 两个重载符号。日志位于 `.refactor-work/build/parameter-pipeline-msvc-20261013-110902-859776` 与 `parameter-pipeline-mingw-20261013-110904-912569`，包括 `compile.log`、`test.log`，MSVC 的 `runtime-dll` 目录另含 DLL 编译日志、`exports.log` 和 `base_commu_symbols.log`。

脚本只读取所选镜像中的 `-cpp.md`、`-h.md`，物化并编译完整 SDK 到 `.refactor-work/build/parameter-pipeline-*`；不写原始源码。MSVC 初始化 VS2015 并选可用旧 UCRT，缺少旧 Kit 系统库时使用实际安装的 Windows 系统库。当所选日志工具含 `localtime_r` 时，MinGW 在隔离构建中使用等价 Windows 时间函数，不修改业务源。机械臂 SDK 函数由失败桩替代，测试要求参数编译期间调用次数为零。`--dll` 在 MSVC 回归通过后另以 `/MD /O2` 编译 Release 运行库，链接只读复制的真实机械臂导入库，未使用失败桩。

测试覆盖编码位模式与字序、三种板卡的默认工艺数值、自定义地址通过原工艺入口生效、地址零、配置隔离与深复制、缓冲区契约、失败保持输出/资源、只读和缺绑定、合法回抽 100、PWM 越界与默认舍入兼容；同时检查完整指令列表仅选工艺时不隐式开启出光，起弧、收弧、缓升缓降和设备参数入口不夹带自由通讯调用，自定义提前送气 5000 不受旧 3000 限制。旧超强完整缓降目标保持工艺功率，兼容工艺包装清除原超时状态；泛用参数内核不包含该控制状态。该验证不执行真实 485 通讯、设备动作或 AArch64 运行验收。新增头和源已同步注册到原工程的 `.pro` 与 `CMakeLists.txt`；接入方需使用匹配的公开头与 SDK 动态库。

`--source` 指定从原仓库 Git 内容或经确认的复制结果生成的可读 MD 快照；全部业务源和头均从该目录物化。快照没有测试目录时，仅复用当前 runner 旁的测试文件，不复用 AI 的业务代码。运行输出记录 `SOURCE` 和 `TEST` 的绝对路径。不可使用加密原始文件的物理字节作为源码判断依据；同步后应对真实原仓库 diff、各批次提交清单及该快照的测试日志分别核验。上游已取消的工艺隐式 CO 动作需保留，包括焊接过程中换工艺；旧超强没有 CO2 片段时，也检查后续 CO1 指令未落入上一行 Lua 注释。
