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



#endif // COMMUBASE_H
