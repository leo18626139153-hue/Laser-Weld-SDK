#include "cl_api.h"
#include "cl_utils.h"
#include "cl_craftscript.h"
#include <functional>
#include <thread>
#include <chrono>
#include <map>


LaserSdkResult cl_sdk_version_get(char *version, int *versionLen)
{
    if(*versionLen < sizeof(LASERSDK_VERSION)+1)
    {
        return LaserSdkResult::MEMORY_ERROR_LASER_SDK;
    }
    memcpy(version, LASERSDK_VERSION, sizeof(LASERSDK_VERSION));
    memcpy(version+sizeof(LASERSDK_VERSION), "\0", 1);
    *versionLen = sizeof(LASERSDK_VERSION)+1;
    return LaserSdkResult::SUCCESS_LASER_SDK;
}


// 焊机资源管理函数
LaserSdkResult cl_machine_resource_init(const MachineResource* laserMachineResource, LaserMachineHandle* laserMachineHandle)
{
    // 设置随机数种子
    std::random_device rd;
    engine.seed(rd());
    int tempHandle = distribution(engine);
    //判断是否句柄重复
    while(machineResourcesMap.count(tempHandle) != 0)
    {
        tempHandle = distribution(engine);
    }
    *laserMachineHandle = tempHandle;
    machineResourcesMap[*laserMachineHandle] = *laserMachineResource;

    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//获取焊机资源句柄对应的数据
LaserSdkResult cw_machine_resource_get(const LaserMachineHandle laserMachineHandle, MachineResource *laserMachineResource)
{
    //判断是否存在该句柄
    if(machineResourcesMap.count(laserMachineHandle) == 0)
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    machineResourcesMap[laserMachineHandle] = *laserMachineResource;
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//释放资源
LaserSdkResult cw_machine_resource_release(const LaserMachineHandle laserMachineHandle)
{
    //判断是否存在该句柄
    if(machineResourcesMap.count(laserMachineHandle) == 0)
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    machineResourcesMap.erase(laserMachineHandle);
    return LaserSdkResult::SUCCESS_LASER_SDK;
}
