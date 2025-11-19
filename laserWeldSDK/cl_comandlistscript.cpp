#include "cl_comandlistscript.h"
#include "cl_utils.h"
#include "cl_internaldatadefine.h"
#include "cl_craftscript.h"
#include "cl_message_prase.h"
#include "cl_math.h"

//返回相应的工艺通讯
LaserSdkResult craft_script_commu_class_get(LaserMachineHandle laserMachineHandle, CommuBase *&craftCommuClass)
{
    //句柄
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    craftCommuClass = nullptr;
    if (machineResource.commuType == CommuTypeLaserSdk::RS485_2_LASER_SDK)
    {
        craftCommuClass = static_cast<CommuRS485*>(CommuRS485::GetInstance());
        if (craftCommuClass == nullptr)
        {
            return LaserSdkResult::COMMU_ERROR_LASER_SDK;
        }
    }
    else
    {
        return LaserSdkResult::COMMU_ERROR_LASER_SDK;
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}


LaserSdkResult GetBasicCraftScript(LaserMachineHandle laserMachineHandle, LaserWeldStartData weldStartData, InstListData &instListData, std::unordered_map<int, PointDataLaserSdk> tempPointsMap, InstListTransEnvironmentData &instListTransEnvironmentData, int instListStartLoc, int instListEndLoc, std::string &bodyScriptText)
{
    //句柄
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    CgxiRobotLaserSdk &robotParams = machineResource.robotParams;
    double emptyRunTCPSpeed = robotParams.emptyRunTCPSpeed;            //空走TCP速度
    double emptyRunTCPAcc = robotParams.emptyRunTCPAcc;              //空走TCP加速度
    double weldTCPAcc = robotParams.weldTCPAcc;                         //焊接TCP加速度
    double emptyRunJointSpeed = robotParams.emptyRunJointSpeed;          //空走关节速度
    double emptyRunJointAcc = robotParams.emptyRunJointAcc;            //空走关节加速度
    double radiusEmpty = robotParams.emptyPointTransitionRadius;  //空走点位过渡半径
    
    double radiusWelding = robotParams.weldPointTransitionRadius;   //焊接点位过渡半径
    if (radiusWelding < 0.0 || radiusWelding > 10.0)
    {
        radiusWelding = 0.0;
    }

    CommuBase* craftCommu = nullptr;
    LaserSdkResult result = craft_script_commu_class_get(laserMachineHandle, craftCommu);
    if (result != LaserSdkResult::SUCCESS_LASER_SDK)
    {
        return result;
    }
    
    std::unordered_map<int, std::pair<InstTypeLaserSdk, int>>& listLocToTypeID = instListData.listLocToTypeID; ///< 指令列表 指令类型 到 类型ID 的映射
    std::unordered_map<int, CraftInfoLaserSdk>& craftsMap = instListData.craftsMap;     ///< 工艺信息

    int CraftID = instListTransEnvironmentData.craftID;
    CraftInfoLaserSdk& curCraftInfo = craftsMap[CraftID];       ///< 当前工艺信息
    
    bool isWeld = instListTransEnvironmentData.isWeld;           ///< 真的焊接（1）还是模拟
    int& prePointLoc = instListTransEnvironmentData.prePointLoc; ///< 前一个点的位置
    bool& isWelding = instListTransEnvironmentData.isWelding;    ///< 是否焊接中
    double weldSpeed = curCraftInfo.craftWeldingSpeed;           ///< 当前焊接速度
    for (int instLoc = instListStartLoc; instLoc <= instListEndLoc; instLoc++)
    {
        if (listLocToTypeID.count(instLoc) != 0)
        {
            InstTypeLaserSdk instType = listLocToTypeID[instLoc].first; ///< 指令类型
            int typeID = listLocToTypeID[instLoc].second;
            // 根据指令类型生成相应的脚本
            switch (instType)
            {
            case InstTypeLaserSdk::CRAFT_SELECT_INST_LASER_WELD_SDK:
            {    
                // 工艺模板指令
                bodyScriptText += "--CRAFT_SELECT--\n";
                std::string scriptTmp;
                result = craftCommu->craft_script_write_craft_resource_script_get(laserMachineHandle, curCraftInfo, scriptTmp);
                if(result != LaserSdkResult::SUCCESS_LASER_SDK)
                    return result;
                if(isWeld)
                {
                    std::ostringstream oss;
                    oss <<  "set_configurable_digital_out(1, false)\n";
                    oss <<  scriptTmp ;
                    oss <<  "set_configurable_digital_out(1, true)\n"
                         << "wait(" << machineResource.laserPowerParams.gasStartupAdvanceTime << ")\n";
                    bodyScriptText += oss.str();
                }
                else {
                    bodyScriptText += scriptTmp;
                }

                break;
            }
            case InstTypeLaserSdk::ARC_START_INST_LASER_WELD_SDK:
            {
                if(isWeld)
                {
                    bodyScriptText += "--ARC_START_LIST--\n"
                            "write_boolean_register(611,true)\n";
                    isWelding = true; //设置焊接状态                    
                    std::string tmpScript ;
                    result = craftCommu->craft_script_slow_arc_function_switch_script_get(laserMachineHandle, SwitchTypeLaserSdk::SWITCH_ON_LASER_SDK, tmpScript);
                    if(result != LaserSdkResult::SUCCESS_LASER_SDK)
                    {
                        return result;
                    }

                    result = craftCommu->craft_script_slow_arc_program_run_script_get(laserMachineHandle, SwitchTypeLaserSdk::SWITCH_ON_LASER_SDK, tmpScript);
                    if(result != LaserSdkResult::SUCCESS_LASER_SDK)
                    {
                        return result;
                    }
                    bodyScriptText += tmpScript;
                }
                break;
            }
            case InstTypeLaserSdk::ARC_END_INST_LASER_WELD_SDK:
            {                
                if (isWeld)
                {
                    bodyScriptText += " --ARC_STOP_LIST--\n"
                            "write_boolean_register(611,false)\n";
                    isWelding = false; //设置焊接状态
                    std::string tmpScript ;
                    if(result != LaserSdkResult::SUCCESS_LASER_SDK)
                    {
                        return result;
                    }
                    result = craftCommu->craft_script_arc_program_run_script_get(laserMachineHandle, SwitchTypeLaserSdk::SWITCH_OFF_LASER_SDK, tmpScript);
                    if(result != LaserSdkResult::SUCCESS_LASER_SDK)
                    {
                        return result;
                    }
                    bodyScriptText += tmpScript;
                }
                
                break;
            }
            case InstTypeLaserSdk::SLOW_DOWN_LASER_WELD_SDK:
            {
                if(isWeld)
                {
                    // 生成缓降脚本
                    bodyScriptText += " --SLOW_DOWN--\n";
                    std::string tmpScript ;
                    result = craftCommu->craft_script_slow_arc_function_switch_script_get(laserMachineHandle, SwitchTypeLaserSdk::SWITCH_OFF_LASER_SDK, tmpScript);
                    if(result != LaserSdkResult::SUCCESS_LASER_SDK)
                    {
                        return result;
                    }

                    result = craftCommu->craft_script_slow_arc_program_run_script_get(laserMachineHandle, SwitchTypeLaserSdk::SWITCH_OFF_LASER_SDK, tmpScript);

                    if(result != LaserSdkResult::SUCCESS_LASER_SDK)
                    {
                        return result;
                    }
                    bodyScriptText += tmpScript;
                }
                break;
            }
            case InstTypeLaserSdk::LINE_POINT_INST_LASER_WELD_SDK:
            {
                // 直线定点指令
                bodyScriptText += " --MOVE_LINE_LIST--\n";
                double transRadius = radiusEmpty;
                double speed = emptyRunTCPSpeed;
                double acc = emptyRunTCPAcc;
                //焊接设置对应的焊接速度与加速度
                if(isWeld && isWelding)
                {
                    speed = weldSpeed;
                    acc = weldTCPAcc;
                }
                //正在焊接且下一个点不是焊接点，则计算交融半径
                if(isWelding && listLocToTypeID[instLoc+1].first != InstTypeLaserSdk::ARC_END_INST_LASER_WELD_SDK)
                {
                    // 根据速度与加速度计算交融半径
                    result = CalBlendingRadius(speed, acc, &transRadius);
                    if (result != LaserSdkResult::SUCCESS_LASER_SDK)
                    {
                        return result;
                    }
                }
                //非焊接 需要进行断点判断
                if (!isWeld)
                {
                    craftCommu->craft_script_judge_break_point_script_get(instLoc, bodyScriptText);
                }
                bodyScriptText += strMoveLReplace(doubleArray2Str(tempPointsMap[typeID].pose, 6), std::to_string(speed), std::to_string(acc), std::to_string(transRadius));
                //如果当前点为焊接点，663置为1
                if(!isWeld)
                {                    
                    bodyScriptText += strWriteInt32RegReplace(WeldControlAddressLaserSdk::INT32_REG_ARRIVE_POINT_LOC_663, instLoc);
                    craftCommu->craft_script_judge_signal_setp_run_script_get(instLoc, bodyScriptText);
                }
                if((isWeld && isWelding) || (isWeld && listLocToTypeID[instLoc+1].first == InstTypeLaserSdk::ARC_START_INST_LASER_WELD_SDK))
                {
                    bodyScriptText += strWriteInt32RegReplace(WeldControlAddressLaserSdk::INT32_REG_WELD_STATE_799, instLoc);                    
                }
                prePointLoc = instLoc;
                break;
            }
            case InstTypeLaserSdk::CIRCLE_POINT_INST_LASER_WELD_SDK:
            {    
                // 生成圆形定点指令
                bodyScriptText += " --MOVE_CIR--\n";
                double transRadius = radiusEmpty;
                double speed = emptyRunTCPSpeed;
                double acc = emptyRunTCPAcc;

                if(isWeld && isWelding)
                {
                    transRadius = radiusWelding;
                    speed = weldSpeed;
                    acc = weldTCPAcc;
                }
                if(isWelding && listLocToTypeID[instLoc+2].first != InstTypeLaserSdk::ARC_END_INST_LASER_WELD_SDK)
                {
                    // 根据速度与加速度计算交融半径
                    result = CalBlendingRadius(speed, acc, &transRadius);
                    if (result != LaserSdkResult::SUCCESS_LASER_SDK)
                    {
                        return result;
                    }
                }

                if (!isWeld)
                {
                    craftCommu->craft_script_judge_break_point_script_get(instLoc+1, bodyScriptText);
                }

                bodyScriptText += "--circlePoint--\n";
                int nextCirclePointID = listLocToTypeID[instLoc+1].second;
                //int typeID = listLocToTypeID[instLoc].second; 
                bodyScriptText += strMoveCReplace(
                            doubleArray2Str(tempPointsMap[typeID].pose, 6),
                            doubleArray2Str(tempPointsMap[typeID].joint, 6),
                            doubleArray2Str(tempPointsMap[nextCirclePointID].pose, 6),
                            doubleArray2Str(tempPointsMap[nextCirclePointID].joint, 6),
                            std::to_string(speed), std::to_string(acc), std::to_string(transRadius), std::to_string(0));
                //跳过下一个圆弧定点并将最后一个定点的列表位置复制为下一个圆弧定点的位置
                instLoc++;
                if(!isWeld)
                {
                    bodyScriptText += strWriteInt32RegReplace(WeldControlAddressLaserSdk::INT32_REG_ARRIVE_POINT_LOC_663, instLoc);
                    craftCommu->craft_script_judge_signal_setp_run_script_get(instLoc, bodyScriptText);
                }
                if((isWeld && isWelding) || (isWeld && listLocToTypeID[instLoc+1].first == InstTypeLaserSdk::ARC_START_INST_LASER_WELD_SDK))
                {
                    bodyScriptText += strWriteInt32RegReplace(WeldControlAddressLaserSdk::INT32_REG_WELD_STATE_799, instLoc);
                }

                prePointLoc = instLoc;
                break;
            }
            case InstTypeLaserSdk::JOINT_POINT_INST_LASER_WELD_SDK:
            {    // 关节运动指令
                bodyScriptText += "--MOVE_JOINT--\n";
                double transRadius = radiusEmpty;
                if (!isWeld)
                {
                    craftCommu->craft_script_judge_break_point_script_get(instLoc, bodyScriptText);
                }
                bodyScriptText += strMoveJReplace(doubleArray2Str(tempPointsMap[typeID].joint, 6), std::to_string(emptyRunJointSpeed), std::to_string(emptyRunJointAcc), std::to_string(transRadius));
                if(!isWeld)
                {
                    bodyScriptText += strWriteInt32RegReplace(WeldControlAddressLaserSdk::INT32_REG_ARRIVE_POINT_LOC_663, instLoc);
                    craftCommu->craft_script_judge_signal_setp_run_script_get(instLoc, bodyScriptText);
                }
                //真焊接且焊接中，则写入焊接状态
                if((isWeld && isWelding) || (isWeld && listLocToTypeID[instLoc+1].first == InstTypeLaserSdk::ARC_START_INST_LASER_WELD_SDK))
                {
                    bodyScriptText += strWriteInt32RegReplace(WeldControlAddressLaserSdk::INT32_REG_WELD_STATE_799, instLoc);
                }
                prePointLoc = instLoc;
            
                break;
            }
            case InstTypeLaserSdk::SAFE_POINT_MOVEL_INST_LASER_WELD_SDK:    // 安全点指令 MoveL 预留
            case InstTypeLaserSdk::SAFE_POINT_MOVEJ_INST_LASER_WELD_SDK:
                // 移动安全点指令 MoveJ
                bodyScriptText += "--safePointMoveJ--\n";
                bodyScriptText += strMoveJReplace(doubleArray2Str(tempPointsMap[typeID].joint, 6), std::to_string(emptyRunJointSpeed), std::to_string(emptyRunJointAcc), std::to_string(0));
                craftCommu->craft_script_judge_signal_setp_run_script_get(instLoc, bodyScriptText);
                //bodyScriptText += strReplace("movej(%1,%2,%3,%4)\n", {doubleArray2Str(tempPointsMap[typeID].joint, 6), std::to_string(emptyRunJointSpeed), std::to_string(emptyRunJointAcc), std::to_string(0)});
                break;
            
            }
        }
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}

int distributionID(std::unordered_set<int> &IDSet)
{
    // 生成唯一ID
    int instID = distribution(engine);
    while(IDSet.count(instID) != 0)
    {
        instID = distribution(engine);
    }
    IDSet.insert(instID);
    return instID;
}

LaserSdkResult CraftInfo2MachineResource(LaserMachineHandle laserMachineHandle, CraftInfoLaserSdk craftInfoLaserSdk , std::vector<unsigned int>& laserVect, std::vector<unsigned int>& laserHeadVect, std::vector<unsigned int>& wireVect)
{
    CommuBase* craftCommu = nullptr;
    LaserSdkResult result = craft_script_commu_class_get(laserMachineHandle, craftCommu);
    if (result != LaserSdkResult::SUCCESS_LASER_SDK)
    {
        return result;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    CgxiLaserPowerLaserSdk &laserPowerParams = machineResource.laserPowerParams;
    /**开关光**/
    laserPowerParams.laserPowerOpen = craftInfoLaserSdk.craftLeadInPower;       /// 开光功率
    laserPowerParams.laserRampUpTime = craftInfoLaserSdk.craftLeadInRampTime;   /// 开光时间
    laserPowerParams.laserPowerClose = craftInfoLaserSdk.craftLeadOutPower;     /// 关光功率
    // 计算关光功率下降时间
    int  disX = craftInfoLaserSdk.craftLeadOutRampDistance - craftInfoLaserSdk.craftRetractionDistance ;
    if(disX  < 0)
    {
        writeLog("craft_script_configure_machine_param_set_script_get :: laserRampDownTime out of range");
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }
    double tempTime = static_cast<double>((disX * 1000)/ craftInfoLaserSdk.craftWeldingSpeed);
    machineResource.laserPowerParams.laserRampDownTime = tempTime;              /// 关光渐进时间
    /**写入焊接模式*/
    laserPowerParams.weldMode = craftInfoLaserSdk.craftweldMode;
    laserVect.push_back(machineResource.spotWeldTypeLoc);
    laserPowerParams.spotWeldDurationTime = craftInfoLaserSdk.craftSpotWeldDuration;
    laserVect.push_back(machineResource.spotWeldDurationLoc);
    laserPowerParams.spotWeldIntervalTime = craftInfoLaserSdk.craftSpotWeldInterval;
    laserVect.push_back(machineResource.spotWeldIntervalLoc);       
    
    /**写入XY偏移放大*/
    laserPowerParams.laserHeadYOffset =craftInfoLaserSdk.craftYAxisOffset;  ///Y偏移
    laserVect.push_back(machineResource.laserHeadYOffsetLoc);

    laserPowerParams.laserHeadXOffset =craftInfoLaserSdk.craftXAxisOffset;  ///X偏移
    laserVect.push_back(machineResource.laserHeadXOffsetLoc);

    laserPowerParams.laserHeadYEnlarge =craftInfoLaserSdk.craftYAxisZoom;   ///Y放大
    laserVect.push_back(machineResource.laserHeadYScaleLoc);

    laserPowerParams.laserHeadXEnlarge =craftInfoLaserSdk.craftXAxisZoom;   ///X放大
    laserVect.push_back(machineResource.laserHeadXScaleLoc);
    
    laserPowerParams.gasStartupAdvanceTime = craftInfoLaserSdk.craftPreGasTime; /// 提前送气时间
    laserVect.push_back(machineResource.gasONDelayLoc);
    
    laserPowerParams.gasShutoffDelayTime = craftInfoLaserSdk.craftPostGasTime;  /// 气体关闭延时 --滞后送气时间
    laserVect.push_back(machineResource.gasOffDelayLoc);
    
    /**写入图案参数*/
    CgxiLaserHeadLaserSdk &laserHeadParams = machineResource.laserHeadParams;     
    laserHeadParams.laserSpotPattern = craftInfoLaserSdk.graphicsSwitching;     ///图案开关
    laserHeadVect.push_back(machineResource.laserHeadPatternIdLoc);        
    laserHeadParams.laserHeadScanSpeed = craftInfoLaserSdk.craftScanSpeed;      ///扫描速度
    laserHeadVect.push_back(machineResource.laserHeadScanSpeedLoc);
    laserHeadParams.laserHeadScanWidth = craftInfoLaserSdk.craftScanWidth;      ///扫描宽度
    laserHeadVect.push_back(machineResource.laserHeadScanWidthLoc);    
    laserHeadParams.laserHeadScanHeight = craftInfoLaserSdk.craftScanHeight;    ///扫描高度
    laserHeadVect.push_back(machineResource.laserHeadScanHeightLoc);    
    laserHeadParams.laserHeadSpinAngle = craftInfoLaserSdk.craftRotationAngle;  ///旋转角度
    laserHeadVect.push_back(machineResource.laserHeadRotationAngleLoc);
    laserHeadParams.laserHeadPeakPower = craftInfoLaserSdk.craftLaserPower;     ///峰值/焊接功率
    laserHeadVect.push_back(machineResource.laserHeadPeakPowerLoc);
    /*** 写入送丝机参数 ***/
    CgxiWireFeederLaserSdk wireFeederParams = machineResource.wireFeederParams;   ///< 送丝机参数
    int distance = craftInfoLaserSdk.craftRetractionDistance;            //回抽焊丝的过程距离 mm
    machineResource.wireFeederParams.lengthBack = distance;
    //计算补丝延迟
    int wireDelayTime = static_cast<int>((distance / craftInfoLaserSdk.craftWeldingSpeed)*1000); //计算补丝延迟,s换算成ms
    if ( wireDelayTime > 2000)
    {
        writeLog("补丝延迟时间过大");
        //return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }
    //  回抽长度判断
    if(craftInfoLaserSdk.craftRetractionLength < 0 || craftInfoLaserSdk.craftRetractionLength > 20)
    {
        craftInfoLaserSdk.craftRetractionLength = 20;
    }
    machineResource.wireFeederParams.wireFeederWaitTime = wireDelayTime;    ///< 补丝延时    0~2000 ms
    machineResource.wireFeederParams.wireFeederStartMotorSpeed = craftInfoLaserSdk.craftLeadInWireFeed;  ///< 开光送丝sudu
    machineResource.wireFeederParams.wireFeederStopMotorSpeed = craftInfoLaserSdk.craftLeadOutWireFeed;  ///< 关光送丝sudu
    machineResource.wireFeederParams.wireFeederMotorSpeed = craftInfoLaserSdk.craftWeldingWireFeed;             ///< 自动送丝速度
    wireVect.push_back(machineResource.autoWireFeedSpeedLoc);
    machineResource.wireFeederParams.wireFeederBackLength = craftInfoLaserSdk.craftRetractionLength;            ///< 回抽长度
    wireVect.push_back(machineResource.wireRetractLengthLoc);
    machineResource.wireFeederParams.wireFeederFeedLength = craftInfoLaserSdk.craftEndLoopbackLength;           ///< 回送/补丝长度
    wireVect.push_back(machineResource.wireFeedLengthLoc);
    machineResource.wireFeederParams.craftWireCutMode = craftInfoLaserSdk.craftWireCutMode;   ///<断丝模式
    wireVect.push_back(machineResource.breakWireFeedingModeLoc);
    
    //std::vector<uint8_t> wireMessageVec = buildMessageVector(wireMap, 0x06, 0x09);
    return SUCCESS_LASER_SDK;
}

LaserSdkResult GetCraftCheck(CraftInfoLaserSdk curCraftInfo)
{

    LaserSdkResult res = SUCCESS_LASER_SDK;
    int maxLimit = (curCraftInfo.graphicsSwitching == SpotPatternLaserSdk::CROSS_LASERSDK) ? 1 : 0;
    int speed = curCraftInfo.craftScanSpeed;
    float rangeW = static_cast<float>(curCraftInfo.craftScanWidth);
    if(rangeW < 0.1f || rangeW > (5.0f - 2.0f * maxLimit))
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    float rangeH = static_cast<float>(curCraftInfo.craftScanHeight);
    if(rangeH < 0.1f || rangeH > (5.0f - 2.0f * maxLimit))
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    //    圆：频率=速度/(3.14*宽度)
    //    直线：频率=速度/[2*宽度]
    //    8 字：频率=速度*2/[3.14*(宽度+高度)]
    //    十字：频率=速度/[2*(宽度+高度)]
    //    三尖瓣：频率=速度/(3*宽度)
    //    四尖瓣：频率=速度*2/[3*(宽度+高度)]，
    //    “扫描频率”必须满足 10Hz< 频率<150Hz（十字型 10Hz<频率<50Hz）
    float freq = 0.0;
    switch (curCraftInfo.graphicsSwitching)
    {
        case SpotPatternLaserSdk::CIR_LASERSDK:
            freq = static_cast<float>(speed)/(static_cast<float>(PI)*rangeW) + 0.5f;
        break;
        case SpotPatternLaserSdk::LINE_LASERSDK:
            freq = static_cast<float>(speed)/(rangeW * 2.0f) + 0.5f;
        break;
        case SpotPatternLaserSdk::STAR_LASERSDK:
            freq = ( static_cast<float>(speed)*2.0f)/(3.0f*(rangeW+rangeH))+0.5f;
        break;
        case SpotPatternLaserSdk::HORIZON_8_LASERSDK:
            freq = ( static_cast<float>(speed)*2.0f)/(static_cast<float>(PI)*(rangeW+rangeH))+0.5f;
        break;
        case SpotPatternLaserSdk::CROSS_LASERSDK:
            freq = static_cast<float>(speed)/(rangeW*2.0f+rangeH*2.0f) + 0.5f;
        break;
        case SpotPatternLaserSdk::TRIANGLE_LASERSDK:
            freq= static_cast<float>(speed)/(3.0f*rangeW)+0.5f;
        break;
    }
    if(freq < 10 || freq > (150 - 100 * maxLimit))
    {
        res = LaserSdkResult::PARAM_SCAN_FREQUENCY_ERROR_LASER_SDK;
    }
    return res;
}
