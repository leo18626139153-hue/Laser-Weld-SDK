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

#define PI_HY (3.1415926535897932384626433832795)
#define ZeroThreshold_HY (1e-8)

// 三维空间向量取模
inline double norm_vec_HY(const double *a)
{
    return sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
}


// 三维空间向量单位化
void normalized_vector_HY(const double* vec, double* result);

// 3X3矩阵与3维向量乘法
void matrix_vector_multiply_HY(const double* matrix, const double* vec, double* result);

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

// 姿态复制(六维)
void copy_pose_HY(const double* oldpose, double* pose);
// pose_trans 仅支持位置，仅用于焊接时抬枪计算
void pose_trans_HY(const double* pose, const double* delat_pose, double* new_pose);


/**
 * 从Modbus响应报文中提取指定寄存器的值（全功能集成版）
 * @param hexResponse Modbus响应报文（如 "09030212345678"）
 * @param regIndex 寄存器索引（从1开始）
 * @param regLen 寄存器长度（从1开始）
 * @return 寄存器值（自动处理有符号16位）
 * @throws std::runtime_error 报文格式错误时抛出异常
 */
int16_t* parse_modbus_register(const std::string& hexResponse, const int regIndex = 1, const int regLen = 1);

int32_t parse_modbus_register32(const std::string& hexResponse);
#endif // CL_MATH_H
