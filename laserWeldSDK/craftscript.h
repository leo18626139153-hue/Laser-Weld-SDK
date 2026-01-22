#ifndef COMMUBASE_H
#define COMMUBASE_H

#include "cl_basedefine.h"
#include "internaldatadefine.h"
#include <vector>
#include <mutex>
#include<functional>

class CommuBase
{
protected:
    //功能函数脚本内容
    std::map<std::string, std::string> startLuaScripts;     //初始化接口
    std::map<std::string, std::string> startDataScripts;    //初始化变量

    /**
     * 功能函数脚本内容
     */
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
    virtual LaserSdkResult craft_laser_mul_params_set_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;
    //送丝机
    virtual LaserSdkResult craft_wire_feeder_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) = 0;
    //工艺参数修改配置生效脚本
    virtual LaserSdkResult craft_script_configure_machine_param_set_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo ,std::string& script) = 0;
    //参数检查
    virtual LaserSdkResult craft_script_laser_value_check_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;
    //激光头参数检查
    virtual LaserSdkResult craft_script_head_value_check_script_get(LaserMachineHandle laserMachineHandle) = 0;
    //送丝机参数检查
    virtual LaserSdkResult craft_script_wire_value_check_script_get(LaserMachineHandle laserMachineHandle, std::string& script) = 0;

    virtual LaserSdkResult craft_script_slow_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch) = 0;

    virtual LaserSdkResult craft_script_slow_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string& script) = 0;
    //将CraftInfo中的参数逐个判断并写入到machineResource,焊接过程中修改工艺参数
    virtual LaserSdkResult craft_script_write_craft_resource_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo, std::string& script) = 0;

public:
    //与具体通讯形式无关的功能函数
    //获取判断当前是否进行单步运行脚本
    LaserSdkResult craft_script_judge_signal_setp_run_script_get(int instLoc, std::string& script);
    //获取判断当前是否在断点脚本
    LaserSdkResult craft_script_judge_break_point_script_get(int instLoc, std::string& script);
    //返回初始化函数定义、初始化变量的脚本，并清空已加载的startLuaScripts、startDataScripts中的脚本，在所有接口调用完后调用
    std::string loadLuaFunctionScriptOut();
};


class CommuRS485 : public CommuBase
{
public:
    ~CommuRS485() override{}
    static CommuRS485* GetInstance();

private:
    CommuRS485();
    void initData();
    static CommuRS485* s_commuRS485;
    static std::once_flag initFlag; ///< initFlag >>> 保证单例对象只被初始化一次，且线程安全
    \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

public:
    //--报文组件
    static std::string get_laser_param_set_script(std::map<unsigned int, int> locVec);

public:
    // 起弧/收弧功能脚本
    LaserSdkResult craft_script_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch) override;
    // 起弧/收弧执行程序
    LaserSdkResult craft_script_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string& script) override;
    //激光器设置脚本
    LaserSdkResult craft_laser_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;
        
    //激光头图案设置脚本
    LaserSdkResult craft_laser_head_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;
    //送丝机设置脚本
    LaserSdkResult craft_wire_feeder_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;    

    //缓升缓降 起收弧功能脚本
    LaserSdkResult craft_script_slow_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch) override;
    //缓升缓降 起收弧执行程序
    LaserSdkResult craft_script_slow_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string& script) override;


    /**
     * @brief 10连续写地址相关接口，无法兼容超强以外的参数
     * @param multipleRegistersValue  传入的初始化脚本（通过loadLuaFunctionScript放入脚本中的初始变量），如果输入""，则调用超强板卡的专用接口
     */
    void init_multiple_registers_value(std::string multipleRegistersValue);

    /**
     * @brief 工艺参数修改配置生效脚本
     * @param laserMachineHandle 句柄
     * @param craftInfo 工艺参数
     * @param script    输出脚本
     * @return LaserSdkResult
     */
    LaserSdkResult craft_script_configure_machine_param_set_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo ,std::string& script) override;
    /**
     * @brief 修改工艺参数，多写（送丝机参数除外）
     * @param laserMachineHandle 句柄
     * @param craftInfo 焊接工艺参数
     * @param script    输出脚本
     * @return LaserSdkResult
     */
    LaserSdkResult craft_script_write_craft_resource_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo, std::string& script) override;

    /************* 未使用的接口，请勿用 ***********/ 
    //激光器设置,提前写入参数到寄存器，脚本直接调用接口发送
    LaserSdkResult craft_laser_mul_params_set_script_get(LaserMachineHandle laserMachineHandle, std::string& script) override;
    //激光器参数检查
    LaserSdkResult craft_script_laser_value_check_script_get(LaserMachineHandle laserMachineHandle, std::string& script) override;
    //激光头参数检查
    LaserSdkResult craft_script_head_value_check_script_get(LaserMachineHandle laserMachineHandle) override;
    //送丝机参数检查
    LaserSdkResult craft_script_wire_value_check_script_get(LaserMachineHandle laserMachineHandle, std::string& script) override;

};

#endif // COMMUBASE_H
