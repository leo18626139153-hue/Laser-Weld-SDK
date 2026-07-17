#include "lasermodbusrtubuilder.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

/*
 * C++11下，如果静态constexpr成员发生ODR使用，
 * 需要在类外提供定义。
 */
constexpr uint8_t LaserModbusRtuBuilder::DefaultSlaveAddress;

/**
 * @brief 向二进制报文末尾追加一个字节。
 */
void LaserModbusRtuBuilder::AppendByte(
        std::string &frame,
        uint8_t value)
{
    frame.push_back(static_cast<char>(value));
}

/**
 * @brief 按Modbus数据字段的高字节在前顺序追加uint16_t。
 */
void LaserModbusRtuBuilder::AppendUint16BigEndian(
        std::string &frame,
        uint16_t value)
{
    AppendByte(
                frame,
                static_cast<uint8_t>((value >> 8) & 0xFF));

    AppendByte(
                frame,
                static_cast<uint8_t>(value & 0xFF));
}

/**
 * @brief 计算Modbus RTU CRC16。
 */
uint16_t LaserModbusRtuBuilder::CalculateCrc16(
        const std::string &data)
{
    uint16_t crc = 0xFFFF;

    for (std::size_t index = 0;
         index < data.size();
         ++index)
    {
        const uint8_t currentByte =
                static_cast<uint8_t>(
                    static_cast<unsigned char>(data[index]));

        crc ^= currentByte;

        for (int bitIndex = 0;
             bitIndex < 8;
             ++bitIndex)
        {
            if ((crc & 0x0001) != 0)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

/**
 * @brief 计算并追加CRC。
 *
 * Modbus RTU规定：
 * CRC低字节先发送，CRC高字节后发送。
 */
void LaserModbusRtuBuilder::AppendCrc(
        std::string &frame)
{
    const uint16_t crc = CalculateCrc16(frame);

    AppendByte(
                frame,
                static_cast<uint8_t>(crc & 0xFF));

    AppendByte(
                frame,
                static_cast<uint8_t>((crc >> 8) & 0xFF));
}

/**
 * @brief 构建0x06写单个保持寄存器报文。
 */
std::string LaserModbusRtuBuilder::BuildWriteSingleRegister(
        uint8_t slaveAddress,
        uint16_t registerAddress,
        uint16_t value)
{
    std::string frame;
    frame.reserve(8);

    // 从站地址
    AppendByte(frame, slaveAddress);

    // 功能码：写单个保持寄存器
    AppendByte(frame, 0x06);

    // 寄存器地址
    AppendUint16BigEndian(
                frame,
                registerAddress);

    // 寄存器值
    AppendUint16BigEndian(
                frame,
                value);

    // CRC低字节、CRC高字节
    AppendCrc(frame);

    return frame;
}

/**
 * @brief 构建0x10连续写多个保持寄存器报文。
 */
std::string LaserModbusRtuBuilder::BuildWriteMultipleRegisters(
        uint8_t slaveAddress,
        uint16_t startAddress,
        const std::vector<uint16_t> &values)
{
    std::string frame;

    if (values.empty())
    {
        return frame;
    }

    /*
     * 标准Modbus功能码0x10单次最多写123个寄存器。
     * 当前激光焊参数只有24个，不会超过该限制。
     */
    if (values.size() > 123)
    {
        return frame;
    }

    const uint16_t registerCount =
            static_cast<uint16_t>(values.size());

    const uint8_t byteCount =
            static_cast<uint8_t>(registerCount * 2);

    /*
     * 报文长度：
     *
     * 从站地址       1字节
     * 功能码         1字节
     * 起始地址       2字节
     * 寄存器数量     2字节
     * 数据字节数     1字节
     * 数据           2×N字节
     * CRC            2字节
     *
     * 总长度：9 + 2×N
     */
    frame.reserve(
                static_cast<std::size_t>(
                    9 + byteCount));

    // 从站地址
    AppendByte(frame, slaveAddress);

    // 功能码：连续写多个保持寄存器
    AppendByte(frame, 0x10);

    // 起始寄存器地址
    AppendUint16BigEndian(
                frame,
                startAddress);

    // 寄存器数量
    AppendUint16BigEndian(
                frame,
                registerCount);

    // 后续数据字节数
    AppendByte(
                frame,
                byteCount);

    // 依次追加寄存器数据
    for (std::size_t index = 0;
         index < values.size();
         ++index)
    {
        AppendUint16BigEndian(
                    frame,
                    values[index]);
    }

    // CRC低字节、CRC高字节
    AppendCrc(frame);

    return frame;
}

/**
 * @brief 校验激光焊参数范围。
 */
bool LaserModbusRtuBuilder::ValidateParameters(
        const ParamMap &current,
        std::string &errorMessage)
{
    /*
     * 检查激光器额定功率。
     */
    const ParamMap::const_iterator ratedPowerIterator =
            current.find(LaserParam::RatedPower);

    if (ratedPowerIterator != current.end())
    {
        const uint16_t ratedPower =
                ratedPowerIterator->second;

        const LaserParam powerParameters[] =
        {
            LaserParam::LaserPower,
            LaserParam::LaserOnPower,
            LaserParam::LaserOffPower
        };

        const std::size_t powerParameterCount =
                sizeof(powerParameters) /
                sizeof(powerParameters[0]);

        for (std::size_t index = 0;
             index < powerParameterCount;
             ++index)
        {
            const ParamMap::const_iterator iterator =
                    current.find(powerParameters[index]);

            if (iterator == current.end())
            {
                continue;
            }

            if (iterator->second > ratedPower)
            {
                errorMessage =
                        "Laser power parameter exceeds rated power.";

                return false;
            }
        }
    }

    /*
     * 占空比范围：0～100。
     */
    const ParamMap::const_iterator dutyCycleIterator =
            current.find(LaserParam::DutyCycle);

    if (dutyCycleIterator != current.end() &&
        dutyCycleIterator->second > 100)
    {
        errorMessage =
                "Duty cycle must be in range 0 to 100.";

        return false;
    }

    /*
     * 摆动形状范围：1～5。
     */
    const ParamMap::const_iterator swingShapeIterator =
            current.find(LaserParam::SwingShape);

    if (swingShapeIterator != current.end())
    {
        const uint16_t swingShape =
                swingShapeIterator->second;

        if (swingShape < 1 ||
            swingShape > 5)
        {
            errorMessage = "Swing shape must be in range 1 to 5.";
            return false;
        }
    }

    /*
     * X比例系数范围：0～1000。
     */
    const ParamMap::const_iterator scaleXIterator = current.find(LaserParam::ScaleFactorX);

    if (scaleXIterator != current.end() && scaleXIterator->second > 1000)
    {
        errorMessage =
                "Scale factor X must be in range 0 to 1000.";

        return false;
    }

    /*
     * Y比例系数范围：0～1000。
     */
    const ParamMap::const_iterator scaleYIterator =
            current.find(LaserParam::ScaleFactorY);

    if (scaleYIterator != current.end() &&
        scaleYIterator->second > 1000)
    {
        errorMessage =
                "Scale factor Y must be in range 0 to 1000.";

        return false;
    }

    return true;
}

/**
 * @brief 根据参数变化生成Modbus RTU写报文。
 */
ModbusBuildResult LaserModbusRtuBuilder::BuildChangedWriteFrames(
    const ParamMap &current, const ParamMap &lastApplied, uint8_t slaveAddress)
{
    ModbusBuildResult result;

    /*
     * Modbus从站地址合法范围为1～247。
     * 0用于广播地址，当前接口不生成广播写报文。
     */
    if (slaveAddress == 0 || slaveAddress > 247)
    {
        result.errorMessage = "Modbus slave address must be in range 1 to 247.";
        return result;
    }
    if (!ValidateParameters(current, result.errorMessage))
    {
        return result;
    }
    std::vector<ChangedRegister> changedRegisters;
    changedRegisters.reserve(current.size());
    /*
     * 只提取以下参数：
     * 1. 从未成功下发过的参数；
     * 2. 当前值与上次成功下发值不同的参数。
     */
    for (ParamMap::const_iterator currentIterator = current.begin();
         currentIterator != current.end();
         ++currentIterator)
    {
        const LaserParam parameter = currentIterator->first;
        const uint16_t currentValue = currentIterator->second;
        const ParamMap::const_iterator appliedIterator = lastApplied.find(parameter);
        const bool neverApplied = appliedIterator == lastApplied.end();
        const bool valueChanged = !neverApplied && appliedIterator->second != currentValue;
        if (!neverApplied && !valueChanged)
        {
            continue;
        }
        ChangedRegister changedRegister;
        changedRegister.parameter = parameter;
        changedRegister.address = static_cast<uint16_t>(parameter);
        changedRegister.value = currentValue;

        changedRegisters.push_back(changedRegister);
    }

    /*
     * 没有参数变化，不需要发送报文。
     */
    if (changedRegisters.empty())
    {
        result.success = true;
        return result;
    }

    /*
     * 按寄存器地址从小到大排序。
     * 这里显式排序可以保证后续更换容器时逻辑仍然正确。
     */
    std::sort(changedRegisters.begin(),changedRegisters.end(), 
    [](const ChangedRegister &left,const ChangedRegister &right){
        return left.address < right.address;
    });

    std::size_t groupBegin = 0;

    while (groupBegin < changedRegisters.size())
    {
        std::size_t groupEnd = groupBegin;
        /*
         * 查找从groupBegin开始的连续寄存器组。
         * 例如：0x1004、0x1005、0x1006会被合并为一条0x10报文。
         */
        while (groupEnd + 1 < changedRegisters.size())
        {
            // 当前寄存器地址
            const uint16_t currentAddress = changedRegisters[groupEnd].address;
            // 下一个寄存器地址
            const uint16_t nextAddress = changedRegisters[groupEnd + 1].address;
            if (currentAddress == 0xFFFF)
            {
                break;
            }
            if (nextAddress != static_cast<uint16_t>(currentAddress + 1))
            {
                break;
            }
            ++groupEnd;
        }

        const std::size_t groupCount = groupEnd - groupBegin + 1;

        ModbusWriteFrame writeFrame;

        /*
         * 只有一个变化寄存器：
         * 使用功能码0x06。
         */
        if (groupCount == 1)
        {
            const ChangedRegister &changedRegister = changedRegisters[groupBegin];

            writeFrame.functionCode = 0x06;
            writeFrame.startAddress =  changedRegister.address;
            writeFrame.registerCount = 1;
            writeFrame.parameters.push_back(changedRegister.parameter);

            writeFrame.bytes = BuildWriteSingleRegister(slaveAddress, changedRegister.address, changedRegister.value);
        }
        /*
         * 两个及以上连续变化寄存器：
         * 使用功能码0x10。
         */
        else
        {
            std::vector<uint16_t> values;
            values.reserve(groupCount);

            writeFrame.functionCode = 0x10;
            writeFrame.startAddress =
                    changedRegisters[groupBegin].address;
            writeFrame.registerCount =
                    static_cast<uint16_t>(groupCount);

            for (std::size_t index = groupBegin;
                 index <= groupEnd;
                 ++index)
            {
                values.push_back(
                            changedRegisters[index].value);

                writeFrame.parameters.push_back(
                            changedRegisters[index].parameter);
            }

            writeFrame.bytes =
                    BuildWriteMultipleRegisters(
                        slaveAddress,
                        writeFrame.startAddress,
                        values);
        }

        /*
         * 报文为空说明构建失败。
         */
        if (writeFrame.bytes.empty())
        {
            result.errorMessage =
                    "Failed to build Modbus RTU frame.";

            result.frames.clear();
            return result;
        }

        result.frames.push_back(
                    writeFrame);

        groupBegin = groupEnd + 1;
    }

    result.success = true;
    return result;
}

/**
 * @brief 将二进制报文转换成便于打印的十六进制文本。
 */
std::string LaserModbusRtuBuilder::ToHexString(
        const std::string &frame)
{
    std::ostringstream stream;

    stream << std::uppercase
           << std::hex
           << std::setfill('0');

    for (std::size_t index = 0;
         index < frame.size();
         ++index)
    {
        const unsigned int currentByte =
                static_cast<unsigned int>(
                    static_cast<unsigned char>(
                        frame[index]));

        if (index != 0)
        {
            stream << ' ';
        }

        stream << std::setw(2)
               << currentByte;
    }

    return stream.str();
}