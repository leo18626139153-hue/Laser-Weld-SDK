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

        script += strReplace(scriptText, std::vector<std::string>{std::to_string(signalSetpStatusLoc), std::to_string(instLoc)});
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

    script += strReplace(scriptText, std::vector<std::string>{std::to_string(instLoc), std::to_string(runBreakPointStatusLoc)});
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

std::string CommuBase::loadLuaFunctionScriptOut()
{
    return loadLuaFunctionScript;
}

CommuRS485 *CommuRS485::GetInstance()
{
    std::call_once(initFlag, [](){s_commuRS485 = new CommuRS485();});
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

    if (laserPowerParams.LASER_POWER_LASER_SDK < 0 || laserPowerParams.LASER_POWER_LASER_SDK > 5000)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserPowerParams.LASER_POWER_OPEN_LASER_SDK < 0 || laserPowerParams.LASER_POWER_OPEN_LASER_SDK > 5000)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserPowerParams.LASER_GRADUAL_TIME_OPEN_LASER_SDK < 0 || laserPowerParams.LASER_GRADUAL_TIME_OPEN_LASER_SDK > 1000)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserPowerParams.LASER_POWER_CLOSE_LASER_SDK < 0 || laserPowerParams.LASER_POWER_CLOSE_LASER_SDK > 5000)
        return PARAMETER_ERROR_LASER_SDK;

    std::string endstr = ")\nwait(1)\n"; //激光器目前无需wait
    std::unordered_map<unsigned int, std::pair<std::string, std::string>> writeDataHash;
    //写脚本 - 脚本函数名 - 传参名 - 地址 - 参数类型 - 参数值
    auto addParam = [&](const std::string& funcName, const std::string& paramName,
                        unsigned int addr, CommuRegType regType, double paramValue) {
        std::string funcDef = generateLuaScript(funcName, paramName, addr, regType);
        std::ostringstream useScript;
        useScript << funcName << "(" << paramValue << endstr;
        writeDataHash.emplace(addr, std::make_pair(useScript.str(), funcDef));
    };

    // 1. 写入激光器功率 (0-5000W, 整型)
    addParam("write_laser_power", "number10",
             machineResource.laserPowerLoc, CommuRegType::int16Reg,
             laserPowerParams.LASER_POWER_LASER_SDK);

    // 2. 写入开光功率 (0-5000W, 整型)
    addParam("write_open_power", "number11",
             machineResource.laserOnPowerLoc, int16Reg,
             laserPowerParams.LASER_POWER_OPEN_LASER_SDK);

    // 3. 写入开光渐进时间 (0~1000ms, 整型)
    addParam("write_gradualLighting_time", "number12",
             machineResource.laserOnRampTimeLoc, int16Reg,
             laserPowerParams.LASER_GRADUAL_TIME_OPEN_LASER_SDK);

    // 4. 写入关光功率 (0-5000W, 整型)
    addParam("write_close_power", "number13",
             machineResource.laserOffPowerLoc, int16Reg,
             laserPowerParams.LASER_POWER_CLOSE_LASER_SDK);

    // 5. 写入关光渐进时间 (0~300mm, 整型)
    addParam("write_offLightAsymptotic_time", "number14",
             machineResource.laserOffRampTimeLoc, int16Reg,
             laserPowerParams.GAS_GRADUAL_TIME_OPEN_LASER_SDK);

    // 6. 写入开气延时函数
    addParam("write_opening_delay", "gasOnDelayNum",
             machineResource.gasONDelayLoc, int16Reg,
             laserPowerParams.GAS_GRADUAL_TIME_OPEN_LASER_SDK);

    // 7. 写入关气延时函数
    addParam("write_closeing_delay", "gasOffDelayNum",
             machineResource.gasOffDelayLoc, int16Reg,
             laserPowerParams.GAS_GRADUAL_TIME_CLOSE_LASER_SDK);

    // 8. 写入送丝延时函数
    addParam("write_wireDelivery_delay", "wireDeliveryNum",
             machineResource.wireOutDelayLoc, int16Reg,
             laserPowerParams.WIRE_FEEDER_AHEAD_OUT_LASER_SDK);

    // 按指定地址顺序添加脚本
    for (unsigned int addr : locVec) {
        auto it = writeDataHash.find(addr);
        if (it != writeDataHash.end()) {
            script += it->second.first;           // 调用函数
            loadLuaFunctionScript += it->second.second; // 函数定义
        } else {
            std::cerr << "Warning: Missing Lua script for laser address 0x"
                      << std::hex << addr << std::dec << std::endl;
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
    if (laserHeadParams.LASERHEAD_SCANSPEED_LASERSDK < 0 || laserHeadParams.LASERHEAD_SCANSPEED_LASERSDK > 30000)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserHeadParams.LASERHEAD_SCANWIDTH_LASERSDK < 0.1 || laserHeadParams.LASERHEAD_SCANWIDTH_LASERSDK > 5)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserHeadParams.LASERHEAD_PEAK_POWER_LASERSDK < 0 || laserHeadParams.LASERHEAD_PEAK_POWER_LASERSDK > 3000)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserHeadParams.LASERHEAD_DULY_CYCLE_LASERSDK < 0 || laserHeadParams.LASERHEAD_DULY_CYCLE_LASERSDK > 100)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserHeadParams.LASERHEAD_SCANHEIGHT_LASERSDK < 0.1 || laserHeadParams.LASERHEAD_SCANHEIGHT_LASERSDK > 5)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserHeadParams.LASERHEAD_SPINANGLE_LASERSDK < 0 || laserHeadParams.LASERHEAD_SPINANGLE_LASERSDK > 360)
        return PARAMETER_ERROR_LASER_SDK;

    std::string endstr = ")\nwait(200)\n";
    std::unordered_map<unsigned int, std::pair<std::string, std::string>> writeDataHash;

    const unsigned int graphicsAddr = machineResource.laserHeadPatternIdLoc;

    auto addParam = [&](const std::string& funcName, const std::string& paramName,
                      unsigned int addr, CommuRegType regType, double paramValue)
    {
        std::string funcDef = generateLuaScript(funcName, paramName, addr, regType);
        std::ostringstream useScript;
        useScript << funcName << "(" << paramValue << endstr;
        writeDataHash.emplace(addr, std::make_pair(useScript.str(), funcDef));
    };

    // 1. 写入扫描速度 (0~30000mm/s, 浮点型)
    addParam("write_scanning_speed", "scanSpeedNum",
             machineResource.laserHeadScanSpeedLoc, floatReg,
             laserHeadParams.LASERHEAD_SCANSPEED_LASERSDK);

    // 2. 写入扫描宽度 (0.1~5mm, 浮点型)
    addParam("write_scanning_width", "scanwidthNum",
             machineResource.laserHeadScanWidthLoc, floatReg,
             laserHeadParams.LASERHEAD_SCANWIDTH_LASERSDK);

    // 3. 写入峰值功率 (0-3000W, 整型)
    addParam("write_peak_power", "peakPowerNum",
             machineResource.laserHeadPeakPowerLoc, int16Reg,
             laserHeadParams.LASERHEAD_PEAK_POWER_LASERSDK);

    // 4. 写入占空比 (0~100, 整型)
    addParam("write_duty_cycle", "dutyCycleNum",
             machineResource.laserHeadDutyCycleLoc, int16Reg,
             laserHeadParams.LASERHEAD_DULY_CYCLE_LASERSDK);

    // 5. 写入脉冲频率低地址 (5~100000HZ, 整型)
    addParam("write_pulse_low_frequency", "pulseLowNum",
             machineResource.laserHeadFrequencyLowLoc, int16Reg,
             laserHeadParams.LASERHEAD_LOW_PULSE_FREQUENCY_LASERSDK);

    // 6. 写入脉冲频率高地址 (5~100000HZ, 整型)
    addParam("write_pulse_high_frequency", "pulseHighNum",
             machineResource.laserHeadFrequencyHighLoc, int16Reg,
             laserHeadParams.LASERHEAD_HIGH_PULSE_FREQUENCY_LASERSDK);

    // 7. 写入扫描高度 (0.1~5mm, 浮点型)
    addParam("write_scanning_height", "scanHeightNum",
             machineResource.laserHeadScanHeightLoc, floatReg,
             laserHeadParams.LASERHEAD_SCANHEIGHT_LASERSDK);

    // 8. 写入旋转角度 (0~360°, 整型)
    addParam("write_rotation_angle", "rotationAngleNum",
             machineResource.laserHeadRotationAngleLoc, int16Reg,
             laserHeadParams.LASERHEAD_SPINANGLE_LASERSDK);

    // 9. 写入图形（特殊处理）
    addParam("write_laser_graphics", "laserGraphicNum",
             graphicsAddr, int16Reg,
             static_cast<int>(laserHeadParams.LASERHEAD_SPOT_PATTERN_LASERSDK));

    for (unsigned int addr : locVec) {
        auto it = writeDataHash.find(addr);
        if (it != writeDataHash.end()) {
            if (addr == graphicsAddr)
            {
                std::string graphicsScript = it->second.first;
                // 图形设置放在所以图案指令前
                script = graphicsScript + script;
                loadLuaFunctionScript += it->second.second;
            }
            else
            {
                script += it->second.first;
                loadLuaFunctionScript += it->second.second;
            }

        }
        else
        {
            std::cerr << "Warning: Missing Lua script for address 0x"
                      << std::hex << addr << std::dec << std::endl;
        }
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}
//送丝机参数
LaserSdkResult CommuRS485::craft_wire_feeder_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string &script)
{
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    CgxiWireFeederLaserSdk &wireFeederParams = machineResource.wireFeederParams;

    const int autoWireFeedSpeed = wireFeederParams.WIREFEEDER_MOTOR_SPEED_LASERSDK;
    const int manualWireFeedSpeed = wireFeederParams.WIREFEEDER_MANUAL_FEED_SPEED_LASERSDK;
    const int manualWireRetractSpeed = wireFeederParams.WIREFEEDER_MANUAL_RETRRAC_SWITCH_LASERSDK;
    const int wireFeedLength = wireFeederParams.WIREFEEDER_FEED_LENGTH_LASERSDK;
    const int wireRetractLength = wireFeederParams.WIREFEEDER_BACK_LENGTH_LASERSDK;

    // 参数范围验证
    if (autoWireFeedSpeed < 15 || autoWireFeedSpeed > 600) return PARAMETER_ERROR_LASER_SDK;
    if (manualWireFeedSpeed < 15 || manualWireFeedSpeed > 600) return PARAMETER_ERROR_LASER_SDK;
    if (manualWireRetractSpeed < 15 || manualWireRetractSpeed > 600) return PARAMETER_ERROR_LASER_SDK;
    if (wireFeedLength < 15 || wireFeedLength > 600) return PARAMETER_ERROR_LASER_SDK;
    if (wireRetractLength < 15 || wireRetractLength > 600) return PARAMETER_ERROR_LASER_SDK;

    // 定义写入操作的结束字符串
    std::string endstr = ")\nwait(200)\n";

    // 存储所有函数定义和调用脚本的数据结构
    std::unordered_map<unsigned int, std::pair<std::string, std::string>> writeDataHash;

    auto addParam = [&](const std::string& funcName, const std::string& paramName,
                        unsigned int addr, CommuRegType regType, int paramValue) {
        std::string funcDef = generateLuaScript(funcName, paramName, addr, regType);
        std::ostringstream useScript;
        useScript << funcName << "(" << paramValue << endstr;
        writeDataHash.emplace(addr, std::make_pair(useScript.str(), funcDef));
    };

    // 1. 自动送丝速度
    addParam("write_wirefeeder_send_motor_speed", "autoWireFeedSpeed",
             machineResource.autoWireFeedSpeedLoc, int16Reg,
             wireFeederParams.WIREFEEDER_MOTOR_SPEED_LASERSDK);

    // 2. 手动送丝速度
    addParam("write_wirefeeder_send_manual_speed", "manualWireFeedSpeed",
             machineResource.manualWireFeedSpeedLoc, int16Reg,
             wireFeederParams.WIREFEEDER_MANUAL_FEED_SPEED_LASERSDK);

    // 3. 手动抽丝速度
    addParam("write_wirefeeder_retrack_manual_speed", "manualWireRetractSpeed",
             machineResource.manualWireRetractSpeedLoc, int16Reg,
             wireFeederParams.WIREFEEDER_MANUAL_RETRRAC_SWITCH_LASERSDK);

    // 4. 送丝长度
    addParam("write_wirefeeder_feed_length_speed", "wireFeedLength",
             machineResource.wireFeedLengthLoc, int16Reg,
             wireFeederParams.WIREFEEDER_FEED_LENGTH_LASERSDK);

    // 5. 抽丝长度
    addParam("write_wirefeeder_retrack_length_speed", "wireRetractLength",
             machineResource.wireRetractLengthLoc, int16Reg,
             wireFeederParams.WIREFEEDER_BACK_LENGTH_LASERSDK);

    // 6. 手动送丝开关
    addParam("write_wirefeeder_manual_send_switch", "manualWireFeedSwitch",
             machineResource.manualWireFeedSwitchLoc, int16Reg,
             wireFeederParams.WIREFEEDER_MANUAL_FEED_SWITCH_LASERSDK);

    // 7. 手动退丝开关
    addParam("write_wirefeeder_manual_retrack_switch", "manualWireRetractSwitch",
             machineResource.manualWireRetractSwitchLoc, int16Reg,
             wireFeederParams.WIREFEEDER_MANUAL_RETRRAC_SWITCH_LASERSDK);

    for (unsigned int addr : locVec) {
        auto it = writeDataHash.find(addr);
        if (it != writeDataHash.end()) {
            script += it->second.first;           // 添加调用脚本
            loadLuaFunctionScript += it->second.second; // 添加函数定义
        } else {
            std::cerr << "Warning: Missing Lua script for wire feeder address 0x"
                      << std::hex << addr << std::dec << std::endl;
        }
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//激光头对应参数读取
LaserSdkResult CommuRS485::craft_laser_head_param_read_script_get(LaserMachineHandle laserMachineHandle, std::string &script)
{
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    CgxiLaserHeadLaserSdk &laserHeadParams  = machineResource.laserHeadParams;
    /// 定义读取扫描速度函数
    std::string read_scanning_speed =
    R"(
    function read_scanning_speed()
        ----------------
        for i=62,69,1 do
            write_int16_register(i,0)
        end

        mes={0x0309,0x0000,0x0100,0x4285}
        ------------------
        for i=1,4,1 do
            write_int16_register(i+61,mes[i])
        end
        while(true) do
        write_bool_register(33,1)
        ---------------
        serial_send(3,4,1,62)
        if(read_int16_register(66)~=0 and read_int16_register(67)~=0 and read_int16_register(68)~=0 and read_int16_register(69)~=0)
        then
        write_bool_register(33,0)
             break
        end
        end
        Lua_data_get(read_int16_register(11),read_int16_register(12))
        write_int16_register(77,result1)
    end
    )";
    /// 定义读取扫描宽度函数
    std::string read_scanning_width =
    R"(
    function read_scanning_width()
        -----------------
        for i=62,69,1 do
            write_int16_register(i,0)
        end
        ------------------
        mes={0x0309,0x0100,0x0100,0x82D4}
        ------------------
        for i=1,4,1 do
            write_int16_register(i+61,mes[i])
        end
        while(true) do
        write_bool_register(33,1)
        ---------------
        serial_send(3,4,1,62)
        if(read_int16_register(66)~=0 and read_int16_register(67)~=0 and read_int16_register(68)~=0 and read_int16_register(69)~=0)
        then
        write_bool_register(33,0)
             break
        end
        end
        Lua_data_get(read_int16_register(11),read_int16_register(12))
        write_int16_register(78,result1)
    end
    )";
    /// 定义读取峰值功率函数
    std::string read_peak_power =
    R"(
    function read_peak_power()
        -----------------
        for i=62,69,1 do
            write_int16_register(i,0)
        end
        ------------------
        mes={0x0309,0x0200,0x0100,0x8224}
        ------------------
        for i=1,4,1 do
            write_int16_register(i+61,mes[i])
        end
        while(true) do
        write_bool_register(33,1)
        ---------------
        serial_send(3,4,1,62)
        if(read_int16_register(66)~=0 and read_int16_register(67)~=0 and read_int16_register(68)~=0 and read_int16_register(69)~=0)
        then
        write_bool_register(33,0)
             break
        end
        end
        Lua_data_get(read_int16_register(11),read_int16_register(12))
        write_int16_register(79,result1)
    end
    )";
    /// 定义读取占空比函数
    std::string read_duty_cycle =
    R"(
    function read_duty_cycle()
        -----------------
        for i=62,69,1 do
            write_int16_register(i,0)
        end
        ------------------
        mes={0x0309,0x0300,0x0100,0x4275}
        ------------------
        for i=1,4,1 do
            write_int16_register(i+61,mes[i])
        end
        while(true) do
        write_bool_register(33,1)
        ---------------
        serial_send(3,4,1,62)
        if(read_int16_register(66)~=0 and read_int16_register(67)~=0 and read_int16_register(68)~=0 and read_int16_register(69)~=0)
        then
        write_bool_register(33,0)
             break
        end
        end
        Lua_data_get(read_int16_register(11),read_int16_register(12))
        write_int16_register(80,result1)
    end
    )";
    /// 定义读取脉冲频率函数
    std::string read_pulse_frequency =
    R"(
    function read_pulse_frequency()
        -----------------
        for i=62,69,1 do
            write_int16_register(i,0)
        end
        ------------------
        mes={0x0309,0x0500,0x0100,0x4395}
        ------------------
        for i=1,4,1 do
            write_int16_register(i+61,mes[i])
        end
        while(true) do
        write_bool_register(33,1)
        ---------------
        serial_send(3,4,1,62)
        if(read_int16_register(66)~=0 and read_int16_register(67)~=0 and read_int16_register(68)~=0 and read_int16_register(69)~=0)
        then
        write_bool_register(33,0)
             break
        end
        end
        Lua_data_get(read_int16_register(11),read_int16_register(12))
        write_int16_register(81,result1)
    end
    )";
    /// 定义读取扫描高度函数
    std::string read_scanning_height =
    R"(
    function read_scanning_height()
        -----------------
        for i=62,69,1 do
            write_int16_register(i,0)
        end
        ------------------
        mes={0x0309,0x0600,0x0100,0x4365}
        ------------------
        for i=1,4,1 do
            write_int16_register(i+61,mes[i])
        end
        while(true) do
        write_bool_register(33,1)
        ---------------
        serial_send(3,4,1,62)
        if(read_int16_register(66)~=0 and read_int16_register(67)~=0 and read_int16_register(68)~=0 and read_int16_register(69)~=0)
        then
        write_bool_register(33,0)
             break
        end
        end
        Lua_data_get(read_int16_register(11),read_int16_register(12))
        write_int16_register(82,result1)
    end
    )";
    /// 定义读取旋转角度函数
    std::string read_rotation_angle =
    R"(
    function read_rotation_angle()
        -----------------
        for i=62,69,1 do
            write_int16_register(i,0)
        end
        ------------------
        mes={0x0309,0x0700,0x0100,0x8334}
        ------------------
        for i=1,4,1 do
            write_int16_register(i+61,mes[i])
        end
        while(true) do
        write_bool_register(33,1)
        ---------------
        serial_send(3,4,1,62)
        if(read_int16_register(66)~=0 and read_int16_register(67)~=0 and read_int16_register(68)~=0 and read_int16_register(69)~=0)
        then
        write_bool_register(33,0)
             break
        end
        end
        Lua_data_get(read_int16_register(11),read_int16_register(12))
        write_int16_register(83,result1)
    end
    )";
    /// 定义读取激光图形函数
    std::string read_laser_graphics =
    R"(
    function read_laser_graphics()
        -----------------
        for i=62,69,1 do
            write_int16_register(i,0)
        end
        ------------------
        mes={0x0309,0x3000,0x0100,0x4D85}
        ------------------
        for i=1,4,1 do
            write_int16_register(i+61,mes[i])
        end
        while(true) do
        write_bool_register(33,1)
        ---------------
        serial_send(3,4,1,62)
        if(read_int16_register(66)~=0 and read_int16_register(67)~=0 and read_int16_register(68)~=0 and read_int16_register(69)~=0)
        then
        write_bool_register(33,0)
             break
        end
        end
        Lua_data_get(read_int16_register(11),read_int16_register(12))
        write_int16_register(84,result1)
        if(result1==0) then
        print("circle is using")
        elseif(result1==1) then
        print("8 is using")
        elseif(result1==2) then
        print("- is using")
        elseif(result1==3) then
        print("star is using")
        elseif(result1==4) then
        print("triangle is using")
        elseif(result1==5) then
        print("ten is using")
        end
    end
    )";
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
    loadLuaFunctionScript += read_scanning_speed + read_scanning_width + read_peak_power + read_duty_cycle + read_pulse_frequency
            + read_scanning_height + read_rotation_angle + read_laser_graphics
            + use_write_data.str();

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

    double gasOnTime = wireFeederParams.WIREFEEDER_ENABLE_LASERSDK;


    std::stringstream use_write_data;
//    use_write_data << "write_laser_power(" << laserPowerParams.LASER_POWER_LASER_SDK << endstr
//                   << "write_open_power(" << laserPowerParams.LASER_POWER_OPEN_LASER_SDK << endstr
//                   << "write_gradualLighting_time(" << laserPowerParams.LASER_GRADUAL_TIME_OPEN_LASER_SDK << endstr
//                   << "write_close_power(" << laserPowerParams.LASER_POWER_CLOSE_LASER_SDK << endstr
//                   << "write_offLightAsymptotic_time(" << laserPowerParams.LASER_GRADUAL_TIME_CLOSE_LASER_SDK << endstr
//                   << "halt()\n";
    script = mainFuncProgram + Lua_split + Lua_get_message + Lua_crc16 + swap_bytes_for_word

            + RobotProgram1
            + use_write_data.str()
            + RobotProgram2;

    return LaserSdkResult::SUCCESS_LASER_SDK;
}






