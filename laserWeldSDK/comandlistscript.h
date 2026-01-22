#ifndef COMANDLISTSCRIPT_H
#define COMANDLISTSCRIPT_H
#include "cl_basedefine.h"
#include "internaldatadefine.h"
#include "craftscript.h"
#include "utils.h"

//生成唯一ID
int distributionID(std::unordered_set<int>& IDSet);

//返回相应的工艺通讯类
LaserSdkResult craft_script_commu_class_get(LaserMachineHandle laserMachineHandle, CommuBase *&craftCommuClass);

/**
 * @brief GetCraftCheck 超强伟业图案参数检查
 * @param craftInfoCheck    工艺参数
 * @return LaserSdkResult
 */
LaserSdkResult GetCraftCheck(CraftInfoLaserSdk craftInfoCheck);

/**
 * @brief GetBasicCraftScript    基本工艺指令列表的转换
 * @param laserMachineHandle     焊机资源句柄
 * @param weldStartData          焊接指令列表数据
 * @param tempPointsMap          临时点位映射
 * @param instListTransEnvironmentData   指令列表转换环境数据
 * @param instListStartLoc    焊接指令列表起始位置
 * @param instListEndLoc      焊接指令列表结束位置           
 * @param bodyScriptText      脚本内容 传出参数
 * @return LaserSdkResult     处理结果
 */
LaserSdkResult GetBasicCraftScript(LaserMachineHandle laserMachineHandle, LaserWeldStartData weldStartData, InstListData& instListData, std::unordered_map<int, PointDataLaserSdk> tempPointsMap,InstListTransEnvironmentData &instListTransEnvironmentData, int instListStartLoc, int instListEndLoc, std::string &bodyScriptText);


#endif // CL_COMANDLISTSCRIPT_H
