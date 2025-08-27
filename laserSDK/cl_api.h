#ifndef CL_API_H
#define CL_API_H

#include "cl_basedefine.h"
#include "robotapi.h"

#ifdef WIN32
#if defined(LASERSDK_LIBRARY)
#  define LASERSDK_EXPORT __declspec(dllexport)
#else
#  define LASERSDK_EXPORT __declspec(dllimport)
#endif
#else
    #ifdef  LASERSDK_LIBRARY
    #define LASERSDK_EXPORT
    #else
    #define LASERSDK_EXPORT
    #endif
#endif

#define LASERSDK_VERSION "1.0.0"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LaserSdkResult     获取当前焊接SDK版本号
 * @param version            焊接SDK版本号 传出参数
 * @param versionLen         焊接SDK版本号字符长度 传入时表示version参数申请的空间大小，传出时表示焊接SDK版本号长度
 * @return WELDSDK_EXPORT    处理结果
 */
LASERSDK_EXPORT LaserSdkResult cl_sdk_version_get(char* version, int* versionLen);

/**
 * @brief initMachineResource    注册焊机资源
 * @param laserMachineResource   焊机资源结构体
 * @param laserMachineHandle     焊机资源句柄 传出参数
 * @return LaserSdkResult        处理结果
 */
LASERSDK_EXPORT LaserSdkResult cl_machine_resource_init(const MachineResource* laserMachineResource, LaserMachineHandle* laserMachineHandle);

/**
 * @brief updateMachineResource  更新焊机资源
 * @param laserMachineHandle     焊机资源句柄
 * @param laserMachineResource   焊机资源结构体
 * @return LaserSdkResult        处理结果
 */
LASERSDK_EXPORT LaserSdkResult cw_machine_resource_update(const LaserMachineHandle laserMachineHandle, const MachineResource* laserMachineResource);

/**
 * @brief getMachineResource     获取焊机资源句柄对应的数据
 * @param laserMachineHandle     焊机资源句柄
 * @param laserMachineResource   焊机资源结构体 传出参数
 * @return LaserSdkResult        处理结果
 */
LASERSDK_EXPORT LaserSdkResult cw_machine_resource_get(const LaserMachineHandle laserMachineHandle, MachineResource* laserMachineResource);

/**
 * @brief cw_machine_resource_release    释放焊机资源
 * @param laserMachineHandle             焊机资源句柄
 * @return LaserSdkResult                处理结果
 */
LASERSDK_EXPORT LaserSdkResult cw_machine_resource_release(const LaserMachineHandle laserMachineHandle);


#ifdef __cplusplus
}
#endif

#endif // CL_API_H

