#include "cl_gradualriseandfall.h"

LaserSdkResult getStraightLineSlowUpPoint(double p1[6], double p2[6], double backDistance, double pointTrans[6])
{
    // 向量 AB = B - A
    double vx = p2[0] - p1[0];
    double vy = p2[1] - p1[1];
    double vz = p2[2] - p1[2];
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
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//圆弧收弧计算回退点为起点的三个示教点
LaserSdkResult getCirclrSlowDownPoints(double p1[6], double p2[6], double p3[6], double stopP[6], double backDistance, double stopMapP[3], double restartP[6], double midP[6], double endP[6])
{
    double centerPoint[3], axisVec[3], radius;
    int result = circle3dCalc(p1, p2, p3, &radius, centerPoint, axisVec);
    if (result != 1)
    {
        std::cout << "空间圆计算失败" << std::endl;
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
    midP[0] = MidP[0] / moerMidP*radius + centerPoint[0];
    midP[1] = MidP[1] / moerMidP * radius + centerPoint[1];
    midP[2] = MidP[2] / moerMidP * radius + centerPoint[2];
    midP[3] = stopP[3];
    midP[4] = stopP[4];
    midP[5] = stopP[5];
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
