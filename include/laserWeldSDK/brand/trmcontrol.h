#ifndef LASER_TRM_CONTROL_H
#define LASER_TRM_CONTROL_H

#include "cl_basedefine.h"
#include "trmdefine.h"
#include "parametercompiler.h"
#include <string>

namespace trm_control {

/** @brief 删除句柄或变更设备配置时撤销档案授权。 */
void Release(LaserMachineHandle handle);

/** @brief 复制当前资源与映射档案。调用方串行化SDK接口。 */
LaserSdkResult Snapshot(LaserMachineHandle handle, MachineResource &resource, LaserTrmProfileSdk &profile);

/** @brief 校验工艺并生成SDK运行时的参数表，过程号固定为1。 */
LaserSdkResult BuildCraft(const MachineResource &resource, const LaserTrmProfileSdk &profile, const CraftInfoLaserSdk &craft, const LaserTrmCraftSdk &values,
    std::string &definition);

/** @brief 判断工艺会话能否启动，包含当前焊接方式的能力检查。 */
bool CanWeld(const MachineResource &resource, const LaserTrmProfileSdk &profile, unsigned capability);

/** @brief 获取特尔码普通工艺参数描述与已验收的本地映射。 */
LaserSdkResult GetParameterConfiguration(LaserMachineHandle handle, laser_parameters::Configuration &configuration);

/** @brief 生成普通工艺寄存器的白名单写入，不改变流程号和控制字。 */
LaserSdkResult BuildParameterScript(LaserMachineHandle handle, const std::vector<LaserParameterValueSdk> &values, std::string &script,
    LaserParameterDiagnosticSdk *diagnostic);

/** @brief 生成特尔码脚本运行时，使用确认过的映射、反馈和控制字。 */
std::string BuildRuntime(const LaserTrmProfileSdk &profile, bool parametersOnly = false);

/** @brief 生成同一程序会话的停、暂停、恢复事件处理。 */
std::string BuildProtection();

} // namespace trm_control

#endif
