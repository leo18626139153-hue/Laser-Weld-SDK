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
 * @brief cw_machine_resource_release    设置激光头参数，测试用
 * @param laserMachineHandle             焊机资源句柄
 * script
 * scriptLen
 * @return LaserSdkResult                处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_laser_head_params_set(const LaserMachineHandle laserMachineHandle, char* script, int scriptLen);

/**
 * @brief cw_machine_resource_release    读激光头参数，测试用
 * @param laserMachineHandle             焊机资源句柄
 * script
 * scriptLen
 * @return LaserSdkResult                处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_laser_head_params_read(const LaserMachineHandle laserMachineHandle, char* script, int scriptLen);

#ifdef __cplusplus
}
#endif

#endif // CL_API_H

