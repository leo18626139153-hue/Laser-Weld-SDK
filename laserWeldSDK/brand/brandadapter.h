#ifndef BRANDADAPTER_H
#define BRANDADAPTER_H

#include "internaldatadefine.h"
#include "utils.h"
#include <vector>

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
    int baudRate;                     ///< 串口波特率
    int dataBits;                     ///< 串口数据位
    int stopBits;                     ///< 串口停止位
};

/**
 * @brief 品牌协议适配器基类
 *
 * 修改理由: 通讯类只负责 RS485/Modbus 脚本拼接, 品牌地址表和送丝机写入策略固定到品牌层.
 * 逻辑: 通讯层通过寄存器语义查询地址, 校验规则和送丝机写入策略, 不再直接判断具体品牌.
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
};

/**
 * @brief 超强伟业 SUP 协议适配器
 */
class SupProtocolAdapter : public BrandProtocolAdapter
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
};

/**
 * @brief 宝辰鑫 X-Weld Omni Controller 1 协议适配器
 */
class BcxOmniProtocolAdapter : public BrandProtocolAdapter
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
};

/**
 * @brief 获取品牌协议适配器
 * @param[in] brand 激光头品牌枚举
 * @return 品牌协议适配器引用, 未知品牌返回默认超强适配器
 */
const BrandProtocolAdapter& GetBrandProtocolAdapter(LaserHeadBrandSdk brand);

/**
 * @brief 超强伟业工艺参数检查旧接口
 * @param[in] craftInfoCheck 工艺参数
 * @return SDK 结果码
 *
 * 保留该接口用于兼容旧调用点, 未传入品牌时默认使用超强校验规则.
 */
LaserSdkResult GetCraftCheck(CraftInfoLaserSdk craftInfoCheck);

#endif // BRANDADAPTER_H
