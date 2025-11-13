#ifndef CWUTILS_H
#define CWUTILS_H
#include "cl_basedefine.h"
#include"cl_internaldatadefine.h"
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <array>
#include <algorithm>
#include <memory>
#include <cstring>
#include <tuple>
#include <sstream>  // 字符串流头文件
#include <iomanip>  // 用于输出格式控制

/**
 * 生成写入参数类型的Lua脚本的函数
 * funcName     函数名
 * funcParam    参数
 * addr         地址
 * regTypr      寄存器类型
 * coAddr      从机地址：coAddr 默认0x0608
 * comNo        端口号：comNo 默认3
 */
std::string generateLuaScript(const std::string funcName, const std::string funcParam, const unsigned int addr, const CommuRegType regTypr, unsigned int coAddr = 0x0906,int comNo = 3);

/**
 * 对应通用写串口的Lua脚本的函数的调用
 * @param functionCode 功能码
 * @param portAddress 地址
 * @param paramValue 写入值
 * @param firstRegisterNum 寄存器首地址
 * @param receiptNeed   是否要回执
 */
inline std::string writeLuaScript(const std::string functionCode , const std::string portAddress, const std::string paramValue, const std::string portNum, const std::string firstRegisterNum, const std::string receiptNeed)
{
    std::ostringstream writeScript ;
    writeScript << "    write_value_port(" << functionCode << "," << portAddress << "," << paramValue << "," << portNum << "," << firstRegisterNum << ","<< receiptNeed <<")\n";
    return writeScript.str();
}

/**
 *字符串转字符
 * @param script 输出字符
 * @param scriptLen 输出字符长度
 * @param generatedScript 输入字符串
 * @return 当内存不够时返回false
*/
bool strToChar(char* script, int* scriptLen, const std::string& generatedScript);

/**
 * 通过throw抛出问题LaserSdkResult
 */
void checkLaserSdkResult(LaserSdkResult result);

/**
 * 将已知的问题写入到日志
 */
void writeLog(std::string recordingLog);



//字符替换
std::string strReplace(std::string srcStr, std::array<double,6> vecDouble);
std::string strReplace(std::string srcStr, std::vector<double> vecDouble);
std::string strReplace(std::string srcStr, std::vector<int> vecInt);
std::string strReplace(std::string srcStr, std::string format, std::string replaceStr);
std::string strReplace(std::string srcStr, std::string format, int replaceInt);
std::string strReplace(std::string srcStr, std::string format, unsigned int replaceUInt);
std::string strReplace(std::string srcStr, std::string format, double replaceDouble);
std::string strReplace(std::string srcStr, std::string format, float replacefloat);
std::string strReplace(std::string srcStr, std::vector<std::string> vecStr);

//各类数据结构转为字符串
std::string doubleArray2Str(double* array, int size);
std::string Double2String(double value, int precision = 2, bool scientific = false);
std::string point2str(std::vector<double> point);
std::string point2str(std::array<double, 6> point);
std::string Vec2Str(double *vec, int vecLen);
std::string pointVec2str(std::vector<std::vector<double>> pointVec);
std::string pointVec2str(std::vector<std::array<double, 6>> pointVec);
std::string PointVec2Str(std::vector<std::vector<double>> pointVec, std::string strName, int segLen);
std::string index2str(std::vector<int> index);

//常用的LUA脚本语句组装
//读取指定pdo地址的值后，给指定位写入值
std::string PdoReadWriteBit(unsigned addr, int wireBitLoc,int writeValue);

//读取指定int16地址的值后，给指定位写入值
std::string Int16ReadWriteBit(unsigned addr, int wireBitLoc,int writeValue);

//常用的LUA脚本语句组装
std::string strSetStandardDigitalReplace(int srcStr, bool vecStr);

/* ---   读写寄存器   --- */
//write_ect_pdo
template<typename T, typename Y>
inline std::string strWritePdoReplace(T srcStr, Y vecStr)
{
    return strReplace(strReplace("%1=write_ect_pdo(%2)\n", "%1", srcStr), "%2", vecStr);
}
//read_ect_pdo
template<typename T, typename Y>
inline std::string strReadPdoReplace(T returnStr, Y addrStr)
{
    return strReplace(strReplace("%1=read_ect_pdo(%2)\n", "%1", returnStr), "%2", addrStr);
}
//write_int16_register
template <typename T, typename Y>
inline std::string strWriteInt16RegReplace(T srcStr, Y vecStr)
{
    return strReplace(strReplace("write_int16_register(%1,%2)\n", "%1", srcStr), "%2", vecStr);
}
//read_int16_register
template<typename T, typename Y>
inline std::string strReadInt16RegReplace(T returnStr, Y addrStr)
{
    return strReplace(strReplace("%1=read_int16_register(%2)\n", "%1", returnStr), "%2", addrStr);
}
//write_int32_register
template <typename T, typename Y>
inline std::string strWriteInt32RegReplace(T srcStr, Y vecStr)
{
    return strReplace(strReplace("write_integer_register(%1,%2)\n", "%1", srcStr), "%2", vecStr);
}
//read_int32_register
template <typename T, typename Y>
inline std::string strReadInt32RegReplace(T returnStr, Y addrStr)
{
    return strReplace(strReplace("%1=read_integer_register(%2)\n", "%1", returnStr), "%2", addrStr);
}
//write_float_register
template<typename T, typename Y>
inline std::string strWriteFloatRegReplace(T srcStr, Y vecStr)
{
    return strReplace(strReplace("write_float_register(%1,%2)\n", "%1", srcStr), "%2", vecStr);
}
//read_float_register
template<typename T, typename Y>
std::string strReadFloatRegReplace(T returnStr, Y addrStr)
{
    return strReplace(strReplace("%1=read_float_register(%2)\n", "%1", returnStr), "%2", addrStr);
}
//write_bool_register
template<typename T, typename Y>
std::string strWriteBoolRegReplace(T srcStr, Y vecStr)
{
    return strReplace(strReplace("write_bool_register(%1,%2)\n", "%1", srcStr), "%2", vecStr);
}

/* --- 运动脚本 --- */
//moveL
template <typename X, typename Y, typename Z, typename N>
std::string strMoveLReplace(X data1, Y data2, Z data3, N data4)
{
    return strReplace(strReplace(strReplace(strReplace("movel(%1,%2,%3,%4)\n", "%1", data1), "%2", data2), "%3", data3), "%4", data4);
}
//moveJ
template <typename X, typename Y, typename Z, typename N>
std::string strMoveJReplace(X data1, Y data2, Z data3, N data4)
{
    return strReplace(strReplace(strReplace(strReplace("movej(%1,%2,%3,%4)\n", "%1", data1), "%2", data2), "%3", data3), "%4", data4);
}
//moveC
template <typename X, typename Y, typename Z, typename N, typename M, typename P>
std::string strMoveCReplace(X data1, Y data2, Z data3, N data4, M data5, P data6)
{
    return strReplace(strReplace(strReplace(strReplace(strReplace(strReplace("movec(%1,%2,%3,%4,%5,%6)\n", "%1", data1), "%2", data2), "%3", data3), "%4", data4), "%5", data5), "%6", data6);
}

template <typename S, typename T, typename X, typename Y, typename Z, typename N, typename M, typename P>
std::string strMoveCReplace(S points0, T joints0, X points1, Y joints1, Z speed, N acc, M radius, P data6)
{
    return strReplace(strReplace(strReplace(strReplace(strReplace(strReplace(strReplace(strReplace("movec(%1,%2,%3,%4,%5,%6,%7,%8)\n",
                                                                                                   "%1", points0),
                                                                                        "%2", joints0),
                                                                             "%3", points1),
                                                                  "%4", joints1),
                                                       "%5", speed),
                                            "%6", acc),
                                 "%7", radius),
                      "%8", data6);
}

/// 比较两个浮点数是否相等的函数
bool isEqual(double a, double b);

//对16位数据进行高低字节拆分
std::tuple<int, int> splitHighLowByteValue(double value);

//CRresult转LaserSdkResult
LaserSdkResult crsdk_return_value_transfer(CRresult res);

//十进制转16进制，输出为对应十六进制的字符串类型
std::string decimalToHexStringStream(unsigned int value);

/**
 * @brief 将 double 转换为字符串，保留指定小数位数
 * @param value 输入的浮点数
 * @param precision 保留的小数位数（默认 2）
 * @return 格式化后的字符串
 */
std::string double2str(double value, int precision);

#endif // CWUTILS_H



