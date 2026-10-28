#ifndef LASER_RAMP_CONTROL_H
#define LASER_RAMP_CONTROL_H

#include "cl_basedefine.h"
#include "rampcontroldefine.h"
#include <string>

/** @brief SDK 外控阶段配置及 Lua 生成；不表示设备已经执行或确认。 */
namespace ramp_control {
/** @brief 校验并深复制句柄配置；失败保留旧配置，不执行设备动作。 */
LaserSdkResult SetConfig(LaserMachineHandle handle, const LaserRampControlConfigSdk *config);
/** @brief 获取独立配置；未配置时返回禁用状态及明确的默认时间参数。 */
LaserSdkResult GetConfig(LaserMachineHandle handle, LaserRampControlConfigSdk *config);
/** @brief 清理配置并恢复禁用状态；不取消控制器上已经加载的旧程序。 */
LaserSdkResult ResetConfig(LaserMachineHandle handle);
/** @brief 释放句柄或切换型号时清理该句柄的外控策略。 */
void ReleaseConfig(LaserMachineHandle handle);
/** @brief 只有显式启用时返回真，保持旧接口及旧工艺单位的默认行为。 */
bool Enabled(LaserMachineHandle handle);
/** @brief 生成共同运行时；自熔焊不查询送丝参数或输出送丝、回抽动作。 */
LaserSdkResult BuildRuntimeScript(LaserMachineHandle handle, const MachineResource &resource, std::string &script);
/** @brief 根据已校验的资源快照生成起弧、降功率点或正常终点动作。 */
LaserSdkResult BuildStageScript(LaserMachineHandle handle, const MachineResource &resource, SwitchTypeLaserSdk arcSwitch, bool rampDownOnly,
    std::string &script);
/** @brief 按新单位及无板卡时间渐变策略纯编译工艺参数，保留原工艺快照。 */
LaserSdkResult BuildCraftScript(LaserMachineHandle handle, const MachineResource &resource, const CraftInfoLaserSdk &craft, std::string &script);
}

#endif
