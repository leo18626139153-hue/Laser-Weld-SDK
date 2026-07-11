#ifndef CL_CRAFTSCRIPT_H
#define CL_CRAFTSCRIPT_H

/**
 * @file    cl_craftscript.h
 * @brief   工艺脚本接口（公共API）— CommuRS485 类定义
 *
 * SDK重构：CommuBase 通讯基类已移至 common/CommuBase.h，
 * 本文件包含 CommuRS485 具体实现类的完整声明。
 */

#include "common/CommuBase.h"
#include "cl_internaldatadefine.h"
#include <vector>
#include <mutex>
#include <functional>

class CommuRS485 : public CommuBase
{
public:
    ~CommuRS485() override {}
    static CommuRS485* GetInstance();

private:
    CommuRS485();
    void initData();
    static CommuRS485* s_commuRS485;
    static std::once_flag initFlag; ///< 保证单例对象只被初始化一次，且线程安全
    int funcCode;  ///< 功能码

public:
    //-- 报文组件
    static std::string get_laser_param_set_script(std::map<unsigned int, int> locVec);

public:
    // 起弧/收弧功能脚本
    LaserSdkResult craft_script_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch) override;
    // 起弧/收弧执行程序
    LaserSdkResult craft_script_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string& script) override;
    // 激光器设置脚本
    LaserSdkResult craft_laser_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;

    // 激光头图案设置脚本
    LaserSdkResult craft_laser_head_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;
    // 送丝机设置脚本
    LaserSdkResult craft_wire_feeder_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;

    // 缓升缓降 起收弧功能脚本
    LaserSdkResult craft_script_slow_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch) override;
    // 缓升缓降 起收弧执行程序
    LaserSdkResult craft_script_slow_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string& script) override;

    /**
     * @brief 10连续写地址相关接口，无法兼容超强以外的参数
     * @param multipleRegistersValue 传入的初始化脚本，
     *        如果输入""，则调用超强板卡的专用接口
     */
    void init_multiple_registers_value(std::string multipleRegistersValue);

    /**
     * @brief 工艺参数修改配置生效脚本
     */
    LaserSdkResult craft_script_configure_machine_param_set_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo, std::string& script) override;

    /**
     * @brief 修改工艺参数，多写（送丝机参数除外）
     */
    LaserSdkResult craft_script_write_craft_resource_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo, std::string& script) override;

    /************* 未使用的接口，请勿用 ***********/
    // 激光器设置,提前写入参数到寄存器，脚本直接调用接口发送
    LaserSdkResult craft_laser_mul_params_set_script_get(LaserMachineHandle laserMachineHandle, std::string& script) override;
    // 激光器参数检查
    LaserSdkResult craft_script_laser_value_check_script_get(LaserMachineHandle laserMachineHandle, std::string& script) override;
    // 激光头参数检查
    LaserSdkResult craft_script_head_value_check_script_get(LaserMachineHandle laserMachineHandle) override;
    // 送丝机参数检查
    LaserSdkResult craft_script_wire_value_check_script_get(LaserMachineHandle laserMachineHandle, std::string& script) override;
};

#endif // CL_CRAFTSCRIPT_H
