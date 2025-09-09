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
        return result;
    }

    //脚本赋值
    std::string funcScript = curCommuFunction->loadLuaFunctionScriptOut();  //功能函数
    std::string readRobotProgram1 = "function readRobotProgram()\n";
    std::string readRobotProgram2 = "end\n"
                                    "-----RobotProgramEnd----\n"
                                    "RobotProgram_Result=task_create(readRobotProgram)\n";
    scriptGet = readRobotProgram1 + scriptGet  +readRobotProgram1;
    scriptGet = funcScript + scriptGet ;
    if(strToChar(script, &scriptLen, scriptGet))
    {
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
        if (curCommuFunction == nullptr)
        {
            return LaserSdkResult::COMMU_ERROR_LASER_SDK;
        }
    }
    else
    {
        return LaserSdkResult::COMMU_ERROR_LASER_SDK;
    }

    //需要设置的激光器地址参数 - 激光器功率 - 开光功率 - 开光渐进时间 -
    std::vector<unsigned int> laserPowerVec =
    {
        machineResource.gasONDelayLoc,              ///<  开气延时地址 0x0235  先等送气结束
        machineResource.laserPowerLoc,              ///<  激光器功率地址 0x0230 560
        machineResource.laserOnPowerLoc,            ///<  开光功率地址 0x0231 561
        machineResource.laserOnRampTimeLoc,         ///<  开光渐进时间地址 0x0232 562
        machineResource.laserHeadYOffsetLoc,        ///<  Y轴偏移地址 0x0239 569
        machineResource.laserHeadXOffsetLoc,        ///<  X轴偏移地址 0x023b 571
        machineResource.laserHeadYScaleLoc,         ///<  Y轴放大地址 0x0238 568
        machineResource.laserHeadXScaleLoc,         ///<  X轴放大地址 0x023a 570
        machineResource.spotWeldTypeLoc             ///<  点焊类型地址 0x0242 578
    };
    //需要设置的激光头地址参数 - 扫描速度 - 扫描宽 - 峰值功率 - 占空比 - 脉冲频率 - 扫描高度 - 旋转角度 - 图形切换
    std::vector<unsigned int> laserHeadVec =
    {
        machineResource.laserHeadPatternIdLoc,      //图形首位
        machineResource.laserHeadScanSpeedLoc,
        machineResource.laserHeadScanWidthLoc,
        machineResource.laserHeadPeakPowerLoc,
        machineResource.laserHeadDutyCycleLoc,
        machineResource.laserHeadFrequencyLowLoc,   ///<  脉冲频率 : 15~0位地址  0x0005  5
        machineResource.laserHeadFrequencyHighLoc,  ///<  脉冲频率 : 31~16位地址 0x0004  4
        machineResource.laserHeadScanHeightLoc,     ///<  扫描高度地址 0x0006 6
        machineResource.laserHeadRotationAngleLoc,  ///<  旋转角度地址 0x0007 7
        machineResource.gasValveSignalLoc,          ///<  送气信号开关地址 0x0031 → ​​49​
        machineResource.wireFeedSignalLoc,          ///<  送丝信号开关地址 0x0032 → ​​50

    };
    //送丝机地址参数 - 自动送丝速度
    std::vector<unsigned int> wireFeedVec = {
        machineResource.wireFeederStatusLoc,        ///< 送丝机运行状态地址 (0:停止 1:运行) 0x0115 → ​​277
        machineResource.autoWireFeedSpeedLoc,       ///<  自动送丝速度地址 0x0100 256        
        machineResource.operationModeLoc,           ///<  送丝机模式设置地址 0x010C 268

//        machineResource.manualWireFeedSpeedLoc,     ///<  手动送丝速度地址 0x0101 257
//        machineResource.manualWireRetractSpeedLoc,  ///<  手动抽丝速度地址 0x0102 258
//        machineResource.wireFeedLengthLoc,          ///<  送丝长度地址 0x0105 261
//        machineResource.wireRetractLengthLoc,       ///<  抽丝长度地址 0x0104 260
//        machineResource.manualWireFeedSwitchLoc,    ///<  手动送丝使能地址 (0	停    1	运行)  0x0140  320
//        machineResource.manualWireRetractSwitchLoc, ///<  手动抽丝使能地址 (0 停    1 运行)  0x0141  321
//        machineResource.pulsePeriodLoc,             ///<  脉冲周期地址 0x0107 263
//        machineResource.smoothnessLoc,              ///<  平滑度地址  0x0108 264

    };
    //获取脚本
    std::string scriptGet;

    try
    {

        //激光器
        checkLaserSdkResult(curCommuFunction->craft_laser_param_set_script_get(laserMachineHandle, laserPowerVec, scriptGet));
        //送丝机
        checkLaserSdkResult(curCommuFunction->craft_wire_feeder_param_set_script_get(laserMachineHandle, wireFeedVec, scriptGet));
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
        if(!strToChar(scripts, scriptLen, scriptGet))
        {
            return LaserSdkResult::MEMORY_ERROR_LASER_SDK;
        }
    }
    catch (const LaserSdkResult& errorResult)
    {
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
        if (curCommuFunction == nullptr)
        {
            return LaserSdkResult::COMMU_ERROR_LASER_SDK;
        }
    }
    else
    {
        return LaserSdkResult::COMMU_ERROR_LASER_SDK;
    }

    //需要设置的激光器地址参数 - 激光器功率 - 关光功率 - 关光渐进时间 -
    std::vector<unsigned int> laserPowerVec =
    {        
        machineResource.laserOffPowerLoc,            ///<  关光功率地址 0x0233 563
        machineResource.laserOffRampTimeLoc,         ///<  关光渐进时间地址 0x0234 564
//        machineResource.laserPowerLoc,               ///<  激光器功率地址 0x0230 560
    };
    //需要设置的激光头地址参数  关气 关送丝信号
    std::vector<unsigned int> laserHeadVec =
    {
        machineResource.gasValveSignalLoc,          ///<  送气信号开关地址 0x0031 → ​​49​
        machineResource.wireFeedSignalLoc,          ///<  送丝信号开关地址 0x0032 → ​​50

    };
    //送丝机地址参数 - 自动送丝速度 - 收尾抽丝
    std::vector<unsigned int> wireFeedVec =
    {
        machineResource.wireRetractLengthLoc, //260
        machineResource.wireFeederStatusLoc,        ///<  送丝机运行状态地址 (0:停止 1:运行) 0x0115 结束关闭送丝机器
    };
    //获取脚本
    std::string scriptGet;
    try
    {
        //送丝机
        checkLaserSdkResult(curCommuFunction->craft_wire_feeder_param_set_script_get(laserMachineHandle, wireFeedVec, scriptGet));
        //激光器
        checkLaserSdkResult(curCommuFunction->craft_laser_param_set_script_get(laserMachineHandle, laserPowerVec, scriptGet));
        //激光头
        checkLaserSdkResult(curCommuFunction->craft_laser_head_param_set_script_get(laserMachineHandle, laserHeadVec, scriptGet));
        std::string arcStopProgram1 = "function arcStopProgram()\n";
        std::string arcStopProgram2 = "end\n"
                                      "arcStopProgram_Result=task_create(arcStopProgram)\n";
        scriptGet = arcStopProgram1 + scriptGet + arcStopProgram2;
        std::string funcScript = curCommuFunction->loadLuaFunctionScriptOut();          //功能函数
        //scriptGet = funcScript + scriptGet;      //脚本赋值
        scriptGet = mainFuncProgram + funcScript + scriptGet + mainFuncProgramEnd;
        if(!strToChar(scripts, scriptLen, scriptGet))
        {
            return LaserSdkResult::MEMORY_ERROR_LASER_SDK;
        }
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}

// 初始化焊机相关的寄存机值
LaserSdkResult cl_craft_weld_status_init(const LaserMachineHandle laserMachineHandle)
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
        if (curCommuFunction == nullptr)
        {
            return LaserSdkResult::COMMU_ERROR_LASER_SDK;
        }
    }
    else
    {
        return LaserSdkResult::COMMU_ERROR_LASER_SDK;
    }

    try
    {
        int terminalButtonAddress = WeldControlAddressLaserSdk::BOOL_REG_TERMINAL_BUTTON_610;
        int weldExtstructAddress = WeldControlAddressLaserSdk::BOOL_REG_EXTSTRUCT_611;
        int safeExtstructAddress = WeldControlAddressLaserSdk::BOOL_REG_EXTSTRUCT_612;
        int weldStatusMonitor = WeldControlAddressLaserSdk::BOOL_REG_WELD_STATUS_MONITOR_613;
        int programStopArcEndAddress = WeldControlAddressLaserSdk::BOOL_REG_PROGRAM_STOP_ARC_END_614;
        int singleStepRunAddress = WeldControlAddressLaserSdk::BOOL_REG_SINGLE_STEP_RUN_616;
        int runBreakPointAddress = WeldControlAddressLaserSdk::BOOL_REG_RUN_BREAK_POINT_617;

        int arcStartFailAddress = WeldControlAddressLaserSdk::INT_REG_ARC_START_FAIL_673;
        int weldInterruptAddress = WeldControlAddressLaserSdk::INT_REG_WELD_INTERRUPT_674;
        int arcEndFailAddress = WeldControlAddressLaserSdk::INT_REG_ARC_END_FAIL_675;
        int weldModeAddress = WeldControlAddressLaserSdk::INT_REG_WELD_MODE_660;
        int weldStateAddress = WeldControlAddressLaserSdk::INT_REG_WELD_STATE_661;
        int weldPathAddress = WeldControlAddressLaserSdk::INT_REG_WELD_PATH_662;
        int arrivePointLocAddress = WeldControlAddressLaserSdk::INT_REG_ARRIVE_POINT_LOC_663;
        int presentCraftLocAddress = WeldControlAddressLaserSdk::INT_REG_PRESENT_CRAFT_LOC_664;

        int writeParameter62Address = WeldControlAddressLaserSdk::INT_REG_WRITE_PARAMETER_62;
        int writeParameter63Address = WeldControlAddressLaserSdk::INT_REG_WRITE_PARAMETER_63;
        int writeParameter64Address = WeldControlAddressLaserSdk::INT_REG_WRITE_PARAMETER_64;
        int writeParameter65Address = WeldControlAddressLaserSdk::INT_REG_WRITE_PARAMETER_65;
        int readParameter66Address = WeldControlAddressLaserSdk::INT_REG_READ_PARAMETER_66;
        int readParameter67Address = WeldControlAddressLaserSdk::INT_REG_READ_PARAMETER_67;
        int readParameter68Address = WeldControlAddressLaserSdk::INT_REG_READ_PARAMETER_68;
        int readParameter69Address = WeldControlAddressLaserSdk::INT_REG_READ_PARAMETER_69;
        int resultAddress = WeldControlAddressLaserSdk::INT_REG_RESULT_77;

        BOOL boolVal = FALSE;
        int intVal = 0;

        cr_set_boolRegValue(machineResource.robotHandle, terminalButtonAddress, 1, &boolVal);
        cr_set_boolRegValue(machineResource.robotHandle, weldExtstructAddress, 1, &boolVal);
        cr_set_boolRegValue(machineResource.robotHandle, safeExtstructAddress, 1, &boolVal);
        cr_set_boolRegValue(machineResource.robotHandle, weldStatusMonitor, 1, &boolVal);
        cr_set_boolRegValue(machineResource.robotHandle, programStopArcEndAddress, 1, &boolVal);
        cr_set_boolRegValue(machineResource.robotHandle, singleStepRunAddress, 1, &boolVal);
        cr_set_boolRegValue(machineResource.robotHandle, runBreakPointAddress, 1, &boolVal);

        cr_set_int16RegValue(machineResource.robotHandle, arcStartFailAddress, 1, &intVal);
        cr_set_int16RegValue(machineResource.robotHandle, weldInterruptAddress, 1, &intVal);
        cr_set_int16RegValue(machineResource.robotHandle, arcEndFailAddress, 1, &intVal);
        cr_set_int16RegValue(machineResource.robotHandle, weldModeAddress, 1, &intVal);
        cr_set_int16RegValue(machineResource.robotHandle, weldStateAddress, 1, &intVal);
        cr_set_int16RegValue(machineResource.robotHandle, weldPathAddress, 1, &intVal);
        cr_set_int16RegValue(machineResource.robotHandle, arrivePointLocAddress, 1, &intVal);
        cr_set_int16RegValue(machineResource.robotHandle, presentCraftLocAddress, 1, &intVal);

        cr_set_int16RegValue(machineResource.robotHandle, writeParameter62Address, 1, &intVal);
        cr_set_int16RegValue(machineResource.robotHandle, writeParameter63Address, 1, &intVal);
        cr_set_int16RegValue(machineResource.robotHandle, writeParameter64Address, 1, &intVal);
        cr_set_int16RegValue(machineResource.robotHandle, writeParameter65Address, 1, &intVal);
        cr_set_int16RegValue(machineResource.robotHandle, readParameter66Address, 1, &intVal);
        cr_set_int16RegValue(machineResource.robotHandle, readParameter67Address, 1, &intVal);
        cr_set_int16RegValue(machineResource.robotHandle, readParameter68Address, 1, &intVal);
        cr_set_int16RegValue(machineResource.robotHandle, readParameter69Address, 1, &intVal);
        cr_set_int16RegValue(machineResource.robotHandle, resultAddress, 1, &intVal);
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//指令列表转换
LaserSdkResult cl_craft_inst_list_script_get(const LaserMachineHandle laserMachineHandle, LaserWeldStartData weldStartData, bool isWeld, char *scripts, int *scriptLen)
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
        if (curCommuFunction == nullptr)
        {
            return LaserSdkResult::COMMU_ERROR_LASER_SDK;
        }
    }
    else
    {
        return LaserSdkResult::COMMU_ERROR_LASER_SDK;
    }

    InstListData instListData;
    std::unordered_map<int, std::pair<InstTypeLaserSdk, int>>& listLocToTypeID = instListData.listLocToTypeID;
    std::unordered_set<int>& IDSet = instListData.IDSet;
    std::unordered_map<int, CraftInfoLaserSdk>& craftsMap = instListData.craftsMap;
    std::unordered_map<int, PointDataLaserSdk>& pointsMap = instListData.pointsMap;

    std::unordered_map<int, PointDataLaserSdk> tempPointsMap;   //临时路点信息
    //将指令树结构体中的数据进行转化
    for(int instLoc = 0; instLoc < weldStartData.instNum; instLoc++)
    {
        InstTypeLaserSdk instType = static_cast<InstTypeLaserSdk>(weldStartData.instList[instLoc*2]);
        int dataListLoc = weldStartData.instList[instLoc*2+1];
        //工艺模板指令
        if(instType == InstTypeLaserSdk::CRAFT_SElECT_INST_LASER_WELD_SDK)
        {
            int instID = distribution(engine);
            while(IDSet.count(instID) != 0)
            {
                instID = distribution(engine);
            }
            IDSet.insert(instID);
            //建立指令在列表中位置与指令类型与ID的联系
            listLocToTypeID[instLoc] = {InstTypeLaserSdk::CRAFT_SElECT_INST_LASER_WELD_SDK, instID};

            //建立指令ID和工艺数据的联系
            craftsMap[instID] = weldStartData.craftInfoList[dataListLoc];


        }
        //安全点指令 MOVEL
        else if(instType == InstTypeLaserSdk::SAFE_POINT_MOVEL_INST_LASER_WELD_SDK)
        {
            int instID = distribution(engine);
            while(IDSet.count(instID) != 0)
            {
                instID = distribution(engine);
            }
            IDSet.insert(instID);
            //建立指令在列表中位置与指令类型与ID的联系
            listLocToTypeID[instLoc] = {InstTypeLaserSdk::SAFE_POINT_MOVEL_INST_LASER_WELD_SDK, instID};
            //建立instID(pointID)与路点数据的连接
            pointsMap[instID] = weldStartData.pointList[dataListLoc];
        }
        //安全点指令 MOVEJ
        else if(instType == InstTypeLaserSdk::SAFE_POINT_MOVEJ_INST_LASER_WELD_SDK)
        {
            int instID = distribution(engine);
            while(IDSet.count(instID) != 0)
            {
                instID = distribution(engine);
            }
            IDSet.insert(instID);
            //建立指令在列表中位置与指令类型与ID的联系
            listLocToTypeID[instLoc] = {InstTypeLaserSdk::SAFE_POINT_MOVEJ_INST_LASER_WELD_SDK, instID};
            //建立instID(pointID)与路点数据的连接
            pointsMap[instID] = weldStartData.pointList[dataListLoc];
        }
        else if(instType == InstTypeLaserSdk::LINE_POINT_INST_LASER_WELD_SDK)
        {
            int instID = distribution(engine);
            while(IDSet.count(instID) != 0)
            {
                instID = distribution(engine);
            }
            IDSet.insert(instID);
            //建立指令在列表中位置与指令类型与ID的联系
            listLocToTypeID[instLoc] = {InstTypeLaserSdk::LINE_POINT_INST_LASER_WELD_SDK, instID};
            //建立instID(pointID)与路点数据的连接
            pointsMap[instID] = weldStartData.pointList[dataListLoc];
        }
        else if(instType == InstTypeLaserSdk::CIRCLE_POINT_INST_LASER_WELD_SDK)
        {
            int instID = distribution(engine);
            while(IDSet.count(instID) != 0)
            {
                instID = distribution(engine);
            }
            IDSet.insert(instID);
            //建立指令在列表中位置与指令类型与ID的联系
            listLocToTypeID[instLoc] = {InstTypeLaserSdk::CIRCLE_POINT_INST_LASER_WELD_SDK, instID};
            //建立instID(pointID)与路点数据的连接
            pointsMap[instID] = weldStartData.pointList[dataListLoc];
        }
        else if(instType == InstTypeLaserSdk::JOINT_POINT_INST_LASER_WELD_SDK)
        {
            int instID = distribution(engine);
            while(IDSet.count(instID) != 0)
            {
                instID = distribution(engine);
            }
            IDSet.insert(instID);
            //建立指令在列表中位置与指令类型与ID的联系
            listLocToTypeID[instLoc] = {InstTypeLaserSdk::JOINT_POINT_INST_LASER_WELD_SDK, instID};
            //建立instID(pointID)与路点数据的连接
            pointsMap[instID] = weldStartData.pointList[dataListLoc];
        }
        else if(instType == InstTypeLaserSdk::ARC_START_INST_LASER_WELD_SDK)
        {
            int instID = distribution(engine);
            while(IDSet.count(instID) != 0)
            {
                instID = distribution(engine);
            }
            IDSet.insert(instID);
            //建立指令在列表中位置与指令类型与ID的联系
            listLocToTypeID[instLoc] = {InstTypeLaserSdk::ARC_START_INST_LASER_WELD_SDK, instID};
        }
        else if(instType == InstTypeLaserSdk::ARC_END_INST_LASER_WELD_SDK)
        {
            int instID = distribution(engine);
            while(IDSet.count(instID) != 0)
            {
                instID = distribution(engine);
            }
            IDSet.insert(instID);
            //建立指令在列表中位置与指令类型与ID的联系
            listLocToTypeID[instLoc] = {InstTypeLaserSdk::ARC_END_INST_LASER_WELD_SDK, instID};
        }
    }

    // 建立点数据与ID的联系
    for(auto elem: pointsMap)
    {
        tempPointsMap[elem.first].pointID = elem.second.pointID;     ///< 点ID
        tempPointsMap[elem.first].pointType = elem.second.pointType; ///< 点类型
        memcpy(tempPointsMap[elem.first].pose, elem.second.pose, sizeof(double)*6);     ///<
        memcpy(tempPointsMap[elem.first].joint, elem.second.joint, sizeof(double)*6);
    }

    return SUCCESS_LASER_SDK;
}
