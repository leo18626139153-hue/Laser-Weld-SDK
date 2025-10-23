#include "cl_luascript.h"


uint16_t crc16Modbus(const std::vector<uint8_t> &data)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < data.size(); ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
            else              crc = (crc >> 1);
        }
    }
    return crc; // 返回的数值 低字节在前
}


std::vector<uint8_t> buildMessageVector(const std::map<uint16_t, uint16_t> &locVec, uint8_t slave, uint8_t funcCode)
{
    std::vector<uint8_t> message;
    putByte(message, slave);
    putByte(message, funcCode);
    auto processFunc = [&](uint16_t addr, uint16_t value) {
        putDoubleBytes(message, addr);
        putDoubleBytes(message, value);
    };
    switch (funcCode)
    {
    case 0x06: // 写单个寄存器
        // 检查locVec是否只有一个元素
        if (locVec.size() != 1) {
            throw std::runtime_error("Write single register function requires exactly one address-value pair");
        }
        for (const auto &pair : locVec) {
            processFunc(pair.first, pair.second);
        }
    case 0x10: // 写多个寄存器
        // 处理写多个寄存器的逻辑
        putDoubleBytes(message, locVec.begin()->first); // 写入起始地址
        putByte(message, static_cast<uint8_t>(locVec.size() * 2)); // 写入寄存器数量，每个寄存器占2个字节
        for (const auto &pair : locVec) {
            if (pair.first != locVec.begin()->first + (&pair - &(*locVec.begin())))
            {
                // 如果首个元素参数不连续，则报错
                throw std::runtime_error("Write multiple registers function requires continuous address range");
            }            
            putDoubleBytes(message, pair.second);
        }
        break;
    case 0x03: // 读多个寄存器
        // 处理读多个寄存器的逻辑
        putDoubleBytes(message, locVec.begin()->first); // 写入起始地址
        putDoubleBytes(message, static_cast<uint16_t>(locVec.size())); // 写入寄存器数量        
        break;    
    default:
        break;
    }
    
    // 计算CRC
    uint16_t crcValue = crc16Modbus(message);
    putDoubleBytes(message, crcValue);
    // 返回报文向量
    return message;
}

LaserSdkResult writeMessageToRegisters(LaserMachineHandle laserMachineHandle, const std::vector<uint8_t> &message, int registers, unsigned int firstRegister, unsigned int registerNums)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }

    // 检查寄存器范围
    if (firstRegister + registerNums > message.size()) {
        writeLog("writeMessageToRegisters: Register range out of bounds");
        return LaserSdkResult::OUT_OF_RANGE_CRSDK_LASER_SDK;
    }
    // 写入寄存器
    if (registers == 0)
    {
        /* bool */
        for (unsigned int i = 0; i < message.size(); i++)
        {
            BOOL tmpRegData = static_cast<BOOL>(message[firstRegister + i]);
            cr_set_boolRegValue(laserMachineHandle, firstRegister, i, &tmpRegData);
        }
    }
    else if (registers == 1)
    {
        /* int16 */
        for (unsigned int i = 0; i < message.size(); i++)
        {
            int value = message[firstRegister + i];
            cr_set_int16RegValue(laserMachineHandle, firstRegister, i, &value);
        }
    }
    else
    {
        writeLog("writeMessageToRegisters: Invalid register type");
        return LaserSdkResult::UNDEFINDED_ERROR_CRSDK_LASER_SDK;
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}


uint16_t modbusCRC16Table(uint8_t *nData, uint16_t wLength)
{
    static const uint16_t wCRCTable[] = {
    0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
    0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
    0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
    0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
    0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
    0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
    0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
    0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
    0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
    0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
    0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
    0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
    0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
    0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
    0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
    0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
    0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
    0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
    0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
    0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
    0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
    0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
    0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
    0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
    0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
    0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
    0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
    0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
    0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
    0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
    0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
    0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040 };
    uint8_t nTemp;
    uint16_t wCRCWord = 0xFFFF;
    uint16_t wCRCResult = 0;
    while (wLength--)
    {
    nTemp = *nData++ ^ wCRCWord;
    wCRCWord >>= 8;
    wCRCWord ^= wCRCTable[nTemp];
    }
    //将 CRC 高低字节交换
    wCRCResult = wCRCWord>>8;
    wCRCResult = wCRCResult | (wCRCWord << 8);
    return wCRCResult;
}
