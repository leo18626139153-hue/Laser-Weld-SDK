#ifndef CL_COMANDLISTSCRIPT_H
#define CL_COMANDLISTSCRIPT_H
#include "cl_basedefine.h"
#include "cl_internaldatadefine.h"
#include "cl_craftscript.h"
#include "cl_utils.h"


#include <functional>

class cl_comandListScript
{
public:
    cl_comandListScript(){};
};

//计算交融半径
LaserSdkResult CalBlendingRadius(double vMax, double aMax, double* r);


//返回相应的工艺通讯类
LaserSdkResult craft_script_commu_class_get(LaserMachineHandle laserMachineHandle, CommuBase*& craftCommuClass);

/**
 * @brief GetBasicCraftScript    基本工艺指令列表的转换
 * @param laserMachineHandle     焊机资源句柄
 * @param weldStartData           焊接指令列表数据
 * @param tempPointsMap          临时点位映射
 * @param instListTransEnvironmentData   指令列表转换环境数据
 * @param instListStartLoc    焊接指令列表起始位置
 * @param instListEndLoc      焊接指令列表结束位置           
 * @param bodyScriptText         脚本内容 传出参数
 * @return LaserSdkResult        处理结果
 */
LaserSdkResult GetBasicCraftScript(LaserMachineHandle laserMachineHandle,LaserWeldStartData weldStartData, InstListData& instListData, std::unordered_map<int, PointDataLaserSdk> tempPointsMap,InstListTransEnvironmentData &instListTransEnvironmentData, int instListStartLoc, int instListEndLoc, std::string &bodyScriptText);


#endif // CL_COMANDLISTSCRIPT_H
