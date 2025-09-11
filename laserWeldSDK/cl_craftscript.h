#ifndef COMMUBASE_H
#define COMMUBASE_H

#include "cl_basedefine.h"
#include "cl_internaldatadefine.h"

#include <mutex>


class CommuBase
{
    protected:
    //功能函数脚本内容
    std::string loadLuaFunctionScript;

public:
    CommuBase(){};
    virtual ~CommuBase(){}

public:
    //----与具体通讯形式有关的功能函数

    //激光器

    //获取设置开光脚本 CO1
    virtual LaserSdkResult craft_script_laser_on_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch, std::string& script) = 0;

    //获取设置起焊脚本 CO2
    virtual LaserSdkResult craft_script_start_weld_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string& script) = 0;
    
    //激光器参数设置脚本
    virtual LaserSdkResult craft_laser_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) = 0;

    //激光头设置页
    virtual LaserSdkResult craft_laser_head_setting_set_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;
    
    //水冷机
    //virtual LaserSdkResult craft_water_cooler_param_set_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;

    //激光头
    virtual LaserSdkResult craft_laser_head_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) = 0;

    //激光头参数读取
    virtual LaserSdkResult craft_laser_head_param_read_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;

    //送丝机
    virtual LaserSdkResult craft_wire_feeder_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) = 0;

    //工艺参数修改配置生效脚本
    virtual LaserSdkResult craft_script_configure_machine_param_set_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo ,std::string& script) = 0;

    //获取配置焊机并起弧脚本
    //virtual LaserSdkResult craft_script_configure_machine_arc_start_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;

public:
    //----与具体通讯形式无关的功能函数
    //预留
    LaserSdkResult craft_script_control_stability_voltage_current_script_get(bool operate, std::string& script);
    //获取判断当前是否进行单步运行脚本
    LaserSdkResult craft_script_judge_signal_setp_run_script_get(int instLoc, std::string& script);
    //获取判断当前是否在断点脚本
    LaserSdkResult craft_script_judge_break_point_script_get(int instLoc, std::string& script);

    std::string loadLuaFunctionScriptOut();
};


class CommuRS485 : public CommuBase
{
public:
    ~CommuRS485() override{}
    static CommuRS485* GetInstance();

private:
    CommuRS485();

    static CommuRS485* s_commuRS485;
    // initFlag >>> 保证单例对象只被初始化一次，且线程安全
    static std::once_flag initFlag;


public:
    //出光控制 CO1
    LaserSdkResult craft_script_laser_on_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch, std::string& script) override;
    //获取设置起焊脚本 CO2
    LaserSdkResult craft_script_start_weld_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string& script) override;
    
    //激光器设置脚本
    LaserSdkResult craft_laser_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;

    //激光头图案资源
    LaserSdkResult craft_laser_head_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;

    //激光头图案资源读取
    LaserSdkResult craft_laser_head_param_read_script_get(LaserMachineHandle laserMachineHandle, std::string& script) override;

    //激光头设置页
    LaserSdkResult craft_laser_head_setting_set_script_get(LaserMachineHandle laserMachineHandle, std::string& script) override;

    //送丝机
    LaserSdkResult craft_wire_feeder_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;

    //工艺参数修改配置生效脚本
    LaserSdkResult craft_script_configure_machine_param_set_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo ,std::string& script) override;

    //获取配置焊机并起弧脚本
    //LaserSdkResult craft_script_configure_machine_arc_start_script_get(LaserMachineHandle laserMachineHandle, std::string& script) override;
};

#endif // COMMUBASE_H
