#ifndef CL_MATH_H
#define CL_MATH_H

#include "cl_basedefine.h"
#include "robotapi.h"
#include "cl_internaldatadefine.h"
#include <stdio.h>
#include <cmath>
#include <iostream>
#include <vector>
#include <fstream>
#include <array>
#include <string>
#include <algorithm>
#include <memory>
#include <cstring>
#include <tuple>

//向量叉乘
/**
 * @brief vecCross  空间向量叉乘计算
 * @param xdir      输入——三维向量1
 * @param ydir      输入——三维向量2
 * @param result    输出与向量1与向量2相互垂直的向量
 */
void vecCross(double xdir[3], double ydir[3], double result[3]);

//计算交融半径
LaserSdkResult CalBlendingRadius(double vMax, double aMax, double* r);

//向量归一化
void normalizeVector(double Fn[3]);

//逆解
LaserSdkResult kineInverse_tcpPose(RobotHandle robotHandle, double srcPose[ROB_AXIS_NUM], double refJointPose[ROB_AXIS_NUM], double tarJointPose[ROB_AXIS_NUM]);


/**
 * 从Modbus响应报文中提取指定寄存器的值（全功能集成版）
 * @param hexResponse Modbus响应报文（如 "09030212345678"）
 * @param regIndex 寄存器索引（从1开始）
 * @param regLen 寄存器长度（从1开始）
 * @return 寄存器值（自动处理有符号16位）
 * @throws std::runtime_error 报文格式错误时抛出异常
 */
int16_t* parse_modbus_register(const std::string& hexResponse, const int regIndex = 1, const int regLen = 1);

#endif // CL_MATH_H
