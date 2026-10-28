#include "utils.h"
#include <sstream>
#include <iomanip>
#include <climits>
#include <cstdio>
#include <ctime>

#include "messageprase.h"

namespace {
constexpr int kDecimalRadix = 10;
constexpr unsigned int kHexadecimalRadix = 16;
constexpr int kHexDigitsPerByte = 2;
constexpr int kHexDigitsPerRegister = 4;
constexpr int kBitsPerByte = 8;
constexpr uint8_t kWriteSingleRegisterFunction = 0x06;
constexpr size_t kSingleRegisterFrameBytes = 8;
}

/**
 * @brief 单次扫描精确替换%N占位符，按最长数字匹配，避免%1误替换%10等子串问题
 * @param[in] srcStr 源字符串
 * @param[in] values 占位符对应的值（字符串形式，索引0对应%1）
 * @return 替换后的字符串
 */
static std::string replacePlaceholdersOnce(const std::string &srcStr, const std::vector<std::string> &values) {
    std::string result;
    result.reserve(srcStr.size());
    size_t i = 0;
    while (i < srcStr.size()) {
        if (srcStr[i] == '%' && i + 1 < srcStr.size() && srcStr[i + 1] >= '0' && srcStr[i + 1] <= '9') {
            size_t numStart = i + 1;
            size_t numEnd = numStart;
            while (numEnd < srcStr.size() && srcStr[numEnd] >= '0' && srcStr[numEnd] <= '9') {
                numEnd++;
            }
            int index = 0;
            bool overflow = false;
            for (size_t k = numStart; k < numEnd; k++) {
                int digit = srcStr[k] - '0';
                if (index > (INT_MAX - digit) / kDecimalRadix) {
                    overflow = true;
                    break;
                }
                index = index * kDecimalRadix + digit;
            }
            if (!overflow && index >= 1 && index <= static_cast<int>(values.size())) {
                result += values[index - 1];
                i = numEnd;
                continue;
            }
        }
        result += srcStr[i];
        i++;
    }
    return result;
}
std::string strReplace(std::string srcStr, std::array<double, 6> vecDouble) {
    std::vector<std::string> values;
    for (size_t i = 0; i < vecDouble.size(); i++) {
        values.push_back(std::to_string(vecDouble[i]));
    }
    return replacePlaceholdersOnce(srcStr, values);
}

std::string strReplace(std::string srcStr, std::vector<double> vecDouble) {
    std::vector<std::string> values;
    for (size_t i = 0; i < vecDouble.size(); i++) {
        values.push_back(std::to_string(vecDouble[i]));
    }
    return replacePlaceholdersOnce(srcStr, values);
}

std::string strReplace(std::string srcStr, std::vector<int> vecInt) {
    std::vector<std::string> values;
    for (size_t i = 0; i < vecInt.size(); i++) {
        values.push_back(std::to_string(vecInt[i]));
    }
    return replacePlaceholdersOnce(srcStr, values);
}

std::string strReplace(std::string srcStr, std::string format, std::string replaceStr) {
    while (true) {
        size_t pos = srcStr.find(format);
        if (pos != std::string::npos) {
            srcStr.replace(pos, format.length(), replaceStr);
        } else {
            break;
        }
    }
    return srcStr;
}

std::string strReplace(std::string srcStr, std::string format, int replaceInt) {
    std::string replaceStr = std::to_string(replaceInt);
    while (true) {
        size_t pos = srcStr.find(format);
        if (pos != std::string::npos) {
            srcStr.replace(pos, format.length(), replaceStr);
        } else {
            break;
        }
    }
    return srcStr;
}

std::string strReplace(std::string srcStr, std::string format, unsigned int replaceUInt) {
    std::string replaceStr = std::to_string(replaceUInt);
    while (true) {
        size_t pos = srcStr.find(format);
        if (pos != std::string::npos) {
            srcStr.replace(pos, format.length(), replaceStr);
        } else {
            break;
        }
    }
    return srcStr;
}

std::string strReplace(std::string srcStr, std::string format, double replaceDouble) {
    std::string replaceStr = std::to_string(replaceDouble);
    while (true) {
        size_t pos = srcStr.find(format);
        if (pos != std::string::npos) {
            srcStr.replace(pos, format.length(), replaceStr);
        } else {
            break;
        }
    }
    return srcStr;
}

std::string strReplace(std::string srcStr, std::string format, float replaceFloat) {
    std::string replaceStr = std::to_string(replaceFloat);
    while (true) {
        size_t pos = srcStr.find(format);
        if (pos != std::string::npos) {
            srcStr.replace(pos, format.length(), replaceStr);
        } else {
            break;
        }
    }
    return srcStr;
}

std::string strReplace(std::string srcStr, std::vector<std::string> vecStr) {
    return replacePlaceholdersOnce(srcStr, vecStr);
}

std::string point2str(std::vector<double> point) {
    std::string pointStr = "{";
    for (int i = 0; i < point.size(); i++) {
        pointStr += std::to_string(point[i]);
        if (i != (point.size() - 1)) {
            pointStr += ",";
        } else {
            pointStr += "}";
        }
    }
    return pointStr;
}

std::string point2str(std::array<double, 6> point) {
    std::string pointStr = "{";
    for (int i = 0; i < point.size(); i++) {
        pointStr += std::to_string(point[i]);
        if (i != (point.size() - 1)) {
            pointStr += ",";
        } else {
            pointStr += "}";
        }
    }
    return pointStr;
}

std::string Vec2Str(double *vec, int vecLen) {
    std::string pointStr = "{";
    for (int i = 0; i < vecLen; i++) {
        pointStr += std::to_string(vec[i]);
        if (i != (vecLen - 1)) {
            pointStr += ",";
        } else {
            pointStr += "}";
        }
    }
    return pointStr;
}

std::string pointVec2str(std::vector<std::vector<double>> pointVec) {
    std::string pointVecStr = "{";
    for (int i = 0; i < pointVec.size(); i++) {
        std::string pointStr = point2str(pointVec[i]);
        pointVecStr += pointStr;
        if (i != (pointVec.size() - 1)) {
            pointVecStr += ",";
        } else {
            pointVecStr += "}";
        }
    }
    return pointVecStr;
}

std::string pointVec2str(std::vector<std::array<double, 6>> pointVec) {
    std::string pointVecStr = "{";
    for (int i = 0; i < pointVec.size(); i++) {
        std::string pointStr = point2str(pointVec[i]);
        pointVecStr += pointStr;
        if (i != (pointVec.size() - 1)) {
            pointVecStr += ",";
        } else {
            pointVecStr += "}";
        }
    }
    return pointVecStr;
}

std::string PointVec2Str(std::vector<std::vector<double>> pointVec, std::string strName, int segLen) {
    std::string pointVecStr;
    int count = 0, segCount = 1;
    if (segLen > 0) {
        pointVecStr = strName + std::to_string(segCount) + "={";
    } else {
        pointVecStr = strName + "={";
    }

    for (int i = 0; i < pointVec.size(); i++) {
        pointVecStr += "{";
        for (int j = 0; j < pointVec[i].size(); j++) {
            pointVecStr += Double2String(pointVec[i][j]);
            if (j != pointVec[i].size() - 1) {
                pointVecStr += ",";
            } else {
                pointVecStr += "}";
            }
        }

        count++;
        if (segLen > 0 && count == segLen) {
            count = 0;
            segCount++;
            if (i != (pointVec.size() - 1)) {
                pointVecStr += "}\n" + strName + std::to_string(segCount) + "={";
            } else {
                pointVecStr += "}\n";
            }
        } else {
            if (i != (pointVec.size() - 1)) {
                pointVecStr += ",";
            } else {
                pointVecStr += "}\n";
            }
        }
    }
    return pointVecStr;
}

std::string Double2String(double value, int precision, bool scientific) {
    std::ostringstream oss;

    // 设置输出格式
    if (scientific) // 科学计数法
    {
        oss << std::scientific << std::setprecision(precision);
    } else {
        // 对于非科学计数法，需要特殊处理以避免末尾多余的0
        oss << std::fixed;

        // 先尝试用足够大的精度输出
        oss << std::setprecision(precision + 2);
        std::string temp = oss.str();

        // 然后手动调整精度
        oss << std::setprecision(precision);
    }

    // 输出值
    oss << value;

    std::string result = oss.str();

    // 对于非科学计数法，移除末尾无意义的0和小数点
    if (!scientific) {
        size_t dot_pos = result.find('.');
        if (dot_pos != std::string::npos) {
            // 从末尾开始移除0
            result.erase(result.find_last_not_of('0') + 1);
            // 如果小数点后没有数字了，移除小数点
            if (result.back() == '.') {
                result.pop_back();
            }
        }
    }

    return result;
}

bool isEqual(double a, double b) {
    // 计算两个浮点数的差值的绝对值
    return std::fabs(a - b) < EPSILON;
}

std::string PdoReadWriteBit(unsigned addr, int wireBitLoc, int writeValue) {
    //焊丝回抽
    std::string returnVal = "returnVal";
    std::string scriptText;
    //读手动测试地址中的数据
    scriptText += strReadPdoReplace(returnVal, addr);

    //给某个数的指定位（从1开始计数）写值  setLocBit函数在之前已经定义
    scriptText += strReplace("%1=setLocBit(%1, %2, %3)\n", std::vector<std::string>{returnVal, std::to_string(wireBitLoc - 1), std::to_string(writeValue)});
    //重新写入值
    scriptText += strWritePdoReplace(addr, returnVal);
    return scriptText;
}

std::string Int16ReadWriteBit(unsigned addr, int wireBitLoc, int writeValue) {
    //焊丝回抽
    std::string returnVal = "returnVal";
    std::string scriptText;
    //读手动测试地址中的数据
    scriptText += strReadInt16RegReplace(returnVal, addr);
    //给某个数的指定位（从1开始计数）写值  setLocBit函数在之前已经定义
    scriptText += strReplace("%1=setLocBit(%1, %2, %3)\n", std::vector<std::string>{returnVal, std::to_string(wireBitLoc - 1), std::to_string(writeValue)});
    //重新写入值
    scriptText += strWriteInt16RegReplace(addr, returnVal);
    return scriptText;
}

std::string doubleArray2Str(const double *array, int size) {
    std::string pointStr = "{";
    for (int i = 0; i < size; i++) {
        pointStr += std::to_string(array[i]);
        if (i != (size - 1)) {
            pointStr += ",";
        } else {
            pointStr += "}";
        }
    }
    return pointStr;
}

std::string decimalToHexStringStream(unsigned int value) {
    if (value == 0) {
        return "0x0000";
    }
    std::string hexStr;
    // 循环将数值转换为16进制
    while (value > 0) {
        unsigned int remainder = value % kHexadecimalRadix;
        // 将余数转换为对应的16进制字符
        char hexChar;
        if (remainder < kDecimalRadix) {
            hexChar = '0' + remainder; // 数字0-9
        } else {
            hexChar = 'A' + (remainder - 10); // 字母A-F
        }

        hexStr += hexChar; // 将字符添加到字符串末尾
        value /= kHexadecimalRadix;
    }
    // 反转字符串
    reverse(hexStr.begin(), hexStr.end());
    // 补零到4位长度
    while (hexStr.length() < kHexDigitsPerRegister) {
        hexStr = "0" + hexStr; // 在前面补零
    }
    // 如果超过4位，取最后4位（处理大于0xFFFF的情况）
    if (hexStr.length() > kHexDigitsPerRegister) {
        hexStr = hexStr.substr(hexStr.length() - kHexDigitsPerRegister);
    }
    // 添加16进制前缀并返回字符串
    return "0x" + hexStr;
}

std::string double2str(double value, int precision) {
    if (precision < 0 || static_cast<size_t>(precision) > sizeof(value)) {
        precision = 2;
    }
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

std::string strSetStandardDigitalReplace(int srcStr, bool vecStr) {
    return strReplace(strReplace("set_standard_digital_out(%1,%2)\n", "%1", srcStr), "%2", vecStr);
}

LaserSdkResult strToChar(char *script, int *scriptLen, const std::string &generatedScript) {
    if (scriptLen == nullptr) {
        return LaserSdkResult::PARAMETER_ERROR_LASER_SDK;
    }

    if (generatedScript.size() > static_cast<size_t>(INT_MAX - 1)) {
        *scriptLen = 0;
        return LaserSdkResult::MEMORY_ERROR_LASER_SDK;
    }

    const int requiredSize = static_cast<int>(generatedScript.size()) + 1;
    if (script == nullptr) {
        *scriptLen = requiredSize;
        return LaserSdkResult::SUCCESS_LASER_SDK;
    }

    // 负数转换为 size_t 后会成为极大正数，必须在无符号容量比较前拒绝。
    if (*scriptLen < 0) {
        return LaserSdkResult::PARAMETER_ERROR_LASER_SDK;
    }
    if (*scriptLen < requiredSize) {
        *scriptLen = requiredSize;
        return LaserSdkResult::BUFFER_TOO_SMALL_LASER_SDK;
    }

    std::memcpy(script, generatedScript.c_str(), static_cast<size_t>(requiredSize));
    *scriptLen = requiredSize;
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

namespace {
// 串口桥接按四个十六位槽接收完整 RTU 帧，收发槽位必须与控制器约定一致。
constexpr int MODBUS_REG_SEND_START = 62;
constexpr int MODBUS_REG_RECV_START = 66;
constexpr int MODBUS_REG_CLEAR_END = 69;
constexpr int MODBUS_SEND_REG_COUNT = 4;
constexpr int MODBUS_RECV_REG_COUNT = 1;
constexpr int MODBUS_REG_TYPE_INT16 = 0;
}

/**
 * @brief 生成预组装 Modbus 报文的 Lua 调用函数。
 *
 * 常量参数在 C++ 侧计算完整帧及 CRC，避免脚本运行时重复计算报文。
 * 变量参数只预组固定帧头，不能在生成阶段猜测运行时取值。
 */
std::string generateLuaScript(const std::string funcName, const std::string funcParam, const unsigned int addr, const CommuRegType regType, unsigned int coAddr,
    int comNo) {
    std::string funcParamScale = funcParam;
    uint16_t dataValue = 0;
    bool isLiteral = false;
    switch (regType) {
        case int16Reg: {
            funcParamScale = funcParam;
            // 仅纯数字参数才能预组帧，变量名必须留待脚本执行时求值。
            char *endptr = nullptr;
            long val = strtol(funcParam.c_str(), &endptr, kDecimalRadix);
            if (endptr != nullptr && *endptr == 0 && endptr != funcParam.c_str()) {

                dataValue = static_cast<uint16_t>(val);
                isLiteral = true;
            }
            break;
        }
        case float10Reg:
            funcParamScale = funcParam + "*10";
            break;
        case float100Reg:
            funcParamScale = funcParam + "*100";
            break;
    }

    // 名称会直接进入脚本语法，必须拒绝标识符以外的字符。
    auto isValidLuaIdentifier = [](const std::string &name) -> bool {
        if (name.empty()) {
            return false;
        }
        char first = name[0];
        if (!((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z') || first == '_')) {
            return false;
        }
        for (size_t i = 1; i < name.size(); i++) {
            char c = name[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
                return false;
            }
        }
        return true;
    };
    if (!isValidLuaIdentifier(funcName)) {
        writeLog("generateLuaScript: invalid Lua function name rejected");
        throw PARAMETER_ERROR_LASER_SDK;
    }
    if (!isLiteral && !isValidLuaIdentifier(funcParam)) {
        writeLog("generateLuaScript: invalid Lua parameter name rejected");
        throw PARAMETER_ERROR_LASER_SDK;
    }
    std::ostringstream luaCode;
    luaCode << std::hex << std::setfill('0');

    if (isLiteral) {
        luaCode << "function " << funcName << "()\n";
    } else {
        luaCode << "function " << funcName << "(" << funcParam << ")\n";
    }
    // 清空整个收发槽，避免前一帧残留影响本次串口桥接结果。
    luaCode << std::dec << "    for i=" << MODBUS_REG_SEND_START << "," << MODBUS_REG_CLEAR_END << ",1 do\n"
            << std::hex << "        write_int16_register(i,0)\n"
            << "    end\n";

    if (isLiteral) {
        std::map<uint16_t, uint16_t> msgMap;
        msgMap.insert(std::make_pair(static_cast<uint16_t>(addr), dataValue));
        std::vector<std::vector<uint8_t>> msgVec = buildMessageVector(msgMap, static_cast<uint8_t>(coAddr), kWriteSingleRegisterFunction);

        if (!msgVec.empty() && msgVec[0].size() == kSingleRegisterFrameBytes) {
            const std::vector<uint8_t> &msg = msgVec[0];
            uint16_t w1 = (static_cast<uint16_t>(msg[0]) << kBitsPerByte) | msg[1];
            uint16_t w2 = (static_cast<uint16_t>(msg[2]) << kBitsPerByte) | msg[3];
            uint16_t w3 = (static_cast<uint16_t>(msg[4]) << kBitsPerByte) | msg[5];
            uint16_t w4 = (static_cast<uint16_t>(msg[6]) << kBitsPerByte) | msg[7];

            luaCode << "    -- Modbus message (SDK pre-assembled): [slave=0x" << std::setw(kHexDigitsPerByte) << static_cast<int>(msg[0])
                    << "][func=0x06][addr=0x" << std::setw(kHexDigitsPerRegister) << addr << "][data=" << std::dec << dataValue << "][CRC16=0x" << std::hex
                    << std::setw(kHexDigitsPerRegister) << w4 << "]\n";
            luaCode << "    write_int16_register(" << MODBUS_REG_SEND_START << ", 0x" << std::setw(kHexDigitsPerRegister) << w1 << ")\n";
            luaCode << "    write_int16_register(" << (MODBUS_REG_SEND_START + 1) << ", 0x" << std::setw(kHexDigitsPerRegister) << w2 << ")\n";
            luaCode << "    write_int16_register(" << (MODBUS_REG_SEND_START + 2) << ", 0x" << std::setw(kHexDigitsPerRegister) << w3 << ")\n";
            luaCode << "    write_int16_register(" << (MODBUS_REG_SEND_START + 3) << ", 0x" << std::setw(kHexDigitsPerRegister) << w4 << ")\n";
        }
    } else {
        // 变量参数尚未求值，只能预先固定从站、功能码和地址。
        uint16_t fixed1 = (static_cast<uint16_t>(coAddr) << kBitsPerByte) | kWriteSingleRegisterFunction;
        uint16_t fixed2 = static_cast<uint16_t>(addr);

        luaCode << "    -- Fixed header (SDK pre-assembled): [slave=0x" << std::setw(kHexDigitsPerByte) << static_cast<int>(coAddr) << "][func=0x06][addr=0x"
                << std::setw(kHexDigitsPerRegister) << addr << "]\n";
        luaCode << "    write_int16_register(" << MODBUS_REG_SEND_START << ", 0x" << std::setw(kHexDigitsPerRegister) << fixed1 << ")\n";
        luaCode << "    write_int16_register(" << (MODBUS_REG_SEND_START + 1) << ", 0x" << std::setw(kHexDigitsPerRegister) << fixed2 << ")\n";
        luaCode << "    -- TODO: variable parameter runtime value + CRC computation\n";
    }

    luaCode << std::dec << "    -- Send message (register layout controlled by SDK)\n";
    luaCode << "    serial_send_receive(" << comNo << "," << MODBUS_SEND_REG_COUNT << "," << MODBUS_RECV_REG_COUNT << "," << MODBUS_REG_SEND_START << ","
            << MODBUS_REG_RECV_START << "," << MODBUS_REG_TYPE_INT16 << ")\n";
    luaCode << "end\n";
    return luaCode.str();
}

void checkLaserSdkResult(LaserSdkResult result) {
    if (result != SUCCESS_LASER_SDK) {
        throw result; // 异常抛出
    }
}

/**
 * @brief 写入分级日志：追加模式，带时间戳与级别前缀，超过大小上限自动滚动归档。
 * @param[in] recordingLog 日志内容
 * @param[in] level 日志级别，默认 WARNING
 * @note SDK 为单例单线程设计，本函数不做内部加锁，调用方须串行调用。
 */
void writeLog(const std::string &recordingLog, LogLevelSdk level) {
    // 日志文件路径与大小上限
    static const std::string kLogFilePath = "./logRecord.txt";
    static const std::string kArchivedLogFilePath = "./logRecord.old.txt";
    static const std::streamoff kMaxLogFileSize = 2 * 1024 * 1024; // 2MB

    const char *levelText = "WARN";
    switch (level) {
        case LOG_INFO_LASER_SDK:
            levelText = "INFO";
            break;
        case LOG_WARNING_LASER_SDK:
            levelText = "WARN";
            break;
        case LOG_ERROR_LASER_SDK:
            levelText = "ERROR";
            break;
        default:
            break;
    }

    std::ofstream file(kLogFilePath, std::ios::app);
    if (!file) {
        return;
    }

    // 滚动处理：超过大小上限时，先归档旧文件再写入新文件
    file.seekp(0, std::ios::end);
    if (file.tellp() >= kMaxLogFileSize) {
        file.close();
        std::remove(kArchivedLogFilePath.c_str());
        std::rename(kLogFilePath.c_str(), kArchivedLogFilePath.c_str());
        file.open(kLogFilePath, std::ios::app);
        if (!file) {
            return;
        }
    }

    // 时间戳：YYYY-MM-DD HH:MM:SS（localtime_s/localtime_r 避免非线程安全的 localtime）
    std::time_t nowTime = std::time(nullptr);
    std::tm localTime;
    std::memset(&localTime, 0, sizeof(localTime));
#ifdef _MSC_VER
    localtime_s(&localTime, &nowTime);
#else
    localtime_r(&nowTime, &localTime);
#endif
    char timeBuffer[32] = {0};
    std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &localTime);

    file << "[" << timeBuffer << "][" << levelText << "] " << recordingLog << "\n";
}

LaserSdkResult crsdk_return_value_transfer(CRresult res) {
    switch (res) {
        case sucess:
            return SUCCESS_LASER_SDK;
        case error:
            return ERROR_CRSDK_LASER_SDK;
        case thread_running:
            return THREAD_RUNNING_CRSDK_LASER_SDK;
        case operate_timeout:
            return OPERATE_TIMEOUT_CRSDK_LASER_SDK;
        case result_invalid:
            return RESULT_INVALID_CRSDK_LASER_SDK;
        case out_of_range:
            return OUT_OF_RANGE_CRSDK_LASER_SDK;
        case mutex_invalid:
            return MUTEX_INVALID_CRSDK_LASER_SDK;
        case para_error:
            return PARA_ERROR_CRSDK_LASER_SDK;
        case no_result:
            return NO_RESULT_CRSDK_LASER_SDK;
        case no_assignTCPindex:
            return NO_ASSIGNTCPINDEX_CRSDK_LASER_SDK;
        case no_handle:
            return NO_HANDLE_CRSDK_LASER_SDK;
        case handle_repeat:
            return HANDLE_REPEAT_CRSDK_LASER_SDK;
        case repeat_name:
            return REPEAT_NAME_CRSDK_LASER_SDK;
        case delete_invalid:
            return DELETE_INVALID_CRSDK_LASER_SDK;
        case set_bit_reg_invalid:
            return SET_BIT_REG_INVALID_CRSDK_LASER_SDK;
        case repeat_id:
            return REPEAT_ID_CRSDK_LASER_SDK;
        case file_encryption:
            return FILE_ENCRYPTION_CRSDK_LASER_SDK;
        case robotmode_error:
            return ROBOT_MODE_ERROR_CRSDK_LASER_SDK;
        case move_error:
            return MOVE_ERROR_CRSDK_LASER_SDK;
        case forcectrl_wrongpara:
            return FORCECTRL_WRONGPARA_CRSDK_LASER_SDK;
        case forcectrl_invaildcmd:
            return FORCECTRL_INVAILDCMD_CRSDK_LASER_SDK;
        case forcectrl_parablocked:
            return FORCECTRL_PARABLOCKED_CRSDK_LASER_SDK;
        case forcectrl_cmdblocked:
            return FORCECTRL_CMDBLOCKED_CRSDK_LASER_SDK;
        case forcectrl_configblocked:
            return FORCECTRL_CONFIGBLOCKED_CRSDK_LASER_SDK;
        case forcectrl_notenable:
            return FORCECTRL_DISABLE_CRSDK_LASER_SDK;
        case forcectrl_wrongsensor:
            return FORCECTRL_WRONGSENSOR_CRSDK_LASER_SDK;
        case outputrule_error:
            return OUTPUT_RULE_ERROR_CRSDK_LASER_SDK;
        case analoginputtype_error:
            return ANALOG_INPUT_TYPE_ERROR_CRSDK_LASER_SDK;
        default:
            return UNDEFINDED_ERROR_CRSDK_LASER_SDK;
    }
}
