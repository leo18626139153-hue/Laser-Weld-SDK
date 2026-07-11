#include "craftscript.h"
#include "cl_api.h"
#include "clmath.h"
#include "utils.h"
#include "messageprase.h"
#include "rs485_communicater.h"
#include <sstream>
#include <mutex>
#include <random>
#include <memory>
#include <unordered_map>


//用于生成随机数，分配焊机句柄，保证唯一性和安全性。
std::mt19937 engine;
//均匀分布的整数随机数生成器
std::uniform_int_distribution<int> distribution;
//焊机资源的全局管理表，key 是句柄，value 是焊机资源对象。
std::unordered_map<int, MachineResource> machineResourcesMap;


//判断单步
LaserSdkResult CommuBase::craft_script_judge_signal_setp_run_script_get(int instLoc, std::string &script)
{
    std::string signalSetpStatusLoc = std::to_string(WeldControlAddressLaserSdk::INT32_REG_SINGLE_STEP_RUN_616);
    std::string scriptText = R"(
    if(read_integer_register(%1) == 1 and signalSetpPointLocListLookUp[%2] == 1)
    then
        pause()
    end
    )";
    script += strReplace(scriptText, std::vector<std::string>{signalSetpStatusLoc, std::to_string(instLoc)});
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//断点判断
LaserSdkResult CommuBase::craft_script_judge_break_point_script_get(int instLoc, std::string &script)
{
    unsigned int runBreakPointStatusLoc = WeldControlAddressLaserSdk::INT32_REG_RUN_BREAK_POINT_617;

    std::string scriptText = R"(
    if(breakPointLocListLookUp[%1] == 1)
    then
        write_bool_register(%2, 1)
        pause()
    end
    )";
    script += strReplace(scriptText, std::vector<std::string> {std::to_string(instLoc), std::to_string(runBreakPointStatusLoc)});
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

std::string CommuBase::loadLuaFunctionScriptOut()
{
    std::string tmpLuaScript;   //接口函数
    for (auto it = startLuaScripts.begin(); it != startLuaScripts.end(); )
    {
        tmpLuaScript += it->second + "\n";
        it = startLuaScripts.erase(it); // erase 自动返回下一个有效迭代器
    }
    std::string tmpLuaStartDataScript;
    for (auto it = startDataScripts.begin(); it != startDataScripts.end(); )
    {
        tmpLuaScript = it->second + tmpLuaScript + "\n";
        it = startDataScripts.erase(it); //加载后删除
    }
    return tmpLuaStartDataScript + loadLuaFunctionScript + tmpLuaScript ;
}


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
    CraftInfoLaserSdk& curCraftInfo = craftsMap[CraftID];        ///< 当前工艺信息

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
                // 获取工艺参数设置脚本
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
                    //bodyScriptText += scriptTmp;
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
                    result = craftCommu->craft_script_slow_arc_function_switch_script_get(laserMachineHandle, SwitchTypeLaserSdk::SWITCH_ON_LASER_SDK);
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
                            "wait(1) \n"
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
                    result = craftCommu->craft_script_slow_arc_function_switch_script_get(laserMachineHandle, SwitchTypeLaserSdk::SWITCH_OFF_LASER_SDK);
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
                else if(isWelding && listLocToTypeID[instLoc+1].first == InstTypeLaserSdk::ARC_END_INST_LASER_WELD_SDK)
                {
                    transRadius = 0;
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
                if(isWelding && listLocToTypeID[instLoc+1].first != InstTypeLaserSdk::ARC_END_INST_LASER_WELD_SDK)
                {
                    // 根据速度与加速度计算交融半径
                    result = CalBlendingRadius(speed, acc, &transRadius);
                    if (result != LaserSdkResult::SUCCESS_LASER_SDK)
                    {
                        return result;
                    }
                }
                else if(isWelding && listLocToTypeID[instLoc+1].first == InstTypeLaserSdk::ARC_END_INST_LASER_WELD_SDK)
                {
                    transRadius = 0;
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

