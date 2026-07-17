#ifndef LASERMODBUSRTUBUILDER_H
#define LASERMODBUSRTUBUILDER_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

/**
 * @brief 激光焊板卡写寄存器参数。
 *
 * 枚举值直接对应Modbus写寄存器地址。
 */
enum class LaserParam : uint16_t
{
    RatedPower              = 60, //0x1001, // 激光器额定功率
    CenterOffsetX           = 61, //0x1002, // 中心偏移X，100代表1 mm
    CenterOffsetY           = 62, //0x1003, // 中心偏移Y，100代表1 mm
    LaserPower              = 63, //0x1004, // 激光功率
    LaserFrequency          = 64, //0x1005, // 激光频率，单位Hz
    DutyCycle               = 65, //0x1006, // 占空比，0～100
    LaserOnPower            = 66, //0x1007, // 开光功率
    LaserOffPower           = 67, //0x1008, // 关光功率
    RampUpTime              = 68, //0x1009, // 缓升时间，单位ms
    RampDownTime            = 69, //0x100A, // 缓降时间，单位ms
    GasOnAdvanceTime        = 70, //0x100B, // 提前开气时间，单位ms
    GasOffDelayTime         = 71, //0x100C, // 延时关气时间，单位ms
    StepEquivalent          = 72, //0x100D, // 步进当量，单位P/mm
    RunningSpeed            = 73, //0x100E, // 运行速度，单位mm/s
    WireCompensationDelay   = 74, //0x100F, // 补丝延时，单位ms
    WireFeedRetractSpeed    = 75, //0x1010, // 抽补丝速度，单位mm/s
    WireRetractDistance     = 76, //0x1011, // 抽丝距离，单位mm
    WireFeedDistance        = 77, //0x1012, // 补丝距离，单位mm
    SingleStepDistance      = 78, //0x1013, // 单步距离，单位mm
    SwingAmplitude          = 79, //0x1014, // 摆动幅度，100代表1 mm
    SwingShape              = 80, //0x1015, // 摆动形状，1～5
    SwingFrequency          = 81, //0x1016, // 摆动频率，单位Hz
    ScaleFactorX            = 82, // 比例系数X，0～1000
    ScaleFactorY            = 83  // 比例系数Y，0～1000
};

/**
 * @brief 一条待发送的Modbus RTU写请求。
 */
struct ModbusWriteFrame
{
    // 使用的Modbus功能码：0x06或0x10。
    uint8_t functionCode = 0;
    // 报文写入的起始寄存器地址。
    uint16_t startAddress = 0;
    // 本条报文写入的寄存器数量。
    uint16_t registerCount = 0;
    /**
     * @brief 本条报文包含的业务参数。
     * 收到从站正确应答后，使用该列表更新上次成功下发缓存。
     */
    std::vector<LaserParam> parameters;
    /**
     * @brief 完整二进制Modbus RTU报文。  
     * 字符串中可能包含0x00，发送时必须使用：    
     * bytes.data()
     * bytes.size()*
     * 不能使用strlen()。
     */
    std::string bytes;
};

/**
 * @brief 报文构建结果。
 */
struct ModbusBuildResult
{
    bool success = false;
    std::string errorMessage;
    std::vector<ModbusWriteFrame> frames;
};

/**
 * @brief 激光焊板卡Modbus RTU报文构建器。
 */
class LaserModbusRtuBuilder
{
public:
    using ParamMap = std::map<LaserParam, uint16_t>;
    // 激光焊板卡默认Modbus从站地址。
    static constexpr uint8_t DefaultSlaveAddress = 0x5A;

    /**
     * @brief 根据当前参数与上次成功下发参数，生成变化参数报文。
     * 规则：
     * 1. 没有变化：不生成报文。
     * 2. 单个变化寄存器：使用0x06。
     * 3. 两个及以上连续变化寄存器：使用0x10。
     * 4. 不连续参数：拆分为多条报文。
     * @param current 当前参数快照。
     * @param lastApplied 上次收到正确应答的参数快照。
     * @param slaveAddress Modbus从站地址。
     */
    static ModbusBuildResult BuildChangedWriteFrames(
        const ParamMap &current, 
        const ParamMap &lastApplied, 
        uint8_t slaveAddress = DefaultSlaveAddress);

    /**
     * @brief 生成0x06写单个保持寄存器报文。
     */
    static std::string BuildWriteSingleRegister(
            uint8_t slaveAddress,
            uint16_t registerAddress,
            uint16_t value);

    /**
     * @brief 生成0x10连续写多个保持寄存器报文。
     * @param slaveAddress 从站地址。
     * @param startAddress 起始寄存器地址。
     * @param values 连续寄存器值。
     */
    static std::string BuildWriteMultipleRegisters(
        uint8_t slaveAddress, 
        uint16_t startAddress, 
        const std::vector<uint16_t> &values);

    /**
     * @brief 将二进制报文转换为十六进制字符串。
     *
     * 仅用于日志打印和调试。
     */
    static std::string ToHexString(const std::string &frame);

private:
    /**
     * @brief 变化寄存器内部描述。
     */
    struct ChangedRegister
    {
        LaserParam parameter;
        uint16_t address = 0;
        uint16_t value = 0;
    };

    /**
     * @brief 计算Modbus RTU CRC16。
     */
    static uint16_t CalculateCrc16(const std::string &data);

    /**
     * @brief 向二进制字符串追加一个字节。
     */
    static void AppendByte(std::string &frame,uint8_t value);

    /**
     * @brief 按高字节在前的顺序追加uint16_t。
     */
    static void AppendUint16BigEndian(std::string &frame,uint16_t value);

    /**
     * @brief 计算并追加CRC。
     *
     * Modbus RTU中CRC低字节在前，高字节在后。
     */
    static void AppendCrc(std::string &frame);

    /**
     * @brief 检查参数范围是否合法。
     */
    static bool ValidateParameters(const ParamMap &current,std::string &errorMessage);
};

#endif // LASERMODBUSRTUBUILDER_H