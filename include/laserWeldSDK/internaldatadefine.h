#ifndef CLINTERNALDATADEFINE_H
#define CLINTERNALDATADEFINE_H

#include "cl_basedefine.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <map>

#define M_PI 3.14159265358979323846
#define EPSILON  1e-9
#define ZeroThreshold 1e-5
#define MOVEPS_SEGMENTATION_LENGTH 200
#define TIME_STEP 100  //电流电压时间渐变步长

extern std::mt19937 engine;
extern std::uniform_int_distribution<int> distribution;
extern std::unordered_map<int, MachineResource> machineResourcesMap;
// 写脚本参数-函数名-函数传参名-地址
struct ParamConfig {
    const char* scriptName;
    const char* numName;
    unsigned int addr;
    double value;
};

//变量类型
enum CommuRegType
{
    int16Reg = 1,
    float10Reg = 2,
    float100Reg = 3
};

typedef struct
{
    double x;
    double y;
    double z;
}Pos;

typedef union
{
    Pos position;
    double posVec[3];
}cartesianPos_U;

typedef struct
{
    double w;
    double x;
    double y;
    double z;
}Ori;

typedef union
{
    Ori orientation;
    double quatVec[4];
}cartesianOri_U;

typedef struct
{
    cartesianPos_U posU;
    cartesianOri_U oriU;
}Pose_S;

typedef struct
{
    double pos[3]; // 位置
    double rot[3]; // 姿态
    double coordSys[12]; // 参考坐标系
}Waypoint_S;

//对指令列表通过各类数据结构进行抽象表示
struct InstListData
{
    std::unordered_map<int, std::pair<InstTypeLaserSdk, int> > listLocToTypeID; ///< 指令列表 指令类型 到 类型ID 的映射
    std::unordered_set<int> IDSet;                           ///< 指令列表中所有类型ID的集合
    std::unordered_map<int, CraftInfoLaserSdk> craftsMap;    ///< 工艺ID 到 工艺信息 的映射
    std::unordered_map<int, PointDataLaserSdk> pointsMap;    ///< 点ID 到 路点数据 的映射
    //std::unordered_map<int, PitchAngleData> pitchAnglesMap; ///<焊接俯仰角°角度信息
    //std::unordered_map<int, CirclePitchAngleData> circlePitchAnglesMap; ///<圆弧俯仰角角度信息
    //std::unordered_map<int, std::vector<int> >moreLayerRoadCraftIDToRoadCraftsID; ///<多层道工艺ID到道工艺ID的映射
    std::vector<int> craftInstListCraftIDVec;   ///<道工艺实例列表
    std::unordered_map<int, std::pair<int, int> > craftInstListStartEndLoc;  ///< 道工艺实例列表起始结束位置
};


enum RoadCraftType
{
    NO_ROAD_CRAFT = 0,      // 非多层多道的道工艺
    START_ROAD_CRAFT = 1,   // 起点道工艺
    END_ROAD_CRAFT = 2,     // 终点道工艺
    MID_ROAD_CRAFT = 3      // 中间道工艺
};

// //////////////////////// HaoYun ////////////////////////
// S型曲线参数
 struct SegmentTimes
 {
    double Tj1;
    double Ta;
    double Tv;
    double Tj2;
    double Td;
};

// 间断焊中，内部存储焊接分段的结构体，直线和圆弧共用(trans_point,center,normal,radius仅在圆弧段才使用)
struct IntervalWeldSegment
{
    double start_point[6];
    double trans_point[6];
    double end_point[6];
    double center[3];
    double normal[3];
    double radius;
    double length;
    int isline;
};
// //////////////////////// HaoYun ////////////////////////
//指令列表转换过程中的环境数据
struct InstListTransEnvironmentData
{
    int craftID;                  // 工艺ID
    int prePointLoc;              // 前一个点的位置    
    bool isWeld;                  // 是否焊接 false->轨迹模拟 true->开始焊接
    bool isWelding;               // 是否正在焊接
    RoadCraftType roadCraftFlag;  // 道工艺类型
};

#endif // CLINTERNALDATADEFINE_H
