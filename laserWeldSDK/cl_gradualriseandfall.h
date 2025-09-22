#ifndef CL_CRAFTCAL_H
#define CL_CRAFTCAL_H
#include "cl_math.h"
#include<vector>


/**
 * @brief getNewTeachPoints    整体圆形轨迹——根据停止点和回退距离获取新的起点和过渡点、终点
 * @param p1                   示教起点
 * @param p2                   示教终点
 * @param backDistance         回退距离
 * @param pointTrans             降功率点
 * @return WeldSdkResult       处理结果
 */
LaserSdkResult getStraightLineSlowUpPoint(double p1[6], double p2[6], double backDistance, double pointTrans[6]);

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
LaserSdkResult getCirclrSlowDownPoints(double p1[6], double p2[6], double p3[6], double stopP[6], double backDistance, double stopMapP[3], double restartP[6], double midP[6], double endP[6]);


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

#endif // CL_CRAFTCAL_H
