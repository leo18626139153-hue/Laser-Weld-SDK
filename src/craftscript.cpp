#include "craftscript.h"
#include "cl_api.h"
#include "clmath.h"
#include "utils.h"
#include "messageprase.h"
#include "rs485_commu.h"
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

/**
 * @brief 获取焊机通讯类实例（兼容外观）
 *
 * 修改理由：cl_api.cpp 中多处重复判断 commuType 并手动获取 CommuRS485 单例，
 *   本函数作为兼容外观保留原有签名，内部委托 CommFactory::GetCommu() 统一选择通讯脚本生成器。
 * 逻辑：先验证句柄有效性，再从 machineResource 取出 commuType 委托工厂获取实例。
 *   函数签名、返回值和调用方式与基线版本完全一致。
 *
 * @legacy_source src/craftscript.cpp
 * @legacy_function craft_script_commu_class_get
 * @legacy_baseline 6fefa14
 * @legacy_lines 22-46
 * @compatibility 函数签名、返回值和调用方式保持不变
 */
LaserSdkResult craft_script_commu_class_get(LaserMachineHandle laserMachineHandle, CommuBase *&craftCommuClass)
{
    //句柄
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    // 委托 CommFactory 按通讯类型获取对应脚本生成器实例
    return CommFactory::GetCommu(machineResource.commuType, craftCommuClass);
}


namespace
{
bool getInstruction(const InstListData& instListData, int instLoc, InstTypeLaserSdk& instType, int& typeID)
{
    std::unordered_map<int, std::pair<InstTypeLaserSdk, int> >::const_iterator it =
            instListData.listLocToTypeID.find(instLoc);
    if (it == instListData.listLocToTypeID.end())
    {
        return false;
    }
    instType = it->second.first;
    typeID = it->second.second;
    return true;
}

const PointDataLaserSdk* getPointByInstruction(const InstListData& instListData, int instLoc)
{
    InstTypeLaserSdk instType;
    int typeID = -1;
    if (!getInstruction(instListData, instLoc, instType, typeID))
    {
        return NULL;
    }
    std::unordered_map<int, PointDataLaserSdk>::const_iterator pointIt = instListData.pointsMap.find(typeID);
    return pointIt == instListData.pointsMap.end() ? NULL : &pointIt->second;
}

const CraftInfoLaserSdk* getCraft(const InstListData& instListData, int craftID)
{
    std::unordered_map<int, CraftInfoLaserSdk>::const_iterator craftIt = instListData.craftsMap.find(craftID);
    return craftIt == instListData.craftsMap.end() ? NULL : &craftIt->second;
}

bool isInstructionType(const InstListData& instListData, int instLoc, InstTypeLaserSdk expectedType)
{
    InstTypeLaserSdk instType;
    int typeID = -1;
    return getInstruction(instListData, instLoc, instType, typeID) && instType == expectedType;
}

bool hasLegacyRampDownMarker(const InstListData& instListData, int movementEndLoc)
{
    return isInstructionType(instListData, movementEndLoc + 1, SLOW_DOWN_LASER_WELD_SDK)
            && isInstructionType(instListData, movementEndLoc + 2, ARC_END_INST_LASER_WELD_SDK);
}

bool shouldSplitRampDownTrajectory(const InstListData& instListData, int movementEndLoc)
{
    // 新指令序列不再要求显式SLOW_DOWN：收弧前最后一条直线或完整圆弧自动拆分。
    return isInstructionType(instListData, movementEndLoc + 1, ARC_END_INST_LASER_WELD_SDK)
            || hasLegacyRampDownMarker(instListData, movementEndLoc);
}

LaserSdkResult appendRampDownControl(LaserMachineHandle laserMachineHandle,
                                     CommuBase& craftCommu,
                                     const ScriptGenerationPolicy& policy,
                                     std::string& bodyScriptText)
{
    if (!policy.emitRampDownControl)
    {
        return LaserSdkResult::SUCCESS_LASER_SDK;
    }

    bodyScriptText += " --SLOW_DOWN--\n";
    LaserSdkResult result = craftCommu.craft_script_slow_arc_function_switch_script_get(
                laserMachineHandle, SwitchTypeLaserSdk::SWITCH_OFF_LASER_SDK);
    if (result != LaserSdkResult::SUCCESS_LASER_SDK)
    {
        return result;
    }
    std::string rampDownScript;
    result = craftCommu.craft_script_slow_arc_program_run_script_get(
                laserMachineHandle, SwitchTypeLaserSdk::SWITCH_OFF_LASER_SDK, rampDownScript);
    if (result == LaserSdkResult::SUCCESS_LASER_SDK)
    {
        bodyScriptText += rampDownScript;
    }
    return result;
}

LaserSdkResult appendStraightRampDownTrajectory(LaserMachineHandle laserMachineHandle,
                                                CommuBase& craftCommu,
                                                const ScriptGenerationPolicy& policy,
                                                const PointDataLaserSdk& startPoint,
                                                const PointDataLaserSdk& endPoint,
                                                double rampDownDistance,
                                                double speed,
                                                double acc,
                                                std::string& bodyScriptText)
{
    double rampDownPoint[6] = {0};
    LaserSdkResult result = getStraightLineSlowUpPoint(startPoint.pose,
                                                        endPoint.pose,
                                                        rampDownDistance,
                                                        rampDownPoint);
    if (result != LaserSdkResult::SUCCESS_LASER_SDK)
    {
        return result;
    }

    // 降功率点是内部合成点，不写到达寄存器；两段边界半径必须为0，保证机器人经过触发点。
    bodyScriptText += strMoveLReplace(doubleArray2Str(rampDownPoint, 6),
                                      std::to_string(speed), std::to_string(acc), "0");
    result = appendRampDownControl(laserMachineHandle, craftCommu, policy, bodyScriptText);
    if (result != LaserSdkResult::SUCCESS_LASER_SDK)
    {
        return result;
    }
    bodyScriptText += strMoveLReplace(doubleArray2Str(endPoint.pose, 6),
                                      std::to_string(speed), std::to_string(acc), "0");
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

LaserSdkResult appendCircleRampDownTrajectory(LaserMachineHandle laserMachineHandle,
                                              const MachineResource& machineResource,
                                              CommuBase& craftCommu,
                                              const ScriptGenerationPolicy& policy,
                                              const PointDataLaserSdk& startPoint,
                                              const PointDataLaserSdk& transitionPoint,
                                              const PointDataLaserSdk& endPoint,
                                              double rampDownDistance,
                                              double speed,
                                              double acc,
                                              std::string& bodyScriptText)
{
    double stopMappingPoint[3] = {0};
    double firstTransitionPoint[6] = {0};
    double firstTransitionJoint[6] = {0};
    double rampDownPoint[6] = {0};
    double secondTransitionPoint[6] = {0};
    double originalEndPoint[6] = {0};
    double rampDownJoint[6] = {0};
    double secondTransitionJoint[6] = {0};
    double originalEndJoint[6] = {0};

    LaserSdkResult result = getCirclrSlowDownPoints(machineResource.robotHandle,
                                                     const_cast<double*>(startPoint.pose),
                                                     const_cast<double*>(transitionPoint.pose),
                                                     const_cast<double*>(endPoint.pose),
                                                     const_cast<double*>(endPoint.pose),
                                                     rampDownDistance,
                                                     const_cast<double*>(endPoint.joint),
                                                     stopMappingPoint,
                                                     firstTransitionPoint,
                                                     firstTransitionJoint,
                                                     rampDownPoint,
                                                     secondTransitionPoint,
                                                     originalEndPoint,
                                                     rampDownJoint,
                                                     secondTransitionJoint,
                                                     originalEndJoint);
    if (result != LaserSdkResult::SUCCESS_LASER_SDK)
    {
        return result;
    }

    bodyScriptText += strMoveCReplace(doubleArray2Str(firstTransitionPoint, 6),
                                      doubleArray2Str(firstTransitionJoint, 6),
                                      doubleArray2Str(rampDownPoint, 6),
                                      doubleArray2Str(rampDownJoint, 6),
                                      std::to_string(speed), std::to_string(acc), "0", "0");
    result = appendRampDownControl(laserMachineHandle, craftCommu, policy, bodyScriptText);
    if (result != LaserSdkResult::SUCCESS_LASER_SDK)
    {
        return result;
    }
    bodyScriptText += strMoveCReplace(doubleArray2Str(secondTransitionPoint, 6),
                                      doubleArray2Str(secondTransitionJoint, 6),
                                      doubleArray2Str(originalEndPoint, 6),
                                      doubleArray2Str(originalEndJoint, 6),
                                      std::to_string(speed), std::to_string(acc), "0", "0");
    return LaserSdkResult::SUCCESS_LASER_SDK;
}
}

LaserSdkResult GetBasicCraftScript(LaserMachineHandle laserMachineHandle,
                                   const InstListData& instListData,
                                   InstListTransEnvironmentData& environment,
                                   std::string& bodyScriptText)
{
    std::unordered_map<int, MachineResource>::iterator machineIt = machineResourcesMap.find(laserMachineHandle);
    if (machineIt == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource& machineResource = machineIt->second;
    const CgxiRobotLaserSdk& robotParams = machineResource.robotParams;

    CommuBase* craftCommu = NULL;
    LaserSdkResult result = craft_script_commu_class_get(laserMachineHandle, craftCommu);
    if (result != LaserSdkResult::SUCCESS_LASER_SDK || craftCommu == NULL)
    {
        return result == LaserSdkResult::SUCCESS_LASER_SDK
                ? LaserSdkResult::COMMU_ERROR_LASER_SDK : result;
    }

    const int instructionCount = static_cast<int>(instListData.listLocToTypeID.size());
    for (int instLoc = 0; instLoc < instructionCount; ++instLoc)
    {
        InstTypeLaserSdk instType;
        int typeID = -1;
        if (!getInstruction(instListData, instLoc, instType, typeID))
        {
            return LaserSdkResult::INSTLIST_ERROR_LASER_SDK;
        }

        const CraftInfoLaserSdk* currentCraft = getCraft(instListData, environment.craftID);
        switch (instType)
        {
        case InstTypeLaserSdk::CRAFT_SELECT_INST_LASER_WELD_SDK:
        {
            currentCraft = getCraft(instListData, typeID);
            if (currentCraft == NULL)
            {
                return LaserSdkResult::INSTLIST_ERROR_LASER_SDK;
            }
            environment.craftID = typeID;
            bodyScriptText += "--CRAFT_SELECT--\n";
            if (environment.policy.emitDeviceParameters)
            {
                std::string parameterScript;
                result = craftCommu->craft_script_configure_machine_param_set_script_get(
                            laserMachineHandle, *currentCraft, parameterScript);
                if (result != LaserSdkResult::SUCCESS_LASER_SDK)
                {
                    return result;
                }
                if (machineResource.laserHeadBrand == LaserHeadBrandSdk::CQLASERPOWER_SDK)
                {
                    std::ostringstream oss;
                    oss << "set_configurable_digital_out(1, false)\n"
                        << parameterScript
                        << "set_configurable_digital_out(1, true)\n"
                        << "wait(" << machineResource.laserPowerParams.gasStartupAdvanceTime << ")\n";
                    bodyScriptText += oss.str();
                }
                else
                {
                    bodyScriptText += parameterScript;
                }
            }
            break;
        }
        case InstTypeLaserSdk::ARC_START_INST_LASER_WELD_SDK:
        {
            if (environment.isWelding || currentCraft == NULL)
            {
                return LaserSdkResult::INSTLIST_ERROR_LASER_SDK;
            }
            environment.isWelding = true;
            if (environment.policy.emitArcControl)
            {
                bodyScriptText += "--ARC_START_LIST--\nwrite_boolean_register(611,true)\n";
                result = craftCommu->craft_script_arc_function_switch_script_get(
                            laserMachineHandle, SwitchTypeLaserSdk::SWITCH_ON_LASER_SDK);
                if (result != LaserSdkResult::SUCCESS_LASER_SDK)
                {
                    return result;
                }
                std::string arcScript;
                result = craftCommu->craft_script_arc_program_run_script_get(
                            laserMachineHandle, SwitchTypeLaserSdk::SWITCH_ON_LASER_SDK, arcScript);
                if (result != LaserSdkResult::SUCCESS_LASER_SDK)
                {
                    return result;
                }
                bodyScriptText += arcScript;
            }
            break;
        }
        case InstTypeLaserSdk::ARC_END_INST_LASER_WELD_SDK:
        {
            if (!environment.isWelding)
            {
                return LaserSdkResult::INSTLIST_ERROR_LASER_SDK;
            }
            if (environment.policy.emitArcControl)
            {
                bodyScriptText += " --ARC_STOP_LIST--\nwait(1) \nwrite_boolean_register(611,false)\n";
                std::string arcScript;
                result = craftCommu->craft_script_arc_program_run_script_get(
                            laserMachineHandle, SwitchTypeLaserSdk::SWITCH_OFF_LASER_SDK, arcScript);
                if (result != LaserSdkResult::SUCCESS_LASER_SDK)
                {
                    return result;
                }
                bodyScriptText += arcScript;
            }
            environment.isWelding = false;
            break;
        }
        case InstTypeLaserSdk::SLOW_DOWN_LASER_WELD_SDK:
            // 缓降标记必须由它前面的直线或完整圆弧消费，单独到达这里说明指令序列非法。
            return LaserSdkResult::INSTLIST_ERROR_LASER_SDK;

        case InstTypeLaserSdk::LINE_POINT_INST_LASER_WELD_SDK:
        {
            const PointDataLaserSdk* endPoint = getPointByInstruction(instListData, instLoc);
            if (endPoint == NULL)
            {
                return LaserSdkResult::INSTLIST_ERROR_LASER_SDK;
            }
            const bool useWeldMotion = environment.isWelding && environment.policy.useWeldMotionParameters;
            if (environment.isWelding && currentCraft == NULL)
            {
                return LaserSdkResult::INSTLIST_ERROR_LASER_SDK;
            }
            const double speed = useWeldMotion ? currentCraft->craftWeldingSpeed : robotParams.emptyRunTCPSpeed;
            const double acc = useWeldMotion ? robotParams.weldTCPAcc : robotParams.emptyRunTCPAcc;
            double transitionRadius = robotParams.emptyPointTransitionRadius;
            const bool hasLegacyMarker = hasLegacyRampDownMarker(instListData, instLoc);
            const bool splitRampDown = shouldSplitRampDownTrajectory(instListData, instLoc);
            if (splitRampDown && (!environment.isWelding || currentCraft == NULL))
            {
                return LaserSdkResult::INSTLIST_ERROR_LASER_SDK;
            }
            if (environment.isWelding && !splitRampDown
                    && !isInstructionType(instListData, instLoc + 1, ARC_END_INST_LASER_WELD_SDK))
            {
                result = CalBlendingRadius(speed, acc, &transitionRadius);
                if (result != LaserSdkResult::SUCCESS_LASER_SDK)
                {
                    return result;
                }
            }
            else if (environment.isWelding)
            {
                transitionRadius = 0.0;
            }

            if (!environment.policy.emitArcControl)
            {
                craftCommu->craft_script_judge_break_point_script_get(instLoc, bodyScriptText);
            }
            bodyScriptText += " --MOVE_LINE_LIST--\n";
            if (splitRampDown && currentCraft->craftLeadOutRampDistance > 0)
            {
                const PointDataLaserSdk* startPoint = getPointByInstruction(instListData, environment.prePointLoc);
                if (!environment.isWelding || startPoint == NULL)
                {
                    return LaserSdkResult::INSTLIST_ERROR_LASER_SDK;
                }
                result = appendStraightRampDownTrajectory(laserMachineHandle, *craftCommu,
                                                          environment.policy, *startPoint, *endPoint,
                                                          currentCraft->craftLeadOutRampDistance,
                                                          speed, acc, bodyScriptText);
                if (result != LaserSdkResult::SUCCESS_LASER_SDK)
                {
                    return result;
                }
            }
            else
            {
                bodyScriptText += strMoveLReplace(doubleArray2Str(endPoint->pose, 6),
                                                  std::to_string(speed), std::to_string(acc),
                                                  std::to_string(transitionRadius));
            }
            bodyScriptText += strWriteInt32RegReplace(INT32_REG_ARRIVE_POINT_LOC_663, instLoc);
            if (!environment.policy.emitArcControl)
            {
                craftCommu->craft_script_judge_signal_setp_run_script_get(instLoc, bodyScriptText);
            }
            if (environment.policy.emitArcControl && (environment.isWelding
                    || isInstructionType(instListData, instLoc + 1, ARC_START_INST_LASER_WELD_SDK)))
            {
                bodyScriptText += strWriteInt32RegReplace(INT32_REG_WELD_STATE_799, instLoc);
            }
            environment.prePointLoc = instLoc;
            if (hasLegacyMarker)
            {
                ++instLoc; // 仅跳过旧数据中的SLOW_DOWN；直接相邻的ARC_END必须留给下一轮处理。
            }
            break;
        }
        case InstTypeLaserSdk::CIRCLE_POINT_INST_LASER_WELD_SDK:
        {
            if (!isInstructionType(instListData, instLoc + 1, CIRCLE_POINT_INST_LASER_WELD_SDK))
            {
                return LaserSdkResult::INSTLIST_ERROR_LASER_SDK;
            }
            const int circleEndLoc = instLoc + 1;
            const PointDataLaserSdk* transitionPoint = getPointByInstruction(instListData, instLoc);
            const PointDataLaserSdk* endPoint = getPointByInstruction(instListData, circleEndLoc);
            if (transitionPoint == NULL || endPoint == NULL)
            {
                return LaserSdkResult::INSTLIST_ERROR_LASER_SDK;
            }
            const bool useWeldMotion = environment.isWelding && environment.policy.useWeldMotionParameters;
            if (environment.isWelding && currentCraft == NULL)
            {
                return LaserSdkResult::INSTLIST_ERROR_LASER_SDK;
            }
            const double speed = useWeldMotion ? currentCraft->craftWeldingSpeed : robotParams.emptyRunTCPSpeed;
            const double acc = useWeldMotion ? robotParams.weldTCPAcc : robotParams.emptyRunTCPAcc;
            double transitionRadius = robotParams.emptyPointTransitionRadius;
            const bool hasLegacyMarker = hasLegacyRampDownMarker(instListData, circleEndLoc);
            const bool splitRampDown = shouldSplitRampDownTrajectory(instListData, circleEndLoc);
            if (splitRampDown && (!environment.isWelding || currentCraft == NULL))
            {
                return LaserSdkResult::INSTLIST_ERROR_LASER_SDK;
            }
            if (environment.isWelding && !splitRampDown
                    && !isInstructionType(instListData, circleEndLoc + 1, ARC_END_INST_LASER_WELD_SDK))
            {
                result = CalBlendingRadius(speed, acc, &transitionRadius);
                if (result != LaserSdkResult::SUCCESS_LASER_SDK)
                {
                    return result;
                }
            }
            else if (environment.isWelding)
            {
                transitionRadius = 0.0;
            }

            if (!environment.policy.emitArcControl)
            {
                craftCommu->craft_script_judge_break_point_script_get(circleEndLoc, bodyScriptText);
            }
            bodyScriptText += " --MOVE_CIR--\n";
            if (splitRampDown && currentCraft->craftLeadOutRampDistance > 0)
            {
                const PointDataLaserSdk* startPoint = getPointByInstruction(instListData, environment.prePointLoc);
                if (!environment.isWelding || startPoint == NULL)
                {
                    return LaserSdkResult::INSTLIST_ERROR_LASER_SDK;
                }
                result = appendCircleRampDownTrajectory(laserMachineHandle, machineResource, *craftCommu,
                                                        environment.policy, *startPoint, *transitionPoint, *endPoint,
                                                        currentCraft->craftLeadOutRampDistance,
                                                        speed, acc, bodyScriptText);
                if (result != LaserSdkResult::SUCCESS_LASER_SDK)
                {
                    return result;
                }
            }
            else
            {
                bodyScriptText += strMoveCReplace(doubleArray2Str(transitionPoint->pose, 6),
                                                  doubleArray2Str(transitionPoint->joint, 6),
                                                  doubleArray2Str(endPoint->pose, 6),
                                                  doubleArray2Str(endPoint->joint, 6),
                                                  std::to_string(speed), std::to_string(acc),
                                                  std::to_string(transitionRadius), "0");
            }
            bodyScriptText += strWriteInt32RegReplace(INT32_REG_ARRIVE_POINT_LOC_663, circleEndLoc);
            if (!environment.policy.emitArcControl)
            {
                craftCommu->craft_script_judge_signal_setp_run_script_get(circleEndLoc, bodyScriptText);
            }
            if (environment.policy.emitArcControl && (environment.isWelding
                    || isInstructionType(instListData, circleEndLoc + 1, ARC_START_INST_LASER_WELD_SDK)))
            {
                bodyScriptText += strWriteInt32RegReplace(INT32_REG_WELD_STATE_799, circleEndLoc);
            }
            environment.prePointLoc = circleEndLoc;
            instLoc = hasLegacyMarker ? circleEndLoc + 1 : circleEndLoc;
            break;
        }
        case InstTypeLaserSdk::JOINT_POINT_INST_LASER_WELD_SDK:
        case InstTypeLaserSdk::SAFE_POINT_MOVEL_INST_LASER_WELD_SDK:
        case InstTypeLaserSdk::SAFE_POINT_MOVEJ_INST_LASER_WELD_SDK:
        {
            const PointDataLaserSdk* point = getPointByInstruction(instListData, instLoc);
            if (point == NULL || hasLegacyRampDownMarker(instListData, instLoc))
            {
                // MoveJ和安全点不支持按笛卡尔距离拆分收弧轨迹。
                return LaserSdkResult::INSTLIST_ERROR_LASER_SDK;
            }
            if (!environment.policy.emitArcControl)
            {
                craftCommu->craft_script_judge_break_point_script_get(instLoc, bodyScriptText);
            }
            bodyScriptText += instType == JOINT_POINT_INST_LASER_WELD_SDK
                    ? "--MOVE_JOINT--\n" : "--safePointMoveJ--\n";
            bodyScriptText += strMoveJReplace(doubleArray2Str(point->joint, 6),
                                              std::to_string(robotParams.emptyRunJointSpeed),
                                              std::to_string(robotParams.emptyRunJointAcc), "0");
            bodyScriptText += strWriteInt32RegReplace(INT32_REG_ARRIVE_POINT_LOC_663, instLoc);
            if (!environment.policy.emitArcControl)
            {
                craftCommu->craft_script_judge_signal_setp_run_script_get(instLoc, bodyScriptText);
            }
            environment.prePointLoc = instLoc;
            break;
        }
        default:
            return LaserSdkResult::INSTLIST_ERROR_LASER_SDK;
        }
    }

    if (environment.isWelding)
    {
        return LaserSdkResult::INSTLIST_ERROR_LASER_SDK;
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

