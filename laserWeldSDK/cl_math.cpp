#include "cl_math.h"
#include <sstream>
#include <iomanip>
#include "cl_utils.h"

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

void normalized_vector_HY(const double *vec, double *result)
{
    double norm = norm_vec_HY(vec);

    if (norm < ZeroThreshold_HY)
    {
        for (int i = 0; i < 3; i++)
        {
            result[i] = 0;
        }
    }
    else {
        for (int i = 0; i < 3; i++)
        {
            result[i] = vec[i] / norm;
        }
    }
}

void matrix_vector_multiply_HY(const double *matrix, const double *vec, double *result)
{
    for (int i = 0; i < 3; i++)
    {
        result[i] = 0;
        for (int j = 0; j < 3; j++)
        {
            result[i] += matrix[i*3+j] * vec[j];
        }
    }
}

// 向量叉乘
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

void copy_pose_HY(const double *oldpose, double *pose)
{
    for(int k=0; k<6; k++)
        pose[k] = oldpose[k];
}

void pose_trans_HY(const double* pose, const double* delat_pose, double* new_pose)
{
    double axis[3];
    double angle = norm_vec_HY(&pose[3]);
    normalized_vector_HY(&pose[3], axis);

    double c = cos(angle);
    double s = sin(angle);
    double t = 1 - c;
    double ux = axis[0];
    double uy = axis[1];
    double uz = axis[2];
    double R[9];
    double dp[3];

    R[0] = t * ux * ux + c;
    R[1] = t * ux * uy - s * uz;
    R[2] = t * ux * uz + s * uy;
    R[3] = t * ux * uy + s * uz;
    R[4] = t * uy * uy + c;
    R[5] = t * uy * uz - s * ux;
    R[6] = t * ux * uz - s * uy;
    R[7] = t * uy * uz + s * ux;
    R[8] = t * uz * uz + c;

    matrix_vector_multiply_HY(R, delat_pose, dp);

    for (int i = 0; i < 3; i++) {
        new_pose[i] = pose[i] + dp[i];
        new_pose[i+3] = pose[i+3];
    }
}

LaserSdkResult getStraightLineSlowUpPoint(int robothandle, double p1[6], double p2[6], double backDistance, double refJoint[6], double pointTrans[6], double pointJointTrans[6])
{
    // 向量 AB = B - A
    double vx = p2[0] - p1[0];
    double vy = p2[1] - p1[1];
    double vz = p2[2] - p1[2];

    double distance = sqrt(vx*vx + vy*vy + vz*vz);
    if (distance < 1e-6 || distance < backDistance) {
        return LaserSdkResult::BACK_DISTANCE_LIMIT_LASER_SDK;
    }

    double temp[3] = {vx,vy,vz};
    // 归一化向量
    normalizeVector(temp);

    // P = 终点 - dist * u
    double px = p2[0] - backDistance * temp[0];
    double py = p2[1] - backDistance * temp[1];
    double pz = p2[2] - backDistance * temp[2];

    pointTrans[0] = px;
    pointTrans[1] = py;
    pointTrans[2] = pz;
    pointTrans[3] = p2[3];
    pointTrans[4] = p2[4];
    pointTrans[5] = p2[5];
    // std::cout << "point trans axis: " << px << ", " << py << ", " << pz << std::endl;
    LaserSdkResult res;
    res = kineInverse_tcpPose(robothandle, pointTrans, refJoint, pointJointTrans);
    if (res != LaserSdkResult::SUCCESS_LASER_SDK){
        return res;
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//圆弧收弧计算回退点为起点的三个示教点
LaserSdkResult getCirclrSlowDownPoints(int robothandle, double p1[6], double p2[6], double p3[6], double stopP[6], double backDistance, double refJoint[6], double stopMapP[3], double newTransPoint[6], double newTransJointPoint[6], double restartP[6], double midP[6], double endP[6], double restartPJoint[6], double midPJoint[6], double endPJoint[6])
{
    double centerPoint[3], axisVec[3], radius;
    int result = circle3dCalc(p1, p2, p3, &radius, centerPoint, axisVec);
    if (result != 1)
    {
        std::cout << "空间圆计算失败" << std::endl;
        return LaserSdkResult::ARC_CALCULATE_FAILED_LASER_SDK;
    }
    // 计算向量
    double vec1[3] = {p1[0] - centerPoint[0], p1[1] - centerPoint[1], p1[2] - centerPoint[2]};
    double vec2[3] = {p2[0] - centerPoint[0], p2[1] - centerPoint[1], p2[2] - centerPoint[2]};
    double vec3[3] = {p3[0] - centerPoint[0], p3[1] - centerPoint[1], p3[2] - centerPoint[2]};

    // 计算夹角
    double angle1 = calculateAngle(vec1, vec2);
    double angle2 = calculateAngle(vec2, vec3);
    double totalAngle = angle1 + angle2;
    double distance = radius * totalAngle;
    if(distance < backDistance){
        return LaserSdkResult::ARC_CALCULATE_FAILED_LASER_SDK;
    }

    getMapPoint(centerPoint, axisVec, radius, stopP, stopMapP);
    double tepP[3];
    calculateP2Point(centerPoint, axisVec, radius, stopMapP, backDistance, tepP);
    restartP[0] = tepP[0];
    restartP[1] = tepP[1];
    restartP[2] = tepP[2];
    restartP[3] = stopP[3];
    restartP[4] = stopP[4];
    restartP[5] = stopP[5];

    endP[0] = p3[0];
    endP[1] = p3[1];
    endP[2] = p3[2];
    endP[3] = p3[3];
    endP[4] = p3[4];
    endP[5] = p3[5];
    double PresPendVec[3] = {endP[0] - restartP[0],endP[1] - restartP[1],endP[2] - restartP[2]};
    double MidP[3];
    vecCross(PresPendVec, axisVec, MidP);
    double moerMidP = sqrt(pow(MidP[0], 2) + pow(MidP[1], 2) + pow(MidP[2], 2));
    midP[0] = MidP[0] / moerMidP * radius + centerPoint[0];
    midP[1] = MidP[1] / moerMidP * radius + centerPoint[1];
    midP[2] = MidP[2] / moerMidP * radius + centerPoint[2];
    midP[3] = stopP[3];
    midP[4] = stopP[4];
    midP[5] = stopP[5];

    double PresPendVec2[3] = {restartP[0] - p1[0],restartP[1] - p1[1],restartP[2] - p1[2]};
    double MidP2[3];
    vecCross(PresPendVec2, axisVec, MidP2);
    double moerMidP2 = sqrt(pow(MidP2[0], 2) + pow(MidP2[1], 2) + pow(MidP2[2], 2));
    newTransPoint[0] = MidP2[0] / moerMidP2 * radius + centerPoint[0];
    newTransPoint[1] = MidP2[1] / moerMidP2 * radius + centerPoint[1];
    newTransPoint[2] = MidP2[2] / moerMidP2 * radius + centerPoint[2];
    newTransPoint[3] = p2[3];
    newTransPoint[4] = p2[4];
    newTransPoint[5] = p2[5];

    LaserSdkResult res;

    res = kineInverse_tcpPose(robothandle, newTransPoint, refJoint, newTransJointPoint);
    if (res != LaserSdkResult::SUCCESS_LASER_SDK){
        return res;
    }
    res = kineInverse_tcpPose(robothandle, restartP, refJoint, restartPJoint);
    if (res != LaserSdkResult::SUCCESS_LASER_SDK){
        return res;
    }
    res = kineInverse_tcpPose(robothandle, midP, refJoint, midPJoint);
    if (res != LaserSdkResult::SUCCESS_LASER_SDK){
        return res;
    }
    res = kineInverse_tcpPose(robothandle, endP, refJoint, endPJoint);
    if (res != LaserSdkResult::SUCCESS_LASER_SDK){
        return res;
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//圆形焊缝映射点
void getMapPoint(double centerP[3], double Fn[3], double radius, double stopPoint[3], double mapPoint[3])
{
    // 归一化Fn
    normalizeVector(Fn);

    // 计算stopPoint到centerP的向量
    double stopVec[3] = { stopPoint[0] - centerP[0], stopPoint[1] - centerP[1], stopPoint[2] - centerP[2] };

    // 计算 Fn 向量的长度平方
    double FnDotFn = Fn[0] * Fn[0] + Fn[1] * Fn[1] + Fn[2] * Fn[2];

    // 计算 stopVec 与 Fn 的点积
    double stopDotFn = stopVec[0] * Fn[0] + stopVec[1] * Fn[1] + stopVec[2] * Fn[2];

    // 计算 stopVec 到平面的投影长度
    double projectionLength = stopDotFn / FnDotFn;

    // 计算投影点
    mapPoint[0] = stopPoint[0] - projectionLength * Fn[0];
    mapPoint[1] = stopPoint[1] - projectionLength * Fn[1];
    mapPoint[2] = stopPoint[2] - projectionLength * Fn[2];

    // 计算投影点到圆心的距离
    double distance = sqrt((mapPoint[0] - centerP[0]) * (mapPoint[0] - centerP[0]) +
                           (mapPoint[1] - centerP[1]) * (mapPoint[1] - centerP[1]) +
                           (mapPoint[2] - centerP[2]) * (mapPoint[2] - centerP[2]));

    // 计算比例因子来调整 mapPoint 使其落在圆上
    double factor = radius / distance;

    // 将投影点调整到圆上
    mapPoint[0] = centerP[0] + factor * (mapPoint[0] - centerP[0]);
    mapPoint[1] = centerP[1] + factor * (mapPoint[1] - centerP[1]);
    mapPoint[2] = centerP[2] + factor * (mapPoint[2] - centerP[2]);
}


// 计算圆的半径、中心点和法向量
int circle3dCalc(double point1[6], double point2[6], double point3[6], double* radius, double centerPoint[3], double axisVec[3])
{
    double x1 = point1[0], y1 = point1[1], z1 = point1[2];
    double x2 = point2[0], y2 = point2[1], z2 = point2[2];
    double x3 = point3[0], y3 = point3[1], z3 = point3[2];

    // 计算系数 a1, b1, c1, d1
    double a1 = (y1 * z2 - y2 * z1 - y1 * z3 + y3 * z1 + y2 * z3 - y3 * z2);
    double b1 = -(x1 * z2 - x2 * z1 - x1 * z3 + x3 * z1 + x2 * z3 - x3 * z2);
    double c1 = (x1 * y2 - x2 * y1 - x1 * y3 + x3 * y1 + x2 * y3 - x3 * y2);
    double d1 = -(x1 * y2 * z3 - x1 * y3 * z2 - x2 * y1 * z3 + x2 * y3 * z1 + x3 * y1 * z2 - x3 * y2 * z1);

    // 计算系数 a2, b2, c2, d2
    double a2 = 2 * (x2 - x1);
    double b2 = 2 * (y2 - y1);
    double c2 = 2 * (z2 - z1);
    double d2 = x1 * x1 + y1 * y1 + z1 * z1 - x2 * x2 - y2 * y2 - z2 * z2;

    // 计算系数 a3, b3, c3, d3
    double a3 = 2 * (x3 - x1);
    double b3 = 2 * (y3 - y1);
    double c3 = 2 * (z3 - z1);
    double d3 = x1 * x1 + y1 * y1 + z1 * z1 - x3 * x3 - y3 * y3 - z3 * z3;

    // 计算行列式 xyzDev
    double xyzDev = a1 * b2 * c3 - a1 * b3 * c2 - a2 * b1 * c3 + a2 * b3 * c1 + a3 * b1 * c2 - a3 * b2 * c1;

    // 如果行列式接近零，无法求解
    if (fabs(xyzDev) < 1e-6)
    {
        printf("Circle3dCalc Fail!\n");
        return 0;  // 失败
    }

    // 计算圆心坐标
    centerPoint[0] = -(b1 * c2 * d3 - b1 * c3 * d2 - b2 * c1 * d3 + b2 * c3 * d1 + b3 * c1 * d2 - b3 * c2 * d1) / xyzDev;
    centerPoint[1] = (a1 * c2 * d3 - a1 * c3 * d2 - a2 * c1 * d3 + a2 * c3 * d1 + a3 * c1 * d2 - a3 * c2 * d1) / xyzDev;
    centerPoint[2] = -(a1 * b2 * d3 - a1 * b3 * d2 - a2 * b1 * d3 + a2 * b3 * d1 + a3 * b1 * d2 - a3 * b2 * d1) / xyzDev;

    // 计算半径
    *radius = sqrt(pow(x1 - centerPoint[0], 2) + pow(y1 - centerPoint[1], 2) + pow(z1 - centerPoint[2], 2));

    // 计算法向量 (a1, b1, c1) 的单位向量
    double m = sqrt(pow(a1, 2) + pow(b1, 2) + pow(c1, 2));
    axisVec[0] = a1 / m;
    axisVec[1] = b1 / m;
    axisVec[2] = c1 / m;

    return 1;  // 成功
}


//圆形回退点
void calculateP2Point(
    const double O[3],    // 圆心坐标 [x,y,z]
    const double Fn[3],   // 法向量 [x,y,z]
    double r,             // 圆半径
    const double P[3],    // 已知点P坐标 [x,y,z]
    double c,             // 圆弧长度
    double P2[3]          // 输出结果坐标 [x,y,z]
)
{
    // 1. 计算向量 OP = P - O
    double OP[3] = { P[0] - O[0], P[1] - O[1], P[2] - O[2] };

    // 2. 归一化法向量
    double k[3];
    double fn_norm = std::sqrt(Fn[0] * Fn[0] + Fn[1] * Fn[1] + Fn[2] * Fn[2]);
    k[0] = Fn[0] / fn_norm;
    k[1] = Fn[1] / fn_norm;
    k[2] = Fn[2] / fn_norm;

    // 3. 计算旋转角度 (弧度制)
    double theta = -c / r;

    // 4. 计算叉乘 k × OP
    double cross[3] =
    {
        k[1] * OP[2] - k[2] * OP[1],
        k[2] * OP[0] - k[0] * OP[2],
        k[0] * OP[1] - k[1] * OP[0]
    };

    // 5. 应用旋转公式计算新向量
    double cos_theta = std::cos(theta);
    double sin_theta = std::sin(theta);
    double OP2[3] =
    {
        OP[0] * cos_theta + cross[0] * sin_theta,
        OP[1] * cos_theta + cross[1] * sin_theta,
        OP[2] * cos_theta + cross[2] * sin_theta
    };

    // 6. 计算最终坐标 P2 = O + OP2
    P2[0] = O[0] + OP2[0];
    P2[1] = O[1] + OP2[1];
    P2[2] = O[2] + OP2[2];
}


//圆形回退点
double calculateAngle(const double v1[3], const double v2[3])
{
    double dot = v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
        double mag1 = sqrt(v1[0]*v1[0] + v1[1]*v1[1] + v1[2]*v1[2]);
        double mag2 = sqrt(v2[0]*v2[0] + v2[1]*v2[1] + v2[2]*v2[2]);

        double cosTheta = dot / (mag1 * mag2);
        // 处理浮点精度问题
        cosTheta = std::max(-1.0, std::min(1.0, cosTheta));
        return acos(cosTheta);
}

