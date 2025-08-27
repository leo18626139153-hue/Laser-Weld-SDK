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
 * 生成Lua脚本的函数
 * @param params 脚本参数结构体
 * @return 生成的Lua脚本字符串
 */
std::string generateLuaScript(const LuaScriptParams& params);

//字符替换
std::string strReplace(std::string srcStr, std::array<double,6> vecDouble);
std::string strReplace(std::string srcStr, std::vector<double> vecDouble);
std::string strReplace(std::string srcStr, std::vector<int> vecInt);
std::string strReplace(std::string srcStr, std::string format, std::string replaceStr);
std::string strReplace(std::string srcStr, std::string format, int replaceInt);
std::string strReplace(std::string srcStr, std::string format, unsigned int replaceUInt);
std::string strReplace(std::string srcStr, std::string format, double replaceDouble);
std::string strReplace(std::string srcStr, std::string format, float replaceFloat);
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
//set_standard_digital_out(0,true) --设置标准数字输出（偏移地址，值） CO1/2 只写不读
std::string strSetStandardDigitalReplace(int srcStr, bool vecStr);
//

//write_ect_pdo
template <typename T, typename Y>
std::string strWritePdoReplace(T srcStr, Y vecStr);

//read_ect_pdo
template <typename T, typename Y>
std::string strReadPdoReplace(T returnStr, Y addrStr);

//write_int16_register
template <typename T, typename Y>
std::string strWriteModbusReplace(T srcStr, Y vecStr);

//read_int16_register
template <typename T, typename Y>
std::string strReadModbusReplace(T returnStr, Y addrStr);

//write_float_register
template <typename T, typename Y>
std::string strWriteFloatRegReplace(T srcStr, Y vecStr);

//read_float_register
template <typename T, typename Y>
std::string strReadFloatRegReplace(T returnStr, Y addrStr);

//write_bool_register
template <typename T, typename Y>
std::string strWriteBoolRegReplace(T srcStr, Y vecStr);

////moveL
template <typename X, typename Y, typename Z, typename N>
std::string strMoveLReplace(X data1, Y data2, Z data3, N data4);
//moveJ
template <typename X, typename Y, typename Z, typename N>
std::string strMoveJReplace(X data1, Y data2, Z data3, N data4);

///moveC
template <typename X, typename Y, typename Z, typename N, typename M, typename P>
std::string strMoveCReplace(X data1, Y data2, Z data3, N data4, M data5, P data6);

/// 比较两个浮点数是否相等的函数
//bool isEqual(double a, double b);

//对16位数据进行高低字节拆分
std::tuple<int, int> splitHighLowByteValue(double value);
//CRresult转LaserSdkResult
LaserSdkResult crsdk_return_value_transfer(CRresult res);

std::string decimalToHexStringStream(unsigned int value);

#endif // CWUTILS_H



