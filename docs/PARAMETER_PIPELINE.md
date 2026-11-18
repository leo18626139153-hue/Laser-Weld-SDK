# 激光焊工艺参数生成

上位机配置机器人控制器的 RS485、Modbus 主从站及设备映射。SDK 根据 `MachineResource` 选择品牌规则，编译当次工艺并生成 Lua。工艺脚本只配置参数，起弧和收弧由焊接程序控制。

## 通用入口

- 通过 `cl_machine_resource_init` 或 `cl_machine_resource_update` 提交焊机资源。资源中保留的地址字段为 `int`，构造时默认为 `-1`。地址 `0` 是有效值。
- `cl_craft_inst_devices_params_script_get` 生成独立的工艺参数脚本。`cl_craft_inst_list_script_get` 生成焊接或模拟程序，`isWeld` 为 `false` 时省略焊机工艺和控制输出。
- 宝辰鑫和 SUP26AD 沿用现有本地映射。旧超强和欧斯睿普兼容路径继续使用既有的自由通讯规则，地址由上位机按设备协议填写。
- 使用与 SDK 运行库同一版本的头文件和链接库，工艺地址与上位机从站映射保持一致。

## 特尔码工艺写入

品牌工厂按本地 Int16 地址生成单字写计划。`-1` 跳过对应项，其他地址须在控制器 Int16 槽位范围内。重复地址返回参数错误。完整六项写入时生成供实焊准备使用的首工艺快照。

| 参数 | `MachineResource` 地址字段 | 值与编码 |
| --- | --- | --- |
| 激光功率 | `laserPowerLoc` | 当次工艺的 `laserHeadParams.laserPower`，单位 W |
| 激光频率 | `laserHeadFrequencyLowLoc` | 单字，单位 Hz |
| 激光占空比 | `laserHeadDutyCycleLoc` | 百分比 |
| 自动送丝速度 | `autoWireFeedSpeedLoc` | 工艺输入 mm/s，换算成 m/min 后乘 100 |
| 扫描频率 | `laserHeadScanSpeedLoc` | 单位 Hz |
| 扫描宽度 | `laserHeadScanWidthLoc` | 输入 mm，寄存器值乘 10 |

例如上位机填 `laserPowerLoc = 60`，当次工艺的 `craftLaserPower = 1200` 时，SDK 在编译副本中设置 `laserHeadParams.laserPower`，生成 `write_int16_register(60, 1200)`。设备额定功率取自设备配置。实际槽位按运行从站映射填写。

## SUP26AD 的模式与固定设置

SUP26AD 工艺快照使用模式版本标记 SUP26AD_CRAFT_BANK_V2:SELF 或 SUP26AD_CRAFT_BANK_V2:WIRE。自融焊只包含 18 项公共参数；填丝焊包含公共参数与 4 项自动送丝参数，共 22 项。SDK 在自融焊中忽略未使用的送丝工艺值，不向 94–97 补写零值。完整默认地址才生成快照；部分参数或重绑定后的地址不能冒充默认完整工艺。

上位机将自动送丝工艺拆为第七组（索引 6，与公共工艺同为 RS485-2、物理站号 9）：

| 语义 | 本地写 | 本地回读 | 设备寄存器 |
| --- | --- | --- | --- |
| 自动送丝速度 | 94 | 109 | 5026 |
| 送丝开关 | 95 | 110 | 5027 |
| 回抽长度 | 96 | 111 | 5028 |
| 补丝长度 | 97 | 112 | 5029 |

专家模式与板卡渐变设置归上位机第二组初始化管理：69/84/4668 = 1；98/113/5032 = 100；99/114/5033 = 0；100/115/5034 = 100；101/116/5035 = 0。每项斜线依次为本地写地址、本地回读地址和设备寄存器。初始化回读成功后移除这五项写映射；SDK 工艺、独立起收弧和完整程序均跳过这五项，功率渐变由 SDK 内部运行逻辑调节工艺功率。

默认协议的自动送丝速度为 15–600 cm/min，回抽和补丝长度均为 0–100 mm；超限在编译脚本前失败。上位机通过 `MachineResource.rampControlEnabled` 启用内部渐变控制时，工艺中的速度使用 mm/s，35 mm/s 转为 210 cm/min。当前工艺速度成员为整数，所以上位机输入限值为 3–100 mm/s；物理下限 2.5 mm/s 不能通过该整数成员表示。未启用内部渐变的历史工艺转换仍保留原单位契约。

上位机、SDK、默认从站配置和重新生成的 Lua 必须配套更新，工艺快照使用 V2 格式。自融时不启动自动送丝组；手动送丝/退丝使用原有独立动作组，由上位机分别管理。

## 回归验证

在 SDK 源码仓库中运行参数编译测试。若从可读镜像编译，使用 `--source` 传入同步后的镜像路径。

```text
python tests/run_parameter_pipeline_test.py.md --compiler mingw
python tests/run_parameter_pipeline_test.py.md --compiler msvc --dll
```

参数测试使用机械臂调用桩，检查脚本生成、地址和编码、缓冲区及导出契约。设备写入及回读需用实际控制器和从站验证。
