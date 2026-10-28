#include "messageprase.h"

namespace {
constexpr uint16_t kModbusCrcInitialValue = 0xFFFF;
constexpr uint16_t kModbusCrcPolynomial = 0xA001;
constexpr int kBitsPerByte = 8;
constexpr int kBytesPerRegister = 2;
constexpr size_t kHexCharactersPerByte = 2;
constexpr int kHexadecimalRadix = 16;
constexpr uint8_t kWriteSingleRegisterFunction = 0x06;
constexpr uint8_t kReadHoldingRegistersFunction = 0x03;
constexpr int kBooleanRegisterType = 0;
constexpr int kInt16RegisterType = 1;
// 解析沿用现有接收布局，集中定义偏移以同步边界检查和数据读取。
constexpr size_t kMinimumRegisterResponseBytes = 6;
constexpr size_t kResponseDataStart = 4;
constexpr size_t kResponseByteCountIndex = 3;
constexpr uint16_t kSignedRegisterBoundary = 0x8000;
constexpr int kRegisterValueRange = 0x10000;
constexpr size_t kMinimumInt32ResponseHexLength = 14;
constexpr uint8_t kInt32ResponseByteCount = 0x07;
constexpr size_t kInt32ResponseDataStart = 6;
constexpr size_t kBytesPerInt32 = 4;
}

uint16_t crc16Modbus(const std::vector<uint8_t> &data) {
    uint16_t crc = kModbusCrcInitialValue;
    for (size_t i = 0; i < data.size(); ++i) {
        crc ^= data[i];
        for (int b = 0; b < kBitsPerByte; ++b) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ kModbusCrcPolynomial;
            } else {
                crc = (crc >> 1);
            }
        }
    }
    return ((crc << kBitsPerByte) | (crc >> kBitsPerByte)); // 报文写入器按高字节在前接收 CRC。
}

std::vector<std::vector<uint8_t>> buildMessageVector(const std::map<uint16_t, uint16_t> &locVec, uint8_t slave, uint8_t funcCode) {
    std::vector<std::vector<uint8_t>> messageVec;
    auto processFunc = [&](uint16_t addr, uint16_t value, std::vector<uint8_t> &message) {
        putDoubleBytes(message, addr);
        putDoubleBytes(message, value);
    };
    if (funcCode == kWriteSingleRegisterFunction) {
        // 写单个寄存器
        for (const auto &pair : locVec) {
            std::vector<uint8_t> calMessage;
            putByte(calMessage, slave);                       //从站号
            putByte(calMessage, funcCode);                    //功能码
            processFunc(pair.first, pair.second, calMessage); //地址；参数
            // 计算CRC
            uint16_t crcValue = crc16Modbus(calMessage);
            putDoubleBytes(calMessage, crcValue);
            messageVec.push_back(calMessage);
        }
    }
    return messageVec;
}

LaserSdkResult writeMessageToRegisters(LaserMachineHandle laserMachineHandle, const std::vector<uint8_t> &message, int registerType, unsigned int firstRegister,
    unsigned int registerNums) {
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end()) {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    // 输入容量以字节计数，必须按目标寄存器宽度换算后比较。
    const size_t bytesPerRegister = (registerType == kInt16RegisterType) ? kBytesPerRegister : 1;
    if (static_cast<size_t>(registerNums) * bytesPerRegister > message.size()) {
        writeLog("writeMessageToRegisters: Register range out of bounds");
        return LaserSdkResult::OUT_OF_RANGE_CRSDK_LASER_SDK;
    }
    if (registerType == kBooleanRegisterType) {
        /* bool：展开为独立BOOL数组，避免把vector对象地址强转为BOOL* */
        std::vector<BOOL> boolValues(message.begin(), message.begin() + registerNums);
        cr_set_boolRegValue(laserMachineHandle, firstRegister, registerNums, boolValues.data());
    } else if (registerType == kInt16RegisterType) {
        for (unsigned int i = 0; i < registerNums; i++) {
            /* 逐字节组装16位值，避免unsigned short*强转造成的非对齐访问 */
            int tmpValue = static_cast<int>(message[i * kBytesPerRegister]) | (static_cast<int>(message[i * kBytesPerRegister + 1]) << kBitsPerByte);
            cr_set_int16RegValue(machineResource.robotHandle, firstRegister + i, 1, &tmpValue);
        }
    } else {
        writeLog("writeMessageToRegisters: Invalid register type");
        return LaserSdkResult::UNDEFINDED_ERROR_CRSDK_LASER_SDK;
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

uint16_t modbusCRC16Table(uint8_t *nData, uint16_t wLength) {
    static const uint16_t wCRCTable[] = {0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241, 0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481,
        0X0440, 0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40, 0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841, 0XD801, 0X18C0,
        0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40, 0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41, 0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701,
        0X17C0, 0X1680, 0XD641, 0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040, 0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
        0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441, 0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41, 0XFA01, 0X3AC0, 0X3B80,
        0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840, 0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41, 0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1,
        0XEC81, 0X2C40, 0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640, 0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041, 0XA001,
        0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240, 0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441, 0X6C00, 0XACC1, 0XAD81, 0X6D40,
        0XAF01, 0X6FC0, 0X6E80, 0XAE41, 0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840, 0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80,
        0XBA41, 0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40, 0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640, 0X7200, 0XB2C1,
        0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041, 0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241, 0X9601, 0X56C0, 0X5780, 0X9741, 0X5500,
        0X95C1, 0X9481, 0X5440, 0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40, 0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
        0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40, 0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41, 0X4400, 0X84C1, 0X8581,
        0X4540, 0X8701, 0X47C0, 0X4680, 0X8641, 0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040};
    uint8_t nTemp;
    uint16_t wCRCWord = kModbusCrcInitialValue;
    uint16_t wCRCResult = 0;
    while (wLength--) {
        nTemp = *nData++ ^ wCRCWord;
        wCRCWord >>= kBitsPerByte;
        wCRCWord ^= wCRCTable[nTemp];
    }
    //将 CRC 高低字节交换
    wCRCResult = wCRCWord >> kBitsPerByte;
    wCRCResult = wCRCResult | (wCRCWord << kBitsPerByte);
    return wCRCResult;
}

uint16_t modbusCRC16(const std::vector<uint8_t> &data) {
    uint16_t crc = kModbusCrcInitialValue;
    for (size_t i = 0; i < data.size(); ++i) {
        crc ^= data[i];
        for (int b = 0; b < kBitsPerByte; ++b) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ kModbusCrcPolynomial;
            } else {
                crc = (crc >> 1);
            }
        }
    }

    return crc;
}

int16_t *parse_modbus_register(const std::string &hexResponse, const int regIndex, const int regLen) {
    // ---- 第一步：参数校验 ----
    if (hexResponse.empty() || hexResponse.size() % kHexCharactersPerByte != 0) {
        throw LaserSdkResult::CALCULATION_RESULT_ILLOGICAL_SDK;
    }
    if (regIndex < 1) {
        throw LaserSdkResult::CALCULATION_RESULT_ILLOGICAL_SDK;
    }
    // ---- 第二步：十六进制转字节 ----
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hexResponse.size(); i += kHexCharactersPerByte) {
        try {
            bytes.push_back(static_cast<uint8_t>(std::stoi(hexResponse.substr(i, kHexCharactersPerByte), nullptr, kHexadecimalRadix)));
        } catch (...) {
            throw LaserSdkResult::CALCULATION_RESULT_ILLOGICAL_SDK;
        }
    }
    // ---- 第三步：校验最小长度 ----
    if (bytes.size() < kMinimumRegisterResponseBytes) {
        throw LaserSdkResult::CALCULATION_RESULT_ILLOGICAL_SDK;
    }
    // ---- 第四步：高低位交换（整个报文）----
    std::vector<uint8_t> swappedBytes;
    for (size_t i = 0; i < bytes.size() - 1; i += kBytesPerRegister) { // 奇数字节长度时保留末字节。
        swappedBytes.push_back(bytes[i + 1]);
        swappedBytes.push_back(bytes[i]);
    }
    if (bytes.size() % kBytesPerRegister != 0) {
        swappedBytes.push_back(bytes.back());
    }
    // 交换后的接收封装从固定数据偏移提取寄存器，不能重新交换头部解释顺序。
    size_t dataStart = kResponseDataStart;
    if (dataStart >= swappedBytes.size()) {
        throw LaserSdkResult::CALCULATION_RESULT_ILLOGICAL_SDK;
    }
    uint8_t dataByteCount = swappedBytes[kResponseByteCountIndex];
    int num = dataByteCount / kBytesPerRegister;
    // ---- 第六步：提取目标寄存器值 ----
    if (regLen < 1 || regLen > num) {
        writeLog("parse_modbus_register: register index out of range", LOG_ERROR_LASER_SDK);
        throw LaserSdkResult::CALCULATION_RESULT_ILLOGICAL_SDK;
    }
    if (regIndex < 1 || regIndex > num) {
        writeLog("parse_modbus_register: register index out of range", LOG_ERROR_LASER_SDK);
        throw LaserSdkResult::CALCULATION_RESULT_ILLOGICAL_SDK;
    }

    int16_t *values = new int16_t[static_cast<size_t>(regLen)];

    for (int i = 0; i < regLen; i++) {
        size_t pos = dataStart + (regIndex - 1 + i) * kBytesPerRegister;
        if (pos + 1 >= swappedBytes.size()) {
            // 发生错误时释放已分配的内存
            delete[] values;
            throw LaserSdkResult::CALCULATION_RESULT_ILLOGICAL_SDK;
        }
        uint16_t rawValue = (swappedBytes[pos] << kBitsPerByte) | swappedBytes[pos + 1];
        values[i] = static_cast<int16_t>(rawValue >= kSignedRegisterBoundary ? rawValue - kRegisterValueRange : rawValue);
    }
    return values;
}

int32_t parse_modbus_register32(const std::string &hexResponse) {
    // ---- 第一步：参数校验 ----
    if (hexResponse.empty() || hexResponse.size() % kHexCharactersPerByte != 0) {
        throw LaserSdkResult::CALCULATION_RESULT_ILLOGICAL_SDK;
    }

    if (hexResponse.size() < kMinimumInt32ResponseHexLength) {
        throw LaserSdkResult::CALCULATION_RESULT_ILLOGICAL_SDK;
    }

    // ---- 第二步：十六进制转字节 ----
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hexResponse.size(); i += kHexCharactersPerByte) {
        try {
            uint8_t byte = static_cast<uint8_t>(std::stoi(hexResponse.substr(i, kHexCharactersPerByte), nullptr, kHexadecimalRadix));
            bytes.push_back(byte);
        } catch (...) {
            throw LaserSdkResult::CALCULATION_RESULT_ILLOGICAL_SDK;
        }
    }

    // ---- 第三步：解析报文结构 ----
    // 报文格式：02 03 09 07 85 59 00 00
    // 02 - 数据长度(2N), 03 - 功能码, 09 - 地址, 07 - 返回字节总数, 85 59 - CRC, 00 00 - 数据

    // 检查功能码
    if (bytes[1] != kReadHoldingRegistersFunction) {
        throw LaserSdkResult::CALCULATION_RESULT_ILLOGICAL_SDK;
    }

    // 检查字节计数
    uint8_t byte_count = bytes[kResponseByteCountIndex];
    if (byte_count != kInt32ResponseByteCount) {
        throw LaserSdkResult::CALCULATION_RESULT_ILLOGICAL_SDK;
    }

    // ---- 第四步：提取数据部分 ----
    // 数据从第6个字节开始（跳过：长度1+功能码1+地址1+字节数1+CRC2）
    size_t data_start_index = kInt32ResponseDataStart;

    if (data_start_index + kBytesPerInt32 > bytes.size()) {
        throw LaserSdkResult::CALCULATION_RESULT_ILLOGICAL_SDK;
    }

    // 接收封装中的数据按低字节在前组装，避免套用线上 RTU 字节序。
    int32_t result = 0;

    result = (static_cast<int32_t>(bytes[data_start_index + 3]) << (kBitsPerByte * 3)) |
        (static_cast<int32_t>(bytes[data_start_index + 2]) << (kBitsPerByte * 2)) | (static_cast<int32_t>(bytes[data_start_index + 1]) << kBitsPerByte) |
        (static_cast<int32_t>(bytes[data_start_index]));

    return result;
}
