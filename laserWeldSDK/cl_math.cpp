#include "cl_math.h"
#include <sstream>
#include <iomanip>
#include"cl_gradualriseandfall.h"
#include "cl_utils.h"

//向量叉乘
void vecCross(double xdir[3], double ydir[3], double result[3])
{
    result[0] = xdir[1] * ydir[2] - xdir[2] * ydir[1];
    result[1] = xdir[2] * ydir[0] - xdir[0] * ydir[2];
    result[2] = xdir[0] * ydir[1] - xdir[1] * ydir[0];
}


void normalizeVector(double Fn[3])
{
    // 计算Fn的长度
    double length = sqrt(Fn[0] * Fn[0] + Fn[1] * Fn[1] + Fn[2] * Fn[2]);

    if (length == 0.0)
    {
        throw std::runtime_error("起点和终点相同，无法计算");
    }

    // 将Fn归一化
    Fn[0] /= length;
    Fn[1] /= length;
    Fn[2] /= length;
}

//计算交融半径
LaserSdkResult CalBlendingRadius(double vMax, double aMax, double* r)
{
    // 1. 参数初始化
    const double pathLen = 500.0;
    const double tAhead = 0.002;
    const double vPercent = 100.0;
    const double jPercent = 100.0;
    vMax = vMax * vPercent / 100.0;
    aMax = aMax * std::sqrt(vPercent / 100.0);
    double jMax = aMax * jPercent / 100.0 * 8.0;

    // 2. 计算初始时间参数
    double Tj = std::pow(pathLen / (2.0 * jMax), 1.0/3.0);
    double amax = jMax * Tj;

    // 3. 条件分支处理
    double Ta = 0.0, Tv = 0.0, vmax = 0.0;

    if(amax > aMax) {
        // 3.1 加速度超过限制的情况
        amax = aMax;
        Tj = aMax / jMax;
        double a = aMax;
        double b = jMax * std::pow(Tj, 2) + 2.0 * aMax * Tj;
        double c = 2.0 * jMax * std::pow(Tj, 3) - pathLen;

        // 解二次方程求Ta
        Ta = (-b + std::sqrt(std::pow(b, 2) - 4.0 * a * c)) / (2.0 * a);
        vmax = jMax * std::pow(Tj, 2) + aMax * Ta;

        if(vmax > vMax) {
            // 3.1.1 速度超过限制的情况
            vmax = vMax;
            if(std::pow(aMax, 2) / jMax > vMax) {
                Tj = std::sqrt(vMax / jMax);
                aMax = jMax * Tj;
                Ta = 0.0;
                Tv = (pathLen - 2.0 * jMax * std::pow(Tj, 3)) / vMax;
            } else {
                Ta = (vMax - jMax * std::pow(Tj, 2)) / aMax;
                Tv = (pathLen - 2.0 * jMax * std::pow(Tj, 3) - aMax * std::pow(Ta, 2) -
                     (jMax * std::pow(Tj, 2) + 2.0 * aMax * Tj) * Ta) / vMax;
            }
        } else {
            Tv = 0.0;
        }
    } else {
        if(std::pow(amax, 2) / jMax > vMax) {
            // 3.2 速度限制主导的情况
            vmax = vMax;
            Tj = std::sqrt(vMax / jMax);
            amax = jMax * Tj;
            Ta = 0.0;
            Tv = (pathLen - 2.0 * jMax * std::pow(Tj, 3)) / vMax;
        } else {
            // 3.3 正常情况
            vmax = std::pow(amax, 2) / jMax;
            Ta = 0.0;
            Tv = 0.0;
        }
    }

    // 4. 计算各阶段速度和位移
    double V1 = jMax * std::pow(Tj, 2) / 2.0;
    double V2 = V1 + jMax * Tj * Ta;
    double V3 = V2 + jMax * std::pow(Tj, 2) / 2.0;
    double V4 = V3;
    double V5 = V4 - jMax * std::pow(Tj, 2) / 2.0;
    double V6 = V5 - jMax * Tj * Ta;

    double S1 = jMax * std::pow(Tj, 3) / 6.0;
    double S2 = S1 + V1 * Ta + jMax * Tj * std::pow(Ta, 2) / 2.0;
    double S3 = S2 + V2 * Tj + jMax * std::pow(Tj, 3) / 3.0;
    double S4 = S3 + V3 * Tv;
    double S5 = S4 + V4 * Tj - jMax * std::pow(Tj, 3) / 6.0;
    double S6 = S5 + V5 * Ta - jMax * Tj * std::pow(Ta, 2) / 2.0;

    // 5. 时间点计算
    double t = Tj + Ta + Tj + tAhead;

    double t1 = t;
    double t2 = t1 - Tj;
    double t3 = t1 - Tj - Ta;
    double t4 = t1 - 2.0 * Tj - Ta;
    double t5 = t1 - 2.0 * Tj - Ta - Tv;
    double t6 = t1 - 3.0 * Tj - Ta - Tv;
    double t7 = t1 - 3.0 * Tj - 2.0 * Ta - Tv;

    // 6. 分段计算半径
    double radius = 0.0;

    if(t <= Tj) {
        radius = 1.0/6.0 * jMax * std::pow(t1, 3);
    } else if(t < Tj + Ta) {
        radius = S1 + V1 * t2 + jMax * Tj * std::pow(t2, 2) / 2.0;
    } else if(t < 2.0 * Tj + Ta) {
        radius = S2 + V2 * t3 + jMax * Tj * std::pow(t3, 2) / 2.0 - jMax * std::pow(t3, 3) / 6.0;
    } else if(t < 2.0 * Tj + Ta + Tv) {
        radius = S3 + V3 * t4;
    } else if(t < 3.0 * Tj + Ta + Tv) {
        radius = S4 + V4 * t5 - jMax * std::pow(t5, 3) / 6.0;
    } else if(t < 3.0 * Tj + 2.0 * Ta + Tv) {
        radius = S5 + V5 * t6 - jMax * Tj * std::pow(t6, 2) / 2.0;
    } else if(t < 4.0 * Tj + 2.0 * Ta + Tv) {
        radius = S6 + V6 * t7 - jMax * Tj * std::pow(t7, 2) / 2.0 + jMax * std::pow(t7, 3) / 6.0;
    } else {
        radius = pathLen;
    }

    // 7. 约束处理
    if (radius > pathLen / 2.0) {
        radius = pathLen / 2.0;
    }
    // 8. 最终约束处理
    if (radius > 10.0) {
        radius = 10.0;
    }

    *r =  radius;
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

// 机械臂位姿逆解
LaserSdkResult kineInverse_tcpPose(RobotHandle robotHandle, double srcPose[ROB_AXIS_NUM], double refJointPose[ROB_AXIS_NUM], double tarJointPose[ROB_AXIS_NUM])
{
    TCPMsg tcpMsg;
    double flangePose[ROB_AXIS_NUM];

    CRresult result = cr_get_currentTCPmsg(robotHandle, &tcpMsg);
    if(result != CRresult::sucess)
    {
        return crsdk_return_value_transfer(result);
    }

    result = cr_TcpToFlangePose(srcPose, tcpMsg.tcpOffset, flangePose);
    if(result != CRresult::sucess)
    {
        return crsdk_return_value_transfer(result);
    }

    result = cr_kineInverse(robotHandle, flangePose, refJointPose, tarJointPose);
    if(result != CRresult::sucess)
    {
        return crsdk_return_value_transfer(result);
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}
