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
    if(*versionLen < sizeof(LASERWELDSDK_VERSION)+1)
    {
        return LaserSdkResult::MEMORY_ERROR_LASER_SDK;
    }
    memcpy(version, LASERWELDSDK_VERSION, sizeof(LASERWELDSDK_VERSION));
    memcpy(version+sizeof(LASERWELDSDK_VERSION), "\0", 1);
    *versionLen = sizeof(LASERWELDSDK_VERSION)+1;
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

LaserSdkResult cl_machine_resource_update(const LaserMachineHandle laserMachineHandle, const MachineResource *laserMachineResource)
{
    //判断是否存在该句柄
    if(machineResourcesMap.count(laserMachineHandle) == 0)
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    machineResourcesMap[laserMachineHandle] = *laserMachineResource;
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
    *laserMachineResource = machineResourcesMap[laserMachineHandle];
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
        return result;    }

    //脚本赋值
    std::string funcScript = curCommuFunction->loadLuaFunctionScriptOut();  //功能函数
    std::string readRobotProgram1 = "function readRobotProgram()\n";
    std::string readRobotProgram2 = "end\n"
                                "-----RobotProgramEnd----\n"
                                "RobotProgram_Result=task_create(readRobotProgram)\n";
    scriptGet = readRobotProgram1 + scriptGet  +readRobotProgram1;
    scriptGet = funcScript + scriptGet ;
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
        machineResource.laserPowerLoc,              ///<  激光器功率地址 0x0230 560
        machineResource.laserOnPowerLoc,            ///<  开光功率地址 0x0231 561
        machineResource.laserOnRampTimeLoc,         ///<  开光渐进时间地址 0x0232 562
        machineResource.laserHeadYOffsetLoc,        ///<  Y轴偏移地址 0x0239 569
        machineResource.laserHeadXOffsetLoc,        ///<  X轴偏移地址 0x023b 571
        machineResource.laserHeadYScaleLoc,         ///<  Y轴放大地址 0x0238 568
        machineResource.laserHeadXScaleLoc,         ///<  X轴放大地址 0x023a 570
        machineResource.spotWeldTypeLoc             ///< 点焊类型地址 0x0242 578
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
        machineResource.autoWireFeedSpeedLoc, //256
    };
    //获取脚本
    std::string scriptGet;

    try
    {
        //提前送气
        checkLaserSdkResult(curCommuFunction->craft_wire_feeder_param_set_script_get(laserMachineHandle, wireFeedVec, scriptGet));
        //激光器
        checkLaserSdkResult(curCommuFunction->craft_laser_param_set_script_get(laserMachineHandle, laserPowerVec, scriptGet));
        //激光图案
        checkLaserSdkResult(curCommuFunction->craft_laser_head_param_set_script_get(laserMachineHandle, laserHeadVec, scriptGet));
        std::string arcStartProgram1 = "function arcStartProgram()\n";
        std::string arcStartProgram2 = "end\n"
                                    "arcStartProgram_Result=task_create(arcStartProgram)\n";
        scriptGet = arcStartProgram1 + scriptGet + arcStartProgram2;
        std::string funcScript = curCommuFunction->loadLuaFunctionScriptOut();  //功能函数
        scriptGet = mainFuncProgram + funcScript + scriptGet + mainFuncProgramEnd;
        //scriptGet = funcScript + scriptGet;
        //脚本赋值
        if(!strToChar(scripts, scriptLen, scriptGet)){
            return LaserSdkResult::MEMORY_ERROR_LASER_SDK;
        }
    }
    catch (const LaserSdkResult& errorResult) {
        return errorResult;
    }
    
    
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

LaserSdkResult cl_get_arc_stop_script(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen)
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

    //需要设置的激光器地址参数 - 激光器功率 - 关光功率 - 关光渐进时间 -
    std::vector<unsigned int> laserPowerVec = {
        machineResource.laserPowerLoc,               ///<  激光器功率地址 0x0230 560
        machineResource.laserOffPowerLoc,            ///<  关光功率地址 0x0233 563
        machineResource.laserOffRampTimeLoc,         ///<  关光渐进时间地址 0x0234 564
    };
    //送丝机地址参数 - 自动送丝速度 - 收尾抽丝
    std::vector<unsigned int> wireFeedVec = {
        machineResource.wireRetractLengthLoc, //260
    };
    //获取脚本
    std::string scriptGet;
    try
    {
        //送丝机
        checkLaserSdkResult(curCommuFunction->craft_wire_feeder_param_set_script_get(laserMachineHandle, wireFeedVec, scriptGet));
        //激光器
        checkLaserSdkResult(curCommuFunction->craft_laser_param_set_script_get(laserMachineHandle, laserPowerVec, scriptGet));
        std::string arcStopProgram1 = "function arcStopProgram()\n";
        std::string arcStopProgram2 = "end\n"
                                      "arcStopProgram_Result=task_create(arcStopProgram)\n";
        scriptGet = arcStopProgram1 + scriptGet + arcStopProgram2;
        std::string funcScript = curCommuFunction->loadLuaFunctionScriptOut();          //功能函数
        //scriptGet = funcScript + scriptGet;      //脚本赋值
        scriptGet = mainFuncProgram + funcScript + scriptGet + mainFuncProgramEnd;
        if(!strToChar(scripts, scriptLen, scriptGet)){
            return LaserSdkResult::MEMORY_ERROR_LASER_SDK;
        }
    }
    catch (const LaserSdkResult& errorResult) {
        return errorResult;
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}


