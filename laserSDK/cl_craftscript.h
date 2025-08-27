#ifndef COMMUBASE_H
#define COMMUBASE_H

#include "cl_basedefine.h"
#include "cl_internaldatadefine.h"

#include <mutex>


class CommuBase
{
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
    virtual LaserSdkResult craft_script_param_set_script_get(LaserMachineHandle laserMachineHandle, CgxiLaserPowerLaserSdk laserParam, std::string& script) = 0;
    
    //水冷机

    //激光头

    //送丝机

//    //获取渐变设置电流电压脚本 --基于传入参数渐变
//    virtual LaserSdkResult craft_script_step_set_current_voltage_script_get(LaserMachineHandle laserMachineHandle, double currentVal, double preCurrentVal, double voltageVal, double preVoltageVal, int totalTime, std::string& script) = 0;
//    //获取配置焊机并起弧脚本
//    virtual LaserSdkResult craft_script_configure_machine_arc_start_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;
//    //获取收弧脚本
//    virtual LaserSdkResult craft_script_arc_End_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;
//    //获取起弧保护脚本
//    virtual LaserSdkResult craft_script_arc_start_protection_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;
//    //获取收弧保护脚本
//    virtual LaserSdkResult craft_script_arc_end_protection_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;
//    //获取焊丝回抽与滞后送气脚本
//    virtual LaserSdkResult craft_script_wire_back_delaye_supply_gas_script_get(LaserMachineHandle laserMachineHandle, int wireRetractionTime, int delayeSupplyGasTime, std::string& script) = 0;
//    //获取焊接状态监控脚本
//    virtual LaserSdkResult craft_script_weld_status_monitor_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;

public:
    //----与具体通讯形式无关的功能函数
    //获取开关稳压稳流脚本 true-开 false-关
    LaserSdkResult craft_script_control_stability_voltage_current_script_get(bool operate, std::string& script);
    //获取设置稳压稳流的基准电流电压脚本
    LaserSdkResult craft_script_set_reference_current_voltage_script_get(float referenceCurrent, float referenceVoltage, std::string& script);
    //设置稳压稳流的基准电流电压
    LaserSdkResult craft_script_reference_current_voltage_set(RobotHandle robotHandle, float refCurrent, float refVoltage);
    //获取稳压稳流的基准电流电压
    LaserSdkResult craft_script_reference_current_voltage_get(RobotHandle robotHandle, float& refCurrent, float& refVoltage);
    //获取开关焊接状态监测脚本 true-开 false-关
    LaserSdkResult craft_script_control_weld_status_monitor_script_get(bool operate, std::string& script);
    //获取开关微调电流电压监控脚本 true-开 false-关
    LaserSdkResult craft_script_control_weld_current_voltage_adjust_monitor_script_get(bool operate, std::string& script);
    //获取设置微调后工艺电流电压的脚本
    LaserSdkResult craft_script_set_craft_adjust_currnet_voltage_val_script_get(float adjustCurrent, float adjustVoltage, std::string& script);
    //获取判断当前是否进行单步运行脚本
    LaserSdkResult craft_script_judge_signal_setp_run_script_get(int instLoc, std::string& script);
    //获取判断当前是否在断点脚本
    LaserSdkResult craft_script_judge_break_point_script_get(int instLoc, std::string& script);


};

class CommuRS485 : public CommuBase
{
public:
    ~CommuRS485() override{}
    static CommuRS485* GetInstance();

private:
    CommuRS485(){}

    static CommuRS485* s_commuRS485;
    // initFlag >>> 保证单例对象只被初始化一次，且线程安全
    static std::once_flag initFlag;

public:
    //获取设置开光 CO1
    LaserSdkResult craft_script_laser_on_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch, std::string& script) override;
    
    //获取设置起焊脚本 CO2
    LaserSdkResult craft_script_start_weld_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string& script) override;
    
    //激光器设置脚本
    LaserSdkResult craft_script_param_set_script_get(LaserMachineHandle laserMachineHandle, CgxiLaserPowerLaserSdk laserParams, std::string& script) override;

    //


};

#endif // COMMUBASE_H
