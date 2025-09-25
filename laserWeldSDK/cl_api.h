#ifndef CL_API_H
#define CL_API_H

#include "cl_basedefine.h"
#include "robotapi.h"

#ifdef WIN32
#if defined(LASERWELDSDK_LIBRARY)
#  define LASERWELDSDK_EXPORT __declspec(dllexport)
#else
#  define LASERWELDSDK_EXPORT __declspec(dllimport)
#endif
#else
#ifdef  LASERWELDSDK_LIBRARY
#define LASERWELDSDK_EXPORT
#else
#define LASERWELDSDK_EXPORT
#endif
#endif

#define LASERWELDSDK_VERSION "1.0.0"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LaserSdkResult     获取当前焊接SDK版本号
 * @param version            焊接SDK版本号 传出参数
 * @param versionLen         焊接SDK版本号字符长度 传入时表示version参数申请的空间大小，传出时表示焊接SDK版本号长度
 * @return WELDSDK_EXPORT    处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_sdk_version_get(char* version, int* versionLen);

/**
 * @brief initMachineResource    注册焊机资源
 * @param laserMachineResource   焊机资源结构体
 * @param laserMachineHandle     焊机资源句柄 传出参数
 * @return LaserSdkResult        处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_machine_resource_init(const MachineResource* laserMachineResource, LaserMachineHandle* laserMachineHandle);

/**
 * @brief updateMachineResource  更新焊机资源
 * @param laserMachineHandle     焊机资源句柄
 * @param laserMachineResource   焊机资源结构体
 * @return LaserSdkResult        处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_machine_resource_update(const LaserMachineHandle laserMachineHandle, const MachineResource* laserMachineResource);

/**
 * @brief getMachineResource     获取焊机资源句柄对应的数据
 * @param laserMachineHandle     焊机资源句柄
 * @param laserMachineResource   焊机资源结构体 传出参数
 * @return LaserSdkResult        处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_machine_resource_get(const LaserMachineHandle laserMachineHandle, MachineResource* laserMachineResource);

/**
 * @brief cw_machine_resource_release    释放焊机资源
 * @param laserMachineHandle             焊机资源句柄
 * @return LaserSdkResult                处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_machine_resource_release(const LaserMachineHandle laserMachineHandle);

/**
 * @brief cl_get_arc_start_script 获取起弧脚本
 * @param laserMachineHandle 焊机资源句柄
 * @param scripts           起弧LUA脚本 传出参数
 * @param scriptLen         脚本长度 传入时为scripts参数申请的空间大小，传出时为生成的起弧LUA脚本大小
 * @return WeldSdkResult    处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_get_arc_start_script(const LaserMachineHandle laserMachineHandle, char* scripts, int* scriptLen);

/**
 * @brief cl_get_arc_start_script 获取收弧脚本脚本
 * @param laserMachineHandle 焊机资源句柄
 * @param scripts           起弧LUA脚本 传出参数
 * @param scriptLen         脚本长度 传入时为scripts参数申请的空间大小，传出时为生成的起弧LUA脚本大小
 * @return WeldSdkResult    处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_get_arc_stop_script(const LaserMachineHandle laserMachineHandle, char* scripts, int* scriptLen);

/**
 * @brief cw_machine_resource_release    获取设置焊接工艺参数脚本 焊接过程中设置焊接参数
 * @param laserMachineHandle             焊机资源句柄
 * @param script                         ArcSet脚本 传出参数
 * @param scriptLen                      脚本长度 传入时为script参数申请的空间大小，传出时为生成的ArcSet脚本大小
 * @return LaserSdkResult                处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_laser_current_craft_set_read(const LaserMachineHandle laserMachineHandle, char* script, int scriptLen);

/**
 * @brief cl_craft_weld_status_init      初始化所有焊接相关状态
 * @param laserMachineHandle     焊机资源句柄
 * @return LaserSdkResult        处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_craft_weld_status_init(const LaserMachineHandle laserMachineHandle);

/**
 * @brief getStartWeldScript    获取指令列表转换后脚本
 * @param laserMachineHandle    焊机资源句柄
 * @param weldStartData         指令列表数据结构体
 * @param isWeld                是否进行焊接 false-轨迹模拟 true-开始焊接
 * @param scripts               指令列表LUA脚本 传出参数
 * @param scriptLen             脚本长度 传入时为scripts参数申请的空间大小，传出时为生成的指令列表LUA脚本大小
 * @return WeldSdkResult        处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_craft_inst_list_script_get(const LaserMachineHandle laserMachineHandle, LaserWeldStartData weldStartData, bool isWeld, char* scripts, int* scriptLen);

/**
 * @brief getLapRollbackCirclePoint     获取直线轨迹——根据终点和降功率点获取新的起点和终点
 * @param rollbackStraightLineData      直线回退参数
 * @return WeldSdkResult                处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_craft_straight_line_back_point_cal(RollbackStraightLineData* rollbackStraightLineData);


/**
 * @brief getLapRollbackCirclePoint     获取圆弧轨迹——根据终点和降功率点距离获取新的起点和过渡点、终点
 * @param rollbackCircleData            圆弧回退参数
 * @return WeldSdkResult                处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_craft_circle_back_point_cal(RollbackCircleData* rollbackCircleData);


/**
 * @brief getLapRollbackCirclePoint     缓升缓降起弧
 * @param laserMachineHandle            激光器句柄
 * @param scripts                       起弧LUA脚本 传出参数
 * @param scriptLen                     脚本长度 传入时为scripts参数申请的空间大小，传出时为生成的起弧LUA脚本大小
 * @return WeldSdkResult                处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_get_arc_start_slow_script(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen);

/**
 * @brief getLapRollbackCirclePoint     缓升缓降收弧
 * @param laserMachineHandle            激光器句柄
 * @param scripts                       起弧LUA脚本 传出参数
 * @param scriptLen                     脚本长度 传入时为scripts参数申请的空间大小，传出时为生成的起弧LUA脚本大小
 * @return WeldSdkResult                处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_get_arc_stop_slow_script(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen);

#ifdef __cplusplus
}
#endif

#endif // CL_API_H

