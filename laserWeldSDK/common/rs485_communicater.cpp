#include "rs485_communicater.h"
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
    //组装最终要下发执行的脚本
    std::string commandListStartData =
    R"(--激光器工艺
    local laserRegisterMap = {
        [1] = {0x0230, 2000},  -- 激光功率
        [2] = {0x0231, 100},  -- 开光功率
        [3] = {0x0232, 0},    -- 开光功率上升时间
        [4] = {0x0233, 100},  -- 关光功率
        [5] = {0x0234, 0},   -- 关光功率下降时间
        [6] = {0x0235, 2000},     -- 气体打开延时 --提前送气时间
        [7] = {0x0236, 1000},     -- 气体关闭延时 --滞后送气时间
        [8] = {0x0237, 0},    -- 送丝延时 默认0
        [9] = {0x0238, 1},    -- 激光头Y轴缩放
        [10] = {0x0239, 0},   -- 激光头Y轴偏移
        [11] = {0x023A, 1 },  -- 激光头X轴缩放
        [12] = {0x023B, 0 },  -- 激光头X轴偏移
    }
    --送丝机参数
    local wireRegisterMap = {
        [1] = {0x0103, 0},      --自动送丝延时
        [2] = {0x0104, 100},    --回抽长度
        [3] = {0x0105, 100},    --补丝长度
        [4] = {0x0106, 0},    --补丝延时

        [100] = {0x0100, 123},  --自动送丝速度
        [115] = {)" + std::to_string(GetBrandProtocolAdapter(LaserHeadBrandSdk::CQLASERPOWER_SDK).registerInfo(BrandRegisterRole::WireFeederRunStatus).writeAddr) + R"(, 0},    --送丝机运行状态
    }
    --点焊工艺
    local spotWeld = {
        [1] = {0x0242, 0}, --点焊类型
        [2] = {0x0243, 0}, --点焊持续时间  0 ~ 60000 ms
        [3] = {0x0244, 0} --点焊间隔时间   0 ~ 60000 ms
    }
    -- 激光头控制参数
    local laserHeadRegisterMap = {
        [1] = { 0x0000, 2000},  -- 激光头扫描速度
        [2] = { 0x0001, 25 },    -- 激光头扫描宽度
        [3] = { 0x0002, 3000 },    -- 峰值功率，焊接功率
        [4] = { 0x0003, 100},   -- 占空比
        [5] = { 0x0004, 0 },
        [6] = { 0x0005, 20000 },    --脉冲频率/工艺无控制
        [7] = { 0x0006, 25 },    -- 激光头扫描高度
        [8] = { 0x0007, 0 },    -- 激光头旋转角度
    }
    )";
    std::string startData = R"(
    local SendDataType = 0 --bool位寄存器 通过bool寄存器发送字节数据
    local RecvDataType = 1 --16位寄存器 接收到的数据放到16位寄存器中
    local SendCommuRegAddr = 0		--发送指令从bool通讯变量的 0 地址开始存储
    local SendCommuReg_n = 16		--发送指令从bool通讯变量的 0 地址开始存储
    local RecvCommuRegAddr = 30		--接收的数据从16位通讯变量的 30 地址开始存储
    local RecvCommuReg_n = 60		--接收30个字节
    local flag = 0  --标识位
    )";

    std::string swap_bytes = R"(
    -- 1.高低字节交换函数
    function swap_bytes(data)
        -- 获取低字节 (通过与操作获取低8位)
        local low_byte = data & 0xFF
        -- 获取高字节 (右移8位)
        local high_byte = data >> 8
        -- 交换并组合
        return (low_byte << 8) | high_byte
    end
    )";

    std::string modbus_crc16 = R"(
    -- 2.CRC-16 (Modbus) 计算函数 无高低字节互转
    function modbus_crc16(data)
        local crc = 0xFFFF  		-- CRC寄存器初始化为0xFFFF
        local polynomial = 0xA001   -- CRC-16多项式（0x8005反转）
        -- 对每个字节进行处理
        for _, byte in ipairs(data) do
            crc = crc ~ byte  -- 将字节与CRC寄存器进行异或
            -- 对CRC寄存器进行16次循环移位
            for i = 1, 8 do
                if (crc & 0x0001) ~= 0 then
                    crc = (crc >> 1) ~ polynomial
                else
                    crc = crc >> 1
                end
            end
        end
        return crc
    end
    )";
    std::string lua_split = R"(
    --3.高低字节拆分 如0x1234 输出{0x12, 0x34}
    function Lua_split(dataSplit)
        local value = dataSplit < 0 and (65536 + dataSplit) or dataSplit
        local low_byte = value & 0xFF
        local high_byte = value >> 8
        return {high_byte, low_byte}
    end
    )";
    std::string register_clear = R"(
    --4.定义寄存器清空函数(起始地址，清空位数)
    function register_clear(firstAddr, cleanNums)
        for i=0,cleanNums-1,1 do
            write_int16_register(i + firstAddr, 0)
        end
    end
    function bool_register_clear(firstAddr, cleanNums)
        for i=0,cleanNums-1,1 do
            write_bool_register(i + firstAddr, 0)
        end
    end
    )";
    // 【修改理由】删除了 Lua_data_get 函数：该函数在任何生成的Lua脚本中均未被调用，属于僵尸代码
    // 【逻辑】保留其他功能函数，不影响 write_value_port / write_multi_value_port 的执行
    std::string concatenate_crc = R"(
    -- 6.将十进制数转换为十六进制，并以两位字符表示
    function to_hex_string(number)
        return string.format("%02X", number)
    end
    -- 7.遍历表中单字节数据拼接对应的十六进制字符串，输入 {0x09, 0x10, {0x1234}, {0x56}}，输出 0910123456
    function concatenate_crc(crc_mes)
        local result = ""
        for _, value in ipairs(crc_mes) do
            if type(value) == "number" then
                local hex_value = to_hex_string(value)
                result = result .. hex_value
            elseif type(value) == "table" then
                local high, low = split_to_bytes(value[1])
                local high_hex = to_hex_string(high)
                local low_hex = to_hex_string(low)
                result = result .. high_hex .. low_hex
            end
        end
        return result
    end
    )" ;
    std::string parse_little_endian_bytes = R"(
    -- 每2个字符为一组(1字节)转换为整数
    local function parse_little_endian_bytes(hex_str)
        local bytes = {}
        for i = 1, #hex_str, 2 do
            local byte = hex_str:sub(i, i + 1)
            if #byte == 2 then
                local value = tonumber(byte, 16)
                table.insert(bytes, value)
            end
        end
        return bytes
    end
    )";

    std::string writeHex2Commu_reg = R"(
    --将数据写入通讯变量
    local function WriteHex2Commu_reg(hex)
        local byte_values = parse_little_endian_bytes(hex)
        local cnt = 0;
        for i, byte in ipairs(byte_values) do
            write_char_register(SendCommuRegAddr+i-1,byte)
            cnt = i
        end
        return cnt
    end
    )";

    std::string task_write_port = R"(
    --单参数写入 (功能码(2 Byte), 寄存器地址(2 Bytes), 寄存器值(2 Bytes), 端口号， 起始寄存器, 接受回执)
    function write_value_port(function_code, port_address, parameter_value, comNo, SendCommuRegAddr, receiptNeed)
        --------将通讯变量清零--------
        bool_register_clear(SendCommuRegAddr, 8)
        register_clear(RecvCommuRegAddr, RecvCommuReg_n)
        ----------高低字节拆分计算校验码---------
        local data1=Lua_split(function_code)
        local data2=Lua_split(port_address)
        local data3=Lua_split(parameter_value)
        local mes_1={data1[1], data1[2], data2[1], data2[2], data3[1], data3[2]}
        local mes_2=Lua_split(modbus_crc16(mes_1))    --计算CRC16校验码无反转
        --构造报文，高低字节反转发送
        mes_3 = {data1[1], data1[2],  data2[1], data2[2], data3[1], data3[2], mes_2[2], mes_2[1]}
        local tmp_string = concatenate_crc(mes_3)  --将数据拼接成字符串
        print(tmp_string)
        local CommuReg_n =  WriteHex2Commu_reg(tmp_string)  --将数据写入通讯变量，返回字节数
        if receiptNeed == 0 then
            serial_send_receive(comNo, CommuReg_n, 0, SendCommuRegAddr,RecvDataType,RecvCommuRegAddr, 0)
            wait(50)
            return
        else
            receiptNeed = 1
            serial_send_receive(comNo, CommuReg_n, 0, SendCommuRegAddr,RecvDataType,RecvCommuRegAddr, 1)
        end
        local tryTimes = 3
        local timeout = 1000
        while(read_int16_register(RecvCommuRegAddr) == 0) do
            wait(20)
            timeout = timeout - 20
            if read_int16_register(RecvCommuRegAddr) ~= 0 then
                print("recieve successful")
                break
            end
            if timeout <= 0 then
                tryTimes = tryTimes - 1
                timeout = 300
            end
            if tryTimes <= 0 then
                print(string.format("超时首地址 : 0x%04X", port_address))
                write_integer_register(821, port_address)
                return 0
            elseif(tryTimes > 0 )
            then
                serial_send_receive(comNo, CommuReg_n, 0, SendCommuRegAddr,RecvDataType,RecvCommuRegAddr,receiptNeed)
            end
        end

    end
    function write_multi_value_port(addr_value_map)
        -- 报文格式 {功能码-寄存器首地址-寄存器数量-字节数量-DATAX.-.CRC16}
        bool_register_clear(0, #addr_value_map * 4 + 18) -- 清空通讯变量
        register_clear(RecvCommuRegAddr, RecvCommuReg_n)
        local crc_mes = {}
        -- 1 功能码由品牌层传入, 例如超强 0x0910, 宝辰鑫 0x5A09
        local split_function_code = Lua_split(slave_addr)
        -- 2 寄存器首地址
        local split_data_2 = Lua_split(addr_value_map[1][1])
        -- 3 寄存器数量
        local split_data_3 = Lua_split(#addr_value_map)
        -- 4 字节数量
        crc_mes = {split_function_code[1], split_function_code[2], split_data_2[1], split_data_2[2], split_data_3[1], split_data_3[2], #addr_value_map * 2}
        -- 5. 将对应的变量进行高低字节拆分 放入mes中
        for k, address  in ipairs(addr_value_map) do
            local split_data_x = Lua_split(address[2])
            table.insert(crc_mes, split_data_x[1])
            table.insert(crc_mes, split_data_x[2])
        end
        --6. 计算校验码
        local split_data_crc = Lua_split(swap_bytes(modbus_crc16(crc_mes)))
        table.insert(crc_mes, split_data_crc[1])
        table.insert(crc_mes, split_data_crc[2])
        --7. 写入报文
        local mes_string = concatenate_crc(crc_mes)
        local CommuReg_n = WriteHex2Commu_reg(mes_string)
        print("发送报文："..mes_string)
        --8. 发送报文
        local timeout = 500
        local tryTimes = 3
        --端口--写寄存器个数--寄存器类型--写起始地址--读取存放寄存器类型--读起始地址--是否需要回执
        serial_send_receive(comNo, CommuReg_n, 0, SendCommuRegAddr, RecvDataType, RecvCommuRegAddr, 1)
        while true do
            wait(20)
            timeout = timeout - 20
            if read_int16_register(RecvCommuRegAddr) ~= 0 then
                print("recieve success")
                break
            end
            if timeout <= 0 then
                tryTimes = tryTimes - 1
                timeout = 310
            end
            if tryTimes <= 0 then
                print(string.format("发送超时|起始地址-0x%04X", addr_value_map[1][1]))
                write_integer_register(821, addr_value_map[1][1])
                return 0
            elseif(tryTimes > 0 )
            then
                serial_send_receive(comNo, CommuReg_n, 0, SendCommuRegAddr, RecvDataType, RecvCommuRegAddr, 1)
            end
        end
    end
    )";

    std::string read_value_port =
        "--单参数写入 (功能码(2 Byte), 寄存器地址(2 Bytes), 寄存器值(2 Bytes), 端口号， 起始寄存器), 接受回执\n"
        "function read_value_port(function_code, port_address, parameter_value, comNo, SendCommuRegAddr,SendCommuReg_n)\n"
        "	--------将通讯变量清零--------\n"
        "	bool_register_clear(SendCommuRegAddr, SendCommuReg_n)\n"
        "	register_clear(RecvCommuRegAddr, RecvCommuReg_n)\n"
        "	----------高低字节拆分计算校验码---------    \n"
        "	local data1=Lua_split(function_code)     \n"
        "	local data2=Lua_split(port_address)      \n"
        "    local data3=Lua_split(parameter_value)  \n"
        "	local mes_1={data1[1], data1[2], data2[1], data2[2], data3[1], data3[2]}\n"
        "	local mes_2=Lua_split(modbus_crc16(mes_1))    --计算CRC16校验码无反转    \n"
        "    print(string.format(\"CRC校验值: 0x%02X\", mes_2[1], mes_2[2])) \n"
        "    --构造报文，高低字节反转发送\n"
        "	mes_3 = {data1[1], data1[2],  data2[1], data2[2], data3[1], data3[2], mes_2[2], mes_2[1]}	\n"
        "    local tmp_string = concatenate_crc(mes_3)  --将数据拼接成字符串\n"
        "	print(\"发送的报文:\",tmp_string)\n"
        "    local CommuReg_n =  WriteHex2Commu_reg(tmp_string)  --将数据写入通讯变量，返回字节数\n"
        "    local timeout = 1   --超时时间\n"
        "    --端口--写字节数--写类型--写起始地址--读取字节数--读类型--读起始地址--超时时间\n"
        "	serial_send_receive(comNo, CommuReg_n, 0, SendCommuRegAddr, RecvDataType,RecvCommuRegAddr)\n"
        "    --等待接收结果\n"
        "    local timeout = 500    \n"
        "	print(\"等待接收结果...\",read_int16_register(RecvCommuRegAddr))  \n"
        "    while(read_int16_register(RecvCommuRegAddr) == 0 and timeout > 0) do   \n"
        "		print(\"等待接收结果...\")    \n"
        "        wait(10)\n"
        "        timeout = timeout - 10\n"
        "    end\n"
        "    print(\"接收结果完成\")\n"
        "end\n";

    // 【修改理由】移除了 lua_data_get（零引用僵尸函数）和 alarm_bits_list（状态监控已注释）
    // 【逻辑】其余函数按依赖顺序拼接：基础工具 → 数据表 → 变量 → CRC → 拆分 → 清空 → 串口读写
    loadLuaFunctionScript = commandListStartData + startData + swap_bytes + modbus_crc16 + lua_split + register_clear
     + concatenate_crc + parse_little_endian_bytes + writeHex2Commu_reg + task_write_port;
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

//激光器
LaserSdkResult CommuRS485::craft_laser_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string &script)
{
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    const BrandProtocolProfile profile = GetBrandProtocolAdapter(machineResource.laserHeadBrand).profile();
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

//激光头图案
LaserSdkResult CommuRS485::craft_laser_head_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string &script)
{
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    const BrandProtocolProfile profile = GetBrandProtocolAdapter(machineResource.laserHeadBrand).profile();
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
    switch (laserHeadParams.laserSpotPattern)
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

//工艺参数设置脚本获取
LaserSdkResult CommuRS485::craft_script_configure_machine_param_set_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk &craftInfo, std::string &bodyScriptText)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    const BrandProtocolProfile profile = GetBrandProtocolAdapter(machineResource.laserHeadBrand).profile();
    const std::string writeSingleFuncCode = scriptCodeToLuaHex(profile.laserWriteSingleScriptCode);
    const std::string writeMultiFuncCode = scriptCodeToLuaHex(profile.writeMultiScriptCode);
    CraftInfoLaserSdk curCraftInfo = craftInfo;
    LaserSdkResult res = LaserSdkResult::SUCCESS_LASER_SDK;
    res = GetBrandProtocolAdapter(machineResource.laserHeadBrand).checkCraft(curCraftInfo);//参数判断
    if(res != LaserSdkResult::SUCCESS_LASER_SDK)
        return res;
    ///激光参数赋值
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
    //计算关光功率百分比
    int craftLeadOutPowerPercent = (curCraftInfo.craftLeadOutPower*100)/machineResource.laserPowerParams.laserPower;
    if(craftLeadOutPowerPercent < 0 || craftLeadOutPowerPercent > 100){
        writeLog("craft_script_configure_machine_param_set_script_get :: craftLeadOutPowerPercent out of range");
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
    //降功率的距离
    double tempTime = static_cast<double>((disX * 1000)/ curCraftInfo.craftWeldingSpeed);
    machineResource.laserPowerParams.laserRampDownTime = tempTime;

    // 提前送气时间
    write_laser_params += "    laserRegisterMap[6][2] = "+ std::to_string(curCraftInfo.craftPreGasTime)+"\n";
    machineResource.laserPowerParams.gasStartupAdvanceTime = curCraftInfo.craftPreGasTime;
    // 气体关闭延时 --滞后送气时间
    write_laser_params += "    laserRegisterMap[7][2] = "+ std::to_string(curCraftInfo.craftPostGasTime)+"\n";
    machineResource.laserPowerParams.gasShutoffDelayTime = curCraftInfo.craftPostGasTime;

    //激光头扫描速度
    write_laser_params += "    laserHeadRegisterMap[1][2] = "+ std::to_string(curCraftInfo.craftScanSpeed)+"\n";
    machineResource.laserHeadParams.laserHeadScanSpeed = curCraftInfo.craftScanSpeed;
    //激光头扫描宽度
    write_laser_params += "    laserHeadRegisterMap[2][2] = "+ double2str(curCraftInfo.craftScanWidth, 1)+"*10 \n";
    machineResource.laserHeadParams.laserHeadScanWidth = curCraftInfo.craftScanWidth;
    //焊接功率、峰值功率
    if(curCraftInfo.craftLaserPower > machineResource.laserPowerParams.laserPower){
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }
    write_laser_params += "    laserHeadRegisterMap[3][2] = "+ std::to_string(curCraftInfo.craftLeadInPower)+"\n";  //  峰值功率从开光功率开始升
    machineResource.laserHeadParams.laserHeadPeakPower = curCraftInfo.craftLaserPower;
    //占空比
    write_laser_params += "    laserHeadRegisterMap[4][2] = "+ std::to_string(machineResource.laserHeadParams.laserHeadDutyCycle)+"\n";
    //脉冲频率高位
    write_laser_params += "    laserHeadRegisterMap[5][2] = "+ std::to_string(machineResource.laserHeadParams.laserHeadHighPulseFrequency)+"\n";
    //脉冲频率低位
    write_laser_params += "    laserHeadRegisterMap[6][2] = "+ std::to_string(machineResource.laserHeadParams.laserHeadLowPulseFrequency )+"\n";

    //激光头扫描高度
    write_laser_params += "    laserHeadRegisterMap[7][2] = "+ double2str(curCraftInfo.craftScanHeight, 1)+"*10 \n";
    machineResource.laserHeadParams.laserHeadScanHeight = curCraftInfo.craftScanHeight;
    //激光头旋转角度
    write_laser_params += "    laserHeadRegisterMap[8][2] = "+ std::to_string(curCraftInfo.craftRotationAngle)+"\n";
    machineResource.laserHeadParams.laserHeadSpinAngle = curCraftInfo.craftRotationAngle;
    // YXYX轴偏移量
    write_laser_params += "    laserRegisterMap[9][2] = "+ double2str(curCraftInfo.craftYAxisZoom, 2)+"*100 \n";        //激光头Y轴缩放
    machineResource.laserPowerParams.laserHeadYEnlarge = curCraftInfo.craftYAxisZoom;
    write_laser_params += "    laserRegisterMap[10][2] = "+ double2str(curCraftInfo.craftYAxisOffset, 2)+"*100 \n";     //激光头Y轴偏移
    machineResource.laserPowerParams.laserHeadYOffset = curCraftInfo.craftYAxisOffset;
    write_laser_params += "    laserRegisterMap[11][2] = "+ double2str(curCraftInfo.craftXAxisZoom, 2)+"*100 \n";       //激光头X轴缩放
    machineResource.laserPowerParams.laserHeadXEnlarge = curCraftInfo.craftXAxisZoom;
    write_laser_params += "    laserRegisterMap[12][2] = "+ double2str(curCraftInfo.craftXAxisOffset, 2)+"*100 \n";     //激光头X轴偏移
    machineResource.laserPowerParams.laserHeadXOffset = curCraftInfo.craftXAxisOffset;
    // 点焊工艺
    //点焊类型
    write_laser_params += "    spotWeld[1][2] = "+ std::to_string(static_cast<int>(curCraftInfo.craftWeldType))+"\n";
    machineResource.laserPowerParams.spotWeldType = curCraftInfo.craftWeldType;
    //点焊时长
    write_laser_params += "    spotWeld[2][2] = "+ std::to_string(curCraftInfo.craftSpotWeldDuration)+"\n";
    machineResource.laserPowerParams.spotWeldDurationTime = curCraftInfo.craftSpotWeldDuration;
    //点焊间隔
    write_laser_params += "    spotWeld[3][2] = "+ std::to_string(curCraftInfo.craftSpotWeldInterval)+"\n";
    machineResource.laserPowerParams.spotWeldIntervalTime = curCraftInfo.craftSpotWeldInterval;
    ///<写入激光焊接参数、点焊参数
    std::string write_weld_params =
            "    comNo = 3\n"
            "    slave_addr = " + writeMultiFuncCode + "\n"
            "    write_multi_value_port(laserRegisterMap)      --写入激光焊接参数\n"
            "    write_multi_value_port(laserHeadRegisterMap)  --写入激光头\n"
            "    write_multi_value_port(spotWeld)              --写入点焊参数\n";

    ///< 写入焊接模式
    write_laser_params += writeLuaScript(writeSingleFuncCode, std::to_string(machineResource.weldingModeLoc), std::to_string(static_cast<int>(curCraftInfo.craftweldMode)), "3", "SendCommuRegAddr", "1");
    machineResource.laserPowerParams.weldMode = curCraftInfo.craftweldMode;
    ///< 写入激光头参数
    /// // 写入图案
    machineResource.laserHeadParams.laserSpotPattern = curCraftInfo.graphicsSwitching;
    switch (curCraftInfo.graphicsSwitching) {
        case SpotPatternLaserSdk::CIR_LASERSDK:
        case SpotPatternLaserSdk::HORIZON_8_LASERSDK:
        case SpotPatternLaserSdk::LINE_LASERSDK:
        case SpotPatternLaserSdk::STAR_LASERSDK:
            write_laser_params += writeLuaScript(writeSingleFuncCode, std::to_string(machineResource.laserHeadPatternIdLoc), std::to_string(curCraftInfo.graphicsSwitching), "3", "SendCommuRegAddr", "1");
        break;
        case SpotPatternLaserSdk::TRIANGLE_LASERSDK:
            write_laser_params += writeLuaScript(writeSingleFuncCode, std::to_string(machineResource.laserHeadPatternIdLoc), std::to_string(4), "3", "SendCommuRegAddr", "1");
        break;
        case SpotPatternLaserSdk::CROSS_LASERSDK:
            write_laser_params += writeLuaScript(writeSingleFuncCode, std::to_string(machineResource.laserHeadPatternIdLoc), std::to_string(5), "3", "SendCommuRegAddr", "1");
        break;
    }
    bodyScriptText += strWriteInt32RegReplace(821, 0);      //存放超时地址的寄存器
    bodyScriptText += write_laser_params + "    wait(290) \n";
    bodyScriptText += write_weld_params;
    //  如果是填丝焊👇👇👇则下发如下参数；自融焊则不下发焊接参数
    if(machineResource.weldingProcessType == WeldingProcessType::WIRE_WELDING_LASERSDK)
    {

        //  送丝机参数
        int distance = curCraftInfo.craftRetractionDistance;    //回抽焊丝的过程距离 mm
        machineResource.wireFeederParams.lengthBack = distance;
        //  计算补丝延迟
        int wireDelayTime = static_cast<int>((distance / curCraftInfo.craftWeldingSpeed)*1000); //计算补丝延迟,s换算成ms
        if ( wireDelayTime > 2000)
        {
            writeLog("补丝延迟时间过大");
            //return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
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

        // 【修改理由】工艺参数写入时送丝机下发集合由品牌规则决定，通讯类不再硬编码超强地址顺序。
        // 【逻辑】适配器在自熔焊下只返回暂停送丝机角色，在填丝焊下返回需要写入的送丝参数角色。
        const BrandProtocolAdapter &adapter = GetBrandProtocolAdapter(machineResource.laserHeadBrand);
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
        script += " slow_arc_start()\n";
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
    // 【修改理由】删除与 initData() 中完全重复的 40+ 行数据表定义（laserRegisterMap/wireRegisterMap/spotWeld/laserHeadRegisterMap）
    // 【逻辑】initData() 已将4个数据表写入 loadLuaFunctionScript（持久存在，不会被 loadLuaFunctionScriptOut() 擦除）
    //   因此每次脚本输出都自动包含表的初始定义，init_multiple_registers_value("") 无需重复添加
    //   非空参数路径保留，用于外部调用者注入自定义初始化数据
}

LaserSdkResult CommuRS485::craft_script_write_craft_resource_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo, std::string &script)
{
    //  1.获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    const BrandProtocolProfile profile = GetBrandProtocolAdapter(machineResource.laserHeadBrand).profile();
    const std::string writeSingleFuncCode = scriptCodeToLuaHex(profile.laserWriteSingleScriptCode);
    const std::string writeMultiFuncCode = scriptCodeToLuaHex(profile.writeMultiScriptCode);
    //  2.初始化写多个寄存器的参数；
    init_multiple_registers_value("");
    CraftInfoLaserSdk curCraftInfo = craftInfo;
    LaserSdkResult res = LaserSdkResult::SUCCESS_LASER_SDK;
    //超强激光头参数的判断
    res = GetBrandProtocolAdapter(machineResource.laserHeadBrand).checkCraft(curCraftInfo);
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
    write_laser_params += writeLuaScript(writeSingleFuncCode, std::to_string(machineResource.weldingModeLoc), std::to_string(static_cast<int>(curCraftInfo.craftweldMode)), "3", "SendCommuRegAddr", "1");
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
                write_laser_params += writeLuaScript(writeSingleFuncCode, std::to_string(machineResource.laserHeadPatternIdLoc), std::to_string(curCraftInfo.graphicsSwitching), "3", "SendCommuRegAddr", "1");
            break;
            case SpotPatternLaserSdk::TRIANGLE_LASERSDK:
                write_laser_params += writeLuaScript(writeSingleFuncCode, std::to_string(machineResource.laserHeadPatternIdLoc), std::to_string(4), "3", "SendCommuRegAddr", "1");
            break;
            case SpotPatternLaserSdk::CROSS_LASERSDK:
                write_laser_params += writeLuaScript(writeSingleFuncCode, std::to_string(machineResource.laserHeadPatternIdLoc), std::to_string(5), "3", "SendCommuRegAddr", "1");
            break;
        }
    }
    script += strWriteInt32RegReplace(821, 0);  //存放超时地址的寄存器
    script += write_laser_params + "    wait(290) \n";
    script += write_weld_params;
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
    const BrandProtocolAdapter &adapter = GetBrandProtocolAdapter(machineResource.laserHeadBrand);
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
