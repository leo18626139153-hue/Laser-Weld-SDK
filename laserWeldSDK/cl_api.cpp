#include "cl_api.h"
#include "cl_utils.h"
#include "cl_craftscript.h"
#include "cl_luascript.h"
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
LaserSdkResult cl_machine_resource_get(const LaserMachineHandle laserMachineHandle, MachineResource *laserMachineResource)
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
LaserSdkResult cl_machine_resource_release(const LaserMachineHandle laserMachineHandle)
{
    //判断是否存在该句柄
    if(machineResourcesMap.count(laserMachineHandle) == 0)
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    machineResourcesMap.erase(laserMachineHandle);
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

LaserSdkResult cl_laser_head_params_read(const LaserMachineHandle laserMachineHandle, char *script, int scriptLen)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    CommuBase* curCommuFunction = nullptr;
    if (machineResource.commuType == CommuTypeLaserSdk::RS485_2_LASER_SDK)
    {
        curCommuFunction = static_cast<CommuRS485*>(CommuRS485::GetInstance());
    }
    else
    {
        return LaserSdkResult::COMMU_ERROR_LASER_SDK;
    }

    //获取脚本
    std::string scriptGet;
    LaserSdkResult result = curCommuFunction->craft_laser_head_param_read_script_get(laserMachineHandle, scriptGet);
    if(result != LaserSdkResult::SUCCESS_LASER_SDK)
    {
        return result;
    }

    //脚本赋值
    if(strToChar(script, &scriptLen, scriptGet)){
        return LaserSdkResult::MEMORY_ERROR_LASER_SDK;
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//起弧！！！
LaserSdkResult cl_get_arc_start_script(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    CommuBase* curCommuFunction = nullptr;
    if (machineResource.commuType == CommuTypeLaserSdk::RS485_2_LASER_SDK)
    {
        curCommuFunction = static_cast<CommuRS485*>(CommuRS485::GetInstance());
        if (curCommuFunction == nullptr) {
            return LaserSdkResult::COMMU_ERROR_LASER_SDK;
        }
    }
    else
    {
        return LaserSdkResult::COMMU_ERROR_LASER_SDK;
    }
    //需要设置的激光器地址参数 - 激光器功率 - 开光功率 - 开光渐进时间 -
    std::vector<unsigned int> laserPowerVec = {
        machineResource.laserPowerLoc,
        machineResource.laserOnPowerLoc,
        machineResource.laserOnRampTimeLoc,
        };
    //需要设置的激光头地址参数 - 扫描速度 - 扫描宽 - 峰值功率 - 占空比 - 脉冲频率 - 扫描高度 - 旋转角度 - 图形切换
    std::vector<unsigned int> laserHeadVec = {
        machineResource.laserHeadScanSpeedLoc,
        machineResource.laserHeadScanWidthLoc,
        machineResource.laserHeadPeakPowerLoc,
        machineResource.laserHeadDutyCycleLoc,
        machineResource.laserHeadFrequencyLowLoc,
        machineResource.laserHeadFrequencyHighLoc,
        machineResource.laserHeadScanHeightLoc,
        machineResource.laserHeadRotationAngleLoc,
        machineResource.laserHeadPatternIdLoc
        };
    //送丝机地址参数 - 自动送丝速度
    std::vector<unsigned int> wireFeedVec = {
        machineResource.autoWireFeedSpeedLoc,
    };
    //获取脚本
    std::string scriptGet;
    LaserSdkResult result;
    result = curCommuFunction->craft_laser_param_set_script_get(laserMachineHandle, laserPowerVec,  scriptGet);    //激光器参数
    result = curCommuFunction->craft_laser_head_param_set_script_get(laserMachineHandle, laserHeadVec, scriptGet); //激光图案
    //result = curCommuFunction->craft_wire_feeder_param_set_script_get(laserMachineHandle, wireFeedVec, scriptGet); //送丝机参数
    scriptGet = mainFuncProgram + RobotProgram1 + scriptGet  +RobotProgram2;
    std::string laserScript = curCommuFunction->loadLuaFunctionScriptOut();  //功能函数
    scriptGet = laserScript + scriptGet ;
    if(result != LaserSdkResult::SUCCESS_LASER_SDK)
    {
        return result;
    }
    //脚本赋值
    if(strToChar(scripts, scriptLen, scriptGet)){
        return LaserSdkResult::MEMORY_ERROR_LASER_SDK;
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}
