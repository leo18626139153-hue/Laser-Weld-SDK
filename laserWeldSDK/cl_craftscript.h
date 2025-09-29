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
    //获取设置开光脚本 CO1
    virtual LaserSdkResult craft_script_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch, std::string& script) = 0;
    //获取设置起焊脚本 CO2
    virtual LaserSdkResult craft_script_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string& script) = 0;    
    //激光器参数设置脚本
    virtual LaserSdkResult craft_laser_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) = 0;
    //激光头设置页
    virtual LaserSdkResult craft_laser_head_setting_set_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;    
    //水冷机
    //virtual LaserSdkResult craft_water_cooler_param_set_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;
    //激光头
    virtual LaserSdkResult craft_laser_head_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) = 0;
    //送丝机
    virtual LaserSdkResult craft_wire_feeder_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) = 0;
    //工艺参数修改配置生效脚本
    virtual LaserSdkResult craft_script_configure_machine_param_set_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo ,std::string& script) = 0;
    //参数检查
    virtual LaserSdkResult craft_script_value_check_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;

    //获取配置焊机并起弧脚本
    //virtual LaserSdkResult craft_script_configure_machine_arc_start_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;

public:
    //----与具体通讯形式无关的功能函数
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
    static std::string abcd;//    static std::string abcd1;

private:
    CommuRS485();
    static CommuRS485* s_commuRS485;
    static std::once_flag initFlag; ///< initFlag >>> 保证单例对象只被初始化一次，且线程安全
private:
    std::string swap_bytes;      ///< 高低字节交换函数
    std::string modbus_crc16;    ///< modbus crc16校验函数
    std::string lua_split;       ///< lua字符串分割函数
    std::string register_clear; ///< lua寄存器清零函数
    std::string lua_data_get;   ///< 定义数据拆分整合函数
    std::string concatenate_crc;    ///< 拼接crc校验函数
    std::string parse_little_endian_bytes;  ///< 每2个字符为一组（1字节），转换为整数
    std::string writeHex2Commu_reg;     ///< 将数据写入通讯变量
    //std::string task_read_port; ///< 读取通讯变量
    std::string task_write_port;    ///< 写入通讯变量
    std::string startData;      ///< 起始变量
public:
    // 起 收弧子程序
    LaserSdkResult craft_script_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch, std::string& script) override;
    // 起收弧执行程序
    LaserSdkResult craft_script_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string& script) override;  
    //激光器设置脚本
    LaserSdkResult craft_laser_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;
    //激光头图案资源
    LaserSdkResult craft_laser_head_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;
    //激光头设置页
    LaserSdkResult craft_laser_head_setting_set_script_get(LaserMachineHandle laserMachineHandle, std::string& script) override;
    //送丝机
    LaserSdkResult craft_wire_feeder_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;
    //工艺参数修改配置生效脚本
    LaserSdkResult craft_script_configure_machine_param_set_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo ,std::string& script) override;
    //获取配置焊机并起弧脚本
    //LaserSdkResult craft_script_configure_machine_arc_start_script_get(LaserMachineHandle laserMachineHandle, std::string& script) override;
    //参数检查
    LaserSdkResult craft_script_value_check_script_get(LaserMachineHandle laserMachineHandle, std::string& script) override;
};

#endif // COMMUBASE_H
