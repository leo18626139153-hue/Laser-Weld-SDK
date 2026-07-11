#ifndef CRAFTSCRIPT_H
#define CRAFTSCRIPT_H

/**
 * @file    craftscript.h
 * @brief   工艺脚本引擎 — 指令列表转Lua焊接脚本
 *
 * SDK重构：CommuBase 通讯基类已移至 common/CommuBase.h，
 * 本文件保留工艺脚本引擎相关的声明。
 */

#include "common/CommuBase.h"
#include "internaldatadefine.h"

/**
 * @brief GetBasicCraftScript    基本工艺指令列表的转换
 * @param laserMachineHandle     焊机资源句柄
 * @param weldStartData          焊接指令列表数据
 * @param instListData           指令列表环境数据
 * @param tempPointsMap          临时点位映射
 * @param instListTransEnvironmentData   指令列表转换环境数据
 * @param instListStartLoc       焊接指令列表起始位置
 * @param instListEndLoc         焊接指令列表结束位置
 * @param bodyScriptText         脚本内容 传出参数
 * @return LaserSdkResult        处理结果
 */
LaserSdkResult GetBasicCraftScript(LaserMachineHandle laserMachineHandle,
    LaserWeldStartData weldStartData,
    InstListData& instListData,
    std::unordered_map<int, PointDataLaserSdk> tempPointsMap,
    InstListTransEnvironmentData &instListTransEnvironmentData,
    int instListStartLoc,
    int instListEndLoc,
    std::string &bodyScriptText);

#endif // CRAFTSCRIPT_H
