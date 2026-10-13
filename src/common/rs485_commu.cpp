#include "rs485_commu.h"
#include "parametercontext.h"
#include "cl_basedefine.h"
#include "utils.h"
#include "internaldatadefine.h"
#include "brandadapter.h"
#include <iomanip>
#include <sstream>
#include <vector>

// 【修改理由】通讯层只处理送丝机语义集合，不再在多个函数里散落具体地址判断。
// 【逻辑】地址到语义的映射先走品牌适配器，后续再由品牌规则决定是否允许写入。
static const BrandRegisterRole WIRE_FEEDER_ROLES[] =
{
    BrandRegisterRole::WireFeederRunStatus,
    BrandRegisterRole::AutoWireFeedSpeed,
    BrandRegisterRole::ManualWireFeedSpeed,
    BrandRegisterRole::ManualWireRetractSpeed,
    BrandRegisterRole::WireRetractLength,
    BrandRegisterRole::WireFeedLength,
    BrandRegisterRole::WireFeedBackDelay,
    BrandRegisterRole::BreakWireFeedingMode,
    BrandRegisterRole::ManualWireFeedSwitch,
    BrandRegisterRole::ManualWireRetractSwitch,
    BrandRegisterRole::SaveWireFeederData,
    BrandRegisterRole::WireFeedSignal
};

// 【修改理由】品牌层返回的是数值型脚本功能码，Lua 脚本需要保持 0x0906 / 0x5A06 这类可读形式。
// 【逻辑】统一格式化为四位十六进制字符串，避免通讯层再次散落品牌功能码常量。
static std::string scriptCodeToLuaHex(unsigned int scriptCode)
{
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << scriptCode;
    return oss.str();
}

LaserSdkResult CommuRS485::addWriteParamScript(std::unordered_map<unsigned int, std::string>& writeDataHash,
                                               const std::string& writeSingleFuncCode,
                                               CommuRegType regType,
                                               double paramValue,
                                               unsigned int addr,
                                               int comNo,
                                               int receiptNeeded)
{
    if (addr >= 0xFFFF)
    {
        return LaserSdkResult::PARAMETER_ERROR_LASER_SDK;
    }

    std::ostringstream useScript;
    switch (regType)
    {
    case float10Reg:
        useScript << std::setprecision(1);
        useScript << "  write_value_port(" << writeSingleFuncCode << ", " << addr << ","
                  << static_cast<int>(paramValue * 10) << ", " << comNo << ", SendCommuRegAddr,"
                  << receiptNeeded << ")\n";
        break;
    case float100Reg:
        useScript << std::setprecision(2);
        useScript << "  write_value_port(" << writeSingleFuncCode << ", " << addr << ","
                  << static_cast<int>(paramValue * 100) << ", " << comNo << ", SendCommuRegAddr,"
                  << receiptNeeded << ")\n";
        break;
    case int16Reg:
        useScript << "  write_value_port(" << writeSingleFuncCode << ", " << addr << ","
                  << paramValue << ", " << comNo << ", SendCommuRegAddr,"
                  << receiptNeeded << ")\n";
        break;
    }
    writeDataHash.insert(std::make_pair(addr, useScript.str()));
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

LaserSdkResult CommuRS485::addWireFeederRoleParamScript(std::unordered_map<unsigned int, std::string>& writeDataHash,
                                                        const BrandProtocolAdapter& adapter,
                                                        const std::string& writeSingleFuncCode,
                                                        BrandRegisterRole role,
                                                        double paramValue,
                                                        unsigned int fallbackAddr,
                                                        int receiptNeeded)
{
    if (adapter.hasRegister(role))
    {
        BrandRegisterInfo info = adapter.registerInfo(role);
        LaserSdkResult res = addWriteParamScript(writeDataHash,
                                                 writeSingleFuncCode,
                                                 info.regType,
                                                 paramValue / info.scale,
                                                 info.writeAddr,
                                                 2,
                                                 receiptNeeded);
        if (res != LaserSdkResult::SUCCESS_LASER_SDK)
        {
            return res;
        }
        if (fallbackAddr != 0 && fallbackAddr != info.writeAddr)
        {
            return addWriteParamScript(writeDataHash,
                                       writeSingleFuncCode,
                                       info.regType,
                                       paramValue / info.scale,
                                       fallbackAddr,
                                       2,
                                       receiptNeeded);
        }
        return LaserSdkResult::SUCCESS_LASER_SDK;
    }
    return addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, paramValue, fallbackAddr, 2, receiptNeeded);
}

// 【修改理由】外部传入的 locVec 仍是地址列表，适配器规则需要基于 BrandRegisterRole 判断。
// 【逻辑】先匹配当前品牌地址；若旧调用方仍传 MachineResource 中的历史地址，则用 fallback 维持兼容。
bool CommuRS485::findWireFeederRoleByAddr(const BrandProtocolAdapter &adapter, const MachineResource &machineResource, unsigned int addr, BrandRegisterRole &role)
{
    for (unsigned int i = 0; i < sizeof(WIRE_FEEDER_ROLES) / sizeof(WIRE_FEEDER_ROLES[0]); ++i)
    {
        BrandRegisterRole curRole = WIRE_FEEDER_ROLES[i];
        if (adapter.hasRegister(curRole) && adapter.registerInfo(curRole).writeAddr == addr)
        {
            role = curRole;
            return true;
        }
    }
    switch (addr)
    {
    case 0:
        break;
    default:
        if (addr == machineResource.wireFeederStatusLoc) role = BrandRegisterRole::WireFeederRunStatus;
        else if (addr == machineResource.autoWireFeedSpeedLoc) role = BrandRegisterRole::AutoWireFeedSpeed;
        else if (addr == machineResource.manualWireFeedSpeedLoc) role = BrandRegisterRole::ManualWireFeedSpeed;
        else if (addr == machineResource.manualWireRetractSpeedLoc) role = BrandRegisterRole::ManualWireRetractSpeed;
        else if (addr == machineResource.wireRetractLengthLoc) role = BrandRegisterRole::WireRetractLength;
        else if (addr == machineResource.wireFeedLengthLoc) role = BrandRegisterRole::WireFeedLength;
        else if (addr == machineResource.wireFeedBackDelayLoc) role = BrandRegisterRole::WireFeedBackDelay;
        else if (addr == machineResource.breakWireFeedingModeLoc) role = BrandRegisterRole::BreakWireFeedingMode;
        else if (addr == machineResource.manualWireFeedSwitchLoc) role = BrandRegisterRole::ManualWireFeedSwitch;
        else if (addr == machineResource.manualWireRetractSwitchLoc) role = BrandRegisterRole::ManualWireRetractSwitch;
        else if (addr == machineResource.wireFeedSignalLoc) role = BrandRegisterRole::WireFeedSignal;
        else return false;
        return true;
    }
    return false;
}

// 【修改理由】缓升脚本原来把功率和送丝速度拼接在一个大字符串中，自熔焊模式无法单独屏蔽送丝机报文
// 【逻辑】填丝焊保留原有功率+送丝速度渐变；自熔焊只生成激光功率渐变，不写送丝机速度
std::string CommuRS485::buildSlowArcStartFunctionScript(const MachineResource &machineResource)
{
    const BrandProtocolAdapter &adapter = GetBrandProtocolAdapter(machineResource.laserHeadBrand);
    const CgxiLaserHeadLaserSdk &laserHeadParams = machineResource.laserHeadParams;
    const CgxiWireFeederLaserSdk &wireFeederParams = machineResource.wireFeederParams;
    const CgxiLaserPowerLaserSdk &laserPowerParams = machineResource.laserPowerParams;
    const bool writeWireFeeder = adapter.allowWireFeederWrite(machineResource.weldingProcessType, BrandRegisterRole::AutoWireFeedSpeed);
    const BrandProtocolProfile profile = adapter.profile();
    const std::string laserWriteCode = scriptCodeToLuaHex(profile.laserWriteSingleScriptCode);
    const std::string wireFeederWriteCode = scriptCodeToLuaHex(profile.wireFeederWriteSingleScriptCode);

    std::ostringstream script;
    script << "function slow_arc_start()\n"
           << "   write_integer_register(673, 0) \n"
           << "   if read_integer_register(821) ~= 0 then \n"
           << "       print(string.format(\"起弧异常，超时 : 0x%04X\", read_integer_register(821))) \n"
           << "       write_integer_register(673, 1) \n"
           << "       pause()  \n"
           << "       wait(1)  \n"
           << "       return  \n"
           << "   end \n"
           << "   laserPower = " << laserHeadParams.laserHeadPeakPower << "\n"
           << "   openPower = " << laserPowerParams.laserPowerOpen << "\n"
           << "   openLightTime = " << laserPowerParams.laserRampUpTime << "\n";
    if (writeWireFeeder)
    {
        script << "   wirefeedeOpenSendSpeed = " << wireFeederParams.wireFeederStartMotorSpeed << "\n"
               << "   wirefeedeWeldSendSpeed = " << wireFeederParams.wireFeederMotorSpeed << "\n";
    }
    script << "   write_value_port(" << laserWriteCode << "," << machineResource.laserHeadPeakPowerQuickLoc << ", openPower,3,0,1)\n";
    if (writeWireFeeder)
    {
        script << "   write_value_port(" << wireFeederWriteCode << "," << machineResource.autoWireFeedSpeedLoc << ", wirefeedeOpenSendSpeed,2,0,1000)\n";
    }
    script << R"(
            function slow_ascent()
                -- 计算功率差
                local v = laserPower - openPower
)";
    if (writeWireFeeder)
    {
        script << R"(                -- 计算送丝速度差
                local s = wirefeedeWeldSendSpeed - wirefeedeOpenSendSpeed
)";
    }
    script << R"(                -- 计算降几次
                local num = openLightTime // 200
                print("num---------",num)
                if num < 1 then
                    -- 缓升距离过短直接起弧，设置起弧功率即可
)";
    script << "                 write_value_port(" << laserWriteCode << "," << machineResource.laserHeadPeakPowerQuickLoc << ",laserPower,3,0,1)\n";
    if (writeWireFeeder)
    {
        script << "                    write_value_port(" << wireFeederWriteCode << "," << machineResource.autoWireFeedSpeedLoc << ",wirefeedeWeldSendSpeed,2,0,1)\n";
    }
    script << R"(                    return
                end
                -- 计算每次降多少
                local powerSingle = math.floor(v / num)
)";
    if (writeWireFeeder)
    {
        script << R"(                local speedSingle = math.floor(s / num)
)";
    }
    script << R"(                -- 循环逐步调整功率
                for i = 1, num do
                    print("power: ", i)
)";
    script << "                    write_value_port(" << laserWriteCode << "," << machineResource.laserHeadPeakPowerQuickLoc << ",openPower + (powerSingle * i),3,0,1)\n";
    if (writeWireFeeder)
    {
        script << "                    write_value_port(" << wireFeederWriteCode << "," << machineResource.autoWireFeedSpeedLoc << ",wirefeedeOpenSendSpeed + (speedSingle * i),2,0,1)\n";
    }
    script << R"(                end
                print("缓升结束!")
            end
            i = 0
            res = false
            while (i<5) and (not res) do
                i=i+1
                set_configurable_digital_out(0,true)
                set_configurable_digital_out(1,true)
                res = get_configurable_digital_out(1)
                print("res:",res)
                wait(5)
            end
            if(not res) then
                print("启动失败")
                return
            end
            -- 开气的延迟等待，不需要继续往下走
            wait()" << laserPowerParams.gasStartupAdvanceTime << R"()  -- 替换动态变量
            ask_thread = task_create(slow_ascent)
end
)";
    return script.str();
}

// 【修改理由】缓降脚本需要和自熔焊送丝机断联场景解耦，避免自熔焊继续写送丝机速度或关丝报文
// 【逻辑】填丝焊保留原有送丝速度渐变和关丝动作；自熔焊只做激光功率缓降和超时控制
std::string CommuRS485::buildSlowArcStopFunctionScript(const MachineResource &machineResource)
{
    const BrandProtocolAdapter &adapter = GetBrandProtocolAdapter(machineResource.laserHeadBrand);
    const CgxiLaserHeadLaserSdk &laserHeadParams = machineResource.laserHeadParams;
    const CgxiWireFeederLaserSdk &wireFeederParams = machineResource.wireFeederParams;
    const CgxiLaserPowerLaserSdk &laserPowerParams = machineResource.laserPowerParams;
    const bool writeWireFeeder = adapter.allowWireFeederWrite(machineResource.weldingProcessType, BrandRegisterRole::AutoWireFeedSpeed);
    const BrandProtocolProfile profile = adapter.profile();
    const std::string laserWriteCode = scriptCodeToLuaHex(profile.laserWriteSingleScriptCode);
    const std::string wireFeederWriteCode = scriptCodeToLuaHex(profile.wireFeederWriteSingleScriptCode);

    std::ostringstream script;
    script << "function slow_arc_stop()\n"
           << "    write_integer_register(675, 0) \n"
           << "    if read_integer_register(821) ~= 0 then \n"
           << "        print(string.format(\"收弧异常，超时 : 0x%04X\", read_integer_register(821))) \n"
           << "        write_integer_register(675, 1) \n"
           << "        return \n"
           << "    end \n"
           << "    laserPower = " << laserHeadParams.laserHeadPeakPower << "\n"
           << "    closePower  = " << laserPowerParams.laserPowerClose << "\n"
           << "    closeLightTime  = " << laserPowerParams.laserRampDownTime << "\n"
           << "    if closeLightTime == 0 then \n"
           << "        print(\"closeLightTime = 0\") \n"
           << "        return \n"
           << "    end \n";
    if (writeWireFeeder)
    {
        script << "   wirefeedeCloseSendSpeed = " << wireFeederParams.wireFeederStopMotorSpeed << "\n"
               << "   wirefeedeWeldSendSpeed = " << wireFeederParams.wireFeederMotorSpeed << "\n"
               << "   wireDelayTime = " << wireFeederParams.wireFeederWaitTime << "\n";
    }
    script << "   flag = 0\n"
           << R"(                    flag = 0
                    wire_off_done = 0
)";
    if (writeWireFeeder)
    {
        script << R"(                    local function send_wire_off()
                        if wire_off_done == 0 then
)";
        script << "                            write_value_port(" << laserWriteCode << "," << machineResource.wireFeedSignalLoc << ", 0,3,0,1)\n";
        script << R"(                            wire_off_done = 1
                        end
                    end
)";
    }
    script << R"(                    function slow_descent()
                        -- 计算功率差
                        local v = laserPower - closePower
)";
    if (writeWireFeeder)
    {
        script << R"(                        -- 计算送丝速度差
                        local s = wirefeedeWeldSendSpeed - wirefeedeCloseSendSpeed
)";
    }
    script << R"(                        -- 计算降几次
                        local num = closeLightTime // 200
                        print("num2=",num)
                        if num < 1 then
)";
    script << "                            write_value_port(" << laserWriteCode << "," << machineResource.laserHeadPeakPowerQuickLoc << ",laserPower,3,0,1)\n";
    if (writeWireFeeder)
    {
        script << "                            write_value_port(" << wireFeederWriteCode << "," << machineResource.autoWireFeedSpeedLoc << ",wirefeedeWeldSendSpeed,2,0,1)\n"
               << "                            send_wire_off()\n";
    }
    script << R"(                            return
                        end
                        -- 计算每次降多少
                        local powerSingle = math.floor(v / num)
)";
    if (writeWireFeeder)
    {
        script << R"(                        local speedSingle = math.floor(s / num)
)";
    }
    script << R"(                        -- 循环逐步调整功率
                        for i = 1, num do
                            if flag == 1 then
                                break
                            end
)";
    script << "                            write_value_port(" << laserWriteCode << "," << machineResource.laserHeadPeakPowerQuickLoc << ",(laserPower - (powerSingle * i)),3,0,0)\n";
    if (writeWireFeeder)
    {
        script << R"(                            if flag == 1 then
                                break
                            end
)";
        script << "                            write_value_port(" << wireFeederWriteCode << "," << machineResource.autoWireFeedSpeedLoc << ",wirefeedeWeldSendSpeed - (speedSingle * i),2,0,0)\n";
    }
    script << R"(
                        end
                        if flag == 0 then
                            print("缓降结束...")
)";
    if (writeWireFeeder)
    {
        script << R"(                            send_wire_off()
)";
    }
    script << R"(                        end
                    end
                    task_thread = task_create(slow_descent)
                    function timeController()
                        wait(closeLightTime)
                        flag = 1
)";
    if (writeWireFeeder)
    {
        script << R"(                        send_wire_off()
)";
    }
    script << R"(                    end
                    task_thread2 = task_create(timeController)
    end
)";
    return script.str();
}

//RS485单例
CommuRS485 *CommuRS485::s_commuRS485 = nullptr;
std::once_flag CommuRS485::initFlag;


/*
 * CommuRS485
*/
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
    initData();
}

void CommuRS485::initData()
{
    funcCode = GetBrandProtocolAdapter(LaserHeadBrandSdk::CQLASERPOWER_SDK).profile().laserWriteSingleScriptCode;
}

std::string CommuRS485::get_laser_param_set_script(std::map<unsigned int, int> locVec)
{
    std::string script;
    for (const auto& it : locVec)
    {
        unsigned int partId = it.first;
        int partValue = it.second;
        script += "write_integer_register(" + std::to_string(partId) + ", " + std::to_string(partValue) + ")\n";
    }
    return script;
}

/**
 * @legacy_source src/common/rs485_commu.cpp
 * @legacy_function CommuRS485::craft_script_arc_function_switch_script_get
 * @legacy_baseline 6fefa14
 * @compatibility 原有Lua函数名(arc_start/arc_stop)、脚本内容和缓存写入行为保持不变
 */
// 485- 起弧/收弧功能脚本
LaserSdkResult CommuRS485::craft_script_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    const bool controlWireThroughDigitalOutput =
            GetBrandProtocolAdapter(machineResource.laserHeadBrand)
                .controlsWireThroughDigitalOutput()
            && machineResource.weldingProcessType == WeldingProcessType::WIRE_WELDING_LASERSDK;
    const std::string wireFeederEnableScript = controlWireThroughDigitalOutput
            ? "                set_configurable_digital_out(2, true)  ---CO2 = 1\n" : "";
    const std::string wireFeederDisableScript = controlWireThroughDigitalOutput
            ? "                    set_configurable_digital_out(2, false)  ---CO2 = 0\n" : "";
    if (laser_parameters::UsesMappedParameters(laserMachineHandle, machineResource.laserHeadBrand))
    {
        // 映射通讯状态由上位机监控，起收弧不能依赖旧自由通讯的821超时槽。
        if (laserSwitch == SWITCH_ON_LASER_SDK)
        {
            startLuaScripts["arc_start"] = "function arc_start()\n"
                    "    write_integer_register(673, 0)\n"
                    "    set_configurable_digital_out(0, true)\n"
                    + wireFeederEnableScript
                    + "    set_configurable_digital_out(1, true)\nend\n";
        }
        else if (laserSwitch == SWITCH_OFF_LASER_SDK)
        {
            startLuaScripts["arc_stop"] = "function arc_stop()\n"
                    "    write_integer_register(675, 0)\n"
                    + wireFeederDisableScript
                    + "    set_configurable_digital_out(1, false)\n"
                      "    set_configurable_digital_out(0, false)\nend\n";
        }
        else return PARAMETER_ERROR_LASER_SDK;
        return SUCCESS_LASER_SDK;
    }
    std::string writeFunction;
    std::string dataFunction;
    int gasWaitTime = 1;
    //获取脚本
    if (laserSwitch == SWITCH_ON_LASER_SDK)
    {
        dataFunction = "gasWaitTime =" + std::to_string(gasWaitTime) + "\n";
        startDataScripts.emplace("gasWaitTime", dataFunction);//预置参数脚本
        writeFunction =
        R"(
    function arc_start()
        --起弧脚本
        write_integer_register(673, 0) --起弧异常信号清空
        if read_integer_register(821) ~= 0 then
            print(string.format("起弧异常，超时 : 0x%04X", read_integer_register(821)))
        )" + wireFeederDisableScript + R"(            set_configurable_digital_out(1, false)  ---CO1 = 0
            set_configurable_digital_out(0, false)  ---CO0 = 0
            write_integer_register(673, 1)
            return
        end
        local retry_times = 5
        while true do
            --检测开始信号
            if true then
                set_configurable_digital_out(0, true)  ---CO0 = 1
        )" + wireFeederEnableScript + R"(
                set_configurable_digital_out(1, true)  ---CO1 = 1
                print("起弧成功")
                break
            else
                retry_times = retry_times - 1
                if(retry_times < 1) then
        )" + wireFeederDisableScript + R"(
                    set_configurable_digital_out(1, false)  ---CO1 = 0
                    set_configurable_digital_out(0, false)  ---CO0 = 0
                    print("起弧失败")
                    --送气使能关
                    write_integer_register(673, 1) --起弧异常信号
                    break
                end
            end
        end
    end
        )";
        // 工艺可能在自融焊和送丝焊之间切换，必须覆盖旧函数，不能沿用首次缓存的 CO2 策略。
        startLuaScripts["arc_start"] = writeFunction;
    }
    else if (laserSwitch == SWITCH_OFF_LASER_SDK)
    {
        writeFunction =
        R"(
        function arc_stop()
            --收弧脚本
            write_integer_register(675, 0) --起弧异常信号清空
            local retry_times = 5
            while true do
                --检测开始信号
                if read_integer_register(821) == 0 then
        )" + wireFeederDisableScript + R"(                    set_configurable_digital_out(1, false)
                    set_configurable_digital_out(0, false)
                    print("收弧成功")
                    break
                else
                    retry_times = retry_times - 1
                    if(retry_times < 1) then
        )" + wireFeederDisableScript + R"(                        set_configurable_digital_out(1, false)
                        set_configurable_digital_out(0, false)
                        print("收弧异常")
                        write_integer_register(675, 1) --收弧异常信号
                        break
                    end
                end
            end
        end
        )";
        startLuaScripts["arc_stop"] = writeFunction; //收弧脚本
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;

}

/**
 * @legacy_source src/common/rs485_commu.cpp
 * @legacy_function CommuRS485::craft_script_arc_program_run_script_get
 * @legacy_baseline 6fefa14
 * @compatibility 原有参数校验范围(0~3000ms)、脚本拼接顺序和错误码保持不变
 */
//485 起收弧运行指令
LaserSdkResult CommuRS485::craft_script_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startSwitch, std::string &script)
{
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    //获取脚本
    craft_script_arc_function_switch_script_get(laserMachineHandle, startSwitch);
    if (startSwitch == SWITCH_ON_LASER_SDK)
    {
        int gasWaitTime = machineResource.laserPowerParams.gasStartupAdvanceTime;
        if (!laser_parameters::UsesMappedParameters(laserMachineHandle, machineResource.laserHeadBrand)
                && (gasWaitTime < 0 || gasWaitTime > 3000))
        {
            writeLog("起弧预置延时参数异常 范围0~3000ms");
            return PARAMETER_OUT_OF_RANGE_LASER_SDK;
        }
        script += "arc_start()\n";
    }
    else if (startSwitch == SWITCH_OFF_LASER_SDK)
    {
        int gasWaitTime = machineResource.laserPowerParams.gasShutoffDelayTime;
        if (!laser_parameters::UsesMappedParameters(laserMachineHandle, machineResource.laserHeadBrand)
                && (gasWaitTime < 0 || gasWaitTime > 3000))
        {
            writeLog("收弧预置延时参数异常 范围0~3000ms");
            return PARAMETER_OUT_OF_RANGE_LASER_SDK;
        }
        script += "arc_stop()\n";
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

/**
 * @legacy_source src/common/rs485_commu.cpp
 * @legacy_function CommuRS485::craft_laser_param_set_script_get
 * @legacy_baseline 6fefa14
 * @compatibility 原有18个参数的写入顺序、缩放规则和locVec遍历逻辑保持不变
 */
//激光器
LaserSdkResult CommuRS485::craft_laser_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string &script)
{
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    const BrandProtocolAdapter &adapter = GetBrandProtocolAdapter(machineResource.laserHeadBrand);
    const BrandProtocolProfile profile = adapter.profile();
    const std::string writeSingleFuncCode = scriptCodeToLuaHex(profile.laserWriteSingleScriptCode);
    CgxiLaserPowerLaserSdk &laserPowerParams = machineResource.laserPowerParams;
    std::unordered_map<unsigned int, std::string> writeDataHash;

    // 1. 写入激光器功率 (0-5000W, 整型)
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, laserPowerParams.laserPower, machineResource.laserPowerLoc, 3, 1);
    // 2. 写入开光功率 (0-5000W, 整型)
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, laserPowerParams.laserPowerOpen, machineResource.laserOnPowerLoc, 3, 1);
    // 3. 写入开光渐进时间 (0~1000ms, 整型)
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, 0, machineResource.laserOnRampTimeLoc, 3, 1);
    // 4. 写入关光功率 (0-5000W, 整型)
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, laserPowerParams.laserPowerClose, machineResource.laserOffPowerLoc, 3, 1);
    // 5. 写入关光渐进时间 (0~300mm, 整型)
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, 0, machineResource.laserOffRampTimeLoc, 3, 1);
    // 6. 写入开气延时函数 提前送气时间	0~3000ms
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, laserPowerParams.gasStartupAdvanceTime, machineResource.gasONDelayLoc, 3, 1);
    // 7. 写入关气延时函数
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, laserPowerParams.gasShutoffDelayTime, machineResource.gasOffDelayLoc, 3, 1);
    // 8. 写入送丝延时函数
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, laserPowerParams.wireFeedAdvanceTime, machineResource.wireOutDelayLoc, 3, 1);
    // 9. 写入驱动器温度报警阈值函数
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, laserPowerParams.driverTemp, machineResource.driverTempLoc, 3, 1);
    //10. 写入保护镜报警阈值
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, laserPowerParams.protectiveWindowTemp, machineResource.protectiveWindowTempLoc, 3, 1);
    //11. 写入准直镜温度报警阈值
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, laserPowerParams.collimatorTemp, machineResource.collimatorTempLoc, 3, 1);
    //12. 写入X偏移
    addWriteParamScript(writeDataHash, writeSingleFuncCode, float100Reg, laserPowerParams.laserHeadXOffset, machineResource.laserHeadXOffsetLoc, 3, 1);
    //13. 写入Y偏移
    addWriteParamScript(writeDataHash, writeSingleFuncCode, float100Reg, laserPowerParams.laserHeadYOffset, machineResource.laserHeadYOffsetLoc, 3, 1);
    //14. 写入X放大
    addWriteParamScript(writeDataHash, writeSingleFuncCode, float100Reg, laserPowerParams.laserHeadXEnlarge, machineResource.laserHeadXScaleLoc, 3, 1);
    //15. 写入Y放大
    addWriteParamScript(writeDataHash, writeSingleFuncCode, float100Reg, laserPowerParams.laserHeadYEnlarge, machineResource.laserHeadYScaleLoc, 3, 1);
    //16. 写入点焊类型
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, static_cast<int>(laserPowerParams.spotWeldType), machineResource.spotWeldTypeLoc, 3, 1);
    //17. 写入点焊持续时间
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, laserPowerParams.spotWeldDurationTime, machineResource.spotWeldDurationLoc, 3, 1);
    //18. 写入点焊间隔时间
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, laserPowerParams.spotWeldIntervalTime, machineResource.spotWeldIntervalLoc, 3, 1);
    // 按指定地址顺序添加脚本
    for (unsigned int addr : locVec)
    {
        auto it = writeDataHash.find(addr);
        if (it != writeDataHash.end())
        {
            script += it->second;           // 调用函数
        }
        else
        {
            {
                std::ostringstream oss;
                oss << "Missing Lua script for laser address 0x" << std::hex << addr << std::dec;
                writeLog(oss.str());
            }
            return LaserSdkResult::PARAMETER_ERROR_LASER_SDK;
        }
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

/**
 * @legacy_source src/common/rs485_commu.cpp
 * @legacy_function CommuRS485::craft_laser_head_param_set_script_get
 * @legacy_baseline 6fefa14
 * @compatibility 原有图形枚举映射(TRIANGLE→4, CROSS→5)和locVec遍历逻辑保持不变
 */
//激光头图案
LaserSdkResult CommuRS485::craft_laser_head_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string &script)
{
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    const BrandProtocolAdapter &adapter = GetBrandProtocolAdapter(machineResource.laserHeadBrand);
    const BrandProtocolProfile profile = adapter.profile();
    const std::string writeSingleFuncCode = scriptCodeToLuaHex(profile.laserWriteSingleScriptCode);
    CgxiLaserHeadLaserSdk &laserHeadParams = machineResource.laserHeadParams;

    std::unordered_map<unsigned int, std::string> writeDataHash;
    // 1. 写入扫描速度 (0~30000mm/s,)
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, laserHeadParams.laserHeadScanSpeed, machineResource.laserHeadScanSpeedLoc, 3, 1);
    // 2. 写入扫描宽度 (0.1~5mm, 浮点型)
    addWriteParamScript(writeDataHash, writeSingleFuncCode, float10Reg, laserHeadParams.laserHeadScanWidth, machineResource.laserHeadScanWidthLoc, 3, 1);
    // 3. 写入峰值功率 (0-3000W, 整型)
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, laserHeadParams.laserHeadPeakPower, machineResource.laserHeadPeakPowerLoc, 3, 1);
    // 4. 写入占空比 (0~100, 整型)
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, laserHeadParams.laserHeadDutyCycle, machineResource.laserHeadDutyCycleLoc, 3, 1);
    // 7. 写入扫描高度 (0.1~5mm, 浮点型)
    addWriteParamScript(writeDataHash, writeSingleFuncCode, float10Reg, laserHeadParams.laserHeadScanHeight, machineResource.laserHeadScanHeightLoc, 3, 1);
    // 8. 写入旋转角度 (0~360°, 整型)
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, laserHeadParams.laserHeadSpinAngle, machineResource.laserHeadRotationAngleLoc, 3, 1);
    // 9. 写入图形（特殊处理）图形
    int spotPatternValue = static_cast<int>(laserHeadParams.laserSpotPattern);
    switch (static_cast<SpotPatternLaserSdk>(laserHeadParams.laserSpotPattern))
    {
    case SpotPatternLaserSdk::CIR_LASERSDK:
    case SpotPatternLaserSdk::HORIZON_8_LASERSDK:
    case SpotPatternLaserSdk::LINE_LASERSDK:
    case SpotPatternLaserSdk::STAR_LASERSDK:
        spotPatternValue = static_cast<int>(laserHeadParams.laserSpotPattern);
        break;
    case SpotPatternLaserSdk::TRIANGLE_LASERSDK:
        spotPatternValue = 4;
        break;
    case SpotPatternLaserSdk::CROSS_LASERSDK:
        spotPatternValue = 5;
        break;
    }
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, spotPatternValue, machineResource.laserHeadPatternIdLoc, 3, 1);
    //脉冲频率低地址 (5~100000HZ, 整型)
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, laserHeadParams.laserHeadLowPulseFrequency, machineResource.laserHeadFrequencyLowLoc, 3, 1);
    //写入脉冲频率高地址 (5~100000HZ, 整型)
    addWriteParamScript(writeDataHash, writeSingleFuncCode, int16Reg, laserHeadParams.laserHeadHighPulseFrequency, machineResource.laserHeadFrequencyHighLoc, 3, 1);
    for (unsigned int addr : locVec)
    {
        auto it = writeDataHash.find(addr);
        if (it != writeDataHash.end())
        {
            script += it->second;
        }
        else
        {
            {
                std::ostringstream oss;
                oss << "Missing Lua script for address 0x" << std::hex << addr << std::dec;
                writeLog(oss.str());
            }
            return LaserSdkResult::PARAMETER_ERROR_LASER_SDK;
        }
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

/**
 * @legacy_source src/common/rs485_commu.cpp
 * @legacy_function CommuRS485::craft_wire_feeder_param_set_script_get
 * @legacy_baseline 6fefa14
 * @compatibility 原有自熔焊送丝机报文截断行为和品牌适配器过滤逻辑保持不变
 */
//送丝机参数
LaserSdkResult CommuRS485::craft_wire_feeder_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string &script)
{
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    const BrandProtocolAdapter &adapter = GetBrandProtocolAdapter(machineResource.laserHeadBrand);
    const BrandProtocolProfile profile = adapter.profile();
    const std::string writeSingleFuncCode = scriptCodeToLuaHex(profile.wireFeederWriteSingleScriptCode);
    CgxiWireFeederLaserSdk &wireFeederParams = machineResource.wireFeederParams;

    // 【修改理由】自熔焊模式下大部分送丝机报文需要被直接截断，避免用户关闭送丝机后 SDK 仍然访问断联设备。
    // 【逻辑】locVec 中只要没有任何品牌规则允许的送丝机写入，就返回成功但不拼接脚本。
    bool hasAllowedWireFeederWrite = false;
    for (unsigned int addr : locVec)
    {
        BrandRegisterRole role = BrandRegisterRole::AutoWireFeedSpeed;
        if (findWireFeederRoleByAddr(adapter, machineResource, addr, role)
                && adapter.allowWireFeederWrite(machineResource.weldingProcessType, role))
        {
            hasAllowedWireFeederWrite = true;
            break;
        }
    }
    if (!hasAllowedWireFeederWrite)
        return LaserSdkResult::SUCCESS_LASER_SDK;

    // 存储所有函数定义和调用脚本的数据结构
    std::unordered_map<unsigned int, std::string> writeDataHash;

    // 【修改理由】0x0115 等送丝机启动/暂停地址已迁移到品牌适配器，通讯类只表达运行状态语义。
    // 【逻辑】自熔焊由适配器返回暂停值，填丝焊由适配器返回恢复值。
    if (machineResource.weldingProcessType == WeldingProcessType::SELF_WELDING_LASERSDK)
    {   //自融焊
        addWireFeederRoleParamScript(writeDataHash, adapter, writeSingleFuncCode, BrandRegisterRole::WireFeederRunStatus, adapter.wireFeederRunStatusValue(machineResource.weldingProcessType), machineResource.wireFeederStatusLoc, 0);  //自动送丝暂停
    }
    else
    {   //送丝焊
        addWireFeederRoleParamScript(writeDataHash, adapter, writeSingleFuncCode, BrandRegisterRole::WireFeederRunStatus, adapter.wireFeederRunStatusValue(machineResource.weldingProcessType), machineResource.wireFeederStatusLoc, 0);  //自动送丝恢复
    }

    // 0x0100 自动送丝速度
    addWireFeederRoleParamScript(writeDataHash, adapter, writeSingleFuncCode, BrandRegisterRole::AutoWireFeedSpeed, wireFeederParams.wireFeederMotorSpeed, machineResource.autoWireFeedSpeedLoc, 1);
    // 0x0101手动送丝速度
    addWireFeederRoleParamScript(writeDataHash, adapter, writeSingleFuncCode, BrandRegisterRole::ManualWireFeedSpeed, wireFeederParams.wireFeederManualFeedSpeed, machineResource.manualWireFeedSpeedLoc, 1);
    // 0x0102手动抽丝速度
    addWireFeederRoleParamScript(writeDataHash, adapter, writeSingleFuncCode, BrandRegisterRole::ManualWireRetractSpeed, wireFeederParams.wireFeederManualBackSpeed, machineResource.manualWireRetractSpeedLoc, 1);
    // 0x0105 补丝长度
    addWireFeederRoleParamScript(writeDataHash, adapter, writeSingleFuncCode, BrandRegisterRole::WireFeedLength, wireFeederParams.wireFeederFeedLength, machineResource.wireFeedLengthLoc, 1);
    // 0x0104 抽丝长度
    addWireFeederRoleParamScript(writeDataHash, adapter, writeSingleFuncCode, BrandRegisterRole::WireRetractLength, wireFeederParams.wireFeederBackLength, machineResource.wireRetractLengthLoc, 1);
    // 0x0106 补丝延时
    addWireFeederRoleParamScript(writeDataHash, adapter, writeSingleFuncCode, BrandRegisterRole::WireFeedBackDelay, wireFeederParams.wireFeederWaitTime, machineResource.wireFeedBackDelayLoc, 1);
    // 0x010D 断丝模式
    addWireFeederRoleParamScript(writeDataHash, adapter, writeSingleFuncCode, BrandRegisterRole::BreakWireFeedingMode, static_cast<int>(wireFeederParams.craftWireCutMode), machineResource.breakWireFeedingModeLoc, 1);
    // 0x0140 手动送丝开关
    addWireFeederRoleParamScript(writeDataHash, adapter, writeSingleFuncCode, BrandRegisterRole::ManualWireFeedSwitch, static_cast<int>(wireFeederParams.isWireFeederManualFeedSwitch), machineResource.manualWireFeedSwitchLoc, 1);
    // 0x0141 手动回抽开关
    addWireFeederRoleParamScript(writeDataHash, adapter, writeSingleFuncCode, BrandRegisterRole::ManualWireRetractSwitch, static_cast<int>(wireFeederParams.isWireFeederManualRetractSwitch), machineResource.manualWireRetractSwitchLoc, 1);

    for (unsigned int addr : locVec)
    {
        BrandRegisterRole role = BrandRegisterRole::AutoWireFeedSpeed;
        // 【修改理由】自熔焊只允许送丝机暂停报文，其它送丝机参数即使已生成候选脚本也不能输出。
        // 【逻辑】逐个地址按品牌规则过滤，允许的地址才追加到最终脚本。
        if (findWireFeederRoleByAddr(adapter, machineResource, addr, role)
                && !adapter.allowWireFeederWrite(machineResource.weldingProcessType, role))
        {
            continue;
        }
        auto it = writeDataHash.find(addr);
        if (it != writeDataHash.end())
        {
            script += it->second;
        }
        else
        {
            std::string tmp;
            writeLog("craft_wire_feeder_param_set_script_get : Missing Lua script for wire feeder address 0x" + std::to_string(addr));
            return LaserSdkResult::PARAMETER_ERROR_LASER_SDK;
        }
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

/**
 * @brief 兼容工艺接口只委托参数编译链路，不修改焊机资源或维护第二份地址表。
 */
LaserSdkResult CommuRS485::craft_script_configure_machine_param_set_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk &craftInfo, std::string &bodyScriptText)
{
    const std::unordered_map<int, MachineResource>::const_iterator it = machineResourcesMap.find(laserMachineHandle);
    if (it == machineResourcesMap.end()) return HANDLE_ERROR_LASER_SDK;
    std::string compiled;
    const LaserSdkResult result = laser_parameters::BuildMachineCraftScript(
                laserMachineHandle, it->second, craftInfo, compiled);
    if (result != SUCCESS_LASER_SDK) return result;
    // 保留旧工艺下发的通讯超时复位，实际执行前不会改变任何资源或控制器状态。
    bodyScriptText += strWriteInt32RegReplace(INT32_REG_COMMU_TIMEOUT_821, 0) + compiled;
    return SUCCESS_LASER_SDK;
}

/**
 * @legacy_source src/common/rs485_commu.cpp
 * @legacy_function CommuRS485::craft_script_slow_arc_function_switch_script_get
 * @legacy_baseline 6fefa14
 * @compatibility 原有缓升缓降函数脚本缓存key(arcStartScript/arcStopScript)和异常处理保持不变
 */
//缓升缓降 起弧/收弧功能脚本
LaserSdkResult CommuRS485::craft_script_slow_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    try
    {
        if (laserSwitch == SWITCH_ON_LASER_SDK)
        {
            startLuaScripts.emplace("arcStartScript", buildSlowArcStartFunctionScript(machineResource));
            return LaserSdkResult::SUCCESS_LASER_SDK;
        }
        if (laserSwitch == SWITCH_OFF_LASER_SDK)
        {
            startLuaScripts.emplace("arcStopScript", buildSlowArcStopFunctionScript(machineResource));
            return LaserSdkResult::SUCCESS_LASER_SDK;
        }
        return LaserSdkResult::PARAMETER_ERROR_LASER_SDK;
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }
}

//缓升缓降 起收弧运行指令
LaserSdkResult CommuRS485::craft_script_slow_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startSwitch, std::string &script)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    //获取脚本
    if (startSwitch == SWITCH_ON_LASER_SDK)
    {
        script += " --slow_arc_start()\n";
    }
    else if (startSwitch == SWITCH_OFF_LASER_SDK)
    {
        script += " slow_arc_stop()\n";
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

void CommuRS485::init_multiple_registers_value(std::string multipleRegistersValue)
{
    if(multipleRegistersValue != "")
    {
        startLuaScripts.emplace("multipleRegistersValue", multipleRegistersValue); //写多个寄存器的初始化变量
        return;
    }
    // 参数数组已由统一编译链按当前配置直接生成，空输入无需维护固定地址 Lua 表。
    // 非空输入仍可注入调用方提供的初始化片段，保留此兼容入口。
}

/** @brief 动态工艺修改复用同一参数编译链路，不使用固定地址Lua表。 */
LaserSdkResult CommuRS485::craft_script_write_craft_resource_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo, std::string &script)
{
    return craft_script_configure_machine_param_set_script_get(laserMachineHandle, craftInfo, script);
}
