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
        //起弧执行程序
        checkLaserSdkResult(curCommuFunction->craft_script_arc_program_run_script_get(laserMachineHandle, SwitchTypeLaserSdk::SWITCH_ON_LASER_SDK, scriptGet));
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

//收弧！！！
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
        //收弧执行程序
        checkLaserSdkResult(curCommuFunction->craft_script_arc_program_run_script_get(laserMachineHandle, SwitchTypeLaserSdk::SWITCH_OFF_LASER_SDK, scriptGet));
        //收弧执行程序
        //scriptGet = funcScript + scriptGet;      //脚本赋值
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

LaserSdkResult cl_laser_current_craft_set_read(const LaserMachineHandle laserMachineHandle, char *script, int *scriptLen)
{
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
    auto checkCrSdkResult = [=](const CRresult& errorResult)
    {
        LaserSdkResult errorLaserResult = crsdk_return_value_transfer(errorResult);
        if (errorLaserResult != LaserSdkResult::SUCCESS_LASER_SDK)
        {
            //res = CR_ERROR;
            throw errorLaserResult;
        }
    };
    try
    {
        BOOL boolVal = FALSE;
        int  intVal  = 0;

        checkCrSdkResult(cr_set_int32RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT32_REG_TERMINAL_BUTTON_610, 1, &intVal));
        checkCrSdkResult(cr_set_boolRegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::BOOL_REG_EXTSTRUCT_611, 1, &boolVal));
        checkCrSdkResult(cr_set_boolRegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::BOOL_REG_EXTSTRUCT_612, 1, &boolVal));

        checkCrSdkResult(cr_set_int32RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT32_REG_WELD_STATUS_MONITOR_613, 1, &intVal));
        checkCrSdkResult(cr_set_int32RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT32_REG_PROGRAM_STOP_ARC_END_614, 1, &intVal));
        checkCrSdkResult(cr_set_int32RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT32_REG_SINGLE_STEP_RUN_616, 1, &intVal));
        checkCrSdkResult(cr_set_int32RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT32_REG_RUN_BREAK_POINT_617, 1, &intVal));

        checkCrSdkResult(cr_set_int32RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT32_REG_ARC_START_FAIL_673, 1, &intVal));
        checkCrSdkResult(cr_set_int32RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT32_REG_WELD_INTERRUPT_674, 1, &intVal));
        checkCrSdkResult(cr_set_int32RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT32_REG_ARC_END_FAIL_675, 1, &intVal));
        checkCrSdkResult(cr_set_int32RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT32_REG_WELD_MODE_660, 1, &intVal));
        checkCrSdkResult(cr_set_int32RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT32_REG_WELD_STATE_799, 1, &intVal));
        checkCrSdkResult(cr_set_int32RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT32_REG_WELD_PATH_662, 1, &intVal));
        checkCrSdkResult(cr_set_int32RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT32_REG_ARRIVE_POINT_LOC_663, 1, &intVal));
        checkCrSdkResult(cr_set_int32RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT32_REG_PRESENT_CRAFT_LOC_664, 1, &intVal));

        checkCrSdkResult(cr_set_int16RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT_REG_WRITE_PARAMETER_62, 1, &intVal));
        checkCrSdkResult(cr_set_int16RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT_REG_WRITE_PARAMETER_63, 1, &intVal));
        checkCrSdkResult(cr_set_int16RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT_REG_WRITE_PARAMETER_64, 1, &intVal));
        checkCrSdkResult(cr_set_int16RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT_REG_WRITE_PARAMETER_65, 1, &intVal));
        checkCrSdkResult(cr_set_int16RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT_REG_READ_PARAMETER_68,  1, &intVal));
        checkCrSdkResult(cr_set_int16RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT_REG_READ_PARAMETER_69,  1, &intVal));
        checkCrSdkResult(cr_set_int16RegValue(machineResource.robotHandle, WeldControlAddressLaserSdk::INT_REG_RESULT_77,  1, &intVal));
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
    std::unordered_map<int, std::pair<InstTypeLaserSdk, int> > & listLocToTypeID = instListData.listLocToTypeID;
    std::unordered_set<int>& IDSet = instListData.IDSet;
    std::unordered_map<int, CraftInfoLaserSdk>& craftsMap = instListData.craftsMap;
    std::unordered_map<int, PointDataLaserSdk>& pointsMap = instListData.pointsMap;
    std::vector<int>& craftInstListCraftIDVec = instListData.craftInstListCraftIDVec;
    std::unordered_map<int, std::pair<int, int> >& craftInstListStartEndLoc = instListData.craftInstListStartEndLoc;
    std::unordered_map<int, PointDataLaserSdk> tempPointsMap;   //临时路点信息
    //生成唯一instID
    for(int instLoc = 0; instLoc < weldStartData.instNum; instLoc++)
    {
        InstTypeLaserSdk instType = static_cast<InstTypeLaserSdk>(weldStartData.instList[instLoc*2]);
        int dataListLoc = weldStartData.instList[instLoc*2+1];
        int instID = distributionID(IDSet); //生成唯一instID,同时添加到IDSet中
        //工艺模板指令
        switch (instType) {
            case InstTypeLaserSdk::CRAFT_SELECT_INST_LASER_WELD_SDK://工艺选择指令
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
            case InstTypeLaserSdk::SLOW_DOWN_LASER_WELD_SDK://缓降·
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
    int tempCraftInstListEndLoc = -1;   //当前工艺实例指令列表终止位置
    bool tempIsWelding = false;         //当前是否在焊接状态
    int tempCraftID = -1;               //当前工艺ID
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
                 ((tempIsWelding == false) &&
                  (instLoc < listLocToTypeID.size()-1) &&
                  (listLocToTypeID[instLoc+1].first == InstTypeLaserSdk::CRAFT_SELECT_INST_LASER_WELD_SDK))))
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
    CgxiRobotLaserSdk &robotParams = machineResource.robotParams;
    double emptyRunTCPSpeed = machineResource.robotParams.emptyRunTCPSpeed;            //空走TCP速度
    double emptyRunTCPAcc = machineResource.robotParams.emptyRunTCPAcc;              //空走TCP加速度
    double emptyRunJointSpeed = machineResource.robotParams.emptyRunJointSpeed;          //空走关节速度
    double emptyRunJointAcc = machineResource.robotParams.emptyRunJointAcc;            //空走关节加速度

    double emptyPointTransitionRadius = 0;           //空走点位过渡半径
    if (emptyPointTransitionRadius < 0 || emptyPointTransitionRadius > 10)
    {
        emptyPointTransitionRadius = 0;
    }
    
    double weldPointTransitionRadius = machineResource.robotParams.weldPointTransitionRadius;             //焊接点位过渡半径
    if (weldPointTransitionRadius < 0 || weldPointTransitionRadius > 10)
    {
        weldPointTransitionRadius = 1.0;
    }
    for(auto craftID: craftInstListCraftIDVec)
    {
        int craftInstListStartLoc = craftInstListStartEndLoc[craftID].first;    //当前工艺指令列表起始位置
        int craftInstListEndLoc = craftInstListStartEndLoc[craftID].second;     //当前工艺指令列表终止位置
        int weldInstListStartLoc = craftInstListStartLoc;    //当前焊接指令列表起始位置
        int weldInstListEndLoc;       //当前焊接指令列表终止位置
        int currentInstLoc = craftInstListStartLoc;         //当前指令位置
        //遍历一段工艺路径内的所有点
        while(true)
        {
            // 遍历非焊接过程中的指令
            while (currentInstLoc == craftInstListEndLoc ||
                   ( currentInstLoc < craftInstListEndLoc &&
                    (listLocToTypeID.count(currentInstLoc + 1) != 0 &&
                     listLocToTypeID[currentInstLoc + 1].first != InstTypeLaserSdk::ARC_START_INST_LASER_WELD_SDK) ) )
                {
                    int instType = listLocToTypeID[currentInstLoc].first;
                    int instID = listLocToTypeID[currentInstLoc].second;
                    if (instType == InstTypeLaserSdk::CRAFT_SELECT_INST_LASER_WELD_SDK)
                    {
                        bodyScriptText += "----change present craft----\n";
                        CraftInfoLaserSdk curCraftInfo = craftsMap[instID];
                        if (!isWeld)
                        {
                            LaserSdkResult result = curCommuFunction->craft_script_configure_machine_param_set_script_get(laserMachineHandle, curCraftInfo, bodyScriptText);
                            if(result != LaserSdkResult::SUCCESS_LASER_SDK)
                            {
                                return result;
                            }
                        }
                        if(isWeld)
                        {
                            LaserSdkResult result = curCommuFunction->craft_script_write_craft_resource_script_get(laserMachineHandle, curCraftInfo, bodyScriptText);
                            if(result != LaserSdkResult::SUCCESS_LASER_SDK)
                            {
                                return result;
                            }
                        }

                    }
                    else if (instType == InstTypeLaserSdk::SAFE_POINT_MOVEJ_INST_LASER_WELD_SDK)
                    {
                        if (!isWeld)
                        {
                            curCommuFunction->craft_script_judge_break_point_script_get(currentInstLoc, bodyScriptText);
                        }
                        bodyScriptText += "--safePointMoveJ--\n";
                        bodyScriptText += strReplace("movej(%1,%2,%3,%4)\n", {doubleArray2Str(tempPointsMap[instID].joint, 6), std::to_string(emptyRunJointSpeed), std::to_string(emptyRunJointAcc), std::to_string(emptyPointTransitionRadius)});
                        if (!isWeld)
                        {
                            bodyScriptText += strWriteInt32RegReplace(WeldControlAddressLaserSdk::INT32_REG_ARRIVE_POINT_LOC_663, currentInstLoc);
                            curCommuFunction->craft_script_judge_signal_setp_run_script_get(currentInstLoc, bodyScriptText);
                        }
                    }
                    else if (instType == InstTypeLaserSdk::LINE_POINT_INST_LASER_WELD_SDK)
                    {
                        if (!isWeld)
                        {
                            curCommuFunction->craft_script_judge_break_point_script_get(currentInstLoc, bodyScriptText);
                        }
                        bodyScriptText += "--linePoint--\n";
                        bodyScriptText += strReplace("movel(%1,%2,%3,%4)\n", {doubleArray2Str(tempPointsMap[instID].pose, 6), std::to_string(emptyRunTCPSpeed), std::to_string(emptyRunTCPAcc), std::to_string(emptyPointTransitionRadius)});
                        if (!isWeld)
                        {
                            bodyScriptText += strWriteInt32RegReplace(WeldControlAddressLaserSdk::INT32_REG_ARRIVE_POINT_LOC_663, currentInstLoc);
                            curCommuFunction->craft_script_judge_signal_setp_run_script_get(currentInstLoc, bodyScriptText);
                        }
                    }
                    else if (instType == InstTypeLaserSdk::CIRCLE_POINT_INST_LASER_WELD_SDK)
                    {
                        if (!isWeld)
                        {
                            curCommuFunction->craft_script_judge_break_point_script_get(currentInstLoc + 1, bodyScriptText);
                        }
                        bodyScriptText += "--circlePoint--\n";
                        int nextCirclePointID = listLocToTypeID[currentInstLoc + 1].second;
                        //点1 ，点2， 速度， 加速度， 过渡半径(单位为 mm) ， 旋转方向(0为顺时针，1为逆时针)
                        bodyScriptText += strReplace("movec(%1,%2,%3,%4,%5,%6)\n",
                            {doubleArray2Str(tempPointsMap[instID].pose, 6), 
                             doubleArray2Str(tempPointsMap[nextCirclePointID].pose, 6),
                            std::to_string(emptyRunTCPSpeed), std::to_string(emptyRunTCPAcc), std::to_string(emptyPointTransitionRadius), std::to_string(0)});
                        if (!isWeld)
                        {
                            bodyScriptText += strWriteInt32RegReplace(WeldControlAddressLaserSdk::INT32_REG_ARRIVE_POINT_LOC_663, currentInstLoc + 1);
                            curCommuFunction->craft_script_judge_signal_setp_run_script_get(currentInstLoc + 1, bodyScriptText);
                        }

                        // 跳过下一个圆弧定点并将最后一个定点的列表位置复制为下一个圆弧定点的位置
                        currentInstLoc++;
                    }
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
                            bodyScriptText += strWriteInt32RegReplace(WeldControlAddressLaserSdk::INT32_REG_ARRIVE_POINT_LOC_663, currentInstLoc);
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



    std::string script;
    script += curCommuFunction->loadLuaFunctionScriptOut();//调用函数定义
    script += breakPointListScript; //断电列表
    script += signalSetpPointLocListScript; //单步运行列表序列
    script += strWriteInt32RegReplace(WeldControlAddressLaserSdk::INT32_REG_WELD_MODE_660, isWeld ? 2 : 1); //执行模式：0.空状态 1.轨迹模拟 2.焊接
    script += bodyScriptText;
    //测试代码scripts
    if(isWeld)
    {    
        std::ofstream file("./startWeldFun.txt");
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

LaserSdkResult cl_craft_end_arc_subprogram_script_get(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen)
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

    std::string endArcProtectionScript = R"(
    function endArcing()        
        set_configurable_digital_out(0, false)  ---CO0 = 0
        set_configurable_digital_out(1, false)  ---CO1 = 0
        write_bool_register(608, 0)    ---开始焊接复位
        n = read_integer_register(673)     --起弧异常判断
        if (n == 1) then
            print("起弧异常判断")
            for i = 1, 5 do
                print("Rewrite the arcEnd")
                write_integer_register(608, 0)    ---开始焊接
                wait(100)
                tempPdo = read_integer_register(673) --焊接反馈地址判断是否起弧成功
                if (tempPdo == 0) then
                    print("Re-arcEnd successful")
                    break
                end
                if (i >= 5) then
                    write_integer_register(675, 1)   -- 收弧异常信号
                    print("Failed to re-arcEnd")
                end
            end
        end
        write_bool_register(600, 1)    -- 末端按钮拖动状态控制位设置有效
    end

    --程序停止后自动收弧
    function stop_func()
        set_configurable_digital_out(0, false)  ---CO0 = 0
        set_configurable_digital_out(1, false)  ---CO1 = 0
        if(read_integer_register(614) == 1) then -- 程序停止后自动收弧开启状态位
            endArcing()
            wait(1)
            write_integer_register(674, 1)   -- 焊接中断信号
            write_integer_register(614, 0)   -- 自动收弧复位
            halt()
        end
    end
    sysEvent_stop_func_1_Result = sys_event_register(1, stop_func)

    --程序暂停 自动收弧
    function pause_func()
        while(true)
        do
            endArcing()
            write_integer_register(614, 0)  -- 自动收弧复位
            sync()
        end
    end
    sysEvent_pause_func_2_Result = sys_event_register(2, pause_func)

    function resume_func()
        write_bool_register(600, 0)     -- 末端按钮拖动位
        write_integer_register(614, 1)  -- 自动收弧信号
        sync()
    end
    sysEvent_resume_func_3_Result = sys_event_register(3, resume_func)
    )";

    std::string scriptText;
    scriptText += endArcProtectionScript;
    if(scriptText.size() > *scriptLen)
    {
        return MEMORY_ERROR_LASER_SDK;
    }
    *scriptLen = scriptText.size()+1;
    memcpy(scripts, scriptText.c_str(), *scriptLen);
    memcpy(scripts + scriptText.size(), "\0", 1);

    return SUCCESS_LASER_SDK;
}

LaserSdkResult cl_craft_after_weld_end_operate_script_get(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen)
{
    std::string script;

    int programStopArcEndAddress = WeldControlAddressLaserSdk::INT32_REG_PROGRAM_STOP_ARC_END_614;   // 程序停止后自动收弧开启状态位：1 有效 0 无效
    int signalSetpRunAddress = WeldControlAddressLaserSdk::INT32_REG_SINGLE_STEP_RUN_616;  // 单步运行状态位：1 有效 0 无效

    script += strWriteInt32RegReplace(programStopArcEndAddress, 0); // 程序停止后自动收弧关闭
    script += strWriteInt32RegReplace(signalSetpRunAddress, 0);

    if(script.size() + 1 > *scriptLen)
    {
        return LaserSdkResult::MEMORY_ERROR_LASER_SDK;
    }
    *scriptLen = script.size()+1;
    memcpy(scripts, script.c_str(), script.size());
    memcpy(scripts + script.size(), "\0", 1);
    return SUCCESS_LASER_SDK;
}

//直线 计算 降功率点
LaserSdkResult cl_craft_straight_line_back_point_cal(const LaserMachineHandle laserMachineHandle, RollbackStraightLineData* rollbackStraightLineData)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    // 调用 getNewTeachPoints 方法计算新的起点和终点
    LaserSdkResult result = getStraightLineSlowUpPoint(
                                machineResource.robotHandle,
                                rollbackStraightLineData->startPoint,
                                rollbackStraightLineData->endPoint,
                                rollbackStraightLineData->distance,
                                rollbackStraightLineData->refJoint,
                                rollbackStraightLineData->pointOut,
                                rollbackStraightLineData->pointJointOut
                            );

    if (result != LaserSdkResult::SUCCESS_LASER_SDK)
    {
        std::cerr << "Failed to calculate new teach points." << std::endl;
        return result;
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}


//圆弧 计算 降功率点
LaserSdkResult cl_craft_circle_back_point_cal(const LaserMachineHandle laserMachineHandle, RollbackCircleData* rollbackCircleData)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    // 调用 getNewTeachPoints 方法计算新的起点和终点
    LaserSdkResult result = getCirclrSlowDownPoints(
                                machineResource.robotHandle,
                                rollbackCircleData->startPoint,
                                rollbackCircleData->transPoint,
                                rollbackCircleData->endPoint,
                                rollbackCircleData->stopPoint,
                                rollbackCircleData->distance,
                                rollbackCircleData->refJoint,
                                rollbackCircleData->stopMappingPoint,
                                rollbackCircleData->newTransPoint,
                                rollbackCircleData->newTransJointPoint,
                                rollbackCircleData->startPointOut,
                                rollbackCircleData->transPointOut,
                                rollbackCircleData->endPointOut,
                                rollbackCircleData->startPointJointOut,
                                rollbackCircleData->transPointJointOut,
                                rollbackCircleData->endPointJointOut
                            );

    if (result != LaserSdkResult::SUCCESS_LASER_SDK)
    {
        std::cerr << "Failed to calculate new teach points." << std::endl;
        return result;
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//缓升起弧
LaserSdkResult cl_get_arc_start_slow_script(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen)
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
        //配置串口信息
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
    // 开光功率为开光一瞬间的功率，是百分比值；
    // 激光器功率为激光器希望激光所能到达到的功率，开光功率的百分比就是乘的激光器功率
    // 峰值功率为激光头所限制激光器的最大功率
    std::vector<unsigned int> laserPowerVec =
    {
        machineResource.gasONDelayLoc,              ///<  开气延时地址 0x0235  先等送气结束
        machineResource.laserOnPowerLoc,            ///<  开光功率百分比地址 0x0231 561
        machineResource.laserPowerLoc,              ///<  激光器功率地址 0x0230 569

        machineResource.laserHeadXOffsetLoc,        ///<  X轴偏移地址 0x023b 571
        machineResource.laserHeadYScaleLoc,         ///<  Y轴放大地址 0x0238 568
        machineResource.laserHeadXScaleLoc,         ///<  X轴放大地址 0x023a 570
        machineResource.laserHeadYOffsetLoc         ///<  Y轴偏移地址 0x0239 578
    };


    //需要设置的激光头地址参数 - 扫描速度 - 扫描宽 - 峰值功率 - 占空比 - 脉冲频率 - 扫描高度 - 旋转角度 - 图形切换
    std::vector<unsigned int> laserHeadVec =
    {
        machineResource.laserHeadPatternIdLoc,      //图形首位
        machineResource.laserHeadScanSpeedLoc,
        machineResource.laserHeadScanWidthLoc,
        machineResource.laserHeadPeakPowerLoc,      ///<  峰值功率
        machineResource.laserHeadDutyCycleLoc,
        machineResource.laserHeadFrequencyLowLoc,   ///<  脉冲频率 : 15~0位地址  0x0005  5
        machineResource.laserHeadFrequencyHighLoc,  ///<  脉冲频率 : 31~16位地址 0x0004  4
        machineResource.laserHeadScanHeightLoc,     ///<  扫描高度地址 0x0006 6
        machineResource.laserHeadRotationAngleLoc,  ///<  旋转角度地址 0x0007 7
    };

    //送丝机地址参数
    std::vector<unsigned int> wireFeedVec =
    {
        machineResource.wireFeederStatusLoc,       ///<  送丝使能 0x0115
        machineResource.wireFeedSignalLoc,         ///<  送丝信号开 0x0032
        //machineResource.operationModeLoc,          ///<  写模式 0x010c
        machineResource.autoWireFeedSpeedLoc,      ///<  自动送丝速度 0x0100
    };

    CgxiLaserHeadLaserSdk &laserHeadParams  = machineResource.laserHeadParams;
    CgxiWireFeederLaserSdk &wireFeederParams  = machineResource.wireFeederParams;
    CgxiLaserPowerLaserSdk &laserPowerParams  = machineResource.laserPowerParams;
    int time = laserPowerParams.gasStartupAdvanceTime;

    //获取脚本
    std::string scriptGet;

    try
    {
        //激光器
        checkLaserSdkResult(curCommuFunction->craft_laser_param_set_script_get(laserMachineHandle, laserPowerVec, scriptGet));
        //送丝机
        checkLaserSdkResult(curCommuFunction->craft_wire_feeder_param_set_script_get(laserMachineHandle, wireFeedVec, scriptGet));
        //激光头
        checkLaserSdkResult(curCommuFunction->craft_laser_head_param_set_script_get(laserMachineHandle, laserHeadVec, scriptGet));

        std::string arcStartProgram1 = "function arcStartProgram()\n";
        std::string arcStartParam =
                "laserPower = " + std::to_string(laserHeadParams.laserHeadPeakPower) + "\n" +
                "openPower = " + std::to_string(laserPowerParams.laserPowerOpen) + "\n" +
                "openLightTime = " + std::to_string(laserPowerParams.laserRampUpTime) + "\n" +
                "wirefeedeOpenSendSpeed = " + std::to_string(wireFeederParams.wireFeederStartMotorSpeed) + "\n" +
                "wirefeedeWeldSendSpeed = " + std::to_string(wireFeederParams.wireFeederMotorSpeed);
        std::ostringstream oss ;
        oss<<R"(
            function slow_ascent(laserPower, openPower, wirefeedeOpenSendSpeed, wirefeedeWeldSendSpeed, openLightTime)
                -- 计算功率差，送丝速度差
                local v = laserPower - openPower
                local s = wirefeedeWeldSendSpeed - wirefeedeOpenSendSpeed

                -- 计算降几次
                local num = openLightTime // 200

                -- 计算每次降多少
                local powerSingle = math.floor(v / num)
                local speedSingle = math.floor(s / num)

                -- 循环逐步调整功率和送丝速度
                for i = 1, num + 1 do
                    print("power: ", i)
                    write_value_port(0x0906, 0x0033, (openPower + (powerSingle * i)), 3, 0)
                    --write_laser_power(openPower + (powerSingle * i))    -- 调整激光功率
                    wait(100)  -- 等待 100 毫秒
                    write_value_port(0x0A06, 0x0100, (wirefeedeOpenSendSpeed + (speedSingle * i)), 2, 0)
                    --write_wirefeede_auto_send_speed(wirefeedeOpenSendSpeed + (speedSingle * i))  -- 调整送丝速度
                    wait(10)   -- 等待 10 毫秒
                end
            end

            i = 0
            res = false
            while (i<5) and (not res) do
                i=i+1
                set_configurable_digital_out(0,true)
                res = get_configurable_digital_out(1)
                print("res:",res)
                wait(5)
            end
            if(not res) then
                print("启动失败")
                return
            end
            --开气的延迟等待，不需要继续往下走
            wait()" << time << R"()  -- 替换动态变量
            ask_thread = task_create(slow_ascent)
        )";

        std::string arcStartProgram = oss.str();
        std::string arcStartProgram2 = "end\n"
                                       "arcStartProgram_Result=task_create(arcStartProgram)\n";
        scriptGet = arcStartProgram1 + scriptGet + arcStartParam + arcStartProgram + arcStartProgram2;
        std::string funcScript = curCommuFunction->loadLuaFunctionScriptOut();  //功能函数
        scriptGet = funcScript + scriptGet;
        //scriptGet = funcScript + scriptGet;
        //脚本赋值
        if(!strToChar(scripts, scriptLen, scriptGet))
        {
            return LaserSdkResult::MEMORY_ERROR_LASER_SDK;
        }
        //将脚本存入示教器日志中
        else
        {
            // 打开文件进行写入
            std::ofstream outFile("script.txt");

            if (!outFile) {
                std::cerr << "无法打开文件进行写入！" << std::endl;
            }
            outFile << scripts;

            // 关闭文件
            outFile.close();
        }
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//缓降收弧
LaserSdkResult cl_get_arc_stop_slow_script(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen)
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
        //配置串口信息
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

    // 需要设置的激光器地址参数 - 激光器功率 - 开光功率 - 开光渐进时间 -
    // 开光功率为开光一瞬间的功率，是百分比值；
    // 激光器功率为激光器希望激光所能到达到的功率，开光功率的百分比就是乘的激光器功率
    // 峰值功率为激光头所限制激光器的最大功率
    std::vector<unsigned int> laserPowerVec =
    {
        machineResource.gasOffDelayLoc,             ///<  关气延迟地址 0x0236
        machineResource.laserOffPowerLoc,           ///<  关光功率百分比地址 0x0230 569
    };

    //送丝机地址参数
    std::vector<unsigned int> wireFeedVec =
    {
        machineResource.autoWireFeedSpeedLoc,       ///<  自动送丝速度 0x0100
        //machineResource.wirefeederCompensateDelay,  ///<  补丝延迟 0x0106
        machineResource.wireRetractLengthLoc,       ///<  回抽丝长度 0x0104
        machineResource.wireFeedLengthLoc           ///<  补丝长度 0x0105
    };

    CgxiLaserHeadLaserSdk &laserHeadParams  = machineResource.laserHeadParams;
    CgxiWireFeederLaserSdk &wireFeederParams  = machineResource.wireFeederParams;
    CgxiLaserPowerLaserSdk &laserPowerParams  = machineResource.laserPowerParams;
    //获取脚本
    std::string scriptGet;
    try
    {
        //激光器
        checkLaserSdkResult(curCommuFunction->craft_laser_param_set_script_get(laserMachineHandle, laserPowerVec, scriptGet));
        //送丝机
        checkLaserSdkResult(curCommuFunction->craft_wire_feeder_param_set_script_get(laserMachineHandle, wireFeedVec, scriptGet));

        std::string arcStartProgram1 = "function arcStartProgram()\n";
        std::string arcStartParam =
                "laserPower = " + std::to_string(laserHeadParams.laserHeadPeakPower) + "\n" +
                "closePower  = " + std::to_string(laserPowerParams.laserPowerClose) + "\n" +
                "closeLightTime  = " + std::to_string(laserPowerParams.laserRampDownTime) + "\n" +
                "wirefeedeCloseSendSpeed = " + std::to_string(wireFeederParams.wireFeederStopMotorSpeed) + "\n" +
                "wirefeedeWeldSendSpeed = " + std::to_string(wireFeederParams.wireFeederMotorSpeed);
        std::string arcStartProgram =
        R"(
            --laserPower, closePower, wirefeedeCloseSendSpeed, wirefeedeWeldSendSpeed, closeLightTime
            function slow_descent()
                -- 计算功率差，送丝速度差
                local v = laserPower - closePower
                local s = wirefeedeWeldSendSpeed - wirefeedeCloseSendSpeed

                -- 计算降几次
                local num = closeLightTime // 200

                -- 计算每次降多少
                local powerSingle = math.floor(v / num)
                local speedSingle = math.floor(s / num)

                -- 循环逐步调整功率和送丝速度
                for i = 1, num + 1 do
                    print("power: ", i)
                    write_value_port(0x0906, 0x0033, (laserPower - (powerSingle * i)), 3, 0)
                    --write_laser_power((laserPower - (powerSingle * i))    -- 调整激光功率
                    wait(10)  -- 等待 10 毫秒
                    write_value_port(0x0A06, 0x0100, (wirefeedeWeldSendSpeed - (speedSingle * i)), 2, 0)
                    --write_wirefeede_auto_send_speed(wirefeedeWeldSendSpeed - (speedSingle * i))  -- 调整送丝速度
                    wait(10)   -- 等待 10 毫秒
                end
            end

            ask_thread = task_create(slow_descent)
        )";
        std::string arcStartProgram2 = "end\n"
                                       "arcStartProgram_Result=task_create(arcStartProgram)\n";
        scriptGet = arcStartProgram1 + scriptGet + arcStartParam + arcStartProgram + arcStartProgram2;
        std::string funcScript = curCommuFunction->loadLuaFunctionScriptOut();  //功能函数
        scriptGet = funcScript + scriptGet;
        //scriptGet = funcScript + scriptGet;
        //脚本赋值
        if(!strToChar(scripts, scriptLen, scriptGet))
        {
            return LaserSdkResult::MEMORY_ERROR_LASER_SDK;
        }
        //将脚本存入示教器日志中
        else
        {
            // 打开文件进行写入
            std::ofstream outFile("script.txt");

            if (!outFile) {
                std::cerr << "无法打开文件进行写入！" << std::endl;
            }
            outFile << scripts;

            // 关闭文件
            outFile.close();
        }
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}


//设备状态监控
LaserSdkResult cl_craft_weld_status_monitor_script_get(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen)
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
        //配置串口信息
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
        //脚本
        std::string globalVar = "local SendDataType = 0 --bool bit register, send byte data through bool register\n"
                                "local RecvDataType = 1 --16-bit register, received data is stored in 16-bit register\n"
                                "local SendCommuRegAddr = 0		--Send command starts storing from address 0 of bool communication variable\n"
                                "local SendCommuReg_n = 16		--Send command starts storing from address 0 of bool communication variable\n"
                                "local RecvCommuRegAddr = 30		--Received data starts storing from address 30 of 16-bit communication variable\n"
                                "local RecvCommuReg_n = 60		--Receive 50 bytes\n"
                                "local alarm_bits_list = {0, 1, 2, 4, 5, 10, 11, 14}	-- Alarm bit types\n";
        std::string scriptGet=
                "--Status Monitoring--\n"
                "while true do\n"
                "    -- Lock\n"
                "    -- while(read_int16_register(900)~=0) do\n"
                "    --   wait(50)\n"
                "    -- end\n"
                "    -- Laser system not enabled status, whether there is general alarm\n"
                "    read_value_port(0x0903, " + std::to_string(machineResource.laserHeadStatusLoc) + " , 0x0003, 3, 0, 16)\n"
                "    local report = read_registers_as_hex(30, 60)\n"
                "    print(\"Concatenated message:\", report)\n"
                "    local swapReport = get_swap_report(report)\n"
                "    local val = get_register_value(swapReport,1)\n"
                "    print(\"Laser system not enabled status:\", val)\n"
                "    write_integer_register(801, val)\n"
                "    local val = get_register_value(swapReport,2)\n"
                "    print(\"General alarm status:\", val)\n"
                "    write_integer_register(802, val)\n"
                "    local val = get_register_value(swapReport,3)\n"
                "    parse_alarm_word(val)\n"
                "    print(\"General alarm status 2:\", val)\n"
                "\n"
                "    -- Laser trigger status, Laser alarm status, Safety lock input status, Air pressure alarm input status, Wire feed enable status, PWM output status, Gas valve output status, DAC analog output status\n"
                "    read_value_port(0x0903, " + std::to_string(machineResource.laserTriggerSignalLoc) + " , 0x000a, 3, 0, 16)\n"
                "    local report = read_registers_as_hex(30, 60)\n"
                "    print(\"Concatenated message:\", report)\n"
                "    local swapReport = get_swap_report(report)\n"
                "    local val = get_register_value(swapReport,1)\n"
                "    print(\"Laser trigger status:\", val)\n"
                "    write_integer_register(813, val)\n"
                "    local val = get_register_value(swapReport,3)\n"
                "    print(\"Safety lock input status:\", val)\n"
                "    write_integer_register(814, val)\n"
                "    local val = get_register_value(swapReport,6)\n"
                "    print(\"Wire feed enable status:\", val)\n"
                "    write_integer_register(815, val)\n"
                "    local val = get_register_value(swapReport,7)\n"
                "    print(\"PWM output status:\", val)\n"
                "    write_integer_register(816, val)\n"
                "    local val = get_register_value(swapReport,9)\n"
                "    print(\"Gas valve output status:\", val)\n"
                "    write_integer_register(817, val)\n"
                "    local val = get_register_value(swapReport,10)\n"
                "    print(\"DAC analog output status:\", val)\n"
                "    write_integer_register(818, val)\n"
                "\n"
                "    -- Wire feeder connection status\n"
                "    read_value_port(0x0903, " + std::to_string(machineResource.wireFeederConnectedLoc) + ", 0x0001, 3, 0, 16)\n"
                "    local report = read_registers_as_hex(30, 35)\n"
                "    print(\"Concatenated message:\", report)\n"
                "    local swapReport = get_swap_report(report)\n"
                "    local val = get_register_value(swapReport,1)\n"
                "    print(\"Wire feeder connection status:\", val)\n"
                "    write_integer_register(812, val)\n"
                "\n"
                "    -- Laser status, Air pressure status\n"
                "    read_value_port(0x0903, " + std::to_string(machineResource.laserAlarmCodeLoc) + ", 0x0001, 3, 0, 16)\n"
                "    local report = read_registers_as_hex(30, 40)\n"
                "    print(\"Concatenated message:\", report)\n"
                "    local swapReport = get_swap_report(report)\n"
                "    local val = get_register_value(swapReport,1)\n"
                "    print(\"Laser status:\", val)\n"
                "    write_integer_register(819, val)\n"
                "\n"
                "    -- Wire feeder overcurrent, Wire feeder running status\n"
                "    read_value_port(0x0A03," + std::to_string(machineResource.wireFeederStatusLoc) + ",0x0001, 2,  0, 16)\n"
                "    local report = read_registers_as_hex(30, 40)\n"
                "    print(\"Concatenated message:\", report)\n"
                "    local swapReport = get_swap_report(report)\n"
                "    local val = get_register_value(swapReport,1)\n"
                "    print(\"Wire feeder running status:\", val)\n"
                "    write_integer_register(800, val)\n"
                "\n"
                "    -- Release lock\n"
                "    -- write_int16_register(900,0)\n"
                "    -- Wait 500ms then continue loop\n"
                "    wait(400)\n"
                "end\n";
        //功能函数
        std::string funcScript = "--cl_craft_weld_status_monitor_script_get--";

        //scriptGet = globalVar + funcScript + scriptGet;
        scriptGet = funcScript;
        //脚本赋值
        if(!strToChar(scripts, scriptLen, scriptGet))
        {
            return LaserSdkResult::MEMORY_ERROR_LASER_SDK;
        }
        //将脚本存入示教器日志中
        else
        {
            // 打开文件进行写入
            std::ofstream outFile("script.txt");

            if (!outFile) {
                std::cerr << "无法打开文件进行写入！" << std::endl;
            }
            outFile << scripts;

            // 关闭文件
            outFile.close();
        }
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}


LaserSdkResult cl_craft_lift_pose_cal(const LaserMachineHandle laserMachineHandle, const double* pose, const double* joint, const double liftDistance,
                                      double* liftPose, double* liftJoint)
{
    ///< 焊枪姿态调整
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    int tcptype = TCPTYPE;
    double d2r = PI_HY/180;
    double r2d = 180.0/PI_HY;
    double delta_pose[6] = { 0, 0, tcptype*liftDistance, 0, 0, 0 };
    double pose_deg[6] = {pose[0], pose[1], pose[2], pose[3]*d2r, pose[4]*d2r, pose[5]*d2r };
    pose_trans_HY(pose_deg, delta_pose, liftPose);
    liftPose[3] *= r2d;
    liftPose[4] *= r2d;
    liftPose[5] *= r2d;

    double liftPoseTmp[6], jointTmp[6], liftJointTmp[6];
    copy_pose_HY(liftPose, liftPoseTmp);
    copy_pose_HY(joint, jointTmp);
    copy_pose_HY(liftJoint, liftJointTmp);

    LaserSdkResult res = kineInverse_tcpPose(machineResource.robotHandle, pose_deg, jointTmp , liftJointTmp);//逆解
    if (res != LaserSdkResult::SUCCESS_LASER_SDK)
    {
        return res;
    }
    
    copy_pose_HY(liftJointTmp, liftJoint);

    return SUCCESS_LASER_SDK;
}


LaserSdkResult cl_craft_device_status_modify(const LaserMachineHandle laserMachineHandle, char* scripts, int* scriptLen)
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
        machineResource.laserPowerLoc,              ///<  激光器功率地址 0x0230 560
        machineResource.driverTempLoc,              ///<  驱动器温度值地址 0x023f 575
        machineResource.protectiveWindowTempLoc,    ///<  保护镜温度值地址 0x0240 576
        machineResource.collimatorTempLoc           ///<  准直镜温度值地址 0x0241 577
    };

    std::vector<unsigned int> laserHeadVec =
    {
        machineResource.laserHeadDutyCycleLoc,
        machineResource.laserHeadFrequencyHighLoc,
        machineResource.laserHeadFrequencyLowLoc,

    };

    //送丝机地址参数 - 自动送丝速度
    std::vector<unsigned int> wireFeedVec =
    {
        0x0115,
        machineResource.manualWireFeedSpeedLoc,       ///<  手动送丝速度地址 0x0101 257
        machineResource.manualWireRetractSpeedLoc,       ///<  手动抽丝速度地址 0x0102 258
    };

    //获取脚本
    std::string scriptGet;
    try
    {
        checkLaserSdkResult(curCommuFunction->craft_laser_head_param_set_script_get(laserMachineHandle, laserHeadVec, scriptGet));
        //激光器
        checkLaserSdkResult(curCommuFunction->craft_laser_param_set_script_get(laserMachineHandle, laserPowerVec, scriptGet));
        //送丝机
        checkLaserSdkResult(curCommuFunction->craft_wire_feeder_param_set_script_get(laserMachineHandle, wireFeedVec, scriptGet));
        scriptGet = curCommuFunction->loadLuaFunctionScriptOut() + scriptGet;     
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }

    //脚本赋值
    if(!strToChar(scripts, scriptLen, scriptGet))
    {
        return LaserSdkResult::MEMORY_ERROR_LASER_SDK;
    }
    else
    {
        // 打开文件 写入脚本
        std::ofstream outFile("device_status_modify_script.txt");
        if (!outFile) {
            std::cerr << "无法打开文件进行写入！" << std::endl;
        }
        outFile << scripts;
        outFile.close();
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}

LaserSdkResult cl_craft_inst_devices_params_script_get(const LaserMachineHandle laserMachineHandle, CraftInfoLaserSdk craftInfo, char *scripts, int *scriptLen)
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
    //获取脚本
    std::string scriptGet;
    try
    {
        checkLaserSdkResult(curCommuFunction->craft_script_configure_machine_param_set_script_get(laserMachineHandle, craftInfo, scriptGet));

        scriptGet = curCommuFunction->loadLuaFunctionScriptOut() + scriptGet;
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }


    //脚本赋值
    if(!strToChar(scripts, scriptLen, scriptGet))
    {
        return LaserSdkResult::MEMORY_ERROR_LASER_SDK;
    }
    //将脚本存入示教器日志中
    else
    {
        // 打开文件进行写入
        std::ofstream outFile("script.txt");

        if (!outFile) {
            std::cerr << "无法打开文件进行写入！" << std::endl;
        }
        outFile << scripts;

        // 关闭文件
        outFile.close();
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}


LaserSdkResult cl_message_generate(const LaserMachineHandle laserMachineHandle, uint16_t firstAddr, uint16_t parameter, uint8_t slaveAddr, uint8_t funcCode, uint8_t *message,int *length)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    // 生成报文
    if (message != nullptr)
    {
        *message = 0;  // 初始化报文
    }
    funcCode = 0x06;    //写死
    std::vector<uint8_t> msg;
    std::map<uint16_t, uint16_t> msgMap;
    msgMap.insert(std::make_pair(firstAddr, parameter));
    msg = buildMessageVector(msgMap, slaveAddr, funcCode);
    int len = static_cast<int>(msg.size());
    if(*length != len)
    {
        std::cout << len << std::endl;
        *length = len;
    }

    memcpy(message, msg.data(), msg.size()); // 将报文数据复制到message中

    return LaserSdkResult::SUCCESS_LASER_SDK;
}

LaserSdkResult cl_parse_modbus_to_int16_register(const LaserMachineHandle laserMachineHandle,const char* hexResponse, const int regIndex, const int regLen,  int16_t* values)
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
        std::string hexResponseStr = hexResponse;
        int16_t* tempValues  = parse_modbus_register(hexResponseStr, regIndex, regLen);
        // 将数据复制到输出参数
        for (int i = 0; i < regLen; i++) {
            values[i] = tempValues[i];
        }
        // 释放临时数组
        delete[] tempValues;
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}

LaserSdkResult cl_write_message_to_bool_register(const LaserMachineHandle laserMachineHandle, unsigned int firstRegister, unsigned int registerNums, uint8_t *message)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    std::vector<int> tmpValue;
    for(unsigned int i = 0; i < registerNums; ++i)
    {
        tmpValue.push_back(message[i]);
    }
    cr_set_boolRegValue(machineResource.robotHandle, firstRegister, registerNums, (int *)tmpValue.data());
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

LaserSdkResult cl_write_message_to_Int16_register(const LaserMachineHandle laserMachineHandle, unsigned int firstRegister, unsigned int registerNums, uint8_t *message)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    std::vector<unsigned int> tmpValue;
    for(unsigned int i = 0; i < registerNums; ++i)
    {
        tmpValue.push_back( *(unsigned short*)&message[i*2]);
    }

    cr_set_int16RegValue(machineResource.robotHandle, firstRegister, registerNums, (int *)tmpValue.data());

    return LaserSdkResult::SUCCESS_LASER_SDK;
}


LaserSdkResult cl_parse_modbus_to_int32_register(const LaserMachineHandle laserMachineHandle,const char* hexResponse, int32_t &value)
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
        std::string hexResponseStr = hexResponse;
        value = parse_modbus_register32(hexResponseStr);
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}

LaserSdkResult cl_write_state_monitor_message_to_Int32_register(const LaserMachineHandle laserMachineHandle, unsigned int firstRegister)
{
   if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
   {
       return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
   }
   MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
   // int32寄存器参数值列表（按顺序排列）
   int32_t register_values[] = {
       352387850,      // int32Reg_872
       1234501888,      // int32Reg_873
       2097929,        // int32Reg_850
       -2104622848,    // int32Reg_851
       18875145,       // int32Reg_852
       1121911040,     // int32Reg_853
       35652361,       // int32Reg_854
       1110376704,     // int32Reg_855

       772014857,      // int32Reg_856
       2078540032,     // int32Reg_857
       151257865,      // int32Reg_858
       1884553472,     // int32Reg_859
       184812297,      // int32Reg_860
       -1326120704,    // int32Reg_861
       235143945,      // int32Reg_862
       -1310392064,    // int32Reg_863
       251921161,      // int32Reg_864
       1907622144,     // int32Reg_865
       285475593,      // int32Reg_866
       2010382592,     // int32Reg_867
       302252809,      // int32Reg_868
       1998848256,     // int32Reg_869
       1006764809,     // int32Reg_870
       -163315456     // int32Reg_871
   };
   CRresult cRes = cr_set_int32RegValue(machineResource.robotHandle, firstRegister, 24, register_values);
   if(cRes != CRresult::sucess)
   {
       return crsdk_return_value_transfer(cRes);
   }
   return LaserSdkResult::SUCCESS_LASER_SDK;
}
