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

/**
 * @brief 向量模运算
 * @param[in]   a   输入三维向量
 * @return 向量模运算结果
 */
inline double norm_vec_HY(const double *a)
{
    return sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
}

// 三维空间向量单位化
void normalized_vector_HY(const double* vec, double* result);

// 3X3矩阵与3维向量乘法
void matrix_vector_multiply_HY(const double* matrix, const double* vec, double* result);

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

/**
 * pose_trans 3D姿态变换,仅支持位置，仅用于焊接时抬枪计算
 * @param[in] pose 输入位姿
 * @param[in] delat_pose 增量姿态（6维数组）
 * @param[out] new_pose 输出姿态
 */
void pose_trans_HY(const double* pose, const double* delat_pose, double* new_pose);

/**
 * @brief getNewTeachPoints    整体圆形轨迹——根据停止点和回退距离获取新的起点和过渡点、终点
 * @param robothandle          机械臂句柄
 * @param p1                   示教起点
 * @param p2                   示教终点
 * @param backDistance         回退距离
 * @param refJoint               逆解参考关节角度
 * @param pointTrans             降功率点
 * @param pointJointTrans        降功率点关节角度
 * @return WeldSdkResult       处理结果
 */
LaserSdkResult getStraightLineSlowUpPoint(int robothandle, double p1[6], double p2[6], double backDistance, double refJoint[6], double pointTrans[6], double pointJointTrans[6]);

/**
 * @brief getNewTeachPoints    整体圆形轨迹——根据停止点和回退距离获取新的起点和过渡点、终点
 * @param p1                   示教起点
 * @param p2                   示教过渡点
 * @param p3                   示教终点
 * @param stopP                停止点
 * @param backDistance         回退距离
 * @param stopMapP             停止点的映射点
 * @param restartP             新起点
 * @param midP                 新过渡点
 * @param endP                 原始终点
 * @return WeldSdkResult       处理结果
 */
LaserSdkResult getCirclrSlowDownPoints(int robothandle, double p1[6], double p2[6], double p3[6], double stopP[6], double backDistance, double refJoint[6], double stopMapP[3], double newTransPoint[6], double newTransJointPoint[6], double restartP[6], double midP[6], double endP[6], double restartPJoint[6], double midPJoint[6], double endPJoint[6]);


// 计算圆的半径、中心点和法向量
/**
 * @brief Circle3dCalc  根据示教的不在同一直线上的三个点计算这三个点所确定的空间圆
 * @param point1        示教起点
 * @param point2        示教中点
 * @param point3        示教终点
 * @param radius        空间圆的半径
 * @param centerPoint   空间圆的圆心
 * @param axisVec       空间圆的法向
 * @return              返回计算是否成功0失败、1成功
 */
int circle3dCalc(double point1[6], double point2[6], double point3[6], double* radius, double centerPoint[3], double axisVec[3]);

void getMapPoint(double centerP[3], double Fn[3], double radius, double stopPoint[3], double mapPoint[3]);//映射点

void calculateP2Point(const double O[3], const double Fn[3], double r, const double P[3], double c, double P2[3]);

double calculateAngle(const double v1[3], const double v2[3]);
#endif // CL_MATH_H
