#ifndef COMMUBASE_H
#define COMMUBASE_H

/**
 * @file    CommuBase.h
 * @brief   通讯基类 — 抽象通讯接口，定义焊接工艺脚本生成的标准通讯协议
 *
 * 作为所有通讯实现（RS485、EtherCAT、模拟量等）的统一抽象基类。
 *
 * 继承关系：
 *   CommuBase  (本文件)
 *     └── CommuRS485  (rs485_commu.h)
 */

#include "cl_basedefine.h"
#include <vector>
#include <map>
#include <string>
#include <unordered_set>

/**
 * @brief 通讯基类
 *
 * 定义焊接工艺中与具体通讯协议无关的标准接口。
 * 子类实现各协议（RS485/EtherCAT/模拟量）的具体报文组装逻辑。
 *
 * SDK重构：报文组装由上层SDK（C++层）统一控制，
 * 脚本中不再出现 Lua_get_message / Lua_crc16 / swap_bytes_for_word 等组装接口，
 * 脚本直接接收完整的 Modbus 报文。
 */
class CommuBase
{
protected:
    // 【逻辑】startLuaScripts 存储初始化函数定义脚本（key=函数名, value=脚本内容）
    std::map<std::string, std::string> startLuaScripts;
    // 【逻辑】startDataScripts 存储初始化数据/变量脚本
    std::map<std::string, std::string> startDataScripts;
    // 【逻辑】loadLuaFunctionScript 汇总所有功能函数脚本，由 loadLuaFunctionScriptOut() 输出
    std::string loadLuaFunctionScript;

public:
    CommuBase() {}
    virtual ~CommuBase() {}

    // =================================================================
    // 纯虚接口 — 与具体通讯协议相关，子类必须实现
    // =================================================================
    /**
     * @brief 起弧/收弧功能开关脚本
     * @param[in]  laserMachineHandle  焊机资源句柄
     * @param[in]  laserSwitch         开关类型（开/关）
     * @return LaserSdkResult
     */
    virtual LaserSdkResult craft_script_arc_function_switch_script_get(
        LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch) = 0;

    /**
     * @brief 执行起弧和收弧程序脚本
     * @param[in]  laserMachineHandle  焊机资源句柄
     * @param[in]  startWelding        焊接启停开关
     * @param[out] script              生成的Lua脚本
     * @return LaserSdkResult
     */
    virtual LaserSdkResult craft_script_arc_program_run_script_get(
        LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding,
        std::string& script) = 0;

    /**
     * @brief 激光器参数设置脚本
     * @param[in]  laserMachineHandle  焊机资源句柄
     * @param[in]  locVec              参数地址列表
     * @param[out] script              生成的Lua脚本
     * @return LaserSdkResult
     */
    virtual LaserSdkResult craft_laser_param_set_script_get(
        LaserMachineHandle laserMachineHandle,
        const std::vector<unsigned int> locVec, std::string& script) = 0;

    /**
     * @brief 激光头参数设置脚本
     * @param[in]  laserMachineHandle  焊机资源句柄
     * @param[in]  locVec              参数地址列表
     * @param[out] script              生成的Lua脚本
     * @return LaserSdkResult
     */
    virtual LaserSdkResult craft_laser_head_param_set_script_get(
        LaserMachineHandle laserMachineHandle,
        const std::vector<unsigned int> locVec, std::string& script) = 0;

    /**
     * @brief 送丝机参数设置脚本
     * @param[in]  laserMachineHandle  焊机资源句柄
     * @param[in]  locVec              参数地址列表
     * @param[out] script              生成的Lua脚本
     * @return LaserSdkResult
     */
    virtual LaserSdkResult craft_wire_feeder_param_set_script_get(
        LaserMachineHandle laserMachineHandle,
        const std::vector<unsigned int> locVec, std::string& script) = 0;

    /**
     * @brief 工艺参数修改配置生效脚本（cl_craft_inst_devices_params_script_get 内部调用）
     * @param[in]  laserMachineHandle  焊机资源句柄
     * @param[in]  craftInfo           工艺参数结构体
     * @param[out] script              生成的Lua脚本
     * @return LaserSdkResult
     */
    virtual LaserSdkResult craft_script_configure_machine_param_set_script_get(
        LaserMachineHandle laserMachineHandle,
        const CraftInfoLaserSdk& craftInfo, std::string& script) = 0;

    /**
     * @brief 缓升缓降起/收弧功能开关脚本
     * @param[in]  laserMachineHandle  焊机资源句柄
     * @param[in]  laserSwitch         开关类型
     * @return LaserSdkResult
     */
    virtual LaserSdkResult craft_script_slow_arc_function_switch_script_get(
        LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch) = 0;

    /**
     * @brief 缓升缓降起/收弧程序执行脚本
     * @param[in]  laserMachineHandle  焊机资源句柄
     * @param[in]  startWelding        焊接启停开关
     * @param[out] script              生成的Lua脚本
     * @return LaserSdkResult
     */
    virtual LaserSdkResult craft_script_slow_arc_program_run_script_get(
        LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding,
        std::string& script) = 0;

    /**
     * @brief 将CraftInfo中的参数逐个判断并写入MachineResource（焊接过程中修改工艺参数）
     * @param[in]  laserMachineHandle  焊机资源句柄
     * @param[in]  craftInfo           工艺参数结构体
     * @param[out] script              生成的Lua脚本
     * @return LaserSdkResult
     */
    virtual LaserSdkResult craft_script_write_craft_resource_script_get(
        LaserMachineHandle laserMachineHandle,
        const CraftInfoLaserSdk& craftInfo, std::string& script) = 0;

    // =================================================================
    // 非虚接口 — 与具体通讯协议无关，基类统一实现
    // =================================================================

    /**
     * @brief 将生成的脚本安全拷贝到传出缓冲区
     * @param[out] script          传出脚本缓冲区
     * @param[in,out] scriptLen    缓冲区大小（传入）/ 实际长度（传出）
     * @param[in]  generatedScript 已生成的脚本字符串
     * @return LaserSdkResult
     */
    LaserSdkResult craft_script_generate_script_copy(
        char* script, int* scriptLen, std::string& generatedScript);

    /**
     * @brief 获取单步运行判断脚本
     * @param[in]  instLoc  指令位置
     * @param[out] script   生成的Lua脚本
     * @return LaserSdkResult
     */
    LaserSdkResult craft_script_judge_signal_setp_run_script_get(
        int instLoc, std::string& script);

    /**
     * @brief 获取断点判断脚本
     * @param[in]  instLoc  指令位置
     * @param[out] script   生成的Lua脚本
     * @return LaserSdkResult
     */
    LaserSdkResult craft_script_judge_break_point_script_get(int instLoc, std::string& script);

    /**
     * @brief 输出已加载的初始化函数定义和变量脚本，并清空内部缓存
     * @return 初始化脚本内容（函数定义 + 变量初始化）
     */
    std::string loadLuaFunctionScriptOut();
};

/**
 * @brief 获取焊机通讯类实例（工厂方法）
 * @param[in]  laserMachineHandle  焊机资源句柄
 * @param[out] craftCommuClass     通讯类实例指针（传出），由调用者负责生命周期
 * @return LaserSdkResult
 */
LaserSdkResult craft_script_commu_class_get(LaserMachineHandle laserMachineHandle, CommuBase *&craftCommuClass);

#endif // COMMUBASE_H
