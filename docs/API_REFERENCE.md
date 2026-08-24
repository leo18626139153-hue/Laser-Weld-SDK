# laserWeldSDK — API 参考文档

> SDK 版本: `v1.2.3-5` | C++ 标准: C++17 | 导出方式: C 风格 (`extern "C"`)

> **⚠️ 调用契约（重要）**：本 SDK 为单例设计，**不支持多线程**。
> 调用方必须保证所有 SDK 接口调用在同一线程内串行执行（包括使用不同 `LaserMachineHandle` 的调用）；并发调用属于未定义行为。
---

## 1. 快速开始

```c
#include "laserWeldSDK/cl_api.h"

// 1. 获取 SDK 版本
char version[64];
int len = sizeof(version);
cl_sdk_version_get(version, &len);

// 2. 初始化焊机资源
MachineResource resource = {0};
resource.commuType = RS485_2_LASER_SDK;
resource.laserHeadBrand = CQLASERPOWER_SDK;
// ... 填充寄存器地址 ...
LaserMachineHandle handle;
cl_machine_resource_init(&resource, &handle);

// 3. 生成焊接脚本
char scripts[4096];
int scriptLen = sizeof(scripts);
cl_get_arc_start_script(handle, scripts, &scriptLen);

// 4. 释放资源
cl_machine_resource_release(handle);
```

---

## 2. 基础类型

### 2.1 句柄类型

```c
typedef int LaserMachineHandle;  // 焊机资源句柄
```

### 2.2 返回码 `LaserSdkResult`

所有 API 函数均返回 `LaserSdkResult` 枚举值，`0` 表示成功。

| 值 | 常量 | 说明 |
|----|------|------|
| 0 | `SUCCESS_LASER_SDK` | 成功 |
| 1 | `ERROR_LASER_SDK` | 一般错误 |
| 2 | `MEMORY_ERROR_LASER_SDK` | 内存错误 |
| 3 | `HANDLE_ERROR_LASER_SDK` | 焊机句柄错误 |
| 4 | `SPEED_LIMIT_LASER_SDK` | 速度超限 |
| 5 | `ACC_LIMIT_LASER_SDK` | 加速度超限 |
| 6 | `SPEED_CAL_ERROR_LASER_SDK` | 速度计算错误 |
| 7 | `ARC_FAILURE_LASER_SDK` | 起弧失败 |
| 8 | `COMMU_ERROR_LASER_SDK` | 通讯错误 |
| 9 | `PARAMETER_ERROR_LASER_SDK` | 参数错误 |
| 10 | `CALCULATE_CIRCLE_ERROR_LASER_SDK` | 计算圆错误 |
| 11 | `SHORT_AXIS_LIMIT_LASER_SDK` | 短轴限制 |
| 12 | `BACK_DISTANCE_LIMIT_LASER_SDK` | 回退距离超限 |
| 13 | `LOCATE_FAIL_LASER_SDK` | 焊丝寻位失败 |
| 16 | `WELD_TYPE_ERROR_LASER_SDK` | 焊接类型错误 |
| 18 | `PARAM_WRITE_ERROR_LASER_SDK` | 参数写入失败 |
| 19 | `INSTLIST_ERROR_LASER_SDK` | 指令列表错误 |
| 20 | `PARAMETER_OUT_OF_RANGE_LASER_SDK` | 参数越界 |
| 24 | `IS_WELDING_LIST_ERROR_LASER_SDK` | 正在焊接中 |
| 30 | `PARAM_TYPE_ERROR_LASER_SDK` | 无效参数类型 |
| 31 | `PARAM_SCAN_FREQUENCY_ERROR_LASER_SDK` | 扫描频率超限 |
| 98 | `CALCULATION_RESULT_ILLOGICAL_SDK` | 计算结果不可用 |
| 99 | `OTHER_ERROR_LASER_SDK` | 未知错误 |

### 2.3 激光头品牌 `LaserHeadBrandSdk`

| 值 | 常量 | 品牌 |
|----|------|------|
| 0 | `CQLASERPOWER_SDK` | 超强伟业 |
| 1 | `OSRPLASERPOWER_SDK` | 欧斯睿普 |
| 2 | `BCXLASERPOWER_SDK` | 宝辰鑫 |

### 2.4 通讯形式 `CommuTypeLaserSdk`

| 值 | 常量 | 说明 |
|----|------|------|
| 0 | `RS485_2_LASER_SDK` | RS-485 串口通讯 |

### 2.5 焊接模式 `WeldModeLaserSdk`

| 值 | 常量 | 说明 |
|----|------|------|
| 0 | `SPOT_LASER_SDK` | 点焊 |
| 1 | `CONTINUOUS_LASER_SDK` | 连续焊 |

---

## 3. 焊机资源管理

### 3.1 `cl_sdk_version_get`

```c
LaserSdkResult cl_sdk_version_get(char* version, int* versionLen);
```

获取 SDK 版本号字符串。

| 参数 | 方向 | 说明 |
|------|------|------|
| `version` | out | 版本号字符串缓冲区 |
| `versionLen` | in/out | in: 缓冲区大小; out: 实际版本号长度 |

---

### 3.2 `cl_machine_resource_init`

```c
LaserSdkResult cl_machine_resource_init(
    const MachineResource* laserMachineResource,
    LaserMachineHandle* laserMachineHandle);
```

注册焊机资源，返回唯一句柄。句柄通过随机数生成并自动去重。

| 参数 | 方向 | 说明 |
|------|------|------|
| `laserMachineResource` | in | 焊机资源配置（通讯类型、品牌、寄存器地址映射等） |
| `laserMachineHandle` | out | 返回的焊机资源句柄 |

---

### 3.3 `cl_machine_resource_update`

```c
LaserSdkResult cl_machine_resource_update(
    LaserMachineHandle laserMachineHandle,
    const MachineResource* laserMachineResource);
```

更新已注册的焊机资源配置。

---

### 3.4 `cl_machine_resource_get`

```c
LaserSdkResult cl_machine_resource_get(
    LaserMachineHandle laserMachineHandle,
    MachineResource* laserMachineResource);
```

根据句柄获取焊机资源数据。

---

### 3.5 `cl_machine_resource_release`

```c
LaserSdkResult cl_machine_resource_release(
    LaserMachineHandle laserMachineHandle);
```

释放焊机资源句柄。

---

## 4. 焊接工艺脚本生成

### 4.1 起弧/收弧

```c
// 起弧脚本
LaserSdkResult cl_get_arc_start_script(
    LaserMachineHandle handle, char* scripts, int* scriptLen);

// 收弧脚本
LaserSdkResult cl_get_arc_stop_script(
    LaserMachineHandle handle, char* scripts, int* scriptLen);

// 缓升缓降起弧
LaserSdkResult cl_get_arc_start_slow_script(
    LaserMachineHandle handle, char* scripts, int* scriptLen);

// 缓升缓降收弧
LaserSdkResult cl_get_arc_stop_slow_script(
    LaserMachineHandle handle, char* scripts, int* scriptLen);
```

生成 LUA 格式的起弧/收弧控制脚本。

| 参数 | 方向 | 说明 |
|------|------|------|
| `handle` | in | 焊机资源句柄 |
| `scripts` | out | 生成的 LUA 脚本字符串 |
| `scriptLen` | in/out | in: scripts 缓冲区大小; out: 实际脚本长度 |

---

### 4.2 焊接状态管理

```c
// 初始化所有焊接相关状态
LaserSdkResult cl_craft_weld_status_init(LaserMachineHandle handle);

// 更改焊接设备状态
LaserSdkResult cl_craft_device_status_modify(
    LaserMachineHandle handle, char* scripts, int* scriptLen);

// 焊接状态监控脚本
LaserSdkResult cl_craft_weld_status_monitor_script_get(
    LaserMachineHandle handle, char* scripts, int* scriptLen);
```

---

### 4.3 指令列表脚本

```c
LaserSdkResult cl_craft_inst_list_script_get(
    LaserMachineHandle handle,
    LaserWeldStartData weldStartData,
    bool isWeld,
    char* scripts,
    int* scriptLen);
```

将指令列表（定点+工艺）转换为 LUA 执行脚本。

| 参数 | 方向 | 说明 |
|------|------|------|
| `handle` | in | 焊机资源句柄 |
| `weldStartData` | in | 指令列表数据（路点+工艺信息） |
| `isWeld` | in | `false`=轨迹模拟, `true`=开始焊接 |
| `scripts` | out | LUA 脚本 |
| `scriptLen` | in/out | 脚本长度 |

---

### 4.4 工艺参数脚本

```c
// 修改工艺参数生效后脚本
LaserSdkResult cl_craft_inst_devices_params_script_get(
    LaserMachineHandle handle,
    CraftInfoLaserSdk craftInfo,
    char* scripts, int* scriptLen);

// 收弧子程序脚本
LaserSdkResult cl_craft_end_arc_subprogram_script_get(
    LaserMachineHandle handle, char* scripts, int* scriptLen);

// 焊接结束后操作脚本
LaserSdkResult cl_craft_after_weld_end_operate_script_get(
    LaserMachineHandle handle, char* scripts, int* scriptLen);

// 获取指令函数脚本
LaserSdkResult cl_get_instance_function_script(
    const char* funcName, const char* funcParam,
    unsigned int addr, int regType,
    unsigned int coAddr, int comNo,
    char* scripts, int* scriptLen);
```

---

## 5. 机器人轨迹计算

### 5.1 直线轨迹回退

```c
LaserSdkResult cl_craft_straight_line_back_point_cal(
    LaserMachineHandle handle,
    RollbackStraightLineData* rollbackStraightLineData);
```

根据终点和降功率点距离，计算直线焊接回退后的新起/终点。

**数据结构**:
```c
struct RollbackStraightLineData {
    double startPoint[6];       // in:  起点位姿
    double endPoint[6];         // in:  终点位姿
    double distance;            // in:  回退距离
    double refJoint[6];         // in:  参考关节角
    double pointOut[6];         // out: 降功率点
    double pointJointOut[6];    // out: 降功率点关节角
};
```

---

### 5.2 圆弧轨迹回退

```c
LaserSdkResult cl_craft_circle_back_point_cal(
    LaserMachineHandle handle,
    RollbackCircleData* data);
```

根据终点和停止点距离，计算圆弧焊接回退后的新路径点。

**数据结构**:
```c
struct RollbackCircleData {
    double startPoint[6];          // in:  起点位姿
    double transPoint[6];          // in:  过渡点位姿
    double endPoint[6];            // in:  终点位姿
    double stopPoint[6];           // in:  停止点位姿
    double distance;               // in:  延迟距离
    double refJoint[6];            // in:  参考关节角
    double stopMappingPoint[6];    // out: 停止点映射点
    double newTransPoint[6];       // out: 新过渡点
    double startPointOut[6];       // out: 新起点
    double transPointOut[6];       // out: 新过渡点(位姿)
    double endPointOut[6];         // out: 新终点
    // 对应的关节角输出...
};
```

---

### 5.3 抬枪姿态计算

```c
LaserSdkResult cl_craft_lift_pose_cal(
    LaserMachineHandle handle,
    const double* pose,         // in:  抬枪前位姿 (6)
    const double* joint,        // in:  抬枪前关节角 (6)
    double liftDistance,        // in:  抬枪距离 (mm)
    double* liftPose,           // out: 抬枪后位姿 (6)
    double* liftJoint);         // out: 抬枪后关节角 (6)
```

---

## 6. Modbus 报文通讯

### 6.1 报文生成

```c
LaserSdkResult cl_message_generate(
    LaserMachineHandle handle,
    uint16_t firstAddr,         // in: 寄存器首地址
    uint16_t parameter,         // in: 参数值
    uint8_t  slaveAddr,         // in: 从站地址
    uint8_t  funcCode,          // in: 功能码 (仅支持 0x06)
    uint8_t* message,           // out: 生成的报文
    int*     length);           // out: 报文长度
```

### 6.2 报文写入寄存器

```c
// 写入 Bool 寄存器（暂不支持）
LaserSdkResult cl_write_message_to_bool_register(
    LaserMachineHandle handle,
    unsigned int firstRegister, unsigned int registerNums,
    uint8_t* message);

// 写入 Int16 寄存器
LaserSdkResult cl_write_message_to_Int16_register(
    LaserMachineHandle handle,
    unsigned int firstRegister, unsigned int registerNums,
    uint8_t* message);

// 写入状态监控到 Int32 寄存器 (占用 24 个内存)
LaserSdkResult cl_write_state_monitor_message_to_Int32_register(
    LaserMachineHandle handle,
    unsigned int firstRegister);
```

### 6.3 报文解析

```c
// 解析 Modbus 响应到 Int16
LaserSdkResult cl_parse_modbus_to_int16_register(
    LaserMachineHandle handle,
    const char* hexResponse,    // in: Modbus 响应报文
    int regIndex,               // in: 解析起始索引
    int regLen,                 // in: 解析个数
    int16_t* value);            // out: 解析结果

// 解析 Modbus 响应到 Int32
LaserSdkResult cl_parse_modbus_to_int32_register(
    LaserMachineHandle handle,
    const char* hexResponse,
    int32_t& value);
```

---

## 7. 数学工具函数 (`clmath.h`)

| 函数 | 说明 |
|------|------|
| `norm_vec_HY(a)` | 三维向量模 |
| `normalized_vector_HY(vec, result)` | 向量单位化 |
| `vecCross(xdir, ydir, result)` | 向量叉乘 |
| `matrix_vector_multiply_HY(matrix, vec, result)` | 3×3 矩阵乘向量 |
| `copy_pose_HY(old, new)` | 六维位姿复制 |
| `pose_trans_HY(pose, delta, newPose)` | 三维姿态变换 |
| `circle3dCalc(p1, p2, p3, r, center, axis)` | 三点定圆（半径+圆心+法向） |
| `getMapPoint(center, Fn, r, stopP, mapP)` | 圆弧上的映射点计算 |
| `kineInverse_tcpPose(handle, srcPose, refJoint, tarJoint)` | TCP 位姿逆解求关节角 |
| `getStraightLineSlowUpPoint(...)` | 直线轨迹降功率点计算 |
| `getCirclrSlowDownPoints(...)` | 圆弧轨迹降功率/回退点计算 |
| `CalBlendingRadius(vMax, aMax, r)` | 交融半径计算 |

---

## 8. 关键数据结构

### 8.1 `MachineResource` — 焊机资源

核心配置结构体，包含所有寄存器地址映射和子设备参数。

| 字段 | 类型 | 说明 |
|------|------|------|
| `commuType` | `CommuTypeLaserSdk` | 通讯形式 |
| `laserHeadBrand` | `LaserHeadBrandSdk` | 激光头品牌 |
| `robotHandle` | `RobotHandle` | 机器人句柄 |
| `weldingProcessType` | `WeldingProcessType` | 焊丝焊/自熔焊 |
| `laserPowerParams` | `CgxiLaserPowerLaserSdk` | 激光器参数 |
| `laserHeadParams` | `CgxiLaserHeadLaserSdk` | 激光头参数 |
| `wireFeederParams` | `CgxiWireFeederLaserSdk` | 送丝机参数 |
| `robotParams` | `CgxiRobotLaserSdk` | 机械臂参数 |
| *(寄存器地址字段)* | `unsigned int` | 50+ 个 Modbus 寄存器地址映射 |

### 8.2 `CraftInfoLaserSdk` — 焊接工艺参数

| 关键字段 | 类型 | 说明 |
|----------|------|------|
| `craftweldMode` | `WeldModeLaserSdk` | 点焊/连续焊 |
| `craftLaserPower` | `int` | 激光器焊接功率 (0-5000W) |
| `craftWeldingSpeed` | `double` | 焊接速度 (mm/s) |
| `craftScanWidth` | `double` | 扫描宽度 (0.1-5mm) |
| `craftScanHeight` | `double` | 扫描高度 (0.1-5mm) |
| `craftScanSpeed` | `int` | 扫描速度 (0-3000mm/s) |
| `craftLeadInPower` | `int` | 开光功率 |
| `craftLeadOutPower` | `int` | 关光功率 |
| `graphicsSwitching` | `SpotPatternLaserSdk` | 光斑图案(圆/8字/线/方/十字/三角) |

### 8.3 `LaserWeldStartData` — 指令列表

```c
struct LaserWeldStartData {
    int*  instList;              // {指令枚举, 数据索引} 数对列表
    int   instNum;               // 数对个数
    CraftInfoLaserSdk* craftInfoList;  // 工艺参数列表
    int   craftNum;              // 工艺参数个数
    PointDataLaserSdk* pointList;      // 路点列表
    int   pointsNum;             // 路点个数
    int*  signalStepPointLocList;      // 单步运行路点索引
    int   signalStepPointNum;
    int*  breakPointLocList;           // 断点位置索引
    int   breakPointNum;
};
```

---

## 9. 指令类型 `InstTypeLaserSdk`

| 值 | 常量 | 说明 |
|----|------|------|
| 0 | `CRAFT_SELECT_INST` | 工艺模板选择 |
| 1 | `ARC_START_INST` | 起弧指令 |
| 2 | `ARC_END_INST` | 收弧指令 |
| 3 | `LINE_POINT_INST` | 直线定点 |
| 4 | `CIRCLE_POINT_INST` | 圆弧定点 |
| 5 | `JOINT_POINT_INST` | 关节运动 |
| 6 | `SAFE_POINT_MOVEL_INST` | 安全点 (MoveL) |
| 7 | `SAFE_POINT_MOVEJ_INST` | 安全点 (MoveJ) |
| 8 | `SLOW_DOWN_INST` | 缓降指令 |

---

## 10. 光斑图案 `SpotPatternLaserSdk`

| 值 | 常量 | 说明 |
|----|------|------|
| 0 | `CIR_LASERSDK` | 圆形 (参数: 直径 mm) |
| 1 | `HORIZON_8_LASERSDK` | 8 字形 (参数: 长轴 mm) |
| 2 | `LINE_LASERSDK` | 水平线 (参数: 长度 mm) |
| 3 | `STAR_LASERSDK` | 正方形 (参数: 边长 mm) |
| 4 | `CROSS_LASERSDK` | 十字形 (参数: 边长 mm) |
| 5 | `TRIANGLE_LASERSDK` | 等边三角形 (参数: 边长 mm) |

---

## 11. 导出函数完整列表

共 **20 个** C 风格导出函数：

| # | 函数 | 分类 |
|---|------|------|
| 1 | `cl_sdk_version_get` | 版本 |
| 2 | `cl_machine_resource_init` | 资源管理 |
| 3 | `cl_machine_resource_update` | 资源管理 |
| 4 | `cl_machine_resource_get` | 资源管理 |
| 5 | `cl_machine_resource_release` | 资源管理 |
| 6 | `cl_get_arc_start_script` | 起弧 |
| 7 | `cl_get_arc_stop_script` | 收弧 |
| 8 | `cl_get_arc_start_slow_script` | 起弧(缓升) |
| 9 | `cl_get_arc_stop_slow_script` | 收弧(缓降) |
| 10 | `cl_craft_weld_status_init` | 状态管理 |
| 11 | `cl_craft_device_status_modify` | 状态管理 |
| 12 | `cl_craft_weld_status_monitor_script_get` | 状态监控 |
| 13 | `cl_craft_inst_list_script_get` | 指令列表 |
| 14 | `cl_craft_inst_devices_params_script_get` | 工艺参数 |
| 15 | `cl_craft_end_arc_subprogram_script_get` | 收弧子程序 |
| 16 | `cl_craft_after_weld_end_operate_script_get` | 焊后操作 |
| 17 | `cl_craft_straight_line_back_point_cal` | 直线回退 |
| 18 | `cl_craft_circle_back_point_cal` | 圆弧回退 |
| 19 | `cl_craft_lift_pose_cal` | 抬枪姿态 |
| 20 | `cl_message_generate` | 报文生成 |
| 21 | `cl_write_message_to_bool_register` | 报文写入 |
| 22 | `cl_write_message_to_Int16_register` | 报文写入 |
| 23 | `cl_write_state_monitor_message_to_Int32_register` | 报文写入 |
| 24 | `cl_parse_modbus_to_int16_register` | 报文解析 |
| 25 | `cl_parse_modbus_to_int32_register` | 报文解析 |
| 26 | `cl_get_instance_function_script` | 指令函数 |
