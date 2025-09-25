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
    static std::string currentLuaScript = std::move(loadLuaFunctionScript);
    loadLuaFunctionScript = ""; //清空
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
    swap_bytes = R"(
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
    modbus_crc16 = R"(
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
    lua_split = R"(
    --3.高低字节拆分 如0x1234 输出{0x12, 0x34}
    function Lua_split(dataSplit)
        -- 处理负数 转换为无符号16位整数
        local value = dataSplit < 0 and (65536 + dataSplit) or dataSplit
        -- 获取低字节 (通过与操作获取低8位)
        local low_byte = dataSplit & 0xFF    
        -- 获取高字节 (右移8位)
        local high_byte = dataSplit >> 8    
        return {high_byte, low_byte}
    end
    )";    
    register_clear = R"(
    --4.定义寄存器清空函数(起始地址，清空位数)
    function register_clear(firstAddr, cleanNums)
        --------将通讯变量清零--------
        for i=0,cleanNums-1,1 do
            write_int16_register(i + firstAddr, 0) 
        end
    end
    function bool_register_clear(firstAddr, cleanNums)
        --------将通讯变量清零--------
        for i=0,cleanNums-1,1 do
            write_bool_register(i + firstAddr, 0) 
        end
    end
    )";
    lua_data_get = R"(
    --5.定义数据拆分整合函数 如0x1234 0x5678 输出0x1278
    function Lua_data_get(data1, data2)
        -- 如果数值小于0，则转换为无符号数计算
        if data1 < 0 then
            data1 = 65536 + data1
        end
        if data2 < 0 then
            data2 = 65536 + data2
        end
        -- 拆分数据1
        local data_1_1 = data1 // (16^3)
        local data_1_2 = (data1 % (16^3)) // (16^2)
        -- 拆分数据2
        local data_2_3 = (data2 % (16^2)) // 16
        local data_2_4 = data2 % 16
        -- 整合数据1和数据2
        local result = (data_1_1 * (16^3)) + (data_1_2 * (16^2)) + (data_2_3 * 16 + data_2_4)
        return result
    end
    )";

    concatenate_crc = R"(
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

    parse_little_endian_bytes = R"(
    -- 每2个字符为一组(1字节)转换为整数
    local function parse_little_endian_bytes(hex_str)
        local bytes = {}
        for i = 1, #hex_str, 2 do
            -- 确保处理最后的字节
            local byte = hex_str:sub(i, i + 1) -- 提取一个字节
            if #byte == 2 then
                -- 将字节转换为整数
                local value = tonumber(byte, 16)
                table.insert(bytes, value)
            end
        end
        return bytes
    end
    )";
    writeHex2Commu_reg = R"(
    --将数据写入通讯变量
    local function WriteHex2Commu_reg(hex)
        local byte_values = parse_little_endian_bytes(hex)
        local cnt = 0;
        for i, byte in ipairs(byte_values) do
            write_char_register(SendCommuRegAddr+i-1,byte)  --将数据写入通讯变量
            cnt = i
        end
        return cnt
    end
    )";
    task_write_port = R"(
    --单参数写入 (功能码(2 Byte), 寄存器地址(2 Bytes), 寄存器值(2 Bytes), 端口号， 起始寄存器), 接受回执
    function write_value_port(function_code, port_address, parameter_value, comNo, SendCommuRegAddr)
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
        --端口--写字节数--写类型--写起始地址--读取字节数--读类型--读起始地址--超时时间
        serial_send_receive(comNo, CommuReg_n, 0, SendCommuRegAddr, RecvCommuReg_n,RecvDataType,RecvCommuRegAddr,1) 
        --等待接收结果
        local timeout = 500
        while(read_int16_register(RecvCommuRegAddr) == 0) do            
            wait(10)
            timeout = timeout - 10
            if timeout <= 0 then
                print(string.format("超时地址 : 0x%04X", port_address))
                break
            end
        end
        
    end
    
    function write_multi_value_port(addr_value_map)
        -- 报文格式 {功能码-寄存器首地址-寄存器数量-字节数量-DATAX.-.CRC16}
        bool_register_clear(0, #addr_value_map * 4 + 18) -- 清空通讯变量    
        register_clear(RecvCommuRegAddr, RecvCommuReg_n)
        local crc_mes = {}
        -- 2 寄存器首地址
        local split_data_2 = Lua_split(addr_value_map[1][1]) 
        -- 3 寄存器数量
        local split_data_3 = Lua_split(#addr_value_map) 
        -- 4 字节数量
        crc_mes = {slave_addr, 0x10, split_data_2[1], split_data_2[2], split_data_3[1], split_data_3[2], #addr_value_map * 2}
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
        print("发送报文长度："..CommuReg_n)
        print("发送报文："..mes_string)
        print("端口号，".. comNo .."长度，"..CommuReg_n.."报文类型，"..SendDataType.." 起始地址：  "..SendCommuRegAddr)
        --8. 发送报文
        local timeout = 1000
        --端口--写字节数--写类型--写起始地址--读取字节数--读类型--读起始地址--超时时间
        serial_send_receive(comNo, CommuReg_n, 0, SendCommuRegAddr, RecvCommuReg_n,RecvDataType,RecvCommuRegAddr,1)
        while(read_int16_register(RecvCommuRegAddr) == 0) do
            wait(10)
            timeout = timeout - 10
            if read_int16_register(RecvCommuRegAddr) ~= 0x1009 and read_int16_register(RecvCommuRegAddr) ~= 0 and read_int16_register(RecvCommuRegAddr) ~= 0x100A then
                print(string.format("参数错误，回执: 0x%04X", read_int16_register(RecvCommuRegAddr)))
                break
            end
            if timeout <= 0 then
                print(string.format("超时地址 : 0x%04X", port_address))
                break
            end
        end
    end
    )";
    startData = R"(
    local SendDataType = 0 --bool位寄存器，通过bool寄存器发送字节数据
    local RecvDataType = 1 --16位寄存器，接收到的数据放到16位寄存器中
    local SendCommuRegAddr = 0		--发送指令从bool通讯变量的 0 地址开始存储
    local RecvCommuRegAddr = 30		--接收的数据从16位通讯变量的 30 地址开始存储
    local RecvCommuReg_n = 20		--接收50个字节
    )";

    loadLuaFunctionScript = startData + swap_bytes + modbus_crc16 + lua_split + register_clear + lua_data_get
     + concatenate_crc + parse_little_endian_bytes + writeHex2Commu_reg + task_write_port; 
                                                        
}

//485- 起弧/收弧功能脚本
LaserSdkResult CommuRS485::craft_script_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch, std::string &script)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    CRresult result;
    std::string writeFunction;
    int gasWaitTime;
    //获取脚本
    if (laserSwitch == SWITCH_ON_LASER_SDK)
    {
        gasWaitTime = machineResource.laserPowerParams.gasStartupAdvanceTime;
        writeFunction = "gasWaitTime =" + std::to_string(gasWaitTime);
        writeFunction +=
        R"(
        function arc_start()
            --起弧脚本
            write_integer_register(673, 0) --起弧异常信号清空
            local retry_times = 5
            while true do
                --检测开始信号
                if(read_boolean_register(900)==true) then
                    --送气使能开
                    write_value_port(0x0906, 0x0031, 1, 3, 30)
                    set_configurable_digital_out(0, true)  ---CO0 = 1
                    wait(gasWaitTime) --等待送气结束
                    set_configurable_digital_out(1, true)  ---CO1 = 1
                    print("起弧成功")
                    break
                else
                    retry_times = retry_times - 1
                    if(retry_times < 1) then
                        set_configurable_digital_out(0, false)  ---CO0 = 0
                        set_configurable_digital_out(1, false)  ---CO1 = 0
                        print("起弧失败")
                        --送气使能开
                        write_value_port(0x0906, 0x0031, 0, 3, 30)
                        write_integer_register(673, 1) --起弧异常信号
                        break
                    end
                end
            end
        end
        )";

    }
    else if (laserSwitch == SWITCH_OFF_LASER_SDK)
    {
        gasWaitTime = machineResource.laserPowerParams.gasShutoffDelayTime;
        writeFunction = "gasWaitTime =" + std::to_string(gasWaitTime);
        writeFunction +=
        R"(
        function arc_stop()
            --收弧脚本
            write_integer_register(675, 0) --起弧异常信号清空
            local retry_times = 5
            while true do
                --检测开始信号
                if(read_boolean_register(900)==true) then
                    --送气使能关
                    write_value_port(0x0906, 0x0031, 0, 3, 30)
                    set_configurable_digital_out(1, false)  ---CO1 = 1
                    wait(gasWaitTime) --等待送气结束
                    set_configurable_digital_out(0, false)  ---CO0 = 1
                    print("收弧异常")
                    break
                else
                    retry_times = retry_times - 1
                    if(retry_times < 1) then
                        set_configurable_digital_out(0, false)  ---CO0 = 0
                        set_configurable_digital_out(1, false)  ---CO1 = 0
                        print("收弧异常")
                        --送气使能关
                        write_value_port(0x0906, 0x0031, 0, 3, 30)
                        write_integer_register(675, 1) --收弧异常信号
                        break
                    end
                end
            end
        end
        )";
    }
    script += writeFunction;
    return LaserSdkResult::SUCCESS_LASER_SDK;

}
//485 起收弧运行指令
LaserSdkResult CommuRS485::craft_script_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startSwitch, std::string &script)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    int laserSwitchIsOpen = 0 ;
    //获取脚本
    if (startSwitch == SWITCH_ON_LASER_SDK)
    {
        script += "arc_start()\n";
    }
    else if (startSwitch == SWITCH_OFF_LASER_SDK)
    {
        script += "arc_stop()\n";
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
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
        std::string endstr = "\n";
        if(waitTime !=0)
            endstr = ")\nwait(" + std::to_string(waitTime) + ")\n";

        std::string funcDef = generateLuaScript(funcName, paramName, addr, regType);
        std::ostringstream useScript;
        switch (regType)
        {
        case float10Reg:
            useScript << std::setprecision(1);//保留小数点后1位
            useScript << "write_value_port(0x0906, " << addr << "," << paramValue << "*10, 2, 62)" << endstr;
            break;
        case float100Reg:
            useScript << std::setprecision(2);//保留小数点后2位
            useScript << "write_value_port(0x0906, " << addr << "," << paramValue << "*100, 2, 62)" << endstr;
            break;
        case int16Reg:
            useScript << "write_value_port(0x0906, " << addr << "," << paramValue << ", 2, 62)" << endstr;
            break;
        default:
            break;
        }
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
        std::string endstr = "\n";
        if(waitTime !=0)
            endstr = ")\nwait(" + std::to_string(waitTime) + ")\n";

        std::string funcDef = generateLuaScript(funcName, paramName, addr, regType);
        std::ostringstream useScript;
        switch (regType)
        {
        case float10Reg:
            useScript << std::setprecision(1);//保留小数点后1位
            useScript << "write_value_port(0x0906, " << addr << "," << paramValue << "*10, 2, 62)" << endstr;
            break;
        case float100Reg:
            useScript << std::setprecision(2);//保留小数点后2位
            useScript << "write_value_port(0x0906, " << addr << "," << paramValue << "*100, 2, 62)" << endstr;
            break;
        case int16Reg:
            useScript << "write_value_port(0x0906, " << addr << "," << paramValue << ", 2, 62)" << endstr;
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
                        CommuRegType regType, double paramValue, unsigned int addr = 0xFFFFFFFF, int waitTime = 0)
    {
        //没有写入地址
        if(addr == 0xFFFFFFFF)
            return LaserSdkResult::PARAMETER_ERROR_LASER_SDK;

        //设置等待时间
        std::string endstr = "\n";
        if(waitTime !=0)
            endstr = ")\nwait(" + std::to_string(waitTime) + ")\n";

        std::string funcDef = generateLuaScript(funcName, paramName, addr, regType);
        std::ostringstream useScript;
        switch (regType)
        {
        case float10Reg:
            useScript << std::setprecision(1);//保留小数点后1位
            useScript << "write_value_port(0x0A06, " << addr << "," << paramValue << "*10, 2, 62)" << endstr;
            break;
        case float100Reg:
            useScript << std::setprecision(2);//保留小数点后2位
            useScript << "write_value_port(0x0A06, " << addr << "," << paramValue << "*100, 2, 62)" << endstr;
            break;
        case int16Reg:
            useScript << "write_value_port(0x0A06, " << addr << "," << paramValue << ", 2, 62)" << endstr;
            break;
        default:
            break;
        }
        writeDataHash.emplace(addr, std::make_pair(useScript.str(), funcDef));
    };

    // 送丝机是否开机
    addParam("write_wirefeeder_enable", "wireFeederEnable",
             int16Reg, wireFeederParams.isWireFeederStart, machineResource.wireFeederStatusLoc);

    // 是否保存当前送丝机数据 地址 0x013F → ​​319​
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
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    CraftInfoLaserSdk curCraftInfo = craftInfo;
    ///激光参数赋值
    std::string write_laser_params;
    write_laser_params += "    laserRegisterMap[1][2] = "+ std::to_string(curCraftInfo.craftLaserPower)+"\n";   //激光功率
    write_laser_params += "    laserRegisterMap[2][2] = "+ std::to_string(curCraftInfo.craftLeadInPower)+"\n";  //开光功率
    write_laser_params += "    laserRegisterMap[3][2] = "+ std::to_string(curCraftInfo.craftLeadInRampTime)+"\n";  //开光功率上升时间
    write_laser_params += "    laserRegisterMap[4][2] = "+ std::to_string(curCraftInfo.craftLeadOutPower)+"\n";    //关光功率
    //计算关光功率下降时间
    int tempTime = curCraftInfo.craftLeadOutRampDistance / curCraftInfo.craftWeldingSpeed;
    write_laser_params += "    laserRegisterMap[5][2] = "+ std::to_string(tempTime)+"\n";  //关光功率下降时间
    write_laser_params += "    laserRegisterMap[6][2] = "+ std::to_string(curCraftInfo.craftPreGasTime)+"\n";      //提前送气时间
    write_laser_params += "    laserHeadRegisterMap[7][2] = "+ std::to_string(curCraftInfo.craftPostGasTime)+"\n"; //气体关闭延时 --滞后送气时间
    //YXYX轴偏移量
    write_laser_params += "    locationCalibration[9][2] = "+ double2str(curCraftInfo.craftYAxisZoom, 2)+"*100 \n";        //激光头Y轴缩放
    write_laser_params += "    locationCalibration[10][2] = "+ double2str(curCraftInfo.craftYAxisOffset, 2)+"*100 \n";     //激光头Y轴偏移
    write_laser_params += "    locationCalibration[11][2] = "+ double2str(curCraftInfo.craftXAxisZoom, 2)+"*100 \n";       //激光头X轴缩放
    write_laser_params += "    locationCalibration[12][2] = "+ double2str(curCraftInfo.craftXAxisOffset, 2)+"*100 \n";     //激光头X轴偏移
    //点焊工艺
    write_laser_params += "    spotWeld[1][2] = "+ std::to_string(static_cast<int>(curCraftInfo.craftWeldType))+"\n"; //点焊类型
    write_laser_params += "    spotWeld[2][2] = "+ std::to_string(curCraftInfo.craftSpotWeldDuration)+"\n"; //点焊时长
    write_laser_params += "    spotWeld[3][2] = "+ std::to_string(curCraftInfo.craftSpotWeldInterval)+"\n"; //点焊间隔
    // 送丝机参数
    write_laser_params += "    wireRegisterMap[1][2] = "+ std::to_string(curCraftInfo.craftEndingWireRetraction)+"\n"; //回抽长度
    write_laser_params += "    wireRegisterMap[2][2] = "+ std::to_string(curCraftInfo.craftEndLoopbackLength)+"\n"; //补丝长度
    write_laser_params += "    laserHeadRegisterMap[1][2] = "+ std::to_string(curCraftInfo.craftScanSpeed)+"\n"; //激光头扫描速度
    write_laser_params += "    laserHeadRegisterMap[2][2] = "+ double2str(curCraftInfo.craftScanSpeed, 1)+"*10 \n";  //激光头扫描宽度
    write_laser_params += "    laserHeadRegisterMap[7][2] = "+ double2str(curCraftInfo.craftScanSpeed, 1)+"*10 \n";  //激光头扫描高度
    write_laser_params += "    laserHeadRegisterMap[8][2] = "+ std::to_string(curCraftInfo.craftScanSpeed)+"\n"; //激光头旋转角度
    // 写入送丝机速度
    write_laser_params += writeLuaScript("0x0A06", "0x0100", std::to_string(curCraftInfo.craftLeadInWireFeed), "2", "99");
    // 写入焊接模式
    write_laser_params += writeLuaScript("0x0906", "0x0202", std::to_string(static_cast<int>(curCraftInfo.craftweldMode)), "3", "99");
    ///< 写入激光头参数
    if(curCraftInfo.graphicsSwitching != machineResource.laserHeadParams.laserSpotPattern)
    {
        // 写入图案
        machineResource.laserHeadParams.laserSpotPattern = curCraftInfo.graphicsSwitching;
        write_laser_params += writeLuaScript("0x0906", "0x0030", std::to_string(curCraftInfo.graphicsSwitching), "3", "99");//激光头图形切换
    }
    //写入激光焊接参数、点焊参数、丝机参数
    std::string write_weld_params = R"(
comNo = 3
slave_addr = 0x09  
SendDataType = 0         --bool位寄存器, 通过bool寄存器发送字节数据
SendCommuRegAddr = 0	 --发送指令从bool通讯变量的 0 地址开始存储
write_multi_value_port(laserRegisterMap)      --写入激光焊接参数
write_multi_value_port(laserHeadRegisterMap)  --写入激光头
write_multi_value_port(spotWeld)              --写入点焊参数
comNo = 2
slave_addr = 0x0A    --从机地址
write_multi_value_port(wireRegisterMap)     --写入丝机参数
    )";
    script += write_laser_params;
    script += write_weld_params;

    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//激光头对应参数读取
LaserSdkResult CommuRS485::craft_laser_head_param_read_script_get(LaserMachineHandle laserMachineHandle, std::string &script)
{
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    //CgxiLaserHeadLaserSdk &laserHeadParams  = machineResource.laserHeadParams;
    std::stringstream use_write_data;
    use_write_data << "read_scanning_speed()\n";
    script += use_write_data.str();
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
