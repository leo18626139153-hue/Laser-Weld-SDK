#include "cl_utils.h"

//#include"cwutils.h"
//#include"cwmath.h"
#include <sstream>
#include <iomanip>

std::string strReplace(std::string srcStr, std::array<double,6> vecDouble)
{
    int vecLen = vecDouble.size();
    for(int i = vecLen; i >= 1; i--)
    {
        std::string format = "%" + std::to_string(i);
        std::string replaceStr = std::to_string(vecDouble[i-1]);
        while(true)
        {
            size_t pos = srcStr.find(format);
            if (pos != std::string::npos) {
                srcStr.replace(pos, format.length(), replaceStr);
            }
            else
            {
                break;
            }
        }
    }
    return srcStr;
}


std::string strReplace(std::string srcStr, std::vector<double> vecDouble)
{
    int vecLen = vecDouble.size();
    for(int i = vecLen; i >= 1; i--)
    {
        std::string format = "%" + std::to_string(i);
        std::string replaceStr = std::to_string(vecDouble[i-1]);
        while(true)
        {
            size_t pos = srcStr.find(format);
            if (pos != std::string::npos) {
                srcStr.replace(pos, format.length(), replaceStr);
            }
            else
            {
                break;
            }
        }
    }
    return srcStr;
}

std::string strReplace(std::string srcStr, std::vector<int> vecInt)
{
    int vecLen = vecInt.size();
    for(int i = vecLen; i >= 1; i--)
    {
        std::string format = "%" + std::to_string(i);
        std::string replaceStr = std::to_string(vecInt[i-1]);
        while(true)
        {
            size_t pos = srcStr.find(format);
            if (pos != std::string::npos) {
                srcStr.replace(pos, format.length(), replaceStr);
            }
            else
            {
                break;
            }
        }
    }
    return srcStr;
}

std::string strReplace(std::string srcStr, std::string format, std::string replaceStr)
{
    while(true)
    {
        size_t pos = srcStr.find(format);
        if (pos != std::string::npos) {
            srcStr.replace(pos, format.length(), replaceStr);
        }
        else
        {
            break;
        }
    }
    return srcStr;
}

std::string strReplace(std::string srcStr, std::string format, int replaceInt)
{
    std::string replaceStr = std::to_string(replaceInt);
    while(true)
    {
        size_t pos = srcStr.find(format);
        if (pos != std::string::npos) {
            srcStr.replace(pos, format.length(), replaceStr);
        }
        else
        {
            break;
        }
    }
    return srcStr;
}

std::string strReplace(std::string srcStr, std::string format, unsigned int replaceUInt)
{
    std::string replaceStr = std::to_string(replaceUInt);
    while(true)
    {
        size_t pos = srcStr.find(format);
        if (pos != std::string::npos) {
            srcStr.replace(pos, format.length(), replaceStr);
        }
        else
        {
            break;
        }
    }
    return srcStr;
}


std::string strReplace(std::string srcStr, std::string format, double replaceDouble)
{
    std::string replaceStr = std::to_string(replaceDouble);
    while(true)
    {
        size_t pos = srcStr.find(format);
        if (pos != std::string::npos) {
            srcStr.replace(pos, format.length(), replaceStr);
        }
        else
        {
            break;
        }
    }
    return srcStr;
}

std::string strReplace(std::string srcStr, std::string format, float replaceFloat)
{
    std::string replaceStr = std::to_string(replaceFloat);
    while(true)
    {
        size_t pos = srcStr.find(format);
        if (pos != std::string::npos) {
            srcStr.replace(pos, format.length(), replaceStr);
        }
        else
        {
            break;
        }
    }
    return srcStr;
}

std::string strReplace(std::string srcStr, std::vector<std::string> vecStr)
{
    int vecLen = vecStr.size();
    for(int i = vecLen; i >= 1; i--)
    {
        std::string format = "%" + std::to_string(i);
        std::string replaceStr = vecStr[i-1];
        while(true)
        {
            size_t pos = srcStr.find(format);
            if (pos != std::string::npos) {
                srcStr.replace(pos, format.length(), replaceStr);
            }
            else
            {
                break;
            }
        }
    }
    return srcStr;
}

std::string point2str(std::vector<double> point)
{
    std::string pointStr = "{";
    for(int i = 0; i < point.size(); i++)
    {
        pointStr += std::to_string(point[i]);
        if(i != (point.size()-1))
        {
            pointStr += ",";
        }
        else
        {
            pointStr += "}";
        }
    }
    return pointStr;
}

std::string point2str(std::array<double, 6> point)
{
    std::string pointStr = "{";
    for(int i = 0; i < point.size(); i++)
    {
        pointStr += std::to_string(point[i]);
        if(i != (point.size()-1))
        {
            pointStr += ",";
        }
        else
        {
            pointStr += "}";
        }
    }
    return pointStr;
}

std::string Vec2Str(double *vec, int vecLen)
{
    std::string pointStr = "{";
    for(int i = 0; i < vecLen; i++)
    {
        pointStr += std::to_string(vec[i]);
        if(i != (vecLen-1))
        {
            pointStr += ",";
        }
        else
        {
            pointStr += "}";
        }
    }
    return pointStr;
}

std::string pointVec2str(std::vector<std::vector<double>> pointVec)
{
    std::string pointVecStr = "{";
    for(int i = 0; i < pointVec.size(); i++)
    {
        std::string pointStr = point2str(pointVec[i]);
        pointVecStr += pointStr;
        if(i != (pointVec.size()-1))
        {
            pointVecStr += ",";
        }
        else
        {
            pointVecStr += "}";
        }
    }
    return pointVecStr;
}

std::string pointVec2str(std::vector<std::array<double, 6>> pointVec)
{
    std::string pointVecStr = "{";
    for(int i = 0; i < pointVec.size(); i++)
    {
        std::string pointStr = point2str(pointVec[i]);
        pointVecStr += pointStr;
        if(i != (pointVec.size()-1))
        {
            pointVecStr += ",";
        }
        else
        {
            pointVecStr += "}";
        }
    }
    return pointVecStr;
}

std::string PointVec2Str(std::vector<std::vector<double>> pointVec, std::string strName, int segLen)
{
    std::string pointVecStr;
    int count = 0, segCount = 1;
    if (segLen > 0)
        pointVecStr = strName + std::to_string(segCount) + "={";
    else
        pointVecStr = strName + "={";

    for(int i = 0; i < pointVec.size(); i++)
    {
        pointVecStr += "{";
        for (int j = 0; j < pointVec[i].size(); j++)
        {
            pointVecStr += Double2String(pointVec[i][j]);
            if (j != pointVec[i].size() - 1)
                pointVecStr += ",";
            else
                pointVecStr += "}";
        }

        count++;
        if (segLen > 0 && count == segLen)
        {
            count = 0;
            segCount++;
            if(i != (pointVec.size()-1))
                pointVecStr += "}\n" + strName + std::to_string(segCount) + "={";
            else
                pointVecStr += "}\n";
        }
        else
        {
            if(i != (pointVec.size()-1))
                pointVecStr += ",";
            else
                pointVecStr += "}\n";
        }
    }
    return pointVecStr;
}

std::string Double2String(double value, int precision, bool scientific)
{
    std::ostringstream oss;

    // 设置输出格式
    if (scientific) // 科学计数法
    {
        oss << std::scientific << std::setprecision(precision);
    }
    else
    {
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
    if (!scientific)
    {
        size_t dot_pos = result.find('.');
        if (dot_pos != std::string::npos)
        {
            // 从末尾开始移除0
            result.erase(result.find_last_not_of('0') + 1);
            // 如果小数点后没有数字了，移除小数点
            if (result.back() == '.')
            {
                result.pop_back();
            }
        }
    }

    return result;
}

//bool isEqual(double a, double b) {
//    // 计算两个浮点数的差值的绝对值
//    return std::fabs(a - b) < EPSILON;
//}

std::string PdoReadWriteBit(unsigned addr, int wireBitLoc, int writeValue)
{
    //焊丝回抽
    std::string returnVal ="returnVal";
    std::string scriptText;
    //读手动测试地址中的数据
    scriptText += strReadPdoReplace(returnVal,addr);

    //给某个数的指定位（从1开始计数）写值  setLocBit函数在之前已经定义
    scriptText += strReplace("%1=setLocBit(%1, %2, %3)\n",std::vector<std::string>{returnVal, std::to_string(wireBitLoc-1),std::to_string(writeValue)});
    //重新写入值
    scriptText += strWritePdoReplace(addr, returnVal);
    return scriptText;
}

std::string Int16ReadWriteBit(unsigned addr, int wireBitLoc, int writeValue)
{
    //焊丝回抽
    std::string returnVal ="returnVal";
    std::string scriptText;
    //读手动测试地址中的数据
    scriptText += strReadModbusReplace(returnVal,addr);

    //给某个数的指定位（从1开始计数）写值  setLocBit函数在之前已经定义
    scriptText += strReplace("%1=setLocBit(%1, %2, %3)\n",std::vector<std::string>{returnVal, std::to_string(wireBitLoc-1),std::to_string(writeValue)});
    //重新写入值
    scriptText += strWriteModbusReplace(addr, returnVal);
    return scriptText;
}

// 定义splitPdoValue的C++版本
std::tuple<int, int> splitHighLowByteValue(double value) {
    int intValue = static_cast<int>(value);
    // 获取高8位
    int high8Bits = (intValue >> 8) & 0xFF;
    // 获取低8位
    int low8Bits = intValue & 0xFF;
    return std::make_tuple(high8Bits, low8Bits);
}

std::string doubleArray2Str(double* array, int size)
{
    std::string pointStr = "{";
    for(int i = 0; i < size; i++)
    {
        pointStr += std::to_string(array[i]);
        if(i != (size-1))
        {
            pointStr += ",";
        }
        else
        {
            pointStr += "}";
        }
    }
    return pointStr;
}


LaserSdkResult crsdk_return_value_transfer(CRresult res)
{
    switch (res)
    {
        case sucess:
            return SUCCESS_LASER_SDK;
        case error:
            return ERROR_LASER_SDK;
        //        case thread_running:
        //            return THREAD_RUNNING_CRSDK_WELD_SDK;
        //        case operate_timeout:
        //            return OPERATE_TIMEOUT_CRSDK_WELD_SDK;
        //        case result_invalid:
        //            return RESULT_INVALID_CRSDK_WELD_SDK;
        default:
            return UNDEFINDED_ERROR_CRSDK_LASER_SDK;
    }
}

std::string decimalToHexStringStream(unsigned int value)
{
    if (value == 0) {
        return "0x0000";
    }
    std::string hexStr;
    // 循环将数值转换为16进制
    while (value > 0) {
        unsigned int remainder = value % 16;  // 求余数（0-15）
        // 将余数转换为对应的16进制字符
        char hexChar;
        if (remainder < 10) {
            hexChar = '0' + remainder;  // 数字0-9
        } else {
            hexChar = 'A' + (remainder - 10);  // 字母A-F
        }

        hexStr += hexChar;  // 将字符添加到字符串末尾
        value /= 16;        // 除以16继续处理下一位
    }
    // 反转字符串
    reverse(hexStr.begin(), hexStr.end());
    // 补零到4位长度
    while (hexStr.length() < 4) {
        hexStr = "0" + hexStr;  // 在前面补零
    }
    // 如果超过4位，取最后4位（处理大于0xFFFF的情况）
    if (hexStr.length() > 4) {
        hexStr = hexStr.substr(hexStr.length() - 4);
    }
    // 添加16进制前缀并返回字符串
    return "0x" + hexStr;
}

std::string strSetStandardDigitalReplace(int srcStr, bool vecStr)
{
    return strReplace(strReplace("set_standard_digital_out(%1,%2)\n", "%1", srcStr), "%2", vecStr);
}

template<typename T, typename Y>
std::string strWritePdoReplace(T srcStr, Y vecStr)
{
    return strReplace(strReplace("%1=write_ect_pdo(%2)\n", "%1", srcStr), "%2", vecStr);
}

template<typename T, typename Y>
std::string strReadPdoReplace(T returnStr, Y addrStr)
{
    return strReplace(strReplace("%1=read_ect_pdo(%2)\n", "%1", returnStr), "%2", addrStr);
}

template<typename T, typename Y>
std::string strWriteModbusReplace(T srcStr, Y vecStr)
{
    return strReplace(strReplace("write_int16_register(%1,%2)\n", "%1", srcStr), "%2", vecStr);

}

template<typename T, typename Y>
std::string strReadModbusReplace(T returnStr, Y addrStr)
{
    return strReplace(strReplace("%1=read_int16_register(%2)\n", "%1", returnStr), "%2", addrStr);

}

template<typename T, typename Y>
std::string strWriteFloatRegReplace(T srcStr, Y vecStr)
{
    return strReplace(strReplace("write_float_register(%1,%2)\n", "%1", srcStr), "%2", vecStr);
}

template<typename T, typename Y>
std::string strReadFloatRegReplace(T returnStr, Y addrStr)
{
    return strReplace(strReplace("%1=read_float_register(%2)\n", "%1", returnStr), "%2", addrStr);
}

template<typename T, typename Y>
std::string strWriteBoolRegReplace(T srcStr, Y vecStr)
{
    return strReplace(strReplace("write_bool_register(%1,%2)\n", "%1", srcStr), "%2", vecStr);
}

template<typename X, typename Y, typename Z, typename N>
std::string strMoveLReplace(X data1, Y data2, Z data3, N data4)
{
    return strReplace(strReplace(strReplace(strReplace("movel(%1,%2,%3,%4)\n", "%1", data1), "%2", data2), "%3", data3), "%4", data4);
}

template<typename X, typename Y, typename Z, typename N>
std::string strMoveJReplace(X data1, Y data2, Z data3, N data4)
{
    return strReplace(strReplace(strReplace(strReplace("movej(%1,%2,%3,%4)\n", "%1", data1), "%2", data2), "%3", data3), "%4", data4);
}

template<typename X, typename Y, typename Z, typename N, typename M, typename P>
std::string strMoveCReplace(X data1, Y data2, Z data3, N data4, M data5, P data6)
{
    return strReplace(strReplace(strReplace(strReplace(strReplace(strReplace("movec(%1,%2,%3,%4,%5,%6)\n", "%1", data1), "%2", data2), "%3", data3), "%4", data4), "%5", data5), "%6", data6);
}

bool strToChar(char* script, int* scriptLen, std::string& generatedScript)
{
    if (script == nullptr || scriptLen == nullptr)
    {
        std::cerr << "Error: Null pointer passed to strToChar." << std::endl;
        return false;
    }

    // 计算所需缓冲区大小（包括空终止符）
    size_t requiredSize = generatedScript.size() + 1;
    if (static_cast<size_t>(*scriptLen) < requiredSize)
    {
        *scriptLen = static_cast<int>(requiredSize); // 通知调用者所需大小
        return false;
    }

    if (generatedScript.empty())
    {
        script[0] = '\0'; // 明确设置空字符串
        *scriptLen = 1;   // 缓冲区使用大小为1
        return true;
    }

    // 复制字符串内容
    memcpy(script, generatedScript.c_str(), generatedScript.size());
    script[generatedScript.size()] = '\0'; // 添加空终止符
    *scriptLen = static_cast<int>(requiredSize); // 设置实际使用的长度
    return true;
}


std::string generateLuaScript(const std::string funcName, const std::string funcParam, const unsigned int addr, const CommuRegType regType,unsigned int coAddr, int comNo)
{
    std::string funcParamScale  = funcParam;
    switch (regType) {
        case int16Reg:
            funcParamScale = funcParam;
            break;
        case float10Reg:
            funcParamScale = funcParam + "*10";
            break;
        case float100Reg:
            funcParamScale = funcParam + "*100";
            break;
    }
    // 计算交换后的地址（高低字节互换）
    unsigned int swapped_coAddr = ((coAddr & 0xFF) << 8) | (coAddr >> 8);
    unsigned int swapped_addr = ((addr & 0xFF) << 8) | (addr >> 8);
    // 创建字符串流用于构建Lua代码
    std::ostringstream luaCode;
    // 设置十六进制输出格式
    luaCode << std::hex << std::setfill('0');
    //定义写入 funcName 函数
    luaCode << "function " << funcName << "(" << funcParam << ")\n"
            << "    for i=62,69,1 do\n"
            << "        write_int16_register(i,0)\n"
            << "    end\n";
    //---定义写入报文----
    luaCode << "    mes_1 = Lua_get_message({0x"<< std::setw(4) << coAddr <<", 0x"<< std::setw(4) << addr << ", "<< funcParamScale << "})\n";
    luaCode << "    mes_2 = Lua_crc16(mes_1)\n";
    luaCode << "    " << funcParam << " = swap_bytes_for_word(" << funcParam << ")\n";
    luaCode << "    mes_3 = {0x"<< std::setw(4) << swapped_coAddr << ", 0x"<< std::setw(4) << swapped_addr << ", "<< funcParamScale << ", mes_2}\n";

    luaCode << "    for i=1,4,1 do\n"
            << "        write_int16_register(i+61, mes_3[i])\n"
            << "    end\n";
    luaCode << "    -- 发送报文\n";
    luaCode << "    serial_send(" << comNo <<",4,1,62)  -- 默认使用int16类型\n";
    luaCode << "end\n";
    return luaCode.str();
}

std::string readLuaScript(const std::string funcName, const std::string funcParam, const unsigned int addr)
{

    return funcName;
}

void checkLaserSdkResult(LaserSdkResult result)
{
    if (result != SUCCESS_LASER_SDK) {
        throw result;  // 异常抛出
    }
}
