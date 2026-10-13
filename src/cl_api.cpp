#include "cl_api.h"
#include "utils.h"
#include "craftscript.h"
#include "rs485_commu.h"
#include "messageprase.h"
#include "clmath.h"
#include "brandadapter.h"
#include "parametercontext.h"
#include <functional>
#include <thread>
#include <chrono>
#include <map>
#include <climits>

/**
 * @brief 检查 CR SDK 返回值，失败时抛出 SDK 错误码。
 * @param[in] errorResult CR SDK 返回值。
 */
static void checkCrSdkResultOrThrow(const CRresult& errorResult)
{
    LaserSdkResult errorLaserResult = crsdk_return_value_transfer(errorResult);
    if (errorLaserResult != SUCCESS_LASER_SDK)
    {
        throw errorLaserResult;
    }
}

/** @brief 从有效参数配置筛选旧设备页对应的可写参数，不推测未支持参数的地址。 */
static LaserSdkResult buildMappedDeviceSettings(LaserMachineHandle handle,
                                                const MachineResource& resource,
                                                std::string& script)
{
    laser_parameters::Configuration configuration;
    LaserSdkResult result = laser_parameters::GetMachineConfiguration(handle, configuration);
    if (result != SUCCESS_LASER_SDK) return result;
    std::vector<LaserParameterValueSdk> values;
    const auto appendSupported = [&configuration, &values](unsigned int id, double value) -> bool
    {
        for (std::size_t i = 0; i < configuration.descriptors.size(); ++i)
        {
            const LaserParameterDescriptorSdk& descriptor = configuration.descriptors[i];
            if (descriptor.parameterId != id || (descriptor.flags & LASER_PARAMETER_WRITABLE_SDK) == 0)
                continue;
            LaserParameterValueSdk parameter = {};
            parameter.structSize = sizeof(parameter);
            parameter.parameterId = id;
            parameter.value = value;
            values.push_back(parameter);
            return true;
        }
        return false;
    };
    if (resource.laserHeadBrand == CQLASERPOWER_SDK)
    {
        appendSupported(LASER_PARAM_WELD_POWER_SDK, resource.laserPowerParams.laserPower);
        appendSupported(LASER_PARAM_DRIVER_TEMPERATURE_SDK, resource.laserPowerParams.driverTemp);
        appendSupported(LASER_PARAM_WINDOW_TEMPERATURE_SDK, resource.laserPowerParams.protectiveWindowTemp);
        appendSupported(LASER_PARAM_COLLIMATOR_TEMPERATURE_SDK, resource.laserPowerParams.collimatorTemp);
    }
    const bool manualFeedSupported = appendSupported(LASER_PARAM_MANUAL_FEED_SPEED_SDK,
                                                     resource.wireFeederParams.wireFeederManualFeedSpeed);
    appendSupported(LASER_PARAM_MANUAL_RETRACT_SPEED_SDK, resource.wireFeederParams.wireFeederManualBackSpeed);
    // 宝辰鑫既有设备页使用抽补丝速度槽配置手动送丝速度，保持其兼容语义。
    if (!manualFeedSupported && resource.laserHeadBrand == BCXLASERPOWER_SDK)
        appendSupported(LASER_PARAM_WIRE_RETRACT_SPEED_SDK, resource.wireFeederParams.wireFeederManualFeedSpeed);
    return laser_parameters::BuildMachineParameterScript(handle, resource, values, script);
}

LaserSdkResult cl_sdk_version_get(char *version, int *versionLen)
{
    // P0修复：入参空指针校验，防止调用方传NULL导致崩溃
    if (version == nullptr || versionLen == nullptr)
    {
        return PARAMETER_ERROR_LASER_SDK;
    }
    if(*versionLen < sizeof(LASERWELDSDK_VERSION)+1)
    {
        return MEMORY_ERROR_LASER_SDK;
    }
    memcpy(version, LASERWELDSDK_VERSION, sizeof(LASERWELDSDK_VERSION));
    memcpy(version+sizeof(LASERWELDSDK_VERSION), "\0", 1);
    *versionLen = sizeof(LASERWELDSDK_VERSION)+1;
    return SUCCESS_LASER_SDK;
}


// 焊机资源管理函数
LaserSdkResult cl_machine_resource_init(const MachineResource* laserMachineResource, LaserMachineHandle* laserMachineHandle)
{
    if (laserMachineResource == nullptr || laserMachineHandle == nullptr)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserMachineResource->laserHeadBrand != CQLASERPOWER_SDK
            && laserMachineResource->laserHeadBrand != OSRPLASERPOWER_SDK
            && laserMachineResource->laserHeadBrand != BCXLASERPOWER_SDK
            && laserMachineResource->laserHeadBrand != CQ_SUP26AD_LASERPOWER_SDK)
        return PARAM_TYPE_ERROR_LASER_SDK;
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

    return SUCCESS_LASER_SDK;
}

LaserSdkResult cl_machine_resource_update(const LaserMachineHandle laserMachineHandle, const MachineResource *laserMachineResource)
{
    if (laserMachineResource == nullptr) return PARAMETER_ERROR_LASER_SDK;
    if (laserMachineResource->laserHeadBrand != CQLASERPOWER_SDK
            && laserMachineResource->laserHeadBrand != OSRPLASERPOWER_SDK
            && laserMachineResource->laserHeadBrand != BCXLASERPOWER_SDK
            && laserMachineResource->laserHeadBrand != CQ_SUP26AD_LASERPOWER_SDK)
        return PARAM_TYPE_ERROR_LASER_SDK;
    if(machineResourcesMap.count(laserMachineHandle) == 0)
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    if (machineResourcesMap[laserMachineHandle].laserHeadBrand != laserMachineResource->laserHeadBrand)
        laser_parameters::ReleaseMachineConfiguration(laserMachineHandle);
    machineResourcesMap[laserMachineHandle] = *laserMachineResource;
    return SUCCESS_LASER_SDK;
}

//获取焊机资源句柄对应的数据
LaserSdkResult cl_machine_resource_get(const LaserMachineHandle laserMachineHandle, MachineResource *laserMachineResource)
{
    if (laserMachineResource == nullptr) return PARAMETER_ERROR_LASER_SDK;
    //判断是否存在该句柄
    if(machineResourcesMap.count(laserMachineHandle) == 0)
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    *laserMachineResource = machineResourcesMap[laserMachineHandle];
    return SUCCESS_LASER_SDK;
}

//释放资源
LaserSdkResult cl_machine_resource_release(const LaserMachineHandle laserMachineHandle)
{
    if(machineResourcesMap.count(laserMachineHandle) == 0)
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    laser_parameters::ReleaseMachineConfiguration(laserMachineHandle);
    machineResourcesMap.erase(laserMachineHandle);
    return SUCCESS_LASER_SDK;
}

//起弧！！！
LaserSdkResult cl_get_arc_start_script(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen)
{
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    // 【修改理由】消除重复的通讯类型判断，统一通过工厂函数获取通讯脚本生成器
    // 【逻辑】委托 craft_script_commu_class_get 按 commuType 获取对应实例
    CommuBase* curCommuFunction = NULL;
    LaserSdkResult commuRes = craft_script_commu_class_get(laserMachineHandle, curCommuFunction);
    if (commuRes != SUCCESS_LASER_SDK)
    {
        return commuRes;
    }

    if (laser_parameters::UsesMappedParameters(laserMachineHandle, machineResource.laserHeadBrand))
    {
        curCommuFunction->resetPendingScriptFragments();
        std::string body;
        LaserSdkResult result = curCommuFunction->craft_script_arc_function_switch_script_get(
                    laserMachineHandle, SWITCH_ON_LASER_SDK);
        if (result == SUCCESS_LASER_SDK)
            result = curCommuFunction->craft_script_arc_program_run_script_get(
                        laserMachineHandle, SWITCH_ON_LASER_SDK, body);
        if (result != SUCCESS_LASER_SDK)
        {
            curCommuFunction->resetPendingScriptFragments();
            return result;
        }
        body = curCommuFunction->loadLuaFunctionScriptOut(machineResource.laserHeadBrand, true) + body;
        return strToChar(scripts, scriptLen, body);
    }

    std::vector<unsigned int> laserPowerVec;
    std::vector<unsigned int> laserHeadVec;
    std::vector<unsigned int> wireFeedVec;
    //需要设置的激光器地址参数 - 激光器功率 - 开光功率 - 开光渐进时间 -
    laserPowerVec.push_back(machineResource.gasONDelayLoc);           ///<  开气延时地址 0x0235  先等送气结束
    laserPowerVec.push_back(machineResource.laserPowerLoc);           ///<  激光器功率地址 0x0230 560
    laserPowerVec.push_back(machineResource.laserOnPowerLoc);         ///<  开光功率地址 0x0231 561
    laserPowerVec.push_back(machineResource.laserOnRampTimeLoc);      ///<  开光渐进时间地址 0x0232 562
    laserPowerVec.push_back(machineResource.laserHeadYOffsetLoc);     ///<  Y轴偏移地址 0x0239 569
    laserPowerVec.push_back(machineResource.laserHeadXOffsetLoc);     ///<  X轴偏移地址 0x023b 571
    laserPowerVec.push_back(machineResource.laserHeadYScaleLoc);      ///<  Y轴放大地址 0x0238 568
    laserPowerVec.push_back(machineResource.laserHeadXScaleLoc);      ///<  X轴放大地址 0x023a 570
    laserPowerVec.push_back(machineResource.spotWeldTypeLoc);         ///<  点焊类型地址 0x0242 578
    //需要设置的激光头地址参数 - 扫描速度 - 扫描宽 - 峰值功率 - 占空比 - 脉冲频率 - 扫描高度 - 旋转角度 - 图形切换
    laserHeadVec.push_back(machineResource.laserHeadPatternIdLoc);      //图形首位
    laserHeadVec.push_back(machineResource.laserHeadScanSpeedLoc);
    laserHeadVec.push_back(machineResource.laserHeadScanWidthLoc);
    laserHeadVec.push_back(machineResource.laserHeadPeakPowerLoc);
    laserHeadVec.push_back(machineResource.laserHeadDutyCycleLoc);
    laserHeadVec.push_back(machineResource.laserHeadFrequencyLowLoc);   ///<  脉冲频率 : 15~0位地址  0x0005  5
    laserHeadVec.push_back(machineResource.laserHeadFrequencyHighLoc);  ///<  脉冲频率 : 31~16位地址 0x0004  4
    laserHeadVec.push_back(machineResource.laserHeadScanHeightLoc);     ///<  扫描高度地址 0x0006 6
    laserHeadVec.push_back(machineResource.laserHeadRotationAngleLoc);  ///<  旋转角度地址 0x0007 7
    laserHeadVec.push_back(machineResource.gasValveSignalLoc);          ///<  送气信号开关地址 0x0031 → ​​49​
    laserHeadVec.push_back(machineResource.wireFeedSignalLoc);          ///<  送丝信号开关地址 0x0032 → ​​50

    //送丝机地址参数 - 自动送丝速度
    wireFeedVec.push_back(machineResource.autoWireFeedSpeedLoc); ///<  自动送丝速度地址 0x0100 256
    wireFeedVec.push_back(machineResource.wireRetractLengthLoc); ///<  抽丝长度地址 0x0104 260
    wireFeedVec.push_back(machineResource.wireFeedLengthLoc);    ///<  送丝长度地址 0x0105 261

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
        checkLaserSdkResult(curCommuFunction->craft_script_arc_program_run_script_get(laserMachineHandle, SWITCH_ON_LASER_SDK, scriptGet));
        scriptGet = curCommuFunction->loadLuaFunctionScriptOut(machineResource.laserHeadBrand,
                laser_parameters::UsesMappedParameters(laserMachineHandle, machineResource.laserHeadBrand)) + scriptGet;
        //脚本赋值
        LaserSdkResult bufferResult = strToChar(scripts, scriptLen, scriptGet);
        if(bufferResult != SUCCESS_LASER_SDK)
        {
            return bufferResult;
        }
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }

    return SUCCESS_LASER_SDK;
}

//收弧！！！
LaserSdkResult cl_get_arc_stop_script(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    // 【修改理由】消除重复的通讯类型判断，统一通过工厂函数获取通讯脚本生成器
    CommuBase* curCommuFunction = NULL;
    LaserSdkResult commuRes = craft_script_commu_class_get(laserMachineHandle, curCommuFunction);
    if (commuRes != SUCCESS_LASER_SDK)
    {
        return commuRes;
    }

    if (laser_parameters::UsesMappedParameters(laserMachineHandle, machineResource.laserHeadBrand))
    {
        curCommuFunction->resetPendingScriptFragments();
        std::string body;
        LaserSdkResult result = curCommuFunction->craft_script_arc_function_switch_script_get(
                    laserMachineHandle, SWITCH_OFF_LASER_SDK);
        if (result == SUCCESS_LASER_SDK)
            result = curCommuFunction->craft_script_arc_program_run_script_get(
                        laserMachineHandle, SWITCH_OFF_LASER_SDK, body);
        if (result != SUCCESS_LASER_SDK)
        {
            curCommuFunction->resetPendingScriptFragments();
            return result;
        }
        body = curCommuFunction->loadLuaFunctionScriptOut(machineResource.laserHeadBrand, true) + body;
        return strToChar(scripts, scriptLen, body);
    }

    std::vector<unsigned int> laserPowerVec;
    std::vector<unsigned int> laserHeadVec;
    std::vector<unsigned int> wireFeedVec;
    if(machineResource.laserHeadBrand == CQLASERPOWER_SDK)
    {
        //需要设置的激光器地址参数 - 激光器功率
        laserPowerVec.push_back(machineResource.laserOffPowerLoc);    ///<  关光功率地址 0x0233 563
        laserPowerVec.push_back(machineResource.laserOffRampTimeLoc); ///<  关光渐进时间地址 0x0234 564
    //  laserPowerVec.push_back(machineResource.laserPowerLoc);       ///<  激光器功率地址 0x0230 560
        //送丝机地址参数 - 自动送丝速度 - 收尾抽丝
        wireFeedVec.push_back(machineResource.wireRetractLengthLoc); //260 焊丝回抽长度
        wireFeedVec.push_back(machineResource.wireFeedLengthLoc);    //261 补丝长度地址

        //需要设置的激光头地址参数  关气 关送丝信号
        laserHeadVec.push_back(machineResource.gasValveSignalLoc); ///<  送气信号开关地址 0x0031 → ​​49​
        laserHeadVec.push_back(machineResource.wireFeedSignalLoc); ///<  送丝信号开关地址 0x0032 → ​​50
        laserHeadVec.push_back(machineResource.gasOffDelayLoc);    ///< 关气延时
    }
    else if(machineResource.laserHeadBrand == BCXLASERPOWER_SDK)
    {
        //需要设置的激光器地址参数 - 激光器功率 - 关光功率 - 关光渐进时间 -
        laserPowerVec.push_back(machineResource.laserOffPowerLoc);    ///<  关光功率地址 0x0233 563
        laserPowerVec.push_back(machineResource.laserOffRampTimeLoc); ///<  关光渐进时间地址 0x0234 564
    //  laserPowerVec.push_back(machineResource.laserPowerLoc);       ///<  激光器功率地址 0x0230 560
        //送丝机地址参数 - 自动送丝速度 - 收尾抽丝
        wireFeedVec.push_back(machineResource.wireRetractLengthLoc); //260 焊丝回抽长度
        wireFeedVec.push_back(machineResource.wireFeedLengthLoc);    //261 补丝长度地址

        //需要设置的激光头地址参数  关气 关送丝信号
        laserHeadVec.push_back(machineResource.wireFeedSignalLoc); ///<  送丝信号开关地址 0x0032 → ​​50
        laserHeadVec.push_back(machineResource.gasOffDelayLoc);    ///< 关气延时
    }
    else if(machineResource.laserHeadBrand == OSRPLASERPOWER_SDK)
    {
        //需要设置的激光器地址参数 - 激光器功率 - 关光功率 - 关光渐进时间 -
        laserPowerVec.push_back(machineResource.laserOffPowerLoc);    ///<  关光功率地址 0x0233 563
        laserPowerVec.push_back(machineResource.laserOffRampTimeLoc); ///<  关光渐进时间地址 0x0234 564
    //  laserPowerVec.push_back(machineResource.laserPowerLoc);       ///<  激光器功率地址 0x0230 560
        //送丝机地址参数 - 自动送丝速度 - 收尾抽丝
        wireFeedVec.push_back(machineResource.wireRetractLengthLoc); //260 焊丝回抽长度
        wireFeedVec.push_back(machineResource.wireFeedLengthLoc);    //261 补丝长度地址

        //需要设置的激光头地址参数  关气 关送丝信号
        laserHeadVec.push_back(machineResource.gasOffDelayLoc); ///< 关气延时
    }

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
        checkLaserSdkResult(curCommuFunction->craft_script_arc_program_run_script_get(laserMachineHandle, SWITCH_OFF_LASER_SDK, scriptGet));
        //收弧执行程序
        //scriptGet = funcScript + scriptGet;      //脚本赋值
        scriptGet = curCommuFunction->loadLuaFunctionScriptOut(machineResource.laserHeadBrand,
                laser_parameters::UsesMappedParameters(laserMachineHandle, machineResource.laserHeadBrand)) + scriptGet;
        LaserSdkResult bufferResult = strToChar(scripts, scriptLen, scriptGet);
        if(bufferResult != SUCCESS_LASER_SDK)
        {
            return bufferResult;
        }
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }

    return SUCCESS_LASER_SDK;
}

// 初始化焊机相关的寄存机值
LaserSdkResult cl_craft_weld_status_init(const LaserMachineHandle laserMachineHandle)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    // 【修改理由】消除重复的通讯类型判断，统一通过工厂函数获取通讯脚本生成器
    CommuBase* curCommuFunction = NULL;
    LaserSdkResult commuRes = craft_script_commu_class_get(laserMachineHandle, curCommuFunction);
    if (commuRes != SUCCESS_LASER_SDK)
    {
        return commuRes;
    }
    try
    {
        BOOL boolVal = FALSE;
        int  intVal  = 0;


        checkCrSdkResultOrThrow(cr_set_boolRegValue(machineResource.robotHandle, BOOL_REG_EXTSTRUCT_611, 1, &boolVal));
        checkCrSdkResultOrThrow(cr_set_boolRegValue(machineResource.robotHandle, BOOL_REG_EXTSTRUCT_612, 1, &boolVal));

        checkCrSdkResultOrThrow(cr_set_boolRegValue(machineResource.robotHandle, BOOL_REG_EXTSTRUCT_650, 1, &boolVal));
        checkCrSdkResultOrThrow(cr_set_boolRegValue(machineResource.robotHandle, BOOL_REG_EXTSTRUCT_651, 1, &boolVal));
        checkCrSdkResultOrThrow(cr_set_boolRegValue(machineResource.robotHandle, BOOL_REG_EXTSTRUCT_652, 1, &boolVal));
        checkCrSdkResultOrThrow(cr_set_boolRegValue(machineResource.robotHandle, BOOL_REG_EXTSTRUCT_653, 1, &boolVal));
        checkCrSdkResultOrThrow(cr_set_boolRegValue(machineResource.robotHandle, BOOL_REG_EXTSTRUCT_654, 1, &boolVal));
        checkCrSdkResultOrThrow(cr_set_boolRegValue(machineResource.robotHandle, BOOL_REG_EXTSTRUCT_700, 1, &boolVal));


        checkCrSdkResultOrThrow(cr_set_int32RegValue(machineResource.robotHandle, INT32_REG_WELD_STATUS_MONITOR_613, 1, &intVal));
        checkCrSdkResultOrThrow(cr_set_int32RegValue(machineResource.robotHandle, INT32_REG_PROGRAM_STOP_ARC_END_614, 1, &intVal));
        //checkCrSdkResultOrThrow(cr_set_int32RegValue(machineResource.robotHandle, INT32_REG_SINGLE_STEP_RUN_616, 1, &intVal));
        checkCrSdkResultOrThrow(cr_set_int32RegValue(machineResource.robotHandle, INT32_REG_RUN_BREAK_POINT_617, 1, &intVal));

        checkCrSdkResultOrThrow(cr_set_int32RegValue(machineResource.robotHandle, INT32_REG_ARC_START_FAIL_673, 1, &intVal));
        checkCrSdkResultOrThrow(cr_set_int32RegValue(machineResource.robotHandle, INT32_REG_WELD_INTERRUPT_674, 1, &intVal));
        checkCrSdkResultOrThrow(cr_set_int32RegValue(machineResource.robotHandle, INT32_REG_ARC_END_FAIL_675, 1, &intVal));
        checkCrSdkResultOrThrow(cr_set_int32RegValue(machineResource.robotHandle, INT32_REG_WELD_MODE_660, 1, &intVal));
        checkCrSdkResultOrThrow(cr_set_int32RegValue(machineResource.robotHandle, INT32_REG_WELD_STATE_799, 1, &intVal));
        checkCrSdkResultOrThrow(cr_set_int32RegValue(machineResource.robotHandle, INT32_REG_WELD_PATH_662, 1, &intVal));
        checkCrSdkResultOrThrow(cr_set_int32RegValue(machineResource.robotHandle, INT32_REG_ARRIVE_POINT_LOC_663, 1, &intVal));
        checkCrSdkResultOrThrow(cr_set_int32RegValue(machineResource.robotHandle, INT32_REG_PRESENT_CRAFT_LOC_664, 1, &intVal));

        checkCrSdkResultOrThrow(cr_set_int32RegValue(machineResource.robotHandle, INT32_REG_COMMU_TIMEOUT_821, 1, &intVal));

        checkCrSdkResultOrThrow(cr_set_int16RegValue(machineResource.robotHandle, INT_REG_WRITE_PARAMETER_62, 1, &intVal));
        checkCrSdkResultOrThrow(cr_set_int16RegValue(machineResource.robotHandle, INT_REG_WRITE_PARAMETER_63, 1, &intVal));
        checkCrSdkResultOrThrow(cr_set_int16RegValue(machineResource.robotHandle, INT_REG_WRITE_PARAMETER_64, 1, &intVal));
        checkCrSdkResultOrThrow(cr_set_int16RegValue(machineResource.robotHandle, INT_REG_WRITE_PARAMETER_65, 1, &intVal));
        checkCrSdkResultOrThrow(cr_set_int16RegValue(machineResource.robotHandle, INT_REG_READ_PARAMETER_68,  1, &intVal));
        checkCrSdkResultOrThrow(cr_set_int16RegValue(machineResource.robotHandle, INT_REG_READ_PARAMETER_69,  1, &intVal));
        checkCrSdkResultOrThrow(cr_set_int16RegValue(machineResource.robotHandle, INT_REG_RESULT_77,  1, &intVal));
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }
    return SUCCESS_LASER_SDK;
}

//指令列表转换
LaserSdkResult cl_craft_inst_list_script_get(const LaserMachineHandle laserMachineHandle,
                                             LaserWeldStartData weldStartData,
                                             bool isWeld,
                                             char *scripts,
                                             int *scriptLen)
{
    if (scriptLen == NULL)
    {
        return PARAMETER_ERROR_LASER_SDK;
    }
    if (scripts != NULL && *scriptLen < 0)
    {
        return PARAMETER_ERROR_LASER_SDK;
    }
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }

    // 1. 完整校验所有计数和裸指针，后续阶段只处理可信数据。
    if (weldStartData.instNum < 0 || weldStartData.craftNum < 0
            || weldStartData.pointsNum < 0 || weldStartData.breakPointNum < 0
            || weldStartData.signalStepPointNum < 0
            || weldStartData.instNum > INT_MAX / 2)
    {
        return PARAMETER_ERROR_LASER_SDK;
    }
    if ((weldStartData.instNum > 0 && weldStartData.instList == NULL)
            || (weldStartData.craftNum > 0 && weldStartData.craftInfoList == NULL)
            || (weldStartData.pointsNum > 0 && weldStartData.pointList == NULL)
            || (weldStartData.breakPointNum > 0 && weldStartData.breakPointLocList == NULL)
            || (weldStartData.signalStepPointNum > 0 && weldStartData.signalStepPointLocList == NULL))
    {
        return PARAMETER_ERROR_LASER_SDK;
    }

    InstListData instListData;
    for (int instLoc = 0; instLoc < weldStartData.instNum; ++instLoc)
    {
        const int rawType = weldStartData.instList[instLoc * 2];
        const int dataListLoc = weldStartData.instList[instLoc * 2 + 1];
        if (rawType < CRAFT_SELECT_INST_LASER_WELD_SDK || rawType > SLOW_DOWN_LASER_WELD_SDK)
        {
            return INSTLIST_ERROR_LASER_SDK;
        }

        InstTypeLaserSdk instType = static_cast<InstTypeLaserSdk>(rawType);
        // SAFE_POINT_MOVEL仅保留旧数据枚举值，进入解释器前统一归一化为MoveJ。
        if (instType == SAFE_POINT_MOVEL_INST_LASER_WELD_SDK)
        {
            instType = SAFE_POINT_MOVEJ_INST_LASER_WELD_SDK;
        }

        const int typeID = instLoc;
        instListData.listLocToTypeID.emplace(instLoc, std::make_pair(instType, typeID));
        if (instType == CRAFT_SELECT_INST_LASER_WELD_SDK)
        {
            if (dataListLoc < 0 || dataListLoc >= weldStartData.craftNum)
            {
                return INSTLIST_ERROR_LASER_SDK;
            }
            instListData.craftsMap.emplace(typeID, weldStartData.craftInfoList[dataListLoc]);
        }
        else if (instType == LINE_POINT_INST_LASER_WELD_SDK
                 || instType == CIRCLE_POINT_INST_LASER_WELD_SDK
                 || instType == JOINT_POINT_INST_LASER_WELD_SDK
                 || instType == SAFE_POINT_MOVEJ_INST_LASER_WELD_SDK)
        {
            if (dataListLoc < 0 || dataListLoc >= weldStartData.pointsNum)
            {
                return INSTLIST_ERROR_LASER_SDK;
            }
            instListData.pointsMap.emplace(typeID, weldStartData.pointList[dataListLoc]);
        }
    }

    for (int i = 0; i < weldStartData.breakPointNum; ++i)
    {
        if (weldStartData.breakPointLocList[i] < 0
                || weldStartData.breakPointLocList[i] >= weldStartData.instNum)
        {
            return INSTLIST_ERROR_LASER_SDK;
        }
    }
    for (int i = 0; i < weldStartData.signalStepPointNum; ++i)
    {
        if (weldStartData.signalStepPointLocList[i] < 0
                || weldStartData.signalStepPointLocList[i] >= weldStartData.instNum)
        {
            return INSTLIST_ERROR_LASER_SDK;
        }
    }

    CommuBase* craftCommu = NULL;
    LaserSdkResult result = craft_script_commu_class_get(laserMachineHandle, craftCommu);
    if (result != SUCCESS_LASER_SDK || craftCommu == NULL)
    {
        return result == SUCCESS_LASER_SDK ? COMMU_ERROR_LASER_SDK : result;
    }

    MachineResource& machineResource = machineResourcesMap[laserMachineHandle];
    struct ScriptGenerationScope
    {
        MachineResource& resource;
        MachineResource snapshot;
        CommuBase& commu;

        ScriptGenerationScope(MachineResource& machineResource, CommuBase& commuFunction)
            : resource(machineResource), snapshot(machineResource), commu(commuFunction)
        {
            // 上一次失败调用可能留下未输出函数，本次生成必须从空缓存开始。
            commu.resetPendingScriptFragments();
        }

        ~ScriptGenerationScope()
        {
            // 脚本生成是纯编译过程，不允许长度查询或失败调用改变焊机资源状态。
            resource = snapshot;
            commu.resetPendingScriptFragments();
        }
    } generationScope(machineResource, *craftCommu);

    InstListTransEnvironmentData environment;
    environment.craftID = -1;
    environment.prePointLoc = -1;
    environment.isWelding = false;
    environment.policy.emitDeviceParameters = isWeld;
    environment.policy.emitArcControl = isWeld;
    environment.policy.emitRampDownControl = isWeld;
    environment.policy.useWeldMotionParameters = isWeld;

    std::string bodyScriptText;
    result = GetBasicCraftScript(laserMachineHandle, instListData, environment, bodyScriptText);
    if (result != SUCCESS_LASER_SDK)
    {
        return result;
    }

    // 2. 模拟模式只生成轨迹控制和交互列表，不下发激光器、送丝机、起收弧及缓降参数。
    std::string breakPointListScript;
    std::string signalStepPointLocListScript;
    if (!isWeld)
    {
        for (int i = 0; i < weldStartData.breakPointNum; ++i)
        {
            if (i != 0)
            {
                breakPointListScript += ", ";
            }
            breakPointListScript += std::to_string(weldStartData.breakPointLocList[i]);
        }
        breakPointListScript = "breakPointLocList = {" + breakPointListScript + "}\n"
                "breakPointLocListLookUp = {}\n"
                "for k, v in pairs(breakPointLocList) do\n"
                "    breakPointLocListLookUp[v] = 1\n"
                "end\n";

        for (int i = 0; i < weldStartData.signalStepPointNum; ++i)
        {
            if (i != 0)
            {
                signalStepPointLocListScript += ", ";
            }
            signalStepPointLocListScript += std::to_string(weldStartData.signalStepPointLocList[i]);
        }
        signalStepPointLocListScript = "signalSetpPointLocList = {" + signalStepPointLocListScript + "}\n"
                "signalSetpPointLocListLookUp = {}\n"
                "for k, v in pairs(signalSetpPointLocList) do\n"
                "    signalSetpPointLocListLookUp[v] = 1\n"
                "end\n";
    }

    std::string script;
    script += craftCommu->loadLuaFunctionScriptOut(machineResource.laserHeadBrand,
                laser_parameters::UsesMappedParameters(laserMachineHandle, machineResource.laserHeadBrand));
    script += breakPointListScript;
    script += signalStepPointLocListScript;
    script += strWriteInt32RegReplace(INT32_REG_WELD_MODE_660, isWeld ? 2 : 1);
    script += bodyScriptText;

    // scripts为nullptr是合法长度查询；strToChar保证容量不足时不写缓冲区。
    //脚本赋值
    LaserSdkResult bufferResult = strToChar(scripts, scriptLen, script);
    if(bufferResult != SUCCESS_LASER_SDK)
    {
        return bufferResult;
    }
    return bufferResult;
}
LaserSdkResult cl_craft_end_arc_subprogram_script_get(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen)
{
    //句柄
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    // 【修改理由】消除重复的通讯类型判断，统一通过工厂函数获取通讯脚本生成器
    CommuBase* curCommuFunction = NULL;
    LaserSdkResult commuRes = craft_script_commu_class_get(laserMachineHandle, curCommuFunction);
    if (commuRes != SUCCESS_LASER_SDK)
    {
        return commuRes;
    }

    std::string endArcProtectionScript = R"(
    function endArcing()
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
            write_integer_register(821, 0)  -- 清空超时寄存器
            write_integer_register(614, 0)  -- 自动收弧复位
            sync()
        end
    end
    sysEvent_pause_func_2_Result = sys_event_register(2, pause_func)

    function resume_func()
        while(true)
        do
            write_bool_register(600, 0)     -- 末端按钮拖动位
            if read_boolean_register(611) then
                set_configurable_digital_out(1, true)  ---CO1 = 0
                wait(%1)
            end
            write_integer_register(821, 0)  -- 超时地址清空
            write_integer_register(614, 1)  -- 自动收弧信号
            sync()
        end
    end
    sysEvent_resume_func_3_Result = sys_event_register(3, resume_func)
    )";
    if(machineResource.laserPowerParams.gasStartupAdvanceTime < 0 || machineResource.laserPowerParams.gasStartupAdvanceTime > 3000)
        return PARAMETER_OUT_OF_RANGE_LASER_SDK;

    std::string scriptText;
    scriptText += strReplace(endArcProtectionScript,"%1", machineResource.laserPowerParams.gasStartupAdvanceTime);
    if(static_cast<int>(scriptText.size()) > *scriptLen)
    {
        return MEMORY_ERROR_LASER_SDK;
    }
    *scriptLen = static_cast<int>(scriptText.size())+1;
    memcpy(scripts, scriptText.c_str(), static_cast<size_t>(*scriptLen));
    memcpy(scripts + scriptText.size(), "\0", 1);

    return SUCCESS_LASER_SDK;
}

LaserSdkResult cl_craft_after_weld_end_operate_script_get(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen)
{
    std::string script;

    int programStopArcEndAddress = INT32_REG_PROGRAM_STOP_ARC_END_614;   // 程序停止后自动收弧开启状态位：1 有效 0 无效
    int signalSetpRunAddress = INT32_REG_SINGLE_STEP_RUN_616;  // 单步运行状态位：1 有效 0 无效
    int normalRunAllScriptAddress = INT32_REG_NORMAL_RUN_ALL_SCRIPT_LOC;
    script += strWriteInt32RegReplace(programStopArcEndAddress, 0); // 程序停止后自动收弧关闭
    script += strWriteInt32RegReplace(signalSetpRunAddress, 0);

    script += strWriteInt32RegReplace(normalRunAllScriptAddress, 1);

    if(static_cast<int>(script.size()) + 1 > *scriptLen)
    {
        return MEMORY_ERROR_LASER_SDK;
    }
    *scriptLen = static_cast<int>(script.size()) + 1;
    memcpy(scripts, script.c_str(), script.size());
    memcpy(scripts + script.size(), "\0", 1);
    return SUCCESS_LASER_SDK;
}

//缓升起弧-阿文
LaserSdkResult cl_get_arc_start_slow_script(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    // 【修改理由】消除重复的通讯类型判断，统一通过工厂函数获取通讯脚本生成器
    CommuBase* curCommuFunction = NULL;
    LaserSdkResult commuRes = craft_script_commu_class_get(laserMachineHandle, curCommuFunction);
    if (commuRes != SUCCESS_LASER_SDK)
    {
        return commuRes;
    }

    //需要设置的激光器地址参数 - 激光器功率 - 开光功率 - 开光渐进时间 -
    // 开光功率为开光一瞬间的功率，是百分比值；
    // 激光器功率为激光器希望激光所能到达到的功率，开光功率的百分比就是乘的激光器功率
    // 峰值功率为激光头所限制激光器的最大功率
    std::vector<unsigned int> laserPowerVec;
    laserPowerVec.push_back(machineResource.gasONDelayLoc);       ///<  开气延时地址 0x0235  先等送气结束
    laserPowerVec.push_back(machineResource.laserOnPowerLoc);     ///<  开光功率百分比地址 0x0231 561
    laserPowerVec.push_back(machineResource.laserPowerLoc);       ///<  激光器功率地址 0x0230 569
    laserPowerVec.push_back(machineResource.laserHeadXOffsetLoc); ///<  X轴偏移地址 0x023b 571
    laserPowerVec.push_back(machineResource.laserHeadYScaleLoc);  ///<  Y轴放大地址 0x0238 568
    laserPowerVec.push_back(machineResource.laserHeadXScaleLoc);  ///<  X轴放大地址 0x023a 570
    laserPowerVec.push_back(machineResource.laserHeadYOffsetLoc); ///<  Y轴偏移地址 0x0239 578


    //需要设置的激光头地址参数 - 扫描速度 - 扫描宽 - 峰值功率 - 占空比 - 脉冲频率 - 扫描高度 - 旋转角度 - 图形切换
    std::vector<unsigned int> laserHeadVec;
    laserHeadVec.push_back(machineResource.laserHeadPatternIdLoc);      //图形首位
    laserHeadVec.push_back(machineResource.laserHeadScanSpeedLoc);
    laserHeadVec.push_back(machineResource.laserHeadScanWidthLoc);
    laserHeadVec.push_back(machineResource.laserHeadPeakPowerLoc);      ///<  峰值功率
    laserHeadVec.push_back(machineResource.laserHeadDutyCycleLoc);
    laserHeadVec.push_back(machineResource.laserHeadFrequencyLowLoc);   ///<  脉冲频率 : 15~0位地址  0x0005  5
    laserHeadVec.push_back(machineResource.laserHeadFrequencyHighLoc);  ///<  脉冲频率 : 31~16位地址 0x0004  4
    laserHeadVec.push_back(machineResource.laserHeadScanHeightLoc);     ///<  扫描高度地址 0x0006 6
    laserHeadVec.push_back(machineResource.laserHeadRotationAngleLoc);  ///<  旋转角度地址 0x0007 7

    //送丝机地址参数
    std::vector<unsigned int> wireFeedVec;
    wireFeedVec.push_back(machineResource.wireFeederStatusLoc);  ///<  送丝使能 0x0115
    wireFeedVec.push_back(machineResource.wireFeedSignalLoc);    ///<  送丝信号开 0x0032
    //wireFeedVec.push_back(machineResource.operationModeLoc);   ///<  写模式 0x010c
    wireFeedVec.push_back(machineResource.autoWireFeedSpeedLoc); ///<  自动送丝速度 0x0100

    CgxiLaserHeadLaserSdk &laserHeadParams  = machineResource.laserHeadParams;
    CgxiWireFeederLaserSdk &wireFeederParams  = machineResource.wireFeederParams;
    CgxiLaserPowerLaserSdk &laserPowerParams  = machineResource.laserPowerParams;
    int time = laserPowerParams.gasStartupAdvanceTime;

    //获取脚本
    std::string scriptGet;

    try
    {
        // 【修改理由】SUP26AD / 宝辰鑫 属于映射从站品牌，参数与控制只能写机器人本地寄存器，
        //   不能生成 write_value_port 自由通讯报文脚本。
        //   缓升参数（开光功率、缓升时间）已由工艺下发写入板卡寄存器，此处只输出数字输出动作，
        //   由板卡按寄存器参数自行完成内控缓升；否则报文对板卡无效，且 task_create 的后台任务
        //   会让机器人程序一直处于运行状态无法停止。
        // 【逻辑】CO0 开气/开光 → 送丝焊且由数字输出管送丝时 CO2 开送丝 → CO1 开激光使能，
        //   与宝辰鑫 arc_start 的时序保持一致；脚本外壳仍为 arcStartProgram + task_create，
        //   保证上层调用方式不变。
        const BrandProtocolAdapter& slowAdapter =
                GetBrandProtocolAdapter(machineResource.laserHeadBrand);
        if (laser_parameters::UsesMappedParameters(laserMachineHandle, machineResource.laserHeadBrand))
        {
            const bool wireWelding =
                    machineResource.weldingProcessType == WeldingProcessType::WIRE_WELDING_LASERSDK;
            std::string mappedBody = "function arcStartProgram()\n"
                                     "    set_configurable_digital_out(0, true)  ---CO0 = 1\n";
            if (wireWelding && slowAdapter.controlsWireThroughDigitalOutput())
            {
                mappedBody += "    set_configurable_digital_out(2, true)  ---CO2 = 1\n";
            }
            mappedBody += "    set_configurable_digital_out(1, true)  ---CO1 = 1\n"
                          "end\n"
                          "arcStartProgram_Result=task_create(arcStartProgram)\n";
            std::string mappedFuncScript =
                    curCommuFunction->loadLuaFunctionScriptOut(machineResource.laserHeadBrand,
                laser_parameters::UsesMappedParameters(laserMachineHandle, machineResource.laserHeadBrand));
            scriptGet = mappedFuncScript + mappedBody;
            LaserSdkResult mappedBufferResult = strToChar(scripts, scriptLen, scriptGet);
            if(mappedBufferResult != SUCCESS_LASER_SDK)
            {
                return mappedBufferResult;
            }
            return SUCCESS_LASER_SDK;
        }
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
        std::string funcScript = curCommuFunction->loadLuaFunctionScriptOut(machineResource.laserHeadBrand,
                laser_parameters::UsesMappedParameters(laserMachineHandle, machineResource.laserHeadBrand));  //功能函数
        scriptGet = funcScript + scriptGet;
        //scriptGet = funcScript + scriptGet;
        //脚本赋值
        LaserSdkResult bufferResult = strToChar(scripts, scriptLen, scriptGet);
        if(bufferResult != SUCCESS_LASER_SDK)
        {
            return bufferResult;
        }
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }

    return SUCCESS_LASER_SDK;
}

//缓降收弧-阿文
LaserSdkResult cl_get_arc_stop_slow_script(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    // 【修改理由】消除重复的通讯类型判断，统一通过工厂函数获取通讯脚本生成器
    CommuBase* curCommuFunction = NULL;
    LaserSdkResult commuRes = craft_script_commu_class_get(laserMachineHandle, curCommuFunction);
    if (commuRes != SUCCESS_LASER_SDK)
    {
        return commuRes;
    }

    // 需要设置的激光器地址参数 - 激光器功率 - 开光功率 - 开光渐进时间 -
    // 开光功率为开光一瞬间的功率，是百分比值；
    // 激光器功率为激光器希望激光所能到达到的功率，开光功率的百分比就是乘的激光器功率
    // 峰值功率为激光头所限制激光器的最大功率
    std::vector<unsigned int> laserPowerVec;
    laserPowerVec.push_back(machineResource.gasOffDelayLoc);   ///<  关气延迟地址 0x0236
    laserPowerVec.push_back(machineResource.laserOffPowerLoc); ///<  关光功率百分比地址 0x0230 569

    //送丝机地址参数
    std::vector<unsigned int> wireFeedVec;
    wireFeedVec.push_back(machineResource.autoWireFeedSpeedLoc);  ///<  自动送丝速度 0x0100
    //wireFeedVec.push_back(machineResource.wirefeederCompensateDelay); ///<  补丝延迟 0x0106
    wireFeedVec.push_back(machineResource.wireRetractLengthLoc);  ///<  回抽丝长度 0x0104
    wireFeedVec.push_back(machineResource.wireFeedLengthLoc);     ///<  补丝长度 0x0105

    CgxiLaserHeadLaserSdk &laserHeadParams  = machineResource.laserHeadParams;
    CgxiWireFeederLaserSdk &wireFeederParams  = machineResource.wireFeederParams;
    CgxiLaserPowerLaserSdk &laserPowerParams  = machineResource.laserPowerParams;
    //获取脚本
    std::string scriptGet;
    try
    {
        // 【修改理由】SUP26AD / 宝辰鑫 属于映射从站品牌，缓降只能由板卡按寄存器参数内控完成，
        //   不能生成 write_value_port 逐步降功率的报文脚本，也不能创建后台缓降任务。
        // 【逻辑】缓降参数（关光功率、缓降时间）已由工艺下发写入板卡寄存器；
        //   此处只在降功率点关闭 CO1（送丝焊且由数字输出管送丝时同时关闭 CO2），
        //   由板卡使能下降沿触发内控缓降，与宝辰鑫 appendRampDownControl 的内控分支一致。
        const BrandProtocolAdapter& slowAdapter =
                GetBrandProtocolAdapter(machineResource.laserHeadBrand);
        if (laser_parameters::UsesMappedParameters(laserMachineHandle, machineResource.laserHeadBrand))
        {
            const bool wireWelding =
                    machineResource.weldingProcessType == WeldingProcessType::WIRE_WELDING_LASERSDK;
            std::string mappedBody = "function arcStartProgram()\n";
            if (wireWelding && slowAdapter.controlsWireThroughDigitalOutput())
            {
                mappedBody += "    set_configurable_digital_out(2, false)  ---CO2 = 0\n";
            }
            mappedBody += "    set_configurable_digital_out(1, false)  ---CO1 = 0\n"
                          "end\n"
                          "arcStartProgram_Result=task_create(arcStartProgram)\n";
            std::string mappedFuncScript =
                    curCommuFunction->loadLuaFunctionScriptOut(machineResource.laserHeadBrand,
                laser_parameters::UsesMappedParameters(laserMachineHandle, machineResource.laserHeadBrand));
            scriptGet = mappedFuncScript + mappedBody;
            LaserSdkResult mappedBufferResult = strToChar(scripts, scriptLen, scriptGet);
            if(mappedBufferResult != SUCCESS_LASER_SDK)
            {
                return mappedBufferResult;
            }
            return SUCCESS_LASER_SDK;
        }
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
        std::string funcScript = curCommuFunction->loadLuaFunctionScriptOut(machineResource.laserHeadBrand,
                laser_parameters::UsesMappedParameters(laserMachineHandle, machineResource.laserHeadBrand));  //功能函数
        scriptGet = funcScript + scriptGet;
        //scriptGet = funcScript + scriptGet;
        //脚本赋值
        LaserSdkResult bufferResult = strToChar(scripts, scriptLen, scriptGet);
        if(bufferResult != SUCCESS_LASER_SDK)
        {
            return bufferResult;
        }
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }

    return SUCCESS_LASER_SDK;
}

//设备状态监控
LaserSdkResult cl_craft_weld_status_monitor_script_get(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen)
{
    //状态监控
    std::string funcScript = "--weld status monito --";
    return SUCCESS_LASER_SDK;
}


LaserSdkResult cl_craft_lift_pose_cal(const LaserMachineHandle laserMachineHandle,
                                      const double* pose,
                                      const double* joint,
                                      const double liftDistance,
                                      double* liftPose, double* liftJoint)
{
    ///< 焊枪姿态调整
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
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
    if (res != SUCCESS_LASER_SDK)
    {
        return res;
    }

    copy_pose_HY(liftJointTmp, liftJoint);

    return SUCCESS_LASER_SDK;
}

//系统设备参数设置
LaserSdkResult cl_craft_device_status_modify(const LaserMachineHandle laserMachineHandle, char* scripts, int* scriptLen)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    if (laser_parameters::UsesMappedParameters(laserMachineHandle, machineResource.laserHeadBrand))
    {
        try
        {
            std::string mappedScript;
            const LaserSdkResult result = buildMappedDeviceSettings(laserMachineHandle, machineResource, mappedScript);
            if (result != SUCCESS_LASER_SDK) return result;
            return strToChar(scripts, scriptLen, mappedScript);
        }
        catch (const std::bad_alloc&) { return MEMORY_ERROR_LASER_SDK; }
        catch (...) { return OTHER_ERROR_LASER_SDK; }
    }

    // 【修改理由】消除重复的通讯类型判断，统一通过工厂函数获取通讯脚本生成器
    CommuBase* curCommuFunction = NULL;
    LaserSdkResult commuRes = craft_script_commu_class_get(laserMachineHandle, curCommuFunction);
    if (commuRes != SUCCESS_LASER_SDK)
    {
        return commuRes;
    }

    std::vector<unsigned int> laserPowerVec;
    std::vector<unsigned int> laserHeadVec;
    std::vector<unsigned int> wireFeedVec;
    if(machineResource.laserHeadBrand == CQLASERPOWER_SDK)
    {
        //需要设置的激光器地址参数 - 激光器功率 - 关光功率 - 关光渐进时间 -
        laserPowerVec.push_back(machineResource.laserPowerLoc);             ///<  激光器功率地址 0x0230 560
        laserPowerVec.push_back(machineResource.driverTempLoc);             ///<  驱动器温度值地址 0x023f 575
        laserPowerVec.push_back(machineResource.protectiveWindowTempLoc);   ///<  保护镜温度值地址 0x0240 576
        laserPowerVec.push_back(machineResource.collimatorTempLoc);         ///<  准直镜温度值地址 0x0240 576
        // laserHeadVec.push_back(machineResource.laserHeadDutyCycleLoc);
        // laserHeadVec.push_back(machineResource.laserHeadFrequencyHighLoc);
        // laserHeadVec.push_back(machineResource.laserHeadFrequencyLowLoc);
        //送丝机地址参数 - 自动送丝速度
        wireFeedVec.push_back(machineResource.manualWireFeedSpeedLoc);     ///<  手动送丝速度地址 0x0101 257
        wireFeedVec.push_back(machineResource.manualWireRetractSpeedLoc);  ///<  手动抽丝速度地址 0x0102 258
    }
    else if(machineResource.laserHeadBrand == BCXLASERPOWER_SDK)
    {
        //需要设置的激光器地址参数 - 激光器功率 - 占空比 - 脉冲频率
        // laserPowerVec.push_back(machineResource.laserPowerLoc);
        // laserHeadVec.push_back(machineResource.laserHeadDutyCycleLoc);
        // laserHeadVec.push_back(machineResource.laserHeadFrequencyHighLoc);
        // laserHeadVec.push_back(machineResource.laserHeadFrequencyLowLoc);
        //送丝机地址参数 - 自动送丝速度
        wireFeedVec.push_back(machineResource.manualWireFeedSpeedLoc);     ///<  手动送丝速度地址
    }

    //获取脚本
    std::string scriptGet;
    try
    {
        checkLaserSdkResult(curCommuFunction->craft_laser_head_param_set_script_get(laserMachineHandle, laserHeadVec, scriptGet));
        //激光器
        checkLaserSdkResult(curCommuFunction->craft_laser_param_set_script_get(laserMachineHandle, laserPowerVec, scriptGet));
        //送丝机
        checkLaserSdkResult(curCommuFunction->craft_wire_feeder_param_set_script_get(laserMachineHandle, wireFeedVec, scriptGet));
        scriptGet = curCommuFunction->loadLuaFunctionScriptOut(machineResource.laserHeadBrand,
                laser_parameters::UsesMappedParameters(laserMachineHandle, machineResource.laserHeadBrand)) + scriptGet;
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }

    //脚本赋值
    LaserSdkResult bufferResult = strToChar(scripts, scriptLen, scriptGet);
    if(bufferResult != SUCCESS_LASER_SDK)
    {
        return bufferResult;
    }

    return SUCCESS_LASER_SDK;
}

/** @brief 纯编译工艺参数，长度查询及失败不修改资源或输出设备动作。 */
LaserSdkResult cl_craft_inst_devices_params_script_get(const LaserMachineHandle laserMachineHandle, CraftInfoLaserSdk craftInfo, char *scripts, int *scriptLen)
{
    if (scriptLen == nullptr) return PARAMETER_ERROR_LASER_SDK;
    const std::unordered_map<int, MachineResource>::const_iterator it = machineResourcesMap.find(laserMachineHandle);
    if (it == machineResourcesMap.end()) return HANDLE_ERROR_LASER_SDK;
    if (it->second.commuType != RS485_2_LASER_SDK) return COMMU_ERROR_LASER_SDK;
    try
    {
        const MachineResource resource = it->second;
        std::string body;
        const LaserSdkResult result = laser_parameters::BuildMachineCraftScript(
                    laserMachineHandle, resource, craftInfo, body);
        if (result != SUCCESS_LASER_SDK) return result;
        const bool mapped = laser_parameters::UsesMappedParameters(laserMachineHandle, resource.laserHeadBrand);
        const std::string script = (mapped ? std::string()
                : GetBrandProtocolAdapter(resource.laserHeadBrand).buildInitLuaFunc())
                + strWriteInt32RegReplace(INT32_REG_COMMU_TIMEOUT_821, 0) + body;
        return strToChar(scripts, scriptLen, script);
    }
    catch (const std::bad_alloc&) { return MEMORY_ERROR_LASER_SDK; }
    catch (const LaserSdkResult& result) { return result; }
    catch (...) { return OTHER_ERROR_LASER_SDK; }
}

LaserSdkResult cl_message_generate(const LaserMachineHandle laserMachineHandle, uint16_t firstAddr, uint16_t parameter, uint8_t slaveAddr, uint8_t funcCode, uint8_t *message,int *length)
{
    try {
        //获取句柄对应的焊机资源对象
        if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
        {
            return HANDLE_ERROR_LASER_SDK;
        }

        // 【修改理由】原代码未校验 length 空指针，直接解引用会导致崩溃
        if (message == NULL || length == NULL)
        {
            return PARAMETER_ERROR_LASER_SDK;
        }

        // 【修改理由】原代码 funcCode = 0x06 强制覆盖了调用者传入的参数，形参失去意义
        // 【逻辑】删除强制覆盖，使用调用者传入的 funcCode，保持接口灵活性
        std::map<uint16_t, uint16_t> msgMap;
        msgMap.insert(std::make_pair(firstAddr, parameter));
        std::vector<std::vector<uint8_t> > msg = buildMessageVector(msgMap, slaveAddr, funcCode);

        // 【修改理由】原代码 len += msg.size() 取的是外层vector的大小（帧数）而非每帧字节数
        // 【逻辑】遍历每帧，累加各帧字节数得到总报文长度
        size_t totalLen = 0;
        for (std::vector<std::vector<uint8_t> >::const_iterator it = msg.begin(); it != msg.end(); ++it)
        {
            const std::vector<uint8_t>& frame = *it;
            totalLen += frame.size();
        }

        // 【修改理由】原代码未检查缓冲区是否足够，直接写入可能越界
        if (static_cast<size_t>(*length) < totalLen)
        {
            *length = static_cast<int>(totalLen);  // 通知调用者所需大小
            return MEMORY_ERROR_LASER_SDK;
        }

        // 【修改理由】原代码 memcpy(message, msg.data(), msg.size()) 在循环中对同一位置重复写入
        //  且 msg.data() 是外层vector的指针(指向vector<vector>的数据)，不是实际的报文数据
        // 【逻辑】逐帧按偏移量拼接到输出缓冲区
        size_t offset = 0;
        for (std::vector<std::vector<uint8_t> >::const_iterator it = msg.begin(); it != msg.end(); ++it)
        {
            const std::vector<uint8_t>& frame = *it;
            if (!frame.empty())
            {
                memcpy(message + offset, &frame[0], frame.size());
            }
            offset += frame.size();
        }
        *length = static_cast<int>(totalLen);

        return SUCCESS_LASER_SDK;
    }
    catch (const LaserSdkResult& errorResult) {
        return errorResult;
    }
    catch (const std::bad_alloc& e) {
        std::string tmpstr = "Memory allocation failed:" + std::string(e.what()) + "\n";
        writeLog(tmpstr);
        return MEMORY_ERROR_LASER_SDK;
    }
    catch(...){
        return OTHER_ERROR_LASER_SDK;
    }

}

LaserSdkResult cl_parse_modbus_to_int16_register(const LaserMachineHandle laserMachineHandle,const char* hexResponse, const int regIndex, const int regLen,  int16_t* values)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    // 【修改理由】消除重复的通讯类型判断，统一通过工厂函数获取通讯脚本生成器
    CommuBase* curCommuFunction = NULL;
    LaserSdkResult commuRes = craft_script_commu_class_get(laserMachineHandle, curCommuFunction);
    if (commuRes != SUCCESS_LASER_SDK)
    {
        return commuRes;
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

    return SUCCESS_LASER_SDK;
}

LaserSdkResult cl_write_message_to_bool_register(const LaserMachineHandle laserMachineHandle, unsigned int firstRegister, unsigned int registerNums, uint8_t *message)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    std::vector<int> tmpValue;
    for(unsigned int i = 0; i < registerNums; ++i)
    {
        tmpValue.push_back(message[i]);
    }
    cr_set_boolRegValue(machineResource.robotHandle, firstRegister, registerNums, tmpValue.empty() ? NULL : (int *)&tmpValue[0]);
    return SUCCESS_LASER_SDK;
}

LaserSdkResult cl_write_message_to_Int16_register(const LaserMachineHandle laserMachineHandle, unsigned int firstRegister, unsigned int registerNums, uint8_t *message)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    std::vector<unsigned int> tmpValue;
    for(unsigned int i = 0; i < registerNums; ++i)
    {
        tmpValue.push_back( *(unsigned short*)&message[i*2]);
    }

    cr_set_int16RegValue(machineResource.robotHandle, firstRegister, registerNums, tmpValue.empty() ? NULL : (int *)&tmpValue[0]);

    return SUCCESS_LASER_SDK;
}


LaserSdkResult cl_parse_modbus_to_int32_register(const LaserMachineHandle laserMachineHandle,const char* hexResponse, int32_t &value)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    // 【修改理由】消除重复的通讯类型判断，统一通过工厂函数获取通讯脚本生成器
    CommuBase* curCommuFunction = NULL;
    LaserSdkResult commuRes = craft_script_commu_class_get(laserMachineHandle, curCommuFunction);
    if (commuRes != SUCCESS_LASER_SDK)
    {
        return commuRes;
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

    return SUCCESS_LASER_SDK;
}

//将状态监控报文写入Int32寄存器-小吴
LaserSdkResult cl_write_state_monitor_message_to_Int32_register(const LaserMachineHandle laserMachineHandle, unsigned int firstRegister)
{
   if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
   {
       return HANDLE_ERROR_LASER_SDK;
   }
   MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

   int32_t register_values[] = {
       18875145,       // int32Reg_854
       1121911040,     // int32Reg_855
       35652361,       // int32Reg_856
       1110376704,     // int32Reg_857
   };
   firstRegister = 854;
   CRresult cRes = cr_set_int32RegValue(machineResource.robotHandle, firstRegister, 4, register_values);
   if(cRes != CRresult::sucess)
   {
       return crsdk_return_value_transfer(cRes);
   }
   return SUCCESS_LASER_SDK;
}

LaserSdkResult cl_get_instance_function_script(const char* funcName, const char* funcParam, const unsigned int addr, const int regType,unsigned int coAddr, int comNo, char *scripts, int *scriptLen)
{
    try
    {
        std::string cppFuncName  = funcName  ? funcName  : "";
        std::string cppFuncParam = funcParam ? funcParam : "";
        CommuRegType cppRegType = static_cast<CommuRegType>(regType);
        std::string scriptGet = generateLuaScript(cppFuncName, cppFuncParam, addr, cppRegType, coAddr, comNo);
        LaserSdkResult bufferResult = strToChar(scripts, scriptLen, scriptGet);
        if(bufferResult != SUCCESS_LASER_SDK)
        {
            throw bufferResult;
        }
        return SUCCESS_LASER_SDK;
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }
    catch (...) {
        return OTHER_ERROR_LASER_SDK;
    }
}
