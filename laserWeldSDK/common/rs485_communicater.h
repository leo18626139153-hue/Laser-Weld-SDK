#ifndef RS485_COMMUNICATER_H
#define RS485_COMMUNICATER_H

#include "craftscript.h"
#include "brandadapter.h"
#include <map>
#include <mutex>
#include <string>
#include <vector>

/**
 * @brief RS485 通讯脚本生成类
 */
class CommuRS485 : public CommuBase
{
public:
    ~CommuRS485() override {}
    static CommuRS485* GetInstance();

private:
    CommuRS485();
    void initData();
    static CommuRS485* s_commuRS485;
    static std::once_flag initFlag;
    int funcCode = 0;

private:
    /**
     * @brief 根据寄存器地址识别送丝机参数角色
     * @param adapter 品牌协议适配器
     * @param machineResource 当前设备资源
     * @param addr 待匹配的寄存器地址
     * @param role 匹配成功后返回的送丝机参数角色
     * @return 匹配到送丝机参数角色返回 true，否则返回 false
     *
     * 修改理由：送丝机寄存器地址由品牌协议层维护，通讯层只根据地址识别业务角色。
     * 逻辑：优先匹配品牌协议适配器地址，再兼容旧版 MachineResource 中的送丝机地址。
     */
    static bool findWireFeederRoleByAddr(const BrandProtocolAdapter& adapter,
                                          const MachineResource& machineResource,
                                          unsigned int addr,
                                          BrandRegisterRole& role);

    /**
     * @brief 构建缓起弧函数脚本
     * @param machineResource 当前设备资源
     * @return 缓起弧 Lua 函数脚本
     *
     * 修改理由：缓起弧需要同时遵守品牌协议规则和焊接模式下的送丝机写入策略。
     * 逻辑：填丝焊输出功率与送丝速度缓升，自熔焊只输出功率缓升。
     */
    static std::string buildSlowArcStartFunctionScript(const MachineResource& machineResource);

    /**
     * @brief 构建缓收弧函数脚本
     * @param machineResource 当前设备资源
     * @return 缓收弧 Lua 函数脚本
     *
     * 修改理由：缓收弧与缓起弧共用品牌协议层的送丝机写入策略。
     * 逻辑：填丝焊输出功率与送丝速度缓降，自熔焊屏蔽送丝速度写入。
     */
    static std::string buildSlowArcStopFunctionScript(const MachineResource& machineResource);

public:
    static std::string get_laser_param_set_script(std::map<unsigned int, int> locVec);

public:
    LaserSdkResult craft_script_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch) override;
    LaserSdkResult craft_script_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string& script) override;

    LaserSdkResult craft_laser_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;
    LaserSdkResult craft_laser_head_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;
    LaserSdkResult craft_wire_feeder_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;

    LaserSdkResult craft_script_slow_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch) override;
    LaserSdkResult craft_script_slow_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string& script) override;

    void init_multiple_registers_value(std::string multipleRegistersValue);

    LaserSdkResult craft_script_configure_machine_param_set_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo, std::string& script) override;
    LaserSdkResult craft_script_write_craft_resource_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo, std::string& script) override;
};

#endif // RS485_COMMUNICATER_H
