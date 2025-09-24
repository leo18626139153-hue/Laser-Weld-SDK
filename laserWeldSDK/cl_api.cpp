#include "cl_api.h"
#include "cl_utils.h"
#include "cl_craftscript.h"
#include "cl_luascript.h"
#include "cl_comandlistscript.h"
#include "cl_gradualriseandfall.h"

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
    std::vector<unsigned int> wireFeedVec =
    {
        machineResource.autoWireFeedSpeedLoc,       ///<  自动送丝速度地址 0x0100 256
        machineResource.wireRetractLengthLoc,       ///<  抽丝长度地址 0x0104 260
        machineResource.wireFeedLengthLoc,          ///<  送丝长度地址 0x0105 261
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
        //起弧程序
        checkLaserSdkResult(curCommuFunction->craft_script_arc_function_switch_script_get(laserMachineHandle, SwitchTypeLaserSdk::SWITCH_ON_LASER_SDK, scriptGet));
        //起弧执行程序
        checkLaserSdkResult(curCommuFunction->craft_script_arc_program_run_script_get(laserMachineHandle, SwitchTypeLaserSdk::SWITCH_ON_LASER_SDK, scriptGet));
        scriptGet = mainFuncProgram + scriptGet + mainFuncProgramEnd;
        scriptGet = curCommuFunction->loadLuaFunctionScriptOut() + scriptGet;
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
    //送丝机地址参数 - 自动送丝速度 - 收尾抽丝
    std::vector<unsigned int> wireFeedVec =
    {
        machineResource.wireRetractLengthLoc,       //260 焊丝回抽长度
        machineResource.wireFeedLengthLoc,          //261 补丝长度地址
    };

    //需要设置的激光头地址参数  关气 关送丝信号
    std::vector<unsigned int> laserHeadVec =
    {
        machineResource.gasValveSignalLoc,          ///<  送气信号开关地址 0x0031 → ​​49​
        machineResource.wireFeedSignalLoc,          ///<  送丝信号开关地址 0x0032 → ​​50
    };

    //获取脚本
    std::string scriptGet;
    try
    {
        //送丝机
        checkLaserSdkResult(curCommuFunction->craft_wire_feeder_param_set_script_get(laserMachineHandle, wireFeedVec, scriptGet));
        //激光器
        checkLaserSdkResult(curCommuFunction->craft_laser_param_set_script_get(laserMachineHandle, laserPowerVec, scriptGet));
        //关气 关送丝信号
        checkLaserSdkResult(curCommuFunction->craft_laser_head_param_set_script_get(laserMachineHandle, laserHeadVec, scriptGet));
        //收弧程序
        checkLaserSdkResult(curCommuFunction->craft_script_arc_function_switch_script_get(laserMachineHandle, SwitchTypeLaserSdk::SWITCH_OFF_LASER_SDK, scriptGet));
        //收弧执行程序
        checkLaserSdkResult(curCommuFunction->craft_script_arc_program_run_script_get(laserMachineHandle, SwitchTypeLaserSdk::SWITCH_OFF_LASER_SDK, scriptGet));
        //收弧执行程序
        //scriptGet = funcScript + scriptGet;      //脚本赋值
        scriptGet = mainFuncProgram + scriptGet + mainFuncProgramEnd;
        scriptGet = curCommuFunction->loadLuaFunctionScriptOut() + scriptGet;
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
    //句柄
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

    InstListData instListData;//指令列表数据
    std::unordered_map<int, std::pair<InstTypeLaserSdk, int>>& listLocToTypeID = instListData.listLocToTypeID;
    std::unordered_set<int>& IDSet = instListData.IDSet;
    std::unordered_map<int, CraftInfoLaserSdk>& craftsMap = instListData.craftsMap;
    std::unordered_map<int, PointDataLaserSdk>& pointsMap = instListData.pointsMap;
    std::vector<int>& craftInstListCraftIDVec = instListData.craftInstListCraftIDVec;
    std::unordered_map<int, std::pair<int, int>>& craftInstListStartEndLoc = instListData.craftInstListStartEndLoc;
    std::unordered_map<int, PointDataLaserSdk> tempPointsMap;   //临时路点信息
    //生成唯一instID
    for(int instLoc = 0; instLoc < weldStartData.instNum; instLoc++)
    {
        InstTypeLaserSdk instType = static_cast<InstTypeLaserSdk>(weldStartData.instList[instLoc*2]);
        int dataListLoc = weldStartData.instList[instLoc*2+1];
        int instID = distributionID(IDSet); //生成唯一instID,同时添加到IDSet中
        //工艺模板指令
        switch (instType) {
            //工艺模板指令
            case InstTypeLaserSdk::CRAFT_SELECT_INST_LASER_WELD_SDK:
                listLocToTypeID[instLoc] = {instType, instID};
                craftsMap[instID] = weldStartData.craftInfoList[dataListLoc];
                break;
            case InstTypeLaserSdk::LINE_POINT_INST_LASER_WELD_SDK:
            case InstTypeLaserSdk::CIRCLE_POINT_INST_LASER_WELD_SDK:
            case InstTypeLaserSdk::JOINT_POINT_INST_LASER_WELD_SDK:
            case InstTypeLaserSdk::SAFE_POINT_MOVEL_INST_LASER_WELD_SDK:
            case InstTypeLaserSdk::SAFE_POINT_MOVEJ_INST_LASER_WELD_SDK:
                listLocToTypeID[instLoc] = {instType, instID};
                pointsMap[instID] = weldStartData.pointList[dataListLoc];
                break;
            case InstTypeLaserSdk::ARC_START_INST_LASER_WELD_SDK:
            case InstTypeLaserSdk::ARC_END_INST_LASER_WELD_SDK:
                listLocToTypeID[instLoc] = {instType, instID};
                break;
        }
    }
    // 建立点数据与ID的联系
    for(auto elem: pointsMap)
    {
        tempPointsMap[elem.first].pointID = elem.second.pointID;     ///< 点ID
        tempPointsMap[elem.first].pointType = elem.second.pointType; ///< 点类型
        memcpy(tempPointsMap[elem.first].pose, elem.second.pose, sizeof(double)*6);
        memcpy(tempPointsMap[elem.first].joint, elem.second.joint, sizeof(double)*6);
    }

    int tempCraftInstListStartLoc = -1; //当前工艺实例指令列表起始位置
    int tempCraftInstListEndLoc = -1;  //当前工艺实例指令列表终止位置
    bool tempIsWelding = false; //当前是否在焊接状态
    int tempCraftID = -1;       //当前工艺ID
    //对指令列表进行预处理获取每个顶层工艺(区别于只进行arc set的工艺)所属指令的起点和终点位置
    craftInstListCraftIDVec.clear();
    craftInstListStartEndLoc.clear();
    for(int instLoc = 0; instLoc < listLocToTypeID.size(); instLoc++)
    {
        InstTypeLaserSdk instType = listLocToTypeID[instLoc].first; //指令类型
        //工艺实例开始
        if(tempCraftInstListStartLoc == -1)
        {
            tempCraftInstListStartLoc = instLoc;    //当前工艺实例指令列表起始位置
            tempCraftID = listLocToTypeID[instLoc].second;  //当前工艺ID
            craftInstListCraftIDVec.emplace_back(tempCraftID);  ///<道工艺实例列表
        }
        //起弧
        if(instType == InstTypeLaserSdk::ARC_START_INST_LASER_WELD_SDK)
        {
            tempIsWelding = true;
        }
        //收弧
        else if(instType == InstTypeLaserSdk::ARC_END_INST_LASER_WELD_SDK)
        {
            tempIsWelding = false;
        }
        //工艺实例结束
        if(tempCraftInstListStartLoc != -1 &&
                (instLoc == listLocToTypeID.size()-1 ||
                 ((tempIsWelding == false) && (instLoc < listLocToTypeID.size()-1) && (listLocToTypeID[instLoc+1].first == InstTypeLaserSdk::CRAFT_SELECT_INST_LASER_WELD_SDK))))
        {
            tempCraftInstListEndLoc = instLoc;
            //建立工艺ID与该工艺实例指令列表起始和终止位置的联系
            craftInstListStartEndLoc[tempCraftID] = {tempCraftInstListStartLoc, tempCraftInstListEndLoc};
            tempCraftInstListStartLoc = -1;
            tempCraftInstListEndLoc = -1;
            continue;
        }
    }


    std::string bodyScriptText;
    CommuTypeLaserSdk commuType = machineResource.commuType;
    CgxiRobotLaserSdk &robotParams = machineResource.robotParams;
    double emptyRunTCPSpeed = robotParams.emptyRunTCPSpeed;            //空走TCP速度
    double emptyRunTCPAcc = robotParams.emptyRunTCPAcc;              //空走TCP加速度
    double emptyRunJointSpeed = robotParams.emptyRunJointSpeed;          //空走关节速度
    double emptyRunJointAcc = robotParams.emptyRunJointAcc;            //空走关节加速度
    //double emptyPointTransitionRadius = robotParams.emptyPointTransitionRadius;  //空走点位过渡半径
    //double weldPointTransitionRadius = robotParams.weldPointTransitionRadius;   //焊接点位过渡半径
    for(auto craftID: craftInstListCraftIDVec)
    {
        CraftInfoLaserSdk& craftInfo = craftsMap[craftID];      //当前工艺信息
        int craftInstListStartLoc = craftInstListStartEndLoc[craftID].first;    //当前工艺指令列表起始位置
        int craftInstListEndLoc = craftInstListStartEndLoc[craftID].second;     //当前工艺指令列表终止位置
        int pitchAngleID = -1;  //当前工艺的起弧指令ID
        int circlePitchAngleID = -1;    //当前工艺的圆弧起弧指令ID
        int weldInstListStartLoc = craftInstListStartLoc;    //当前焊接指令列表起始位置
        int weldInstListEndLoc;       //当前焊接指令列表终止位置
        int currentInstLoc = craftInstListStartLoc;         //当前指令位置
        //遍历一段工艺路径内的所有点
        while(true)
        {
            // 遍历非焊接过程中的指令
            while (currentInstLoc == craftInstListEndLoc || (currentInstLoc < craftInstListEndLoc && (listLocToTypeID.count(currentInstLoc + 1) != 0 && listLocToTypeID[currentInstLoc + 1].first != InstTypeLaserSdk::ARC_START_INST_LASER_WELD_SDK)))
            {
                int instType = listLocToTypeID[currentInstLoc].first;
                int instID = listLocToTypeID[currentInstLoc].second;
                //工艺选择指令
                if (instType == InstTypeLaserSdk::CRAFT_SELECT_INST_LASER_WELD_SDK)
                {
                    bodyScriptText += "--change present craft--\n";
                    // 当前工艺指令在指令列表中的位置
                    //bodyScriptText += strReplace("write_int16_register(%1, %2)\n", std::vector<int> {WeldControlAddressLaserSdk::INT_REG_PRESENT_CRAFT_LOC_664, currentInstLoc});
                    CraftInfoLaserSdk curCraftInfo = craftsMap[instID];
                    ///激光参数赋值
                    std::string write_laser_params;
                    write_laser_params += "    laserRegisterMap[1][2] = "+ std::to_string(curCraftInfo.craftLaserPower)+"\n";   //激光功率
                    write_laser_params += "    laserRegisterMap[2][2] = "+ std::to_string(curCraftInfo.craftLeadInPower)+"\n";  //开光功率
                    write_laser_params += "    laserRegisterMap[3][2] = "+ std::to_string(curCraftInfo.craftLeadInRampTime)+"\n";  //开光功率上升时间
                    write_laser_params += "    laserRegisterMap[4][2] = "+ std::to_string(curCraftInfo.craftLeadOutPower)+"\n";    //关光功率
                    //计算关光功率下降时间
                    int tempTime = curCraftInfo.craftLeadOutRampDistance / curCraftInfo.craftWeldingSpeed;
                    write_laser_params += "    laserRegisterMap[5][2] = "+ std::to_string(tempTime)+"\n";  //关光功率下降时间
                    write_laser_params += "    laserRegisterMap[6][2] = "+ std::to_string(curCraftInfo.craftPreGasTime)+"\n";      //提前送气时间
                    write_laser_params += "    laserRegisterMap[7][2] = "+ std::to_string(curCraftInfo.craftPostGasTime)+"\n"; //气体关闭延时 --滞后送气时间
                    //YXYX轴偏移量
                    write_laser_params += "    laserRegisterMap[9][2] = "+ double2str(curCraftInfo.craftYAxisZoom, 2)+"*100 \n";        //激光头Y轴缩放
                    write_laser_params += "    laserRegisterMap[10][2] = "+ double2str(curCraftInfo.craftYAxisOffset, 2)+"*100 \n";     //激光头Y轴偏移
                    write_laser_params += "    laserRegisterMap[11][2] = "+ double2str(curCraftInfo.craftXAxisZoom, 2)+"*100 \n";       //激光头X轴缩放
                    write_laser_params += "    laserRegisterMap[12][2] = "+ double2str(curCraftInfo.craftXAxisOffset, 2)+"*100 \n";     //激光头X轴偏移
                    //点焊工艺
                    write_laser_params += "    spotWeld[1][2] = "+ std::to_string(curCraftInfo.craftWeldType)+"\n"; //点焊类型
                    write_laser_params += "    spotWeld[2][2] = "+ std::to_string(curCraftInfo.craftSpotWeldDuration)+"\n"; //点焊时长
                    write_laser_params += "    spotWeld[3][2] = "+ std::to_string(curCraftInfo.craftSpotWeldInterval)+"\n"; //点焊间隔
                    // 送丝机参数
                    write_laser_params += "    wireRegisterMap[1][2] = "+ std::to_string(curCraftInfo.craftEndingWireRetraction)+"\n"; //回抽长度
                    write_laser_params += "    wireRegisterMap[2][2] = "+ std::to_string(curCraftInfo.craftEndLoopbackLength)+"\n"; //补丝长度
                    write_laser_params += "    laserHeadRegisterMap[1][2] = "+ std::to_string(curCraftInfo.craftScanSpeed)+"\n"; //激光头扫描速度
                    write_laser_params += "    laserHeadRegisterMap[2][2] = "+ double2str(curCraftInfo.craftScanSpeed, 1)+"*10 \n";  //激光头扫描宽度
                    write_laser_params += "    laserHeadRegisterMap[7][2] = "+ double2str(curCraftInfo.craftScanSpeed, 1)+"*10 \n";  //激光头扫描高度
                    write_laser_params += "    laserHeadRegisterMap[8][2] = "+ std::to_string(curCraftInfo.craftScanSpeed)+"\n"; //激光头旋转角度
                    // 写入送丝机速度
                    write_laser_params += writeLuaScript("0x0A06", "0x0100", std::to_string(curCraftInfo.craftLeadInWireFeed), "2", "0");
                    // 写入焊接模式
                    write_laser_params += writeLuaScript("0x0906", "0x0202", std::to_string(curCraftInfo.craftweldMode), "3", "0");
                    ///< 写入激光头参数
                    if(curCraftInfo.graphicsSwitching != machineResource.laserHeadParams.laserSpotPattern)
                    {
                        // 写入图案
                        machineResource.laserHeadParams.laserSpotPattern = curCraftInfo.graphicsSwitching;
                        write_laser_params += writeLuaScript("0x0906", "0x0030", std::to_string(curCraftInfo.graphicsSwitching), "3", "0");//激光头图形切换
                    }
                    //写入激光焊接参数、点焊参数、丝机参数
                    std::string write_weld_params = R"(
    comNo = 3
    slave_addr = 0x09  
    first_register_num = 100 --通讯变量起始地址
    SendDataType = 0         --bool位寄存器, 通过bool寄存器发送字节数据
    SendCommuRegAddr = 0	 --发送指令从bool通讯变量的 0 地址开始存储
    write_multi_value_port(laserRegisterMap)      --写入激光焊接参数
    write_multi_value_port(laserHeadRegisterMap)  --写入激光头
    write_multi_value_port(spotWeld)              --写入点焊参数
    comNo = 2
    slave_addr = 0x0A    --从机地址
    write_multi_value_port(wireRegisterMap)     --写入丝机参数

                    )";
                    bodyScriptText += write_laser_params;
                    bodyScriptText += write_weld_params;
                }
                //安全点movej
                else if (instType == InstTypeLaserSdk::SAFE_POINT_MOVEJ_INST_LASER_WELD_SDK)
                {
                    if (!isWeld)
                    {
                        curCommuFunction->craft_script_judge_break_point_script_get(currentInstLoc, bodyScriptText);
                    }
                    bodyScriptText += "--safePointMoveJ--\n";
                    bodyScriptText += strReplace("movej(%1,%2,%3,%4)\n", {doubleArray2Str(tempPointsMap[instID].joint, 6), std::to_string(emptyRunJointSpeed), std::to_string(emptyRunJointAcc), std::to_string(0)});
                    if (!isWeld)
                    {
                        bodyScriptText += strWriteInt16RegReplace(WeldControlAddressLaserSdk::INT_REG_ARRIVE_POINT_LOC_663, currentInstLoc);
                        curCommuFunction->craft_script_judge_signal_setp_run_script_get(currentInstLoc, bodyScriptText);
                    }
                }
                //直线点位move
                else if (instType == InstTypeLaserSdk::LINE_POINT_INST_LASER_WELD_SDK)
                {
                    if (!isWeld)
                    {
                        curCommuFunction->craft_script_judge_break_point_script_get(currentInstLoc, bodyScriptText);
                    }
                    bodyScriptText += "--linePoint--\n";
                    bodyScriptText += strReplace("movel(%1,%2,%3,%4)\n", {doubleArray2Str(tempPointsMap[instID].pose, 6), std::to_string(emptyRunTCPSpeed), std::to_string(emptyRunTCPAcc), std::to_string(0)});
                    if (!isWeld)
                    {
                        bodyScriptText += strWriteInt16RegReplace(WeldControlAddressLaserSdk::INT_REG_ARRIVE_POINT_LOC_663, currentInstLoc);
                        curCommuFunction->craft_script_judge_signal_setp_run_script_get(currentInstLoc, bodyScriptText);
                    }
                }
                //圆弧点位move
                else if (instType == InstTypeLaserSdk::CIRCLE_POINT_INST_LASER_WELD_SDK)
                {
                    if (!isWeld)
                    {
                        curCommuFunction->craft_script_judge_break_point_script_get(currentInstLoc + 1, bodyScriptText);
                    }
                    bodyScriptText += "--circlePoint--\n";
                    int nextCirclePointID = listLocToTypeID[currentInstLoc + 1].second;
                    bodyScriptText += strReplace("movec(%1,%2,%3,%4,%5,%6)\n", {doubleArray2Str(tempPointsMap[instID].pose, 6), doubleArray2Str(tempPointsMap[nextCirclePointID].pose, 6), std::to_string(emptyRunTCPSpeed), std::to_string(emptyRunTCPAcc), std::to_string(0), std::to_string(0)});
                    if (!isWeld)
                    {
                        bodyScriptText += strWriteInt16RegReplace(WeldControlAddressLaserSdk::INT_REG_ARRIVE_POINT_LOC_663, currentInstLoc + 1);
                        curCommuFunction->craft_script_judge_signal_setp_run_script_get(currentInstLoc + 1, bodyScriptText);
                    }

                    // 跳过下一个圆弧定点并将最后一个定点的列表位置复制为下一个圆弧定点的位置
                    currentInstLoc++;
                }
                //关节运动指令
                else if (instType == InstTypeLaserSdk::JOINT_POINT_INST_LASER_WELD_SDK)
                {
                    if (!isWeld)
                    {
                        curCommuFunction->craft_script_judge_break_point_script_get(currentInstLoc, bodyScriptText);
                    }

                    bodyScriptText += "--jointPoint--\n";
                    bodyScriptText += strReplace("movej(%1,%2,%3,%4)\n", {doubleArray2Str(tempPointsMap[instID].joint, 6),
                                                 std::to_string(emptyRunJointSpeed),
                                                 std::to_string(emptyRunJointAcc), std::to_string(0)
                                                                         });
                    if (!isWeld)
                    {
                        bodyScriptText += strWriteInt16RegReplace(WeldControlAddressLaserSdk::INT_REG_ARRIVE_POINT_LOC_663, currentInstLoc);
                        curCommuFunction->craft_script_judge_signal_setp_run_script_get(currentInstLoc, bodyScriptText);
                    }
                }
                currentInstLoc++;
            }

            // 遍历完了当前工艺下的所有指令
            if (currentInstLoc > craftInstListEndLoc)
            {
                break;
            }

            // 获取当前焊接路径的起始与结束位置
            weldInstListStartLoc = currentInstLoc;
            weldInstListEndLoc = weldInstListStartLoc;
            while (listLocToTypeID[weldInstListEndLoc].first != InstTypeLaserSdk::ARC_END_INST_LASER_WELD_SDK)
            {
                weldInstListEndLoc++;
            }

            InstListTransEnvironmentData instListTransEnvironmentData; // 指令列表转换过程中的环境数据
            instListTransEnvironmentData.craftID = craftID;
            instListTransEnvironmentData.prePointLoc = -1;
            instListTransEnvironmentData.roadCraftFlag = RoadCraftType::NO_ROAD_CRAFT;
            instListTransEnvironmentData.isWeld = isWeld;
            instListTransEnvironmentData.isWelding = false;


            //拆分后路径的起始与终点位置
            int segWeldInstListStartLoc = weldInstListStartLoc; //起始位置
            int segWeldInstListEndLoc = weldInstListStartLoc;   //结束位置
            // 遍历一段起收弧相应焊接路径内所有的指令
            while (true)
            {
                // 根据焊接过程中的工艺指令将一段起收弧路径拆分为多段
                while ((segWeldInstListEndLoc < weldInstListEndLoc) && (listLocToTypeID.count(segWeldInstListEndLoc + 1) != 0 && listLocToTypeID[segWeldInstListEndLoc + 1].first != InstTypeLaserSdk::CRAFT_SELECT_INST_LASER_WELD_SDK))
                {
                    if (listLocToTypeID[segWeldInstListEndLoc].first == InstTypeLaserSdk::CRAFT_SELECT_INST_LASER_WELD_SDK)
                    {
                        instListTransEnvironmentData.craftID = listLocToTypeID[segWeldInstListEndLoc].second;
                    }
                    segWeldInstListEndLoc++;
                }
                CraftInfoLaserSdk &segCraftInfo = craftsMap[instListTransEnvironmentData.craftID];
                //指令列表转换接口
                LaserSdkResult result = GetBasicCraftScript(laserMachineHandle, weldStartData, instListData, tempPointsMap, instListTransEnvironmentData, segWeldInstListStartLoc, segWeldInstListEndLoc, bodyScriptText);
                if (result != LaserSdkResult::SUCCESS_LASER_SDK)
                {
                    return result;
                }
                // 结束当前起收弧路径的转换或者继续遍历拆分后的下一段
                if (segWeldInstListEndLoc >= weldInstListEndLoc)
                {
                    break;
                }
                else
                {
                    segWeldInstListEndLoc += 1;
                    segWeldInstListStartLoc = segWeldInstListEndLoc;
                }
            }
            currentInstLoc = weldInstListEndLoc+1;
        }
    }

    std::string breakPointListScript;
    std::string signalSetpPointLocListScript;
    if(!isWeld)
    {
        // 进行断点列表的处理
        for (int i = 0; i < weldStartData.breakPointNum; i++)
        {
            breakPointListScript += std::to_string(weldStartData.breakPointLocList[i]);  // 断点列表
            if (i != weldStartData.breakPointNum - 1)
            {
                breakPointListScript += ", ";  // 逗号分隔
            }
        }

        breakPointListScript = "breakPointLocList = {" + breakPointListScript + "}\n";
        breakPointListScript += R"(
        breakPointLocListLookUp = {}
        for k, v in pairs(breakPointLocList) do
            breakPointLocListLookUp[v] = 1
        end
        )";

        // 进行单步运行路点地址列表的处理
        for (int i = 0; i < weldStartData.signalStepPointNum; i++)
        {
            signalSetpPointLocListScript += std::to_string(weldStartData.signalStepPointLocList[i]);
            if (i != weldStartData.signalStepPointNum - 1)
            {
                signalSetpPointLocListScript += ", ";
            }
        }

        signalSetpPointLocListScript = "signalSetpPointLocList = {" + signalSetpPointLocListScript + "}\n";
        signalSetpPointLocListScript += R"(
        signalSetpPointLocListLookUp = {}
        for k, v in pairs(signalSetpPointLocList) do
            signalSetpPointLocListLookUp[v] = 1
        end
        )";
    }

    //组装最终要下发执行的脚本
    std::string commandListStartData = 
    R"(--激光器工艺
    local laserRegisterMap = {
        [1] = {0x0230, 2000},  -- 激光功率
        [2] = {0x0231, 2000},  -- 开光功率
        [3] = {0x0232, 50},    -- 开光功率上升时间
        [4] = {0x0233, 1000},  -- 关光功率
        [5] = {0x0234, 10},   -- 关光功率下降时间
        [6] = {0x0235, 2000},     -- 气体打开延时 --提前送气时间
        [7] = {0x0236, 1000},     -- 气体关闭延时 --滞后送气时间
        [8] = {0x0237, 0},    -- 送丝延时 默认0
        [9] = {0x0238, 0},    -- 激光头Y轴缩放
        [10] = {0x0239, 0},   -- 激光头Y轴偏移
        [11] = {0x023A, 0 },  -- 激光头X轴缩放
        [12] = {0x023B, 0 },  -- 激光头X轴偏移
    }
    --送丝机参数
    local wireRegisterMap = {
        [1] = {0x0103, 0},      --自动送丝延时
        [2] = {0x0104, 100},    --回抽长度
        [3] = {0x0105, 100},    --补丝长度
        [4] = {0x0106, 0},    --补丝延时
        [100] = {0x0100, 123},  --自动送丝速度
    }
    --点焊工艺
    local spotWeld = {
        [1] = {0x0242, 0}, --点焊类型 1：鱼鳞焊 0：间断
        [2] = {0x0243, 0}, --点焊持续时间  0 ~ 60000 ms
        [3] = {0x0244, 0} --点焊间隔时间   0 ~ 60000 ms
    }
    -- 激光头控制参数
    local laserHeadRegisterMap = {
        [1] = { 0x0000, 2000},  -- 激光头扫描速度
        [2] = { 0x0001, 25 },    -- 激光头扫描宽度
        [3] = { 0x0002, 3000 },    -- 峰值功率，焊接功率
        [4] = { 0x0003, 100},   -- 占空比
        [5] = { 0x0004, 0 },
        [6] = { 0x0005, 20000 },    --脉冲频率/工艺无控制
        [7] = { 0x0006, 2.5 },    -- 激光头扫描高度
        [8] = { 0x0007, 0 },    -- 激光头旋转角度
    }
    )";
    std::string RobotProgramScriptStart = " function RobotProgram()\n";
    std::string RobotProgramScriptEnd = "\n end\n"
                                " -----RobotProgramEnd----\n"
                                " RobotProgram_Result=task_create(RobotProgram)\n";
    std::string script;
    script += commandListStartData; //起始参数
    script += curCommuFunction->loadLuaFunctionScriptOut();//调用函数定义
    script += breakPointListScript; //断电列表
    script += signalSetpPointLocListScript; //单步运行列表序列
    script += strWriteInt16RegReplace(WeldControlAddressLaserSdk::INT_REG_WELD_MODE_660, isWeld ? 2 : 1); //执行模式：0.空状态 1.轨迹模拟 2.焊接
    script += RobotProgramScriptStart + bodyScriptText + RobotProgramScriptEnd;
    script = mainFuncProgram + script + mainFuncProgramEnd;
    //测试代码scripts
    if(isWeld)
    {    std::ofstream file("./startWeldFun.txt");
        file << script;
        file.close();
    }
    else
    {
        std::ofstream file("./trackSimulateFun.txt");
        file << script;
        file.close();
    }

    if(!strToChar(scripts, scriptLen, script))
    {
        return LaserSdkResult::MEMORY_ERROR_LASER_SDK;
    }

    return SUCCESS_LASER_SDK;
}


//直线 计算 降功率点
LaserSdkResult cl_craft_straight_line_back_point_cal(RollbackStraightLineData* rollbackStraightLineData)
{
    // 调用 getNewTeachPoints 方法计算新的起点和终点
    LaserSdkResult result = getStraightLineSlowUpPoint(
                                rollbackStraightLineData->startPoint,
                                rollbackStraightLineData->endPoint,
                                rollbackStraightLineData->distance,
                                rollbackStraightLineData->pointOut
                            );

    if (result != LaserSdkResult::SUCCESS_LASER_SDK)
    {
        std::cerr << "Failed to calculate new teach points." << std::endl;
        return result;
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}


//圆弧 计算 降功率点
LaserSdkResult cl_craft_circle_back_point_cal(RollbackCircleData* rollbackCircleData)
{
    // 调用 getNewTeachPoints 方法计算新的起点和终点
    LaserSdkResult result = getCirclrSlowDownPoints(
                                rollbackCircleData->startPoint,
                                rollbackCircleData->transPoint,
                                rollbackCircleData->endPoint,
                                rollbackCircleData->stopPoint,
                                rollbackCircleData->distance,
                                rollbackCircleData->stopMappingPoint,
                                rollbackCircleData->startPointOut,
                                rollbackCircleData->transPointOut,
                                rollbackCircleData->endPointOut
                            );

    if (result != LaserSdkResult::SUCCESS_LASER_SDK)
    {
        std::cerr << "Failed to calculate new teach points." << std::endl;
        return result;
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}
