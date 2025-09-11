#include "cl_comandlistscript.h"
#include "cl_utils.h"
#include "cl_internaldatadefine.h"
#include "cl_craftscript.h"

//计算交融半径
LaserSdkResult CalBlendingRadius(double vMax, double aMax, double* r)
{
    // 1. 参数初始化
    const double pathLen = 500.0;
    const double tAhead = 0.002;
    const double vPercent = 100.0;
    const double jPercent = 100.0;
    vMax = vMax * vPercent / 100.0;
    aMax = aMax * std::sqrt(vPercent / 100.0);
    double jMax = aMax * jPercent / 100.0 * 8.0;

    // 2. 计算初始时间参数
    double Tj = std::pow(pathLen / (2.0 * jMax), 1.0/3.0);
    double amax = jMax * Tj;

    // 3. 条件分支处理
    double Ta = 0.0, Tv = 0.0, vmax = 0.0;

    if(amax > aMax) {
        // 3.1 加速度超过限制的情况
        amax = aMax;
        Tj = aMax / jMax;
        double a = aMax;
        double b = jMax * std::pow(Tj, 2) + 2.0 * aMax * Tj;
        double c = 2.0 * jMax * std::pow(Tj, 3) - pathLen;

        // 解二次方程求Ta
        Ta = (-b + std::sqrt(std::pow(b, 2) - 4.0 * a * c)) / (2.0 * a);
        vmax = jMax * std::pow(Tj, 2) + aMax * Ta;

        if(vmax > vMax) {
            // 3.1.1 速度超过限制的情况
            vmax = vMax;
            if(std::pow(aMax, 2) / jMax > vMax) {
                Tj = std::sqrt(vMax / jMax);
                aMax = jMax * Tj;
                Ta = 0.0;
                Tv = (pathLen - 2.0 * jMax * std::pow(Tj, 3)) / vMax;
            } else {
                Ta = (vMax - jMax * std::pow(Tj, 2)) / aMax;
                Tv = (pathLen - 2.0 * jMax * std::pow(Tj, 3) - aMax * std::pow(Ta, 2) -
                     (jMax * std::pow(Tj, 2) + 2.0 * aMax * Tj) * Ta) / vMax;
            }
        } else {
            Tv = 0.0;
        }
    } else {
        if(std::pow(amax, 2) / jMax > vMax) {
            // 3.2 速度限制主导的情况
            vmax = vMax;
            Tj = std::sqrt(vMax / jMax);
            amax = jMax * Tj;
            Ta = 0.0;
            Tv = (pathLen - 2.0 * jMax * std::pow(Tj, 3)) / vMax;
        } else {
            // 3.3 正常情况
            vmax = std::pow(amax, 2) / jMax;
            Ta = 0.0;
            Tv = 0.0;
        }
    }

    // 4. 计算各阶段速度和位移
    double V1 = jMax * std::pow(Tj, 2) / 2.0;
    double V2 = V1 + jMax * Tj * Ta;
    double V3 = V2 + jMax * std::pow(Tj, 2) / 2.0;
    double V4 = V3;
    double V5 = V4 - jMax * std::pow(Tj, 2) / 2.0;
    double V6 = V5 - jMax * Tj * Ta;

    double S1 = jMax * std::pow(Tj, 3) / 6.0;
    double S2 = S1 + V1 * Ta + jMax * Tj * std::pow(Ta, 2) / 2.0;
    double S3 = S2 + V2 * Tj + jMax * std::pow(Tj, 3) / 3.0;
    double S4 = S3 + V3 * Tv;
    double S5 = S4 + V4 * Tj - jMax * std::pow(Tj, 3) / 6.0;
    double S6 = S5 + V5 * Ta - jMax * Tj * std::pow(Ta, 2) / 2.0;

    // 5. 时间点计算
    double t = Tj + Ta + Tj + tAhead;

    double t1 = t;
    double t2 = t1 - Tj;
    double t3 = t1 - Tj - Ta;
    double t4 = t1 - 2.0 * Tj - Ta;
    double t5 = t1 - 2.0 * Tj - Ta - Tv;
    double t6 = t1 - 3.0 * Tj - Ta - Tv;
    double t7 = t1 - 3.0 * Tj - 2.0 * Ta - Tv;

    // 6. 分段计算半径
    double radius = 0.0;

    if(t <= Tj) {
        radius = 1.0/6.0 * jMax * std::pow(t1, 3);
    } else if(t < Tj + Ta) {
        radius = S1 + V1 * t2 + jMax * Tj * std::pow(t2, 2) / 2.0;
    } else if(t < 2.0 * Tj + Ta) {
        radius = S2 + V2 * t3 + jMax * Tj * std::pow(t3, 2) / 2.0 - jMax * std::pow(t3, 3) / 6.0;
    } else if(t < 2.0 * Tj + Ta + Tv) {
        radius = S3 + V3 * t4;
    } else if(t < 3.0 * Tj + Ta + Tv) {
        radius = S4 + V4 * t5 - jMax * std::pow(t5, 3) / 6.0;
    } else if(t < 3.0 * Tj + 2.0 * Ta + Tv) {
        radius = S5 + V5 * t6 - jMax * Tj * std::pow(t6, 2) / 2.0;
    } else if(t < 4.0 * Tj + 2.0 * Ta + Tv) {
        radius = S6 + V6 * t7 - jMax * Tj * std::pow(t7, 2) / 2.0 + jMax * std::pow(t7, 3) / 6.0;
    } else {
        radius = pathLen;
    }

    // 7. 约束处理
    if (radius > pathLen / 2.0) {
        radius = pathLen / 2.0;
    }
    // 8. 最终约束处理
    if (radius > 10.0) {
        radius = 10.0;
    }

    *r =  radius;
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

LaserSdkResult craft_script_commu_class_get(LaserMachineHandle laserMachineHandle, CommuBase *&craftCommuClass)
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
    double emptyPointTransitionRadius = robotParams.emptyPointTransitionRadius;  //空走点位过渡半径
    double weldPointTransitionRadius = robotParams.weldPointTransitionRadius;   //焊接点位过渡半径

    CommuBase* craftCommu = nullptr;
    LaserSdkResult result = craft_script_commu_class_get(laserMachineHandle, craftCommu);
    if (result != LaserSdkResult::SUCCESS_LASER_SDK)
    {
        return result;
    }
    
    std::unordered_map<int, std::pair<InstTypeLaserSdk, int>>& listLocToTypeID = instListData.listLocToTypeID; ///< 指令列表 指令类型 到 类型ID 的映射
    std::unordered_map<int, CraftInfoLaserSdk>& craftsMap = instListData.craftsMap;     ///< 工艺信息

    int CraftID = instListTransEnvironmentData.craftID;
    CraftInfoLaserSdk& curCraftInfo = craftsMap[CraftID]; ///< 当前工艺信息

    bool isWeld = instListTransEnvironmentData.isWeld;      ///< 真的焊接（1）还是模拟

    int& prePointLoc = instListTransEnvironmentData.prePointLoc; ///< 前一个点的位置
    bool& isWelding = instListTransEnvironmentData.isWelding;    ///< 是否焊接中

    double weldSpeed = curCraftInfo.craftWeldingSpeed; ///< 当前焊接速度
    for (int instLoc = instListStartLoc; instLoc <= instListEndLoc; instLoc++)
    {
        if (listLocToTypeID.count(instLoc) != 0)
        {
            InstTypeLaserSdk instType = listLocToTypeID[instLoc].first;
            int typeID = listLocToTypeID[instLoc].second; ///< 类型ID
            // 根据指令类型生成相应的脚本
            switch (instType)
            {
            case InstTypeLaserSdk::CRAFT_SELECT_INST_LASER_WELD_SDK:
            {    // 工艺模板指令
                bodyScriptText += "--CRAFT_SELECT--\n";
                bodyScriptText += "--111111111111--\n";
                break;
            }
            case InstTypeLaserSdk::ARC_START_INST_LASER_WELD_SDK:
            {
                isWelding = true; //设置焊接状态
                // 生成起弧脚本
                bodyScriptText += "--ARC_START--\n";
                if(isWeld)
                {
                    // int arcStartScriptLen = 1024*5;
                    // std::unique_ptr<char[]> arcStartScript(new char[static_cast<unsigned long long>(arcStartScriptLen)]);

                    // LaserSdkResult result = craftCommu->craft_script_start_weld_script_get(instLoc, arcStartScriptLen, arcStartScript.get());
                    // if(result != LaserSdkResult::SUCCESS_LASER_SDK)
                    // {
                    //     return result;
                    // }

                    // bodyScriptText += arcStartScript.get();
                    bodyScriptText += "set_configurable_digital_out(1,true)\n";
                }
                break;
            }
            case InstTypeLaserSdk::ARC_END_INST_LASER_WELD_SDK:
            {    // 生成收弧脚本
                bodyScriptText += "--ARC_STOP--\n";
                if (isWeld)
                {
                    bodyScriptText += "set_configurable_digital_out(1,false)\n";
                }
                
                break;
            }
            case InstTypeLaserSdk::LINE_POINT_INST_LASER_WELD_SDK:
            {    // 直线定点指令
                bodyScriptText += "--MOVE_LINE--\n";
                double transRadius = 0;
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
                    LaserSdkResult result = CalBlendingRadius(speed, acc, &transRadius);
                    if (result != LaserSdkResult::SUCCESS_LASER_SDK)
                    {
                        return result;
                    }
                }
                //非起弧 需要进行断点判断
                if (!isWeld)
                {
                    craftCommu->craft_script_judge_break_point_script_get(instLoc, bodyScriptText);
                }
                bodyScriptText += strReplace("movel(%1,%2,%3,%4)\n", {doubleArray2Str(tempPointsMap[typeID].pose, 6), std::to_string(speed), std::to_string(acc), std::to_string(transRadius)});
                //如果当前点为焊接点，663置为1
                if(!isWeld)
                {
                    bodyScriptText += strWriteModbusReplace(WeldControlAddressLaserSdk::INT_REG_ARRIVE_POINT_LOC_663, instLoc);
                }
                if((isWeld && isWelding) || (isWeld && listLocToTypeID[instLoc+1].first == InstTypeLaserSdk::ARC_START_INST_LASER_WELD_SDK))
                {
                    bodyScriptText += strWriteModbusReplace(WeldControlAddressLaserSdk::INT_REG_WELD_STATE_661, instLoc);
                }
                prePointLoc = instLoc;
            break;
            }
            case InstTypeLaserSdk::CIRCLE_POINT_INST_LASER_WELD_SDK:
            {    // 生成圆形定点指令
                bodyScriptText += "--MOVE_CIR--\n";
                //
                double transRadius = 0;
                double speed = emptyRunTCPSpeed;
                double acc = emptyRunTCPAcc;

                if(isWeld && isWelding)
                {
                    speed = weldSpeed;
                    acc = weldTCPAcc;
                }
                if(isWelding && listLocToTypeID[instLoc+2].first != InstTypeLaserSdk::ARC_END_INST_LASER_WELD_SDK)
                {
                    // 根据速度与加速度计算交融半径
                    LaserSdkResult result = CalBlendingRadius(speed, acc, &transRadius);
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
                bodyScriptText += strReplace("movec(%1,%2,%3,%4,%5,%6)\n", {doubleArray2Str(tempPointsMap[typeID].pose, 6), doubleArray2Str(tempPointsMap[nextCirclePointID].pose, 6), std::to_string(speed), std::to_string(acc), std::to_string(transRadius), std::to_string(0)});
                //跳过下一个圆弧定点并将最后一个定点的列表位置复制为下一个圆弧定点的位置
                instLoc++;
                if((isWeld && isWelding) || (isWeld && listLocToTypeID[instLoc+1].first == InstTypeLaserSdk::ARC_START_INST_LASER_WELD_SDK))
                {
                    bodyScriptText += strWriteModbusReplace(WeldControlAddressLaserSdk::INT_REG_WELD_STATE_661, instLoc);
                }
                prePointLoc = instLoc;
                break;
            }
            case InstTypeLaserSdk::JOINT_POINT_INST_LASER_WELD_SDK:
            {    // 关节运动指令
                bodyScriptText += "--MOVE_JOINT--\n";
                double transRadius = 0;
                double speed = emptyRunJointSpeed;
                double acc = emptyRunJointAcc;
                if (!isWeld)
                {
                    craftCommu->craft_script_judge_break_point_script_get(instLoc, bodyScriptText);
                }
                
                bodyScriptText += strReplace("movej(%1,%2,%3,%4)\n", {doubleArray2Str(tempPointsMap[typeID].joint, 6), std::to_string(speed), std::to_string(acc), std::to_string(transRadius)});
                if(!isWeld)
                {
                    bodyScriptText += strWriteModbusReplace(WeldControlAddressLaserSdk::INT_REG_ARRIVE_POINT_LOC_663, instLoc);
                }
                //真焊接且焊接中，则写入焊接状态
                if((isWeld && isWelding) || (isWeld && listLocToTypeID[instLoc+1].first == InstTypeLaserSdk::ARC_START_INST_LASER_WELD_SDK))
                {
                    bodyScriptText += strWriteModbusReplace(WeldControlAddressLaserSdk::INT_REG_WELD_STATE_661, instLoc);
                }
                prePointLoc = instLoc;
            
                break;
            }
            case InstTypeLaserSdk::SAFE_POINT_MOVEL_INST_LASER_WELD_SDK:
                // 安全点指令 MoveL 预留
            case InstTypeLaserSdk::SAFE_POINT_MOVEJ_INST_LASER_WELD_SDK:
                // 移动安全点指令 MoveJ
                bodyScriptText += "--safePointMoveJ--\n";
                bodyScriptText += strReplace("movej(%1,%2,%3,%4)\n", {doubleArray2Str(tempPointsMap[typeID].joint, 6), std::to_string(emptyRunJointSpeed), std::to_string(emptyRunJointAcc), std::to_string(0)});
                break;
            }
        }
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}
