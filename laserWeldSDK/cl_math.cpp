#include "cl_math.h"
#include <sstream>
#include <iomanip>
#include"cl_gradualriseandfall.h"

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
