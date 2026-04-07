#ifndef COMMUBASE_H
#define COMMUBASE_H

#include "cl_basedefine.h"
#include "internaldatadefine.h"
#include <vector>

/**
 * @brief 通讯基类
 */
class CommuBase
{
protected:
    //功能函数脚本内容
    std::map<std::string, std::string> startLuaScripts;     //初始化接口
    std::map<std::string, std::string> startDataScripts;    //初始化变量
    //输出功能函数脚本
    std::string loadLuaFunctionScript;
public:
    CommuBase(){}
    virtual ~CommuBase(){}

public:
    //----与具体通讯形式有关的功能函数    
    virtual LaserSdkResult craft_script_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch) = 0;
    //执行起弧和收弧
    virtual LaserSdkResult craft_script_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string& script) = 0;    
    //激光器参数设置脚本
    virtual LaserSdkResult craft_laser_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) = 0;
    //激光头
    virtual LaserSdkResult craft_laser_head_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) = 0;
    //激光器多参数设置脚本
    //virtual LaserSdkResult craft_laser_mul_params_set_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;
    //送丝机
    virtual LaserSdkResult craft_wire_feeder_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) = 0;
    //工艺参数修改配置生效脚本
    virtual LaserSdkResult craft_script_configure_machine_param_set_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo ,std::string& script) = 0;
//    //参数检查
//    virtual LaserSdkResult craft_script_laser_value_check_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;
//    //激光头参数检查
//    virtual LaserSdkResult craft_script_head_value_check_script_get(LaserMachineHandle laserMachineHandle) = 0;
//    //送丝机参数检查
//    virtual LaserSdkResult craft_script_wire_value_check_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;

    virtual LaserSdkResult craft_script_slow_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch) = 0;

    virtual LaserSdkResult craft_script_slow_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string& script) = 0;
    //将CraftInfo中的参数逐个判断并写入到machineResource,焊接过程中修改工艺参数
    virtual LaserSdkResult craft_script_write_craft_resource_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo, std::string& script) = 0;

public:
    //与具体通讯形式无关的功能函数
    //将生成的脚本赋值给传入的内存空间并进行安全检测
    LaserSdkResult craft_script_generate_script_copy(char* script, int* scriptLen, std::string& generatedScript);
    //获取判断当前是否进行单步运行脚本
    LaserSdkResult craft_script_judge_signal_setp_run_script_get(int instLoc, std::string& script);
    //获取判断当前是否在断点脚本
    LaserSdkResult craft_script_judge_break_point_script_get(int instLoc, std::string& script);
    //返回初始化函数定义、初始化变量的脚本，并清空已加载的startLuaScripts、startDataScripts中的脚本，在所有接口调用完后调用
    std::string loadLuaFunctionScriptOut();
};


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



#endif // COMMUBASE_H
