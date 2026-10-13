#ifndef LASER_PARAMETER_SCRIPT_H
#define LASER_PARAMETER_SCRIPT_H

#include "common/parametercompiler.h"
#include <string>

/** @file parameterscript.h @brief 寄存器写入计划到控制器本地映射 Lua 的渲染接口。 */

namespace laser_parameters
{

/**
 * @brief 将写入计划渲染为已配置的本地十六位映射寄存器 Lua 写入。
 * @param[in] plan 参数编译器输出；不包含端口、站号或机器人句柄。
 * @param[in,out] script 成功时整体替换，任何失败均保持原脚本不变。
 * @retval SUCCESS_LASER_SDK 渲染成功；空计划得到空脚本。
 * @retval PARAMETER_ERROR_LASER_SDK 参数编号、寄存器字数、符号标志或地址重叠无效。
 * @retval PARAMETER_OUT_OF_RANGE_LASER_SDK 寄存器地址或多字占用范围越界。
 * @retval MEMORY_ERROR_LASER_SDK 输出流或字符串分配失败。
 * @details 按计划顺序输出 write_int16_register；多字参数逐字写入连续地址。
 *          十六位有符号参数输出负数表示，其他字输出无符号原始值，不重新缩放或改变字序。
 * @note 只返回脚本文本，不执行通讯、主从站配置或设备动作；多字写入的实际执行不具有事务保证。
 */
LaserSdkResult EmitMappedLua(const WritePlan& plan, std::string& script);

} // namespace laser_parameters

#endif // LASER_PARAMETER_SCRIPT_H
