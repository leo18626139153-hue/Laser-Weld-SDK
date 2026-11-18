#ifndef LASER_RAMP_CONTROL_H
#define LASER_RAMP_CONTROL_H

#include "cl_basedefine.h"
#include <string>

/** @brief SDK 外控阶段配置及 Lua 生成；不表示设备已经执行或确认。 */
namespace ramp_control {
/** @brief 校验待注册资源的外控配置及其动态目标，失败时保持已注册资源。 */
LaserSdkResult ValidateResource(const MachineResource &resource);
/** @brief 只有显式启用时返回真，保持旧接口及旧工艺单位的默认行为。 */
bool Enabled(LaserMachineHandle handle);
/** @brief 生成共同运行时；自熔焊不查询送丝参数或输出送丝、回抽动作。 */
LaserSdkResult BuildRuntimeScript(LaserMachineHandle handle, const MachineResource &resource, std::string &script);
/** @brief 根据已校验的资源快照生成起弧、降功率点或正常终点动作。 */
LaserSdkResult BuildStageScript(LaserMachineHandle handle, const MachineResource &resource, SwitchTypeLaserSdk arcSwitch, bool rampDownOnly,
    std::string &script, const std::string *startPreparation = nullptr);
/** @brief 按新单位及无板卡时间渐变策略纯编译工艺参数，保留原工艺快照。 */
LaserSdkResult BuildCraftScript(LaserMachineHandle handle, const MachineResource &resource, const CraftInfoLaserSdk &craft, std::string &script);
}

#endif
