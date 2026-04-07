#ifndef COMANDLISTSCRIPT_H
#define COMANDLISTSCRIPT_H
#include "cl_basedefine.h"
#include "internaldatadefine.h"
#include "utils.h"
#include "craftscript.h"

/**
 * @brief distributionID 生成指令列表转换过程中需要用到的ID，保证每次调用GetBasicCraftScript生成的ID不重复
 * @param IDSet  已生成的ID集合，传入时包含之前生成的ID，函数内部会将新生成的ID加入集合
 * @return int 生成的ID
 */
int distributionID(std::unordered_set<int>& IDSet);

/**
 * @brief craft_script_commu_class_get 获取焊机通讯类实例
 * @param laserMachineHandle 焊机资源句柄
 * @param craftCommuClass 焊机通讯类实例 传出参数
 * @return LaserSdkResult 处理结果
 */
LaserSdkResult craft_script_commu_class_get(LaserMachineHandle laserMachineHandle, CommuBase *&craftCommuClass);



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
