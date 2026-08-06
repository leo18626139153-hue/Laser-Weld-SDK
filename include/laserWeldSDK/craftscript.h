#ifndef CRAFTSCRIPT_H
#define CRAFTSCRIPT_H

/**
 * @file    craftscript.h
 * @brief   工艺脚本引擎 — 指令列表转Lua焊接脚本
 *
 * SDK重构：CommuBase 通讯基类已移至 common/CommuBase.h，
 * 本文件保留工艺脚本引擎相关的声明。
 */

#include "common/base_commu.h"
#include "internaldatadefine.h"

/**
 * @brief 将经过校验的完整指令列表转换为Lua脚本。
 * @param[in] laserMachineHandle 焊机资源句柄。
 * @param[in] instListData       已校验的指令及关联数据。
 * @param[in,out] environment    顺序解释过程中的工艺、轨迹和生成策略状态。
 * @param[out] bodyScriptText    生成的脚本正文；失败时调用方不得使用。
 * @return LaserSdkResult        处理结果。
 */
LaserSdkResult GetBasicCraftScript(LaserMachineHandle laserMachineHandle,
    const InstListData& instListData,
    InstListTransEnvironmentData& environment,
    std::string &bodyScriptText);

#endif // CRAFTSCRIPT_H
