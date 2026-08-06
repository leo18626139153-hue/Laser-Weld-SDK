#include "rs485_commu.h"
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
    script << "    function slow_arc_stop()\n"
           << "   write_integer_register(675, 0) \n"
           << "   if read_integer_register(821) ~= 0 then \n"
           << "       print(string.format(\"收弧异常，超时 : 0x%04X\", read_integer_register(821))) \n"
           << "       write_integer_register(675, 1) \n"
           << "       return \n"
           << "   end \n"
           << "   laserPower = " << laserHeadParams.laserHeadPeakPower << "\n"
           << "   closePower  = " << laserPowerParams.laserPowerClose << "\n"
           << "   closeLightTime  = " << laserPowerParams.laserRampDownTime << "\n"
           << "   if closeLightTime == 0 then \n"
           << "       print(\"closeLightTime = 0\") \n"
           << "       return \n"
           << "   end \n";
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
    //MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
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
            write_integer_register(673, 1)
            return
        end
        local retry_times = 5
        while true do
            --检测开始信号
            if true then
                set_configurable_digital_out(0, true)  ---CO0 = 1
                set_configurable_digital_out(1, true)  ---CO1 = 1
                print("起弧成功")
                break
            else
                retry_times = retry_times - 1
                if(retry_times < 1) then
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
        startLuaScripts.emplace("arc_start", writeFunction);
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
                    set_configurable_digital_out(1, false)
                    set_configurable_digital_out(0, false)
                    print("收弧成功")
                    break
                else
                    retry_times = retry_times - 1
                    if(retry_times < 1) then
                        set_configurable_digital_out(0, false)
                        set_configurable_digital_out(1, false)
                        print("收弧异常")
                        write_integer_register(675, 1) --收弧异常信号
                        break
                    end
                end
            end
        end
        )";
        startLuaScripts.emplace("arc_stop", writeFunction); //收弧脚本
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
        if (gasWaitTime < 0 || gasWaitTime > 3000)
        {
            writeLog("起弧预置延时参数异常 范围0~3000ms");
            return PARAMETER_OUT_OF_RANGE_LASER_SDK;
        }
        script += "arc_start()\n";
    }
    else if (startSwitch == SWITCH_OFF_LASER_SDK)
    {
        int gasWaitTime = machineResource.laserPowerParams.gasShutoffDelayTime;
        if (gasWaitTime < 0 || gasWaitTime > 3000)
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
            std::cerr << "Warning: Missing Lua script for laser address 0x"
                      << std::hex << addr << std::dec << std::endl;
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
            std::cerr << "Warning: Missing Lua script for address 0x"
                      << std::hex << addr << std::dec << std::endl;
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
 * @legacy_source src/common/rs485_commu.cpp
 * @legacy_function CommuRS485::craft_script_configure_machine_param_set_script_get
 * @legacy_baseline 6fefa14
 * @compatibility 原有工艺参数计算、MachineResource更新时机和品牌适配器委托逻辑保持不变
 */
//工艺参数设置脚本获取
// 【修改理由】重构拆分：本接口只负责计算工艺参数并更新 MachineResource，
//            脚本写入逻辑移到品牌适配器的 buildConfigureMachineBodyScript。
// 【逻辑】
// 1. 检查句柄有效性和工艺参数合法性
// 2. 计算开光/关光功率百分比和缓升缓降时间
// 3. 将工艺参数逐项赋值到 machineResource 各子结构
// 4. 调用品牌适配器生成品牌特有的脚本体并组装到 bodyScriptText
// 5. 填丝焊时额外下发送丝机参数脚本
LaserSdkResult CommuRS485::craft_script_configure_machine_param_set_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk &craftInfo, std::string &bodyScriptText)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    const BrandProtocolAdapter &adapter = GetBrandProtocolAdapter(machineResource.laserHeadBrand);
    CraftInfoLaserSdk curCraftInfo = craftInfo;
    LaserSdkResult res = LaserSdkResult::SUCCESS_LASER_SDK;
    res = adapter.checkCraft(curCraftInfo);//参数判断
    if(res != LaserSdkResult::SUCCESS_LASER_SDK)
        return res;

    // ====== 工艺参数计算与赋值（仅更新 machineResource，不写脚本） ======

    /*
     * Lead-in and lead-out power belong to the current craft and must be
     * validated against this craft's welding power. machineResource contains
     * the previous applied result and cannot be used as the validation base.
     * Direct comparison also avoids division by zero and integer overflow.
     */
    const int craftLaserPower = curCraftInfo.craftLaserPower;
    if (craftLaserPower < 0)
    {
        writeLog("craft_script_configure_machine_param_set_script_get :: craftLaserPower out of range, craftLaserPower="
                 + std::to_string(craftLaserPower));
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }
    if (curCraftInfo.craftLeadInPower < 0
            || curCraftInfo.craftLeadInPower > craftLaserPower)
    {
        writeLog("craft_script_configure_machine_param_set_script_get :: craftLeadInPower out of range, craftLeadInPower="
                 + std::to_string(curCraftInfo.craftLeadInPower)
                 + ", craftLaserPower=" + std::to_string(craftLaserPower));
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }
    machineResource.laserPowerParams.laserPowerOpen = curCraftInfo.craftLeadInPower;
    machineResource.laserPowerParams.laserRampUpTime = curCraftInfo.craftLeadInRampTime;

    if (curCraftInfo.craftLeadOutPower < 0
            || curCraftInfo.craftLeadOutPower > craftLaserPower)
    {
        writeLog("craft_script_configure_machine_param_set_script_get :: craftLeadOutPower out of range, craftLeadOutPower="
                 + std::to_string(curCraftInfo.craftLeadOutPower)
                 + ", craftLaserPower=" + std::to_string(craftLaserPower));
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }
    machineResource.laserPowerParams.laserPowerClose = curCraftInfo.craftLeadOutPower;

    // 计算关光功率下降时间
    int  disX = curCraftInfo.craftLeadOutRampDistance - curCraftInfo.craftRetractionDistance ;  //关光渐进距离-提前回抽距离
    if(disX  < 0)
    {
        writeLog("craft_script_configure_machine_param_set_script_get :: laserRampDownTime out of range");
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }
    //降功率的距离 → 时间
    double tempTime = static_cast<double>((disX * 1000)/ curCraftInfo.craftWeldingSpeed);
    machineResource.laserPowerParams.laserRampDownTime = tempTime;
    // 提前送气时间
    machineResource.laserPowerParams.gasStartupAdvanceTime = curCraftInfo.craftPreGasTime;
    // 气体关闭延时 --滞后送气时间
    machineResource.laserPowerParams.gasShutoffDelayTime = curCraftInfo.craftPostGasTime;

    //激光头扫描速度
    machineResource.laserHeadParams.laserHeadScanSpeed = curCraftInfo.craftScanSpeed;
    //激光头扫描宽度
    machineResource.laserHeadParams.laserHeadScanWidth = curCraftInfo.craftScanWidth;
    /*
     * laserHeadPeakPower is mapped from device configuration and represents
     * the rated power limit. craftLaserPower is the actual welding power.
     * Keep compatibility when an old caller does not provide a rated limit.
     */
    const int ratedLaserPower = curCraftInfo.laserHeadPeakPower;
    if (ratedLaserPower > 0 && craftLaserPower > ratedLaserPower)
    {
        writeLog("craft_script_configure_machine_param_set_script_get :: craftLaserPower exceeds ratedLaserPower, craftLaserPower="
                 + std::to_string(craftLaserPower)
                 + ", ratedLaserPower=" + std::to_string(ratedLaserPower));
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }
    machineResource.laserHeadParams.laserHeadPeakPower = ratedLaserPower;
    // Store the current welding power for script generation only.
    machineResource.laserPowerParams.laserPower = craftLaserPower;
    //激光头占空比
    machineResource.laserHeadParams.laserHeadDutyCycle = curCraftInfo.laserHeadDutyCycle;
    //激光的脉冲频率
    //激光头脉冲频率低地址
    machineResource.laserHeadParams.laserHeadLowPulseFrequency = curCraftInfo.laserHeadLowPulseFrequency;
    //激光头脉冲频率高地址
    machineResource.laserHeadParams.laserHeadHighPulseFrequency = curCraftInfo.laserHeadHighPulseFrequency;

    //激光头扫描高度
    machineResource.laserHeadParams.laserHeadScanHeight = curCraftInfo.craftScanHeight;
    //激光头旋转角度
    machineResource.laserHeadParams.laserHeadSpinAngle = curCraftInfo.craftRotationAngle;
    // YX轴缩放/偏移量
    machineResource.laserPowerParams.laserHeadYEnlarge = curCraftInfo.craftYAxisZoom;
    machineResource.laserPowerParams.laserHeadYOffset = curCraftInfo.craftYAxisOffset;
    machineResource.laserPowerParams.laserHeadXEnlarge = curCraftInfo.craftXAxisZoom;
    machineResource.laserPowerParams.laserHeadXOffset = curCraftInfo.craftXAxisOffset;

    // 点焊工艺
    machineResource.laserPowerParams.spotWeldType = curCraftInfo.craftWeldType;
    machineResource.laserPowerParams.spotWeldDurationTime = curCraftInfo.craftSpotWeldDuration;
    machineResource.laserPowerParams.spotWeldIntervalTime = curCraftInfo.craftSpotWeldInterval;
    ///< 写入焊接模式
    machineResource.laserPowerParams.weldMode = curCraftInfo.craftweldMode;
    ///< 写入激光头图案
    machineResource.laserHeadParams.laserSpotPattern = curCraftInfo.graphicsSwitching;
    //  送丝机参数
    int distance = curCraftInfo.craftRetractionDistance;
    //回抽焊丝的过程距离 mm
    machineResource.wireFeederParams.lengthBack = distance;
    //  计算补丝延迟//计算补丝延迟,s换算成ms
    int wireDelayTime = static_cast<int>((distance / curCraftInfo.craftWeldingSpeed)*1000);
    if ( wireDelayTime > 9000)
    {
        writeLog("补丝延迟时间过大");
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }
    //  回抽长度判断
    if(curCraftInfo.craftRetractionLength < 0 || curCraftInfo.craftRetractionLength > 20)
    {
        curCraftInfo.craftRetractionLength = 20;
    }

    machineResource.wireFeederParams.wireFeederWaitTime = wireDelayTime;    ///< 补丝延时    0~2000 ms
    machineResource.wireFeederParams.wireFeederBackLength = curCraftInfo.craftRetractionLength;     ///< 收尾回抽     回抽长度
    machineResource.wireFeederParams.wireFeederFeedLength = curCraftInfo.craftEndLoopbackLength;    ///< 回送长度
    machineResource.wireFeederParams.wireFeederMotorSpeed = curCraftInfo.craftWeldingWireFeed;      ///< 自动送丝速度 15~600cm/min 0x0100 R/W
    machineResource.wireFeederParams.wireFeederStartMotorSpeed = curCraftInfo.craftLeadInWireFeed;  ///< 开光送丝sudu
    machineResource.wireFeederParams.wireFeederStopMotorSpeed = curCraftInfo.craftLeadOutWireFeed;  ///< 关光送丝sudu
    machineResource.wireFeederParams.craftWireCutMode = curCraftInfo.craftWireCutMode;              ///<断丝模式
    // ====== 脚本生成：委托品牌适配器 ======
    bodyScriptText += strWriteInt32RegReplace(821, 0);      //存放超时地址的寄存器
    std::string str;
    adapter.buildConfigureMachineBodyScript(machineResource, curCraftInfo, str);
    bodyScriptText += str;

    //  如果是填丝焊👇👇👇则下发如下参数；自融焊则不下发焊接参数
    if(machineResource.weldingProcessType == WeldingProcessType::WIRE_WELDING_LASERSDK && machineResource.laserHeadBrand == LaserHeadBrandSdk::CQLASERPOWER_SDK)
    {

        // 【修改理由】工艺参数写入时送丝机下发集合由品牌规则决定，通讯类不再硬编码超强地址顺序。
        // 【逻辑】适配器在自熔焊下只返回暂停送丝机角色，在填丝焊下返回需要写入的送丝参数角色。
        std::vector<unsigned int> wireFeedVec;
        std::vector<BrandRegisterRole> wireRoles = adapter.craftWireFeederRoles(machineResource.weldingProcessType);
        for (BrandRegisterRole role : wireRoles)
        {
            if (adapter.hasRegister(role))
            {
                wireFeedVec.push_back(adapter.registerInfo(role).writeAddr);
            }
        }
        craft_wire_feeder_param_set_script_get(laserMachineHandle, wireFeedVec, bodyScriptText);
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
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
        script += " --slow_arc_stop()\n";
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
    // 【修改理由】删除与 initData() 中完全重复的 40+ 行数据表定义（laserRegisterMap/wireRegisterMap/spotWeld/laserHeadRegisterMap）
    // 【逻辑】initData() 已将4个数据表写入 loadLuaFunctionScript（持久存在，不会被 loadLuaFunctionScriptOut(machineResource.laserHeadBrand) 擦除）
    //   因此每次脚本输出都自动包含表的初始定义，init_multiple_registers_value("") 无需重复添加
    //   非空参数路径保留，用于外部调用者注入自定义初始化数据
}

/**
 * @legacy_source src/common/rs485_commu.cpp
 * @legacy_function CommuRS485::craft_script_write_craft_resource_script_get
 * @legacy_baseline 6fefa14
 * @compatibility 原有批量写/单写品牌分支、数据表赋值顺序和送丝机参数下发逻辑保持不变
 */
LaserSdkResult CommuRS485::craft_script_write_craft_resource_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo, std::string &script)
{
    //  1.获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    const BrandProtocolAdapter &adapter = GetBrandProtocolAdapter(machineResource.laserHeadBrand);
    const BrandProtocolProfile profile = adapter.profile();
    const std::string writeSingleFuncCode = scriptCodeToLuaHex(profile.laserWriteSingleScriptCode);
    const std::string writeMultiFuncCode = scriptCodeToLuaHex(profile.writeMultiScriptCode);
    //  2.初始化写多个寄存器的参数；
    init_multiple_registers_value("");
    CraftInfoLaserSdk curCraftInfo = craftInfo;
    LaserSdkResult res = LaserSdkResult::SUCCESS_LASER_SDK;
    //超强激光头参数的判断
    res = adapter.checkCraft(curCraftInfo);
    if(res != LaserSdkResult::SUCCESS_LASER_SDK)    return res;
    //  3. 激光参数赋值
    std::string write_laser_params;
    write_laser_params += "    laserRegisterMap[1][2] = "+ std::to_string(machineResource.laserPowerParams.laserPower)+"\n";
    int craftLeadInPowerPercent = (curCraftInfo.craftLeadInPower*100)/machineResource.laserPowerParams.laserPower;  ///开光功率百分比
    if(craftLeadInPowerPercent < 0 || craftLeadInPowerPercent > 100){
        std::cout << "craftLeadInPowerPercent out of range" << std::endl;
        writeLog("craft_script_configure_machine_param_set_script_get :: craftLeadInPowerPercent out of range");
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }
    machineResource.laserPowerParams.laserPowerOpen = curCraftInfo.craftLeadInPower;/// 开光功率
    machineResource.laserPowerParams.laserRampUpTime = curCraftInfo.craftLeadInRampTime;
    //  4.计算关光功率百分比
    int craftLeadOutPowerPercent = (curCraftInfo.craftLeadOutPower*100)/machineResource.laserPowerParams.laserPower;
    if(craftLeadOutPowerPercent < 0 || craftLeadOutPowerPercent > 100){
        writeLog("craft_script_configure_machine_param_set_script_get :: craftLeadOutPowerPercent out of range");
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }
    machineResource.laserPowerParams.laserPowerClose = curCraftInfo.craftLeadOutPower;
    //  5.计算关光功率下降时间
    int  disX = curCraftInfo.craftLeadOutRampDistance - curCraftInfo.craftRetractionDistance ;  //关光渐进距离-提前回抽距离
    if(disX  < 0)
    {
        writeLog("craft_script_configure_machine_param_set_script_get :: laserRampDownTime out of range");
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }
    //  6.降功率的距离
    double tempTime = static_cast<double>((disX * 1000)/ curCraftInfo.craftWeldingSpeed);
    machineResource.laserPowerParams.laserRampDownTime = tempTime;

    //  7.提前送气时间
    write_laser_params += "    laserRegisterMap[6][2] = "+ std::to_string(curCraftInfo.craftPreGasTime)+"\n";
    machineResource.laserPowerParams.gasStartupAdvanceTime = curCraftInfo.craftPreGasTime;
    //  8.气体关闭延时 --滞后送气时间
    write_laser_params += "    laserRegisterMap[7][2] = "+ std::to_string(curCraftInfo.craftPostGasTime)+"\n";
    machineResource.laserPowerParams.gasShutoffDelayTime = curCraftInfo.craftPostGasTime;

    //  9.激光头扫描速度
    write_laser_params += "    laserHeadRegisterMap[1][2] = "+ std::to_string(curCraftInfo.craftScanSpeed)+"\n";
    machineResource.laserHeadParams.laserHeadScanSpeed = curCraftInfo.craftScanSpeed;
    //  10.激光头扫描宽度
    write_laser_params += "    laserHeadRegisterMap[2][2] = "+ double2str(curCraftInfo.craftScanWidth, 1)+"*10 \n";
    machineResource.laserHeadParams.laserHeadScanWidth = curCraftInfo.craftScanWidth;
    //  11.焊接功率、峰值功率
    if(curCraftInfo.craftLaserPower > machineResource.laserPowerParams.laserPower){
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }
    write_laser_params += "    laserHeadRegisterMap[3][2] = "+ std::to_string(curCraftInfo.craftLaserPower)+"\n";  //  峰值功率从开光功率开始升
    machineResource.laserHeadParams.laserHeadPeakPower = curCraftInfo.craftLaserPower;
    //  12.占空比
    write_laser_params += "    laserHeadRegisterMap[4][2] = "+ std::to_string(machineResource.laserHeadParams.laserHeadDutyCycle)+"\n";
    //  13.脉冲频率高位
    write_laser_params += "    laserHeadRegisterMap[5][2] = "+ std::to_string(machineResource.laserHeadParams.laserHeadHighPulseFrequency)+"\n";
    //  14.脉冲频率低位
    write_laser_params += "    laserHeadRegisterMap[6][2] = "+ std::to_string(machineResource.laserHeadParams.laserHeadLowPulseFrequency )+"\n";

    //  15.激光头扫描高度
    write_laser_params += "    laserHeadRegisterMap[7][2] = "+ double2str(curCraftInfo.craftScanHeight, 1)+"*10 \n";
    machineResource.laserHeadParams.laserHeadScanHeight = curCraftInfo.craftScanHeight;
    //  16.激光头旋转角度
    write_laser_params += "    laserHeadRegisterMap[8][2] = "+ std::to_string(curCraftInfo.craftRotationAngle)+"\n";
    machineResource.laserHeadParams.laserHeadSpinAngle = curCraftInfo.craftRotationAngle;
    //  17.YXYX轴偏移量
    write_laser_params += "    laserRegisterMap[9][2] = "+ double2str(curCraftInfo.craftYAxisZoom, 2)+"*100 \n";        //激光头Y轴缩放
    machineResource.laserPowerParams.laserHeadYEnlarge = curCraftInfo.craftYAxisZoom;
    write_laser_params += "    laserRegisterMap[10][2] = "+ double2str(curCraftInfo.craftYAxisOffset, 2)+"*100 \n";     //激光头Y轴偏移
    machineResource.laserPowerParams.laserHeadYOffset = curCraftInfo.craftYAxisOffset;
    write_laser_params += "    laserRegisterMap[11][2] = "+ double2str(curCraftInfo.craftXAxisZoom, 2)+"*100 \n";       //激光头X轴缩放
    machineResource.laserPowerParams.laserHeadXEnlarge = curCraftInfo.craftXAxisZoom;
    write_laser_params += "    laserRegisterMap[12][2] = "+ double2str(curCraftInfo.craftXAxisOffset, 2)+"*100 \n";     //激光头X轴偏移
    machineResource.laserPowerParams.laserHeadXOffset = curCraftInfo.craftXAxisOffset;
    // 18.点焊工艺
    //点焊类型
    write_laser_params += "    spotWeld[1][2] = "+ std::to_string(static_cast<int>(curCraftInfo.craftWeldType))+"\n";
    machineResource.laserPowerParams.spotWeldType = curCraftInfo.craftWeldType;
    //点焊时长
    write_laser_params += "    spotWeld[2][2] = "+ std::to_string(curCraftInfo.craftSpotWeldDuration)+"\n";
    machineResource.laserPowerParams.spotWeldDurationTime = curCraftInfo.craftSpotWeldDuration;
    //点焊间隔
    write_laser_params += "    spotWeld[3][2] = "+ std::to_string(curCraftInfo.craftSpotWeldInterval)+"\n";
    machineResource.laserPowerParams.spotWeldIntervalTime = curCraftInfo.craftSpotWeldInterval;
    //  19.写入激光焊接参数、点焊参数
    std::string write_weld_params =
            "    comNo = 3\n"
            "    slave_addr = " + writeMultiFuncCode + "\n"
            "    write_multi_value_port(laserRegisterMap)      --写入激光焊接参数\n"
            "    write_multi_value_port(laserHeadRegisterMap)  --写入激光头\n"
            "    write_multi_value_port(spotWeld)              --写入点焊参数\n";

    ///< 写入焊接模式
    write_laser_params += adapter.writeLuaScript(writeSingleFuncCode, std::to_string(machineResource.weldingModeLoc), std::to_string(static_cast<int>(curCraftInfo.craftweldMode)), "3", "SendCommuRegAddr", "1");
    machineResource.laserPowerParams.weldMode = curCraftInfo.craftweldMode;

    ///< 写入激光头参数
    if(true)
    {
        // 写入图案
        machineResource.laserHeadParams.laserSpotPattern = curCraftInfo.graphicsSwitching;
        switch (curCraftInfo.graphicsSwitching) {
            case SpotPatternLaserSdk::CIR_LASERSDK:
            case SpotPatternLaserSdk::HORIZON_8_LASERSDK:
            case SpotPatternLaserSdk::LINE_LASERSDK:
            case SpotPatternLaserSdk::STAR_LASERSDK:
                write_laser_params += adapter.writeLuaScript(writeSingleFuncCode, std::to_string(machineResource.laserHeadPatternIdLoc), std::to_string(curCraftInfo.graphicsSwitching), "3", "SendCommuRegAddr", "1");
            break;
            case SpotPatternLaserSdk::TRIANGLE_LASERSDK:
                write_laser_params += adapter.writeLuaScript(writeSingleFuncCode, std::to_string(machineResource.laserHeadPatternIdLoc), std::to_string(4), "3", "SendCommuRegAddr", "1");
            break;
            case SpotPatternLaserSdk::CROSS_LASERSDK:
                write_laser_params += adapter.writeLuaScript(writeSingleFuncCode, std::to_string(machineResource.laserHeadPatternIdLoc), std::to_string(5), "3", "SendCommuRegAddr", "1");
            break;
        }
    }
    script += strWriteInt32RegReplace(821, 0);  //存放超时地址的寄存器
    if (adapter.brand() == LaserHeadBrandSdk::CQLASERPOWER_SDK)
    {
        // 修改理由：write_multi_value_port 当前按超强连续地址表组织批量报文，宝辰鑫地址表不能复用该批量写入口。
        // 逻辑：超强保持原批量写行为；其他品牌改走已有激光器、激光头单寄存器写脚本。
        script += write_laser_params + "    wait(290) \n";
        script += write_weld_params;
    }
    else
    {
        std::vector<unsigned int> laserPowerVec;
        laserPowerVec.push_back(machineResource.laserPowerLoc);
        laserPowerVec.push_back(machineResource.laserOnPowerLoc);
        laserPowerVec.push_back(machineResource.laserOnRampTimeLoc);
        laserPowerVec.push_back(machineResource.laserOffPowerLoc);
        laserPowerVec.push_back(machineResource.laserOffRampTimeLoc);
        laserPowerVec.push_back(machineResource.gasONDelayLoc);
        laserPowerVec.push_back(machineResource.gasOffDelayLoc);
        laserPowerVec.push_back(machineResource.laserHeadYScaleLoc);
        laserPowerVec.push_back(machineResource.laserHeadYOffsetLoc);
        laserPowerVec.push_back(machineResource.laserHeadXScaleLoc);
        laserPowerVec.push_back(machineResource.laserHeadXOffsetLoc);
        laserPowerVec.push_back(machineResource.spotWeldTypeLoc);
        laserPowerVec.push_back(machineResource.spotWeldDurationLoc);
        laserPowerVec.push_back(machineResource.spotWeldIntervalLoc);

        std::vector<unsigned int> laserHeadVec;
        laserHeadVec.push_back(machineResource.laserHeadPatternIdLoc);
        laserHeadVec.push_back(machineResource.laserHeadScanSpeedLoc);
        laserHeadVec.push_back(machineResource.laserHeadScanWidthLoc);
        laserHeadVec.push_back(machineResource.laserHeadPeakPowerLoc);
        laserHeadVec.push_back(machineResource.laserHeadDutyCycleLoc);
        laserHeadVec.push_back(machineResource.laserHeadFrequencyLowLoc);
        laserHeadVec.push_back(machineResource.laserHeadFrequencyHighLoc);
        laserHeadVec.push_back(machineResource.laserHeadScanHeightLoc);
        laserHeadVec.push_back(machineResource.laserHeadRotationAngleLoc);

        res = craft_laser_param_set_script_get(laserMachineHandle, laserPowerVec, script);
        if (res != LaserSdkResult::SUCCESS_LASER_SDK)
        {
            return res;
        }
        res = craft_laser_head_param_set_script_get(laserMachineHandle, laserHeadVec, script);
        if (res != LaserSdkResult::SUCCESS_LASER_SDK)
        {
            return res;
        }
        script += adapter.writeLuaScript(writeSingleFuncCode, std::to_string(machineResource.weldingModeLoc), std::to_string(static_cast<int>(curCraftInfo.craftweldMode)), "3", "SendCommuRegAddr", "1");
        script += "    wait(290) \n";
    }
    //  送丝机参数
    //  计算焊丝回抽的距离
    //double backVelocity = (curCraftInfo.craftEndingWireRetraction * 100) / 60; //收尾抽丝速度 cm/min换算成 mm/s
    int distance = curCraftInfo.craftRetractionDistance;            //回抽焊丝的过程距离 mm
    machineResource.wireFeederParams.lengthBack = distance;
    //计算补丝延迟
    int wireDelayTime = static_cast<int>((distance / curCraftInfo.craftWeldingSpeed)*1000); //计算补丝延迟,s换算成ms
    if ( wireDelayTime > 2000)
    {
        writeLog("补丝延迟时间过大");
    }
    //  回抽长度判断
    if(curCraftInfo.craftRetractionLength < 0 || curCraftInfo.craftRetractionLength > 20)
    {
        curCraftInfo.craftRetractionLength = 20;
    }
    machineResource.wireFeederParams.wireFeederWaitTime = wireDelayTime;    ///< 补丝延时    0~2000 ms
    machineResource.wireFeederParams.wireFeederBackLength = curCraftInfo.craftRetractionLength;     ///< 收尾回抽     回抽长度
    machineResource.wireFeederParams.wireFeederFeedLength = curCraftInfo.craftEndLoopbackLength;    ///< 回送长度
    machineResource.wireFeederParams.wireFeederMotorSpeed = curCraftInfo.craftWeldingWireFeed;      ///< 自动送丝速度 15~600cm/min 0x0100 R/W
    machineResource.wireFeederParams.wireFeederStartMotorSpeed = curCraftInfo.craftLeadInWireFeed;  ///< 开光送丝sudu
    machineResource.wireFeederParams.wireFeederStopMotorSpeed = curCraftInfo.craftLeadOutWireFeed;  ///< 关光送丝sudu
    machineResource.wireFeederParams.craftWireCutMode = curCraftInfo.craftWireCutMode;              ///<断丝模式
    // 【修改理由】送丝机地址由品牌适配器提供，通讯类不再绑定超强地址表
    // 【逻辑】填丝焊返回恢复送丝+送丝参数，自熔焊只返回暂停送丝
    std::vector<unsigned int> wireFeedVec;
    std::vector<BrandRegisterRole> wireRoles = adapter.craftWireFeederRoles(machineResource.weldingProcessType);
    for (BrandRegisterRole role : wireRoles)
    {
        if (adapter.hasRegister(role))
        {
            wireFeedVec.push_back(adapter.registerInfo(role).writeAddr);
        }
    }
    craft_wire_feeder_param_set_script_get(laserMachineHandle, wireFeedVec, script);

    return LaserSdkResult::SUCCESS_LASER_SDK;
}
