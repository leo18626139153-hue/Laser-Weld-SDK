#ifndef RS485_COMMUNICATER_H
#define RS485_COMMUNICATER_H

#include "base_commu.h"
#include "brandadapter.h"
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief RS485 通讯脚本生成类
 *
 * 品牌地址、功能码及缩放必须来自协议适配器，避免跨品牌复用报文规则。
 * 实例由单例管理，不转移所有权，调用方不得释放。
 *
 */
class CommuRS485 : public CommuBase {
public:
    ~CommuRS485() override {
    }
    static CommuRS485 *GetInstance();

private:
    CommuRS485();
    void initData();
    static CommuRS485 *s_commuRS485;
    static std::once_flag initFlag;
    int funcCode = 0;

private:
    /**
     * @brief 添加单寄存器写入脚本
     * @param writeDataHash 地址到 Lua 写入脚本的映射
     * @param writeSingleFuncCode 品牌单写脚本功能码
     * @param regType 参数编码类型
     * @param paramValue 参数值
     * @param addr 写寄存器地址
     * @param comNo 通讯口编号
     * @param receiptNeeded 是否等待回执
     * @return SDK 结果码
     *
     * 参数缩放与编码集中处理，避免不同入口对同一寄存器生成不同数值。
     */
    static LaserSdkResult addWriteParamScript(std::unordered_map<unsigned int, std::string> &writeDataHash, const std::string &writeSingleFuncCode,
        CommuRegType regType, double paramValue, unsigned int addr, int comNo, int receiptNeeded);

    /**
     * @brief 添加品牌语义对应的送丝机写入脚本
     * @param writeDataHash 地址到 Lua 写入脚本的映射
     * @param adapter 品牌协议适配器
     * @param writeSingleFuncCode 送丝机单写脚本功能码
     * @param role 送丝机参数语义
     * @param paramValue 参数值
     * @param fallbackAddr 当前 MachineResource 中的地址
     * @param receiptNeeded 是否等待回执
     * @return SDK 结果码
     *
     * 品牌地址之外仍需保留资源地址回退，兼容使用 locVec 历史地址的调用方。
     */
    static LaserSdkResult addWireFeederRoleParamScript(std::unordered_map<unsigned int, std::string> &writeDataHash, const BrandProtocolAdapter &adapter,
        const std::string &writeSingleFuncCode, BrandRegisterRole role, double paramValue, unsigned int fallbackAddr, int receiptNeeded);

    /**
     * @brief 根据寄存器地址识别送丝机参数角色
     * @param adapter 品牌协议适配器
     * @param machineResource 当前设备资源
     * @param addr 待匹配的寄存器地址
     * @param role 匹配成功后返回的送丝机参数角色
     * @return 匹配到送丝机参数角色返回 true，否则返回 false
     *
     * 角色决定当前焊接模式是否允许写入；资源地址回退用于兼容历史调用。
     */
    static bool findWireFeederRoleByAddr(const BrandProtocolAdapter &adapter, const MachineResource &machineResource, unsigned int addr,
        BrandRegisterRole &role);

    /**
     * @brief 构建缓起弧函数脚本
     * @param machineResource 当前设备资源
     * @return 缓起弧 Lua 函数脚本
     *
     * 自熔焊可能未连接送丝机，缓升时必须屏蔽送丝速度报文。
     */
    static std::string buildSlowArcStartFunctionScript(const MachineResource &machineResource);

    /**
     * @brief 构建缓收弧函数脚本
     * @param machineResource 当前设备资源
     * @return 缓收弧 Lua 函数脚本
     *
     * 自熔焊不依赖送丝机在线，缓降时必须屏蔽送丝速度报文。
     */
    static std::string buildSlowArcStopFunctionScript(const MachineResource &machineResource);

public:
    static std::string get_laser_param_set_script(std::map<unsigned int, int> locVec);

public:
    LaserSdkResult craft_script_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch) override;

    LaserSdkResult craft_script_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding,
        std::string &script) override;

    LaserSdkResult craft_laser_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec,
        std::string &script) override;

    LaserSdkResult craft_laser_head_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec,
        std::string &script) override;

    LaserSdkResult craft_wire_feeder_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec,
        std::string &script) override;

    LaserSdkResult craft_script_slow_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch) override;

    LaserSdkResult craft_script_slow_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding,
        std::string &script) override;

    void init_multiple_registers_value(std::string multipleRegistersValue);
    //切工艺
    LaserSdkResult craft_script_configure_machine_param_set_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk &craftInfo,
        std::string &script) override;
    //指令列表切工艺
    LaserSdkResult craft_script_write_craft_resource_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk &craftInfo,
        std::string &script) override;
};

#endif // RS485_COMMUNICATER_H
