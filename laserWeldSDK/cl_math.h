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
#endif // CL_MATH_H
