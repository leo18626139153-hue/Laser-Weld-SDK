#ifndef BRANDADAPTER_H
#define BRANDADAPTER_H

#include "internaldatadefine.h"
#include "utils.h"
#include "lasermodbusrtubuilder.h"
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
 * @brief 旧兼容接口使用的品牌寄存器描述。
 *
 * 地址含义由原品牌协议定义；新参数流水线使用 Configuration 绑定本地映射槽，
 * 不将本结构中的设备远端地址直接写入机器人本地寄存器。
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
 * @brief 旧自由通讯接口保留的品牌协议参数。
 *
 * 新映射参数路径不以本结构配置串口或主从站；这些配置由上位机负责。
 */
struct BrandProtocolProfile
{
    unsigned int slaveAddr;           ///< Modbus 从站地址
    unsigned int readFuncCode;        ///< 读寄存器功能码
    unsigned int writeSingleFuncCode; ///< 写单寄存器功能码
    unsigned int writeMultiFuncCode;  ///< 写多寄存器功能码
    unsigned int laserWriteSingleScriptCode;      ///< 旧脚本复合码，高字节为从站地址、低字节为功能码
    unsigned int wireFeederWriteSingleScriptCode; ///< 送丝机旧脚本复合码，不是十六位 Modbus 功能码
    unsigned int writeMultiScriptCode;            ///< 批量写入旧脚本复合码
    int baudRate;                     ///< 串口波特率
    int dataBits;                     ///< 串口数据位
    int stopBits;                     ///< 串口停止位
};

/**
 * @brief 品牌协议适配器基类
 *
 * 保留旧品牌调用契约及型号能力策略；工艺参数生成统一委托设备参数流水线。
 * 参数语义、类型、缩放及地址由描述和绑定表达，串口及主从站配置不进入新参数模型。
 *
 * @legacy_source src/brand/brandadapter.cpp
 * @legacy_baseline 6fefa14
 * @compatibility 原自由通讯方法保留；新增映射能力和 SUP26AD 策略不改变旧品牌枚举值
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
     * @brief 生成旧自由通讯单寄存器写入调用行，不执行设备动作。
     * @param[in] functionCode      旧脚本复合码，如 "0x0906" 表示从站 0x09 和功能码 0x06
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
     * @brief 委托设备参数流水线生成工艺脚本体，不修改资源或控制数字输出。
     * @param[in] machineResource 设备型号及兼容地址配置，不依赖提前写入参数缓存。
     * @param[in] craftInfo       工艺参数
     * @param[out] scriptBody     生成的Lua脚本体
     * @return true 表示脚本生成成功；false 表示工艺或配置错误，输出保持原值。
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
     * 当前三种已接入型号的参数流水线都包含所需送丝参数，返回 false。
     * 后续兼容适配器若自行省略送丝参数，可通过本接口声明需要独立补充。
     */
    virtual bool shouldGenerateWireFeederScript(WeldingProcessType weldingType) const = 0;

    /**
     * @brief 保留原兼容接口的会话清理入口。
     * 旧适配器可清理历史缓存；新的参数计划使用不可变快照，不依赖跨会话缓存。
     * @return 无；不确认任何设备参数已下发成功。
     */
    virtual void BeginNewSession() = 0;

    /**
     * @brief 生成旧自由通讯需要的 Lua 工具函数及兼容初始化内容。
     *
     * 【修改理由】initData() 中的 laserRegisterMap / wireRegisterMap / spotWeld /
     *   laserHeadRegisterMap 数据表属于超强品牌地址规则, 不应硬编码在通讯层.
     * 新工艺写计划使用实时地址绑定，不依赖此处历史固定地址数据表。
     * 调用方仅在旧自由通讯路径加载这些工具，本地映射路径无需拼报文或 CRC。
     * @return 初始化脚本字符串；不需要工具函数的映射品牌可返回空字符串。
     */
    virtual std::string buildInitLuaFunc() const = 0;

    /**
     * @brief 判断该型号的默认参数路径是否使用机器人本地映射。
     * @return true 表示默认使用本地映射；句柄的显式配置由通讯上下文另行判断。
     */
    virtual bool usesMappedSlaveRegisters() const { return false; }

    /**
     * @brief 判断该型号是否由板卡执行缓降。
     * @return true 表示不生成旧自由通讯逐步降功率脚本，SDK 保留起收弧控制边界。
     */
    virtual bool usesBoardManagedRampDown() const { return false; }

    /**
     * @brief 判断正式焊接的送丝启停是否使用板卡数字输出。
     * @return true 表示起收弧流程按焊接类型控制送丝数字输出，不在参数配置中启停。
     */
    virtual bool controlsWireThroughDigitalOutput() const { return false; }
};

/**
 * @brief 超强伟业 SUP 协议适配器
 */
class SupProtocolAdapter : public BrandProtocolAdapter
{
private:
    ParamMap m_lastAppliedParameters; ///< 保留旧会话缓存；新参数编译不会更新此成员
public:
    LaserHeadBrandSdk brand() const override;
    BrandProtocolProfile profile() const override;
    LaserSdkResult checkCraft(const CraftInfoLaserSdk& craftInfo) const override;
    bool hasRegister(BrandRegisterRole role) const override;
    BrandRegisterInfo registerInfo(BrandRegisterRole role) const override;
    bool allowWireFeederWrite(WeldingProcessType weldingType, BrandRegisterRole role) const override;
    int wireFeederRunStatusValue(WeldingProcessType weldingType) const override;
    std::vector<BrandRegisterRole> craftWireFeederRoles(WeldingProcessType weldingType) const override;

    /**
     * @brief 以设备参数流水线生成旧超强及其兼容别名的参数脚本体。
     * @return true 表示成功；不控制数字输出，不预先修改资源参数。
     */
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
    /**
     * @brief 生成宝辰鑫默认本地映射参数脚本体，瓦特值与偏移按描述统一编码。
     * @return true 表示成功；失败时保留原输出。
     */
    bool buildConfigureMachineBodyScript(
        const MachineResource& machineResource,
        const CraftInfoLaserSdk& craftInfo, 
        std::string& scriptBody) const override;

    bool shouldGenerateWireFeederScript(WeldingProcessType weldingType) const override;

    // 保留会话入口；当前默认本地映射采用全量参数计划，无增量缓存需要清理。
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
    /**
     * @brief 生成 SUP26AD 默认映射工艺，保持百分比、扫描频率及原始回抽长度。
     * @return true 表示成功；不进入旧超强自由通讯路径。
     */
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
 * @return 原接口保留的适配器引用；未知枚举仍返回旧超强适配器。
 * @note 新参数流水线会在调用本工厂前拒绝未知品牌，不以此回退推测设备地址。
 *
 * @legacy_source src/brand/brandadapter.cpp
 * @legacy_function GetBrandProtocolAdapter
 * @legacy_baseline 6fefa14
 * @compatibility 未知品牌回退到超强适配器的默认行为保持不变
 */
const BrandProtocolAdapter& GetBrandProtocolAdapter(LaserHeadBrandSdk brand);

#endif // BRANDADAPTER_H
