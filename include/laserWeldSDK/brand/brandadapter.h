#ifndef BRANDADAPTER_H
#define BRANDADAPTER_H

#include "internaldatadefine.h"
#include "utils.h"
#include "laserModbusRtuBuilder.h"
#include <vector>

using ParamMap = LaserModbusRtuBuilder::ParamMap;

/**
 * @brief 品牌寄存器语义枚举
 *
 * 通讯层只按业务语义请求寄存器信息, 不直接依赖具体品牌地址表.
 */
enum class BrandRegisterRole
{
    WireFeederRunStatus,     ///< 送丝机启动或暂停
    AutoWireFeedSpeed,       ///< 自动送丝速度
    ManualWireFeedSpeed,     ///< 手动送丝速度
    ManualWireRetractSpeed,  ///< 手动抽丝速度
    WireRetractLength,       ///< 抽丝长度
    WireFeedLength,          ///< 补丝长度
    WireFeedBackDelay,       ///< 补丝延时
    BreakWireFeedingMode,    ///< 断丝模式
    ManualWireFeedSwitch,    ///< 手动送丝开关
    ManualWireRetractSwitch, ///< 手动回抽开关
    SaveWireFeederData,      ///< 保存送丝机数据
    WireFeedSignal,          ///< 送丝信号
    GasValveSignal,          ///< 气阀信号
    LaserOnOff               ///< 激光开关信号
};

/**
 * @brief 品牌寄存器描述
 */
struct BrandRegisterInfo
{
    unsigned int writeAddr; ///< 写寄存器地址
    unsigned int readAddr;  ///< 读寄存器地址
    CommuRegType regType;   ///< 通讯层写入时使用的数据编码类型
    double scale;           ///< 参数缩放比例
    bool writable;          ///< 是否允许写入
};

/**
 * @brief 品牌 Modbus 通讯基础参数
 */
struct BrandProtocolProfile
{
    unsigned int slaveAddr;           ///< Modbus 从站地址
    unsigned int readFuncCode;        ///< 读寄存器功能码
    unsigned int writeSingleFuncCode; ///< 写单寄存器功能码
    unsigned int writeMultiFuncCode;  ///< 写多寄存器功能码
    unsigned int laserWriteSingleScriptCode;      ///< 激光器和激光头单写脚本功能码
    unsigned int wireFeederWriteSingleScriptCode; ///< 送丝机单写脚本功能码
    unsigned int writeMultiScriptCode;            ///< 批量写脚本功能码
    int baudRate;                     ///< 串口波特率
    int dataBits;                     ///< 串口数据位
    int stopBits;                     ///< 串口停止位
};

/**
 * @brief 品牌协议适配器基类
 *
 * 修改理由: 通讯类只负责 RS485/Modbus 脚本拼接, 品牌地址表和送丝机写入策略固定到品牌层.
 * 逻辑: 通讯层通过寄存器语义查询地址, 校验规则和送丝机写入策略, 不再直接判断具体品牌.
 *
 * @legacy_source src/brand/brandadapter.cpp
 * @legacy_baseline 6fefa14
 * @compatibility 所有虚函数签名和品牌地址映射保持不变，新增品牌只需新增适配器子类
 */
class BrandProtocolAdapter
{
public:
    virtual ~BrandProtocolAdapter() {}

    /**
     * @brief 获取适配器对应的激光头品牌
     * @return 激光头品牌枚举
     */
    virtual LaserHeadBrandSdk brand() const = 0;

    /**
     * @brief 获取品牌通讯协议基础参数
     * @return 品牌 Modbus 通讯参数
     */
    virtual BrandProtocolProfile profile() const = 0;

    /**
     * @brief 检查品牌工艺参数范围
     * @param[in] craftInfo 工艺参数
     * @return SDK 结果码
     */
    virtual LaserSdkResult checkCraft(const CraftInfoLaserSdk& craftInfo) const = 0;

    /**
     * @brief 判断品牌是否定义指定语义寄存器
     * @param[in] role 寄存器语义
     * @return true 表示已定义, false 表示未定义
     */
    virtual bool hasRegister(BrandRegisterRole role) const = 0;

    /**
     * @brief 获取指定语义寄存器的品牌地址信息
     * @param[in] role 寄存器语义
     * @return 品牌寄存器描述, 未定义时返回无效地址描述
     */
    virtual BrandRegisterInfo registerInfo(BrandRegisterRole role) const = 0;

    /**
     * @brief 判断当前焊接类型下是否允许写入指定送丝机语义
     * @param[in] weldingType 焊接类型
     * @param[in] role 寄存器语义
     * @return true 表示允许写入, false 表示应屏蔽该报文
     */
    virtual bool allowWireFeederWrite(WeldingProcessType weldingType, BrandRegisterRole role) const = 0;

    /**
     * @brief 获取送丝机运行状态写入值
     * @param[in] weldingType 焊接类型
     * @return 品牌协议规定的启动或暂停值
     */
    virtual int wireFeederRunStatusValue(WeldingProcessType weldingType) const = 0;

    /**
     * @brief 获取工艺下发时需要写入的送丝机语义集合
     * @param[in] weldingType 焊接类型
     * @return 送丝机寄存器语义列表
     */
    virtual std::vector<BrandRegisterRole> craftWireFeederRoles(WeldingProcessType weldingType) const = 0;

    /**
     * @brief 生成单寄存器写入的Lua脚本调用行
     * @param[in] functionCode      功能码(如"0x0906")
     * @param[in] portAddress       寄存器地址
     * @param[in] paramValue        参数值
     * @param[in] portNum           通讯端口号
     * @param[in] firstRegisterNum  起始寄存器标识
     * @param[in] receiptNeed       是否等待回执("0"或"1")
     * @return write_value_port(...) Lua脚本行
     */
    virtual std::string writeLuaScript(const std::string& functionCode,
                                       const std::string& portAddress,
                                       const std::string& paramValue,
                                       const std::string& portNum,
                                       const std::string& firstRegisterNum,
                                       const std::string& receiptNeed) const;

    /**
     * @brief 生成工艺参数配置的Lua脚本体（品牌特有部分）
     * @param[in] machineResource 已计算好工艺参数的焊机资源
     * @param[in] craftInfo       工艺参数
     * @param[out] scriptBody     生成的Lua脚本体
     * @return 品牌特有的Lua脚本体字符串
     */
    virtual bool buildConfigureMachineBodyScript(
        const MachineResource& machineResource,
        const CraftInfoLaserSdk& craftInfo,
        std::string& scriptBody) const = 0;

    /**
     * @brief 判断是否需要通讯层额外生成送丝机配置脚本
     * @param[in] weldingType 焊接类型
     * @return true 表示通讯层需要调用 craft_wire_feeder_param_set_script_get 追加送丝机脚本;
     *         false 表示品牌适配器的 buildConfigureMachineBodyScript 已自行处理送丝机参数
     *
     * 【修改理由】不同品牌对送丝机参数的处理方式不同：
     *   - SUP(超强) 品牌在 buildConfigureMachineBodyScript 中仅生成激光器批量写脚本，
     *     送丝机参数需要通讯层通过 write_value_port 逐寄存器追加；
     *   - BCX(宝辰鑫) 品牌在 buildConfigureMachineBodyScript 中已通过 write_int16_register
     *     将送丝机参数一并写入，通讯层不应再重复生成。
     * 通过此虚方法将决策权归还品牌层，消除通讯类中的硬编码品牌判断。
     */
    virtual bool shouldGenerateWireFeederScript(WeldingProcessType weldingType) const = 0;

    /**
     * @brief 开始新的配置会话
     * 协议需要在新会话开始时清空上次成功参数, 避免跨会话残留影响.
     * 品牌协议适配器可在新会话开始时清理内部状态, 通讯层无需感知品牌内部状态.
     */
    virtual void BeginNewSession() = 0;

    /**
     * @brief 生成品牌特有的Lua初始化数据表脚本
     *
     * 【修改理由】initData() 中的 laserRegisterMap / wireRegisterMap / spotWeld /
     *   laserHeadRegisterMap 数据表属于超强品牌地址规则, 不应硬编码在通讯层.
     * 【逻辑】通讯层调用此方法获取品牌特有的Lua数据表定义, 通用Modbus工具函数仍由通讯层维护.
     * @return 品牌特有的Lua数据表定义脚本字符串
     */
    virtual std::string buildInitLuaFunc() const = 0;

    /** @brief true表示工艺参数通过机器人本地映射寄存器下发。 */
    virtual bool usesMappedSlaveRegisters() const { return false; }

    /** @brief true表示板卡自行完成缓降，SDK不再生成轨迹分段缓降。 */
    virtual bool usesBoardManagedRampDown() const { return false; }

    /** @brief true表示正式焊接的送丝启停由板卡数字输出管理。 */
    virtual bool controlsWireThroughDigitalOutput() const { return false; }
};

/**
 * @brief 超强伟业 SUP 协议适配器
 */
class SupProtocolAdapter : public BrandProtocolAdapter
{
private:
    // 上次已经收到从站成功响应的参数。
    ParamMap m_lastAppliedParameters;
public:
    LaserHeadBrandSdk brand() const override;
    BrandProtocolProfile profile() const override;
    LaserSdkResult checkCraft(const CraftInfoLaserSdk& craftInfo) const override;
    bool hasRegister(BrandRegisterRole role) const override;
    BrandRegisterInfo registerInfo(BrandRegisterRole role) const override;
    bool allowWireFeederWrite(WeldingProcessType weldingType, BrandRegisterRole role) const override;
    int wireFeederRunStatusValue(WeldingProcessType weldingType) const override;
    std::vector<BrandRegisterRole> craftWireFeederRoles(WeldingProcessType weldingType) const override;

    bool buildConfigureMachineBodyScript(
        const MachineResource& machineResource, 
        const CraftInfoLaserSdk& craftInfo, 
        std::string& scriptBody) const override;

    bool shouldGenerateWireFeederScript(WeldingProcessType weldingType) const override;

    void BeginNewSession() override;

    std::string buildInitLuaFunc() const override;
};

/**
 * @brief 宝辰鑫 X-Weld Omni Controller 1 协议适配器
 */
class BcxOmniProtocolAdapter : public BrandProtocolAdapter
{
//private:
//    // 上次已经收到从站成功响应的参数。
//    ParamMap m_lastAppliedParameters;
//    // 当前连接是否尚未完成首次同步。
//    bool m_firstSynchronization;
    
public:
    LaserHeadBrandSdk brand() const override;
    BrandProtocolProfile profile() const override;
    LaserSdkResult checkCraft(const CraftInfoLaserSdk& craftInfo) const override;
    bool hasRegister(BrandRegisterRole role) const override;
    BrandRegisterInfo registerInfo(BrandRegisterRole role) const override;
    bool allowWireFeederWrite(WeldingProcessType weldingType, BrandRegisterRole role) const override;
    int wireFeederRunStatusValue(WeldingProcessType weldingType) const override;
    std::vector<BrandRegisterRole> craftWireFeederRoles(WeldingProcessType weldingType) const override;
    bool buildConfigureMachineBodyScript(
        const MachineResource& machineResource,
        const CraftInfoLaserSdk& craftInfo, 
        std::string& scriptBody) const override;

    bool shouldGenerateWireFeederScript(WeldingProcessType weldingType) const override;

    // 新的配置会话开始时清空上次成功参数, 避免跨会话残留影响.
    void BeginNewSession() override;

    std::string buildInitLuaFunc() const override;
    bool usesMappedSlaveRegisters() const override { return true; }
    bool usesBoardManagedRampDown() const override { return true; }
    bool controlsWireThroughDigitalOutput() const override { return true; }
};

/**
 * @brief 超强伟业 SUP26AD 映射从站协议适配器
 *
 * 仅下发现有工艺使用的参数。送丝机设备参数由应用的设备页通过
 * 独立从站一次性写入，不进入工艺脚本。
 */
class Sup26AdProtocolAdapter : public BrandProtocolAdapter
{
public:
    LaserHeadBrandSdk brand() const override;
    BrandProtocolProfile profile() const override;
    LaserSdkResult checkCraft(const CraftInfoLaserSdk& craftInfo) const override;
    bool hasRegister(BrandRegisterRole role) const override;
    BrandRegisterInfo registerInfo(BrandRegisterRole role) const override;
    bool allowWireFeederWrite(WeldingProcessType weldingType, BrandRegisterRole role) const override;
    int wireFeederRunStatusValue(WeldingProcessType weldingType) const override;
    std::vector<BrandRegisterRole> craftWireFeederRoles(WeldingProcessType weldingType) const override;
    bool buildConfigureMachineBodyScript(
        const MachineResource& machineResource,
        const CraftInfoLaserSdk& craftInfo,
        std::string& scriptBody) const override;
    bool shouldGenerateWireFeederScript(WeldingProcessType weldingType) const override;
    void BeginNewSession() override;
    std::string buildInitLuaFunc() const override;
    bool usesMappedSlaveRegisters() const override { return true; }

    /**
     * @brief 缓降是否由板卡自行完成（与宝辰鑫一致）
     *
     * 【修改理由】SUP26AD 与 BCX 同属映射从站品牌：工艺参数与控制只能通过机器人本地寄存器下发，
     *   不接受 write_value_port 自由通讯报文。此前未覆盖本开关（默认 false），
     *   craftscript.cpp 的 appendRampDownControl 会按超强外控品牌生成 slow_arc_stop()：
     *   内部用 write_value_port(0x0906/0x0910, 超强地址, ...) 发报文，并用 task_create 创建后台任务。
     *   报文对映射从站板卡无效，而后台任务与 wait() 会让机器人程序一直停留在运行状态、无法停止。
     * 【逻辑】声明缓降由板卡按已写入的 RAMP_DOWN_TIME / LASER_OFF_POWER 寄存器自行完成，
     *   SDK 只在降功率点关闭外部使能 CO1（送丝焊且由数字输出管理送丝时同时关闭 CO2），
     *   不再生成任何报文脚本，与宝辰鑫内控缓降行为一致。
     * @return true 表示缓降由板卡内控
     */
    bool usesBoardManagedRampDown() const override { return true; }
};

/**
 * @brief 获取品牌协议适配器（品牌工厂函数）
 * @param[in] brand 激光头品牌枚举
 * @return 品牌协议适配器引用, 未知品牌返回默认超强适配器
 *
 * @legacy_source src/brand/brandadapter.cpp
 * @legacy_function GetBrandProtocolAdapter
 * @legacy_baseline 6fefa14
 * @compatibility 未知品牌回退到超强适配器的默认行为保持不变
 */
const BrandProtocolAdapter& GetBrandProtocolAdapter(LaserHeadBrandSdk brand);

#endif // BRANDADAPTER_H
