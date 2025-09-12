#include "cl_craftscript.h"
#include "cl_api.h"
#include "cl_utils.h"
#include "cl_luascript.h"
#include <sstream>
#include <mutex>
#include <random>
#include <memory>
#include <unordered_map>
//#pragma execution_character_set("utf-8")   //防止中文乱码

//用于生成随机数，分配焊机句柄，保证唯一性和安全性。
std::mt19937 engine;
//均匀分布的整数随机数生成器
std::uniform_int_distribution<int> distribution;
//焊机资源的全局管理表，key 是句柄，value 是焊机资源对象。
std::unordered_map<int, MachineResource> machineResourcesMap;

//RS485单例
CommuRS485 *CommuRS485::s_commuRS485 = nullptr;
std::once_flag CommuRS485::initFlag;

//预留
LaserSdkResult CommuBase::craft_script_control_stability_voltage_current_script_get(bool operate, std::string &script)
{
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//判断单步
LaserSdkResult CommuBase::craft_script_judge_signal_setp_run_script_get(int instLoc, std::string &script)
{
    unsigned int signalSetpStatusLoc = WeldControlAddressLaserSdk::BOOL_REG_SINGLE_STEP_RUN_616;

    std::string scriptText = R"(
        if(read_bool_register(%1) == 1 and signalSetpPointLocListLookUp[%2] == 1)
        then
            pause()
        end
        )";

    script += strReplace(scriptText, std::vector<std::string> {std::to_string(signalSetpStatusLoc), std::to_string(instLoc)});
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//断点判断
LaserSdkResult CommuBase::craft_script_judge_break_point_script_get(int instLoc, std::string &script)
{
    unsigned int runBreakPointStatusLoc = WeldControlAddressLaserSdk::BOOL_REG_RUN_BREAK_POINT_617;

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
    //每次调用之后清空loadLuaFunctionScript为初始状态
    std::string currentLuaScript = std::move(loadLuaFunctionScript);
    loadLuaFunctionScript.clear();
    loadLuaFunctionScript = Lua_split + Lua_get_message + Lua_crc16 + swap_bytes_for_word;
    return currentLuaScript;
}

CommuRS485 *CommuRS485::GetInstance()
{
    std::call_once(initFlag, []()
    {
        s_commuRS485 = new CommuRS485();
    });
    return s_commuRS485;
}
CommuRS485::CommuRS485()
{
    loadLuaFunctionScript = Lua_split + Lua_get_message + Lua_crc16 + swap_bytes_for_word;
}

//485-激光器使能开|关 废案
LaserSdkResult CommuRS485::craft_script_laser_on_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch, std::string &script)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    CRresult result;
    //获取脚本
    if (laserSwitch == SWITCH_ON_LASER_SDK)
    {
        script = strSetStandardDigitalReplace(0,TRUE); // 开光
        result = cr_set_configDigitalOut(machineResource.robotHandle, 0, TRUE); // 开光
        return crsdk_return_value_transfer(result);
    }
    else if (laserSwitch == SWITCH_OFF_LASER_SDK)
    {
        script = strSetStandardDigitalReplace(0,FALSE); // 不出光
        result = cr_set_configDigitalOut(machineResource.robotHandle, 0, FALSE); // 关光
        return crsdk_return_value_transfer(result);
    }
}
//485-起焊 开|关 废案
LaserSdkResult CommuRS485::craft_script_start_weld_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string &script)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    int laserSwitchIsOpen = 0 ;
    CRresult result = cr_get_stdDigitalOut(machineResource.robotHandle, 1, &laserSwitchIsOpen);
    if(result != CRresult::sucess)
    {
        return crsdk_return_value_transfer(result);
    }
    //CO1置1使能
    if(laserSwitchIsOpen == 1)
    {
        //获取脚本
        if (startWelding == SWITCH_ON_LASER_SDK)
        {
            BOOL open = 1;
            result = cr_set_configDigitalOut(machineResource.robotHandle, 1, open); // 开焊
            script = strSetStandardDigitalReplace(1,TRUE); // 开焊
            return crsdk_return_value_transfer(result);
        }
        else if (startWelding == SWITCH_OFF_LASER_SDK)
        {
            BOOL close = 0;
            result = cr_set_configDigitalOut(machineResource.robotHandle, 1, close); // 关焊
            script = strSetStandardDigitalReplace(1,FALSE); // 不出焊
            return crsdk_return_value_transfer(result);
        }
    }
    else if (laserSwitchIsOpen == 0)
    {
        return LaserSdkResult::ARC_FAILURE_LASER_SDK ;
    }

}

//激光器
LaserSdkResult CommuRS485::craft_laser_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string &script)
{
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    CgxiLaserPowerLaserSdk &laserPowerParams = machineResource.laserPowerParams;
//    if (laserPowerParams.laserPower < 0 || laserPowerParams.laserPower > 5000)
//        return PARAMETER_ERROR_LASER_SDK;
//    if (laserPowerParams.laserPowerOpen < 0 || laserPowerParams.laserPowerOpen > 5000)
//        return PARAMETER_ERROR_LASER_SDK;
//    if (laserPowerParams.laserRampUpTime < 0 || laserPowerParams.laserRampUpTime > 1000)
//        return PARAMETER_ERROR_LASER_SDK;
//    if (laserPowerParams.laserPowerClose < 0 || laserPowerParams.laserPowerClose > 5000)
//        return PARAMETER_ERROR_LASER_SDK;
//    if (laserPowerParams.driverTemp < 0 || laserPowerParams.driverTemp > 85)
//        return PARAMETER_ERROR_LASER_SDK;//    驱动器温度报警阈值
//    if (laserPowerParams.protectiveWindowTemp < 0 || laserPowerParams.protectiveWindowTemp > 85)
//        return PARAMETER_ERROR_LASER_SDK;//    保护镜温度报警阈值
//    if (laserPowerParams.collimatorTemp < 0 || laserPowerParams.collimatorTemp > 85)
//        return PARAMETER_ERROR_LASER_SDK;//    准直镜温度报警阈值
//    if (laserPowerParams.laserHeadXOffset < -3 || laserPowerParams.laserHeadXOffset > 3)
//        return PARAMETER_ERROR_LASER_SDK;//    X偏移 -3~+3mm
//    if (laserPowerParams.laserHeadYOffset < -3 || laserPowerParams.laserHeadYOffset > 3)
//        return PARAMETER_ERROR_LASER_SDK;//    Y偏移 -3~+3mm
//    if (laserPowerParams.laserHeadXEnlarge < 0.01 || laserPowerParams.laserHeadXEnlarge > 4)
//        return PARAMETER_ERROR_LASER_SDK;//    X放大 0.01~4
//    if (laserPowerParams.laserHeadYEnlarge < 0.01 || laserPowerParams.laserHeadYEnlarge > 4)
//        return PARAMETER_ERROR_LASER_SDK;//    Y放大 0.01~4
//    if (laserPowerParams.spotWeldDurationTime < 0 || laserPowerParams.spotWeldDurationTime > 60000)
//        return PARAMETER_ERROR_LASER_SDK;//    点焊持续时间 0~60000ms
//    if (laserPowerParams.spotWeldIntervalTime < 0 || laserPowerParams.spotWeldIntervalTime > 60000)
//        return PARAMETER_ERROR_LASER_SDK;//    点焊间隔时间 0~60000ms

    std::unordered_map<unsigned int, std::pair<std::string, std::string>> writeDataHash;
    //写脚本 - 脚本函数名 - 传参名 - 参数类型 - 参数值 - 地址
    auto addParam = [&](const std::string& funcName, const std::string& paramName,
                        CommuRegType regType, double paramValue, unsigned int addr = 0xFFFFFFFF, int waitTime = 0)
    {
        //没有写入地址
        if(addr == 0xFFFFFFFF)
            return LaserSdkResult::PARAMETER_ERROR_LASER_SDK;

        std::string endstr = ")\n";
        if(waitTime !=0)
            endstr = ")\nwait(" + std::to_string(waitTime) + ")\n";

        std::string funcDef = generateLuaScript(funcName, paramName, addr, regType);
        std::ostringstream useScript;
        switch (regType)
        {
        case float10Reg:
            useScript << std::fixed << std::setprecision(1); //保留小数点后1位
            useScript << funcName << "(" << paramValue << endstr;
            break;
        case float100Reg:
            useScript << std::fixed << std::setprecision(2); //保留小数点后2位
            useScript << funcName << "(" << paramValue << endstr;
            break;
        case int16Reg:
            useScript << funcName << "(" << static_cast<int>(paramValue) << endstr;
            break;
        default:
            break;
        }
        //如果地址未赋值，返回错误
        writeDataHash.emplace(addr, std::make_pair(useScript.str(), funcDef));
    };


    // 1. 写入激光器功率 (0-5000W, 整型)
    addParam("write_laser_power", "number10",
             int16Reg, laserPowerParams.laserPower, machineResource.laserPowerLoc);

    // 2. 写入开光功率 (0-5000W, 整型)
    addParam("write_open_power", "number11",
             int16Reg, laserPowerParams.laserPowerOpen, machineResource.laserOnPowerLoc);

    // 3. 写入开光渐进时间 (0~1000ms, 整型)
    addParam("write_gradualLighting_time", "number12",
             int16Reg, laserPowerParams.laserRampUpTime, machineResource.laserOnRampTimeLoc);

    // 4. 写入关光功率 (0-5000W, 整型)
    addParam("write_close_power", "number13",
             int16Reg, laserPowerParams.laserPowerClose, machineResource.laserOffPowerLoc);

    // 5. 写入关光渐进时间 (0~300mm, 整型)
    addParam("write_offLightAsymptotic_time", "number14",
             int16Reg, laserPowerParams.laserRampDownTime, machineResource.laserOffRampTimeLoc);

    // 6. 写入开气延时函数 提前送气时间	0~3000ms
    addParam("write_opening_delay", "gasOnDelayNum",
             int16Reg, laserPowerParams.gasStartupAdvanceTime, machineResource.gasONDelayLoc);

    // 7. 写入关气延时函数
    addParam("write_closeing_delay", "gasOffDelayNum",
             int16Reg, laserPowerParams.gasShutoffDelayTime, machineResource.gasOffDelayLoc);

    // 8. 写入送丝延时函数
    addParam("write_wireDelivery_delay", "wireDeliveryNum",
             int16Reg, laserPowerParams.wireFeedAdvanceTime, machineResource.wireOutDelayLoc);
    // 9. 写入驱动器温度报警阈值函数
    addParam("write_driver_temp", "driverTempNum",
             int16Reg, laserPowerParams.driverTemp, machineResource.driverTempLoc);
    //10. 写入保护镜报警阈值
    addParam("write_protective_window_temp", "protectiveWindowTempNum",
             int16Reg, laserPowerParams.protectiveWindowTemp, machineResource.protectiveWindowTempLoc);
    //11. 写入准直镜温度报警阈值
    addParam("write_collimator_temp", "collimatorTempNum",
             int16Reg, laserPowerParams.collimatorTemp, machineResource.collimatorTempLoc);
    //12. 写入X偏移
    addParam("write_laserHead_x_offset", "laserHeadXOffsetNum",
             float100Reg, laserPowerParams.laserHeadXOffset, machineResource.laserHeadXOffsetLoc);
    //13. 写入Y偏移
    addParam("write_laserHead_y_offset", "laserHeadYOffsetNum",
             float100Reg, laserPowerParams.laserHeadYOffset, machineResource.laserHeadYOffsetLoc);
    //14. 写入X放大
    addParam("write_laserHead_x_enlarge", "laserHeadXEnlargeNum",
             float100Reg, laserPowerParams.laserHeadXEnlarge, machineResource.laserHeadXScaleLoc);
    //15. 写入Y放大
    addParam("write_laserHead_y_enlarge", "laserHeadYEnlargeNum",
             float100Reg, laserPowerParams.laserHeadYEnlarge, machineResource.laserHeadYScaleLoc);
    //16. 写入点焊类型
    addParam("write_spotWeld_type", "spotWeldTypeNum",
             int16Reg, static_cast<int>(laserPowerParams.spotWeldType), machineResource.spotWeldTypeLoc);
    //17. 写入点焊持续时间
    addParam("write_spotWeld_duration_time", "spotWeldDurationTimeNum",
             int16Reg, laserPowerParams.spotWeldDurationTime, machineResource.spotWeldDurationLoc);
    //18. 写入点焊间隔时间
    addParam("write_spotWeld_interval_time", "spotWeldIntervalTimeNum",
             int16Reg, laserPowerParams.spotWeldIntervalTime, machineResource.spotWeldIntervalLoc);

    // 按指定地址顺序添加脚本
    for (unsigned int addr : locVec)
    {
        auto it = writeDataHash.find(addr);
        if (it != writeDataHash.end())
        {
            script += it->second.first;           // 调用函数
            loadLuaFunctionScript += it->second.second; // 函数定义
        }
        else
        {
            std::cerr << "Warning: Missing Lua script for laser address 0x"
                      << std::hex << addr << std::dec << std::endl;
            return LaserSdkResult::PARAMETER_ERROR_LASER_SDK;
        }
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}
//激光头图案
LaserSdkResult CommuRS485::craft_laser_head_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string &script)
{
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    CgxiLaserHeadLaserSdk &laserHeadParams = machineResource.laserHeadParams;
    // 参数校验
//    if (laserHeadParams.laserHeadScanSpeed < 0 || laserHeadParams.laserHeadScanSpeed > 30000)
//        return PARAMETER_ERROR_LASER_SDK;
//    if (laserHeadParams.laserHeadScanWidth < 0.1 || laserHeadParams.laserHeadScanWidth > 5)
//        return PARAMETER_ERROR_LASER_SDK;
//    if (laserHeadParams.laserHeadPeakPower < 0 || laserHeadParams.laserHeadPeakPower > 3000)
//        return PARAMETER_ERROR_LASER_SDK;
//    if (laserHeadParams.laserHeadDutyCycle < 0 || laserHeadParams.laserHeadDutyCycle > 100)
//        return PARAMETER_ERROR_LASER_SDK;
//    if (laserHeadParams.laserHeadScanHeight < 0.1 || laserHeadParams.laserHeadScanHeight > 5)
//        return PARAMETER_ERROR_LASER_SDK;
//    if (laserHeadParams.laserHeadSpinAngle < 0 || laserHeadParams.laserHeadSpinAngle > 360)
//        return PARAMETER_ERROR_LASER_SDK;


    std::unordered_map<unsigned int, std::pair<std::string, std::string>> writeDataHash;
    auto addParam = [&](const std::string& funcName, const std::string& paramName,
                        CommuRegType regType, double paramValue, unsigned int addr = 0xFFFFFFFF, int waitTime = 0)
    {
        //没有写入地址
        if(addr == 0xFFFFFFFF)
            return LaserSdkResult::PARAMETER_ERROR_LASER_SDK;

        //设置等待时间
        std::string endstr = ")\n";
        if(waitTime !=0)
            endstr = ")\nwait(" + std::to_string(waitTime) + ")\n";

        std::string funcDef = generateLuaScript(funcName, paramName, addr, regType);
        std::ostringstream useScript;
        switch (regType)
        {
        case float10Reg:
            useScript << std::setprecision(1);//保留小数点后1位
            useScript << funcName << "(" << paramValue << endstr;
            break;
        case float100Reg:
            useScript << std::setprecision(2);//保留小数点后2位
            useScript << funcName << "(" << paramValue << endstr;
            break;
        case int16Reg:
            useScript << funcName << "(" << static_cast<int>(paramValue) << endstr;
            break;
        default:
            break;
        }
        writeDataHash.emplace(addr, std::make_pair(useScript.str(), funcDef));
    };
    ///<01. 送气信号开关
    addParam("write_wirefeeder_gas_valve_signal", "gasValveSignal",
             int16Reg, static_cast<int>(laserHeadParams.WireFeederGasSignalSwitch), machineResource.gasValveSignalLoc);
    ///<02. 送丝信号开关
    addParam("write_wirefeeder_wire_feed_signal", "wireFeedSignal",
             int16Reg, static_cast<int>(laserHeadParams.WireFeederSignalSwitch), machineResource.wireFeedSignalLoc);

    // 1. 写入扫描速度 (0~30000mm/s,)
    addParam("write_scanning_speed", "scanSpeedNum",
             int16Reg, laserHeadParams.laserHeadScanSpeed, machineResource.laserHeadScanSpeedLoc);

    // 2. 写入扫描宽度 (0.1~5mm, 浮点型)
    addParam("write_scanning_width", "scanwidthNum",
             float10Reg, laserHeadParams.laserHeadScanWidth, machineResource.laserHeadScanWidthLoc);

    // 3. 写入峰值功率 (0-3000W, 整型)
    addParam("write_peak_power", "peakPowerNum",
             int16Reg, laserHeadParams.laserHeadPeakPower, machineResource.laserHeadPeakPowerLoc);

    // 4. 写入占空比 (0~100, 整型)
    addParam("write_duty_cycle", "dutyCycleNum",
             int16Reg, laserHeadParams.laserHeadDutyCycle, machineResource.laserHeadDutyCycleLoc);

    // 5. 写入脉冲频率低地址 (5~100000HZ, 整型)
    addParam("write_pulse_low_frequency", "pulseLowNum",
             int16Reg, laserHeadParams.laserHeadLowPulseFrequency, machineResource.laserHeadFrequencyLowLoc);

    // 6. 写入脉冲频率高地址 (5~100000HZ, 整型)
    addParam("write_pulse_high_frequency", "pulseHighNum",
             int16Reg, laserHeadParams.laserHeadHighPulseFrequency, machineResource.laserHeadFrequencyHighLoc);

    // 7. 写入扫描高度 (0.1~5mm, 浮点型)
    addParam("write_scanning_height", "scanHeightNum",
             float10Reg, laserHeadParams.laserHeadScanHeight, machineResource.laserHeadScanHeightLoc);

    // 8. 写入旋转角度 (0~360°, 整型)
    addParam("write_rotation_angle", "rotationAngleNum",
             int16Reg, laserHeadParams.laserHeadSpinAngle, machineResource.laserHeadRotationAngleLoc);

    // 9. 写入图形（特殊处理）图形
    addParam("write_laser_graphics", "laserGraphicNum",
             int16Reg, static_cast<int>(laserHeadParams.laserSpotPattern), machineResource.laserHeadPatternIdLoc);

    for (unsigned int addr : locVec)
    {
        auto it = writeDataHash.find(addr);
        if (it != writeDataHash.end())
        {
            script += it->second.first;
            loadLuaFunctionScript += it->second.second;
        }
        else
        {
            std::cerr << "Warning: Missing Lua script for address 0x"
                      << std::hex << addr << std::dec << std::endl;
            return LaserSdkResult::PARAMETER_ERROR_LASER_SDK;
        }
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}
//送丝机参数
LaserSdkResult CommuRS485::craft_wire_feeder_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string &script)
{

    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    CgxiWireFeederLaserSdk &wireFeederParams = machineResource.wireFeederParams;

    const int autoWireFeedSpeed = wireFeederParams.wireFeederMotorSpeed;
    const int manualWireFeedSpeed = wireFeederParams.wireFeederManualFeedSpeed;
    const int manualWireRetractSpeed = wireFeederParams.wireFeederManualBackSpeed;
    const int wireFeedLength = wireFeederParams.wireFeederFeedLength;
    const int wireRetractLength = wireFeederParams.wireFeederBackLength;


    // 存储所有函数定义和调用脚本的数据结构
    std::unordered_map<unsigned int, std::pair<std::string, std::string>> writeDataHash;

    auto addParam = [&](const std::string& funcName, const std::string& paramName,
                        CommuRegType regType, int paramValue, unsigned int addr = 0xFFFFFFFF)
    {
        //没有写入地址
        if(addr == 0xFFFFFFFF)
            return LaserSdkResult::PARAMETER_ERROR_LASER_SDK;

        // 定义写入操作的结束字符串
        std::string endstr = ")\n";
        std::string funcDef = generateLuaScript(funcName, paramName, addr, regType, 0x0A06, 2); //送丝机暂时先用RS485-1 == 2
        std::ostringstream useScript;
        useScript << funcName << "(" << paramValue << endstr;
        writeDataHash.emplace(addr, std::make_pair(useScript.str(), funcDef));
    };
    /// 送丝机使能
    addParam("write_wirefeeder_enable", "wireFeederEnable",
             int16Reg, wireFeederParams.isWireFeederStart, machineResource.wireFeederStatusLoc);
    //0.送丝机模式设置
    addParam("write_wirefeeder_mode", "wireFeedFunction",
             int16Reg, wireFeederParams.wireFeedFunction, machineResource.operationModeLoc);
    // 1. 脉冲模式-脉冲周期 0x0107 263
    addParam("write_wirefeeder_pulse_period", "pulsePeriod",
             int16Reg, wireFeederParams.wireFeederPulsePeriod, machineResource.pulsePeriodLoc);
    // 2. 脉冲模式-平滑度  0x0108 264
    addParam("write_wirefeeder_smoothness", "smoothness",
             int16Reg, wireFeederParams.wireFeederSmoothness, machineResource.smoothnessLoc);

    // 0. 是否保存当前送丝机数据 地址 0x013F → ​​319​
    addParam("write_wirefeeder_save_current_data", "saveCurrentWireFeederData",
             int16Reg, wireFeederParams.isWireFeederDataSave, machineResource.saveCurrentWireFeederDataLoc);

    // 1. 自动送丝速度
    addParam("write_wirefeeder_auto_send_speed", "autoWireFeedSpeed",
             int16Reg, wireFeederParams.wireFeederMotorSpeed, machineResource.autoWireFeedSpeedLoc);

    // 2. 手动送丝速度
    addParam("write_wirefeeder_send_manual_speed", "manualWireFeedSpeed",
             int16Reg, wireFeederParams.wireFeederManualFeedSpeed, machineResource.manualWireFeedSpeedLoc);

    // 3. 手动抽丝速度
    addParam("write_wirefeeder_retrack_manual_speed", "manualWireRetractSpeed",
             int16Reg, wireFeederParams.wireFeederManualBackSpeed, machineResource.manualWireRetractSpeedLoc);

    // 4. 送丝长度
    addParam("write_wirefeeder_feed_length_speed", "wireFeedLength",
             int16Reg, wireFeederParams.wireFeederFeedLength, machineResource.wireFeedLengthLoc);

    // 5. 抽丝长度
    addParam("write_wirefeeder_retrack_length_speed", "wireRetractLength",
             int16Reg, wireFeederParams.wireFeederBackLength, machineResource.wireRetractLengthLoc);

    // 6. 手动送丝开关
    addParam("write_wirefeeder_manual_send_switch", "manualWireFeedSwitch",
             int16Reg, static_cast<int>(wireFeederParams.isWireFeederManualFeedSwitch), machineResource.manualWireFeedSwitchLoc);

    // 7. 手动退丝开关
    addParam("write_wirefeeder_manual_retrack_switch", "manualWireRetractSwitch",
             int16Reg, static_cast<int>(wireFeederParams.isWireFeederManualRetractSwitch), machineResource.manualWireRetractSwitchLoc);

    for (unsigned int addr : locVec)
    {
        auto it = writeDataHash.find(addr);
        if (it != writeDataHash.end())
        {
            script += it->second.first;
            loadLuaFunctionScript += it->second.second;
        }
        else
        {
            std::cout << "Warning: Missing Lua script for wire feeder address 0x"
                      << std::hex << addr << std::dec << std::endl;
            return LaserSdkResult::PARAMETER_ERROR_LASER_SDK;
        }
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//工艺参数设置脚本获取
LaserSdkResult CommuRS485::craft_script_configure_machine_param_set_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk &craftInfo, std::string &script)
{
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    CgxiLaserHeadLaserSdk &laserHeadParams  = machineResource.laserHeadParams;
    CgxiWireFeederLaserSdk &wireFeederParams  = machineResource.wireFeederParams;
    CgxiLaserPowerLaserSdk &laserPowerParams  = machineResource.laserPowerParams;
    std::string localAddr = "{0X%04X}";
    std::stringstream localParam;

    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//激光头对应参数读取
LaserSdkResult CommuRS485::craft_laser_head_param_read_script_get(LaserMachineHandle laserMachineHandle, std::string &script)
{
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    //CgxiLaserHeadLaserSdk &laserHeadParams  = machineResource.laserHeadParams;
    std::stringstream use_write_data;
    use_write_data << "read_scanning_speed()\n"
                   << "read_peak_power()\n"
                   << "read_scanning_width()\n"
                   << "read_duty_cycle()\n"
                   << "read_pulse_frequency()\n"
                   << "read_scanning_height()\n"
                   << "read_rotation_angle()\n"
                   << "read_laser_graphics()\n";
    script += use_write_data.str();
    loadLuaFunctionScript += read_scanning_speed + read_scanning_width + read_peak_power + read_duty_cycle
                             + read_pulse_frequency + read_scanning_height + read_rotation_angle + read_laser_graphics;
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

LaserSdkResult CommuRS485::craft_laser_head_setting_set_script_get(LaserMachineHandle laserMachineHandle, std::string &script)
{
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    CgxiWireFeederLaserSdk &wireFeederParams  = machineResource.wireFeederParams;

    // 定义写入开气延时函数
    const std::string gasOnDelay = "write_opening_delay";
    const std::string gasOnDelayNum = "gasOnDelayNum";
    const unsigned int gasOnDelayAddr = machineResource.gasONDelayLoc;

    double gasOnTime = wireFeederParams.isWireFeederEnable;

    std::stringstream use_write_data;
    script = mainFuncProgram + Lua_split + Lua_get_message + Lua_crc16 + swap_bytes_for_word

             + RobotProgram1
             + use_write_data.str()
             + RobotProgram2;

    return LaserSdkResult::SUCCESS_LASER_SDK;
}
