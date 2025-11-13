#include "cl_craftscript.h"
#include "cl_api.h"
#include "cl_utils.h"
#include "cl_luascript.h"
#include "cl_comandlistscript.h"
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
//RS485单例
CommuRS485 *CommuRS485::s_commuRS485 = nullptr;
std::once_flag CommuRS485::initFlag;


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
    //static std::string currentLuaScript = std::move(loadLuaFunctionScript);
    //loadLuaFunctionScript = ""; //清空
    return tmpLuaStartDataScript + loadLuaFunctionScript + tmpLuaScript ;
}

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
        [115] = {0x0115, 0},    --送丝机运行状态
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

    startData = R"(
    local SendDataType = 0 --bool位寄存器 通过bool寄存器发送字节数据
    local RecvDataType = 1 --16位寄存器 接收到的数据放到16位寄存器中
    local SendCommuRegAddr = 0		--发送指令从bool通讯变量的 0 地址开始存储
    local SendCommuReg_n = 16		--发送指令从bool通讯变量的 0 地址开始存储
    local RecvCommuRegAddr = 30		--接收的数据从16位通讯变量的 30 地址开始存储
    local RecvCommuReg_n = 60		--接收30个字节
    local alarm_bits_list = {0,1,2,4,5,10,11,14}		--接收50个字节
    local flag = 0  --标识位
    )";

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
        local low_byte = value & 0xFF
        -- 获取高字节 (右移8位)
        local high_byte = value >> 8
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
        -- 如果数值小于0,则转换为无符号数计算
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
            wait(20)
            return
        else
            serial_send_receive(comNo, CommuReg_n, 0, SendCommuRegAddr,RecvDataType,RecvCommuRegAddr, 1)
        end
        local tryTimes = 3
        local timeout = 1000
        while(read_int16_register(RecvCommuRegAddr) == 0) do
            wait(10)
            timeout = timeout - 10
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
        print("发送报文："..mes_string)
        --8. 发送报文
        local timeout = 1000
        local tryTimes = 3
        --端口--写寄存器个数--寄存器类型--写起始地址--读取存放寄存器类型--读起始地址--是否需要回执
        serial_send_receive(comNo, CommuReg_n, 0, SendCommuRegAddr, RecvDataType, RecvCommuRegAddr, 1)
        while true do
            wait(10)
            timeout = timeout - 10
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
            end
        end
    end
    )";

    read_value_port =
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

     hex_to_bytes =
        "local function hex_to_bytes(hex_str)\n"
        "    local bytes = {}\n"
        "    for byte in hex_str:gmatch(\"%x%x\") do\n"
        "        table.insert(bytes, tonumber(byte, 16))\n"
        "    end\n"
        "    return bytes\n"
        "end\n";

     swap_bytes_to_table =
        "local function swap_bytes_to_table(bytes)\n"
        "    local swapped = {}\n"
        "    for i = 1, #bytes, 2 do\n"
        "        local b1 = bytes[i]\n"
        "        local b2 = bytes[i+1]\n"
        "        if b2 then\n"
        "            table.insert(swapped, b2)\n"
        "            table.insert(swapped, b1)\n"
        "        else\n"
        "            table.insert(swapped, b1)\n"
        "        end\n"
        "    end\n"
        "    return swapped\n"
        "end\n";

     get_swap_report =
        "local function get_swap_report(hex_str)\n"
        "    local bytes = hex_to_bytes(hex_str)\n"
        "\n"
        "    -- 第一步：高低位交换\n"
        "    local swapped = swap_bytes_to_table(bytes)\n"
        "	\n"
        "    -- 第二步：去掉前三个字节\n"
        "    local data = {}\n"
        "    for i = 4, #swapped do\n"
        "		--print(\"报文:\", swapped[i])\n"
        "        table.insert(data, swapped[i])\n"
        "    end\n"
        "	return data\n"
        "end\n";

     int16_to_hex =
        "local function int16_to_hex(val)\n"
        "    -- 转成无符号16位\n"
        "    if val < 0 then\n"
        "        val = val + 0x10000\n"
        "    end\n"
        "    local high = (val >> 8) & 0xFF\n"
        "    local low  = val & 0xFF\n"
        "    return string.format(\"%02X%02X\", high, low)\n"
        "end\n";

     read_registers_as_hex =
        "local function read_registers_as_hex(start_reg, end_reg)\n"
        "    local report = {}\n"
        "    for i = start_reg, end_reg do\n"
        "        local value = read_int16_register(i)  -- 假设这个函数已存在\n"
        "        local hex_str = int16_to_hex(value)\n"
        "        table.insert(report, hex_str)\n"
        "    end\n"
        "    return table.concat(report)\n"
        "end\n";

    get_register_value =
        "function get_register_value(data, reg_index)\n"
        "	-- 第三步：按寄存器取值\n"
        "    local pos = (reg_index - 1) * 2 + 1\n"
        "    if pos + 1 > #data then\n"
        "        print(\"寄存器索引超出范围\")\n"
        "		return nil\n"
        "    end\n"
        "\n"
        "    local high = data[pos]\n"
        "    local low  = data[pos + 1]\n"
        "    local value = (high << 8) | low\n"
        "    if value >= 0x8000 then\n"
        "        value = value - 0x10000\n"
        "    end\n"
        "    return value\n"
        "end\n";

     parse_alarm_word =
        "function parse_alarm_word(value)\n"
        "    if value == 0 then\n"
        "		write_integer_register(803, value)\n"
        "        return\n"
        "    end\n"
        "    for idx, i in ipairs(alarm_bits_list) do\n"
        "        if (value >> i) & 0x1 == 1 then\n"
        "            write_integer_register(803+idx, 1)\n"
        "        end\n"
        "    end\n"
        "end\n";

    loadLuaFunctionScript = commandListStartData + startData + swap_bytes + modbus_crc16 + lua_split + register_clear + lua_data_get
     + concatenate_crc + parse_little_endian_bytes + writeHex2Commu_reg + task_write_port
     + read_value_port + get_register_value + parse_alarm_word;
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
            if read_integer_register(821) ~= 0 then
                print(string.format("收弧异常，超时 : 0x%04X", read_integer_register(821)))
                write_integer_register(675, 1)
                return
            end
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
    CgxiLaserPowerLaserSdk &laserPowerParams = machineResource.laserPowerParams;
    std::unordered_map<unsigned int, std::string> writeDataHash;
    //参数类型 - 参数值 - 地址
    auto addParam = [&](CommuRegType regType, double paramValue, unsigned int addr)
    {
        std::string endstr = "\n";
        std::ostringstream useScript;
        switch (regType)
        {
        case float10Reg:
            useScript << std::setprecision(1);  //保留小数点后1位
            useScript << "write_value_port(0x0906, " << addr << "," << paramValue << "*10, 3, SendCommuRegAddr,1)" << endstr;
            break;
        case float100Reg:
            useScript << std::setprecision(2);  //保留小数点后2位
            useScript << "write_value_port(0x0906, " << addr << "," << paramValue << "*100, 3, SendCommuRegAddr,1)" << endstr;
            break;
        case int16Reg:
            useScript << "write_value_port(0x0906, " << addr << "," << paramValue << ", 3, SendCommuRegAddr,1)" << endstr;
            break;
        }
        writeDataHash.emplace(addr, useScript.str());
    };
    // 1. 写入激光器功率 (0-5000W, 整型)
    addParam( int16Reg, laserPowerParams.laserPower, machineResource.laserPowerLoc);
    // 2. 写入开光功率 (0-5000W, 整型)
    addParam( int16Reg, laserPowerParams.laserPowerOpen, machineResource.laserOnPowerLoc);
    // 3. 写入开光渐进时间 (0~1000ms, 整型)
    addParam( int16Reg, 0, machineResource.laserOnRampTimeLoc);
    // 4. 写入关光功率 (0-5000W, 整型)
    addParam( int16Reg, laserPowerParams.laserPowerClose, machineResource.laserOffPowerLoc);
    // 5. 写入关光渐进时间 (0~300mm, 整型)
    addParam( int16Reg, 0, machineResource.laserOffRampTimeLoc);
    // 6. 写入开气延时函数 提前送气时间	0~3000ms
    addParam( int16Reg, laserPowerParams.gasStartupAdvanceTime, machineResource.gasONDelayLoc);
    // 7. 写入关气延时函数
    addParam( int16Reg, laserPowerParams.gasShutoffDelayTime, machineResource.gasOffDelayLoc);
    // 8. 写入送丝延时函数
    addParam( int16Reg, laserPowerParams.wireFeedAdvanceTime, machineResource.wireOutDelayLoc);
    // 9. 写入驱动器温度报警阈值函数
    addParam( int16Reg, laserPowerParams.driverTemp, machineResource.driverTempLoc);
    //10. 写入保护镜报警阈值
    addParam( int16Reg, laserPowerParams.protectiveWindowTemp, machineResource.protectiveWindowTempLoc);
    //11. 写入准直镜温度报警阈值
    addParam( int16Reg, laserPowerParams.collimatorTemp, machineResource.collimatorTempLoc);
    //12. 写入X偏移
    addParam( float100Reg, laserPowerParams.laserHeadXOffset, machineResource.laserHeadXOffsetLoc);
    //13. 写入Y偏移
    addParam( float100Reg, laserPowerParams.laserHeadYOffset, machineResource.laserHeadYOffsetLoc);
    //14. 写入X放大
    addParam( float100Reg, laserPowerParams.laserHeadXEnlarge, machineResource.laserHeadXScaleLoc);
    //15. 写入Y放大
    addParam( float100Reg, laserPowerParams.laserHeadYEnlarge, machineResource.laserHeadYScaleLoc);
    //16. 写入点焊类型
    addParam( int16Reg, static_cast<int>(laserPowerParams.spotWeldType), machineResource.spotWeldTypeLoc);
    //17. 写入点焊持续时间
    addParam( int16Reg, laserPowerParams.spotWeldDurationTime, machineResource.spotWeldDurationLoc);
    //18. 写入点焊间隔时间
    addParam( int16Reg, laserPowerParams.spotWeldIntervalTime, machineResource.spotWeldIntervalLoc);
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

LaserSdkResult CommuRS485::craft_laser_mul_params_set_script_get(LaserMachineHandle laserMachineHandle, std::string &script)
{
    // 获取激光机资源
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return LaserSdkResult::HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    std::unordered_map<uint16_t, uint16_t> crc16_modbus_map;
    try {
        // 激光器参数校验
        crc16_modbus_map.emplace(machineResource.laserPowerLoc, machineResource.laserPowerParams.laserPower);   //激光功率
        crc16_modbus_map.emplace(machineResource.gasONDelayLoc, machineResource.laserPowerParams.gasStartupAdvanceTime);   //提前送气时间
        crc16_modbus_map.emplace(machineResource.gasOffDelayLoc, machineResource.laserPowerParams.gasShutoffDelayTime);   //滞后送气时间
        crc16_modbus_map.emplace(machineResource.laserHeadYScaleLoc, machineResource.laserPowerParams.laserHeadYEnlarge);   //激光头Y轴缩放
        crc16_modbus_map.emplace(machineResource.laserHeadYOffsetLoc, machineResource.laserPowerParams.laserHeadYOffset);   //激光头Y轴偏移
        crc16_modbus_map.emplace(machineResource.laserHeadXScaleLoc, machineResource.laserPowerParams.laserHeadXEnlarge);   //激光头X轴缩放
        crc16_modbus_map.emplace(machineResource.laserHeadXOffsetLoc, machineResource.laserPowerParams.laserHeadXOffset);   //激光头X轴偏移
    }
    catch (std::out_of_range) {

    }
    // 生成脚本
    std::string useScript = R"(
    --激光器参数
    local laserRegisterMap = {
    [1] = {0x0230, 2000},  -- 激光功率
    [6] = {0x0235, 2000},     -- 气体打开延时 --提前送气时间
    [7] = {0x0236, 1000},     -- 气体关闭延时 --滞后送气时间

    [9] = {0x0238, 0},    -- 激光头Y轴缩放
    [10] = {0x0239, 0},   -- 激光头Y轴偏移
    [11] = {0x023A, 0 },  -- 激光头X轴缩放
    [12] = {0x023B, 0 },  -- 激光头X轴偏移
    })";


    return LaserSdkResult();
}

//激光头图案
LaserSdkResult CommuRS485::craft_laser_head_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string &script)
{
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    CgxiLaserHeadLaserSdk &laserHeadParams = machineResource.laserHeadParams;

    std::unordered_map<unsigned int, std::string> writeDataHash;
    auto addParam = [&](CommuRegType regType, double paramValue, unsigned int addr = 0xFFFFFFFF, int waitTime = 0)
    {
        //没有写入地址
        if(addr >= 0xFFFF)
            return LaserSdkResult::PARAMETER_ERROR_LASER_SDK;
        std::string endstr = "\n";
        std::ostringstream useScript;
        switch (regType)
        {
        case float10Reg:
            useScript << std::setprecision(1);//保留小数点后1位
            useScript << "write_value_port(0x0906, " << addr << "," << paramValue << "*10, 3, SendCommuRegAddr,1))" << endstr;
            break;
        case float100Reg:
            useScript << std::setprecision(2);//保留小数点后2位
            useScript << "write_value_port(0x0906, " << addr << "," << paramValue << "*100, 3, SendCommuRegAddr,1))" << endstr;
            break;
        case int16Reg:
            useScript << "write_value_port(0x0906, " << addr << "," << paramValue << ", 3, SendCommuRegAddr,1)" << endstr;
            break;
        }
        writeDataHash.emplace(addr, useScript.str());
    };
    ///<01. 送气信号开关
    addParam(int16Reg, static_cast<int>(laserHeadParams.WireFeederGasSignalSwitch), machineResource.gasValveSignalLoc);
    ///<02. 送丝信号开关
    addParam(int16Reg, static_cast<int>(laserHeadParams.WireFeederSignalSwitch), machineResource.wireFeedSignalLoc);
    // 1. 写入扫描速度 (0~30000mm/s,)
    addParam(int16Reg, laserHeadParams.laserHeadScanSpeed, machineResource.laserHeadScanSpeedLoc);
    // 2. 写入扫描宽度 (0.1~5mm, 浮点型)
    addParam(float10Reg, laserHeadParams.laserHeadScanWidth, machineResource.laserHeadScanWidthLoc);
    // 3. 写入峰值功率 (0-3000W, 整型)
    addParam(int16Reg, laserHeadParams.laserHeadPeakPower, machineResource.laserHeadPeakPowerLoc);
    // 4. 写入占空比 (0~100, 整型)
    addParam(int16Reg, laserHeadParams.laserHeadDutyCycle, machineResource.laserHeadDutyCycleLoc);
    // 7. 写入扫描高度 (0.1~5mm, 浮点型)
    addParam(float10Reg, laserHeadParams.laserHeadScanHeight, machineResource.laserHeadScanHeightLoc);
    // 8. 写入旋转角度 (0~360°, 整型)
    addParam(int16Reg, laserHeadParams.laserHeadSpinAngle, machineResource.laserHeadRotationAngleLoc);
    // 9. 写入图形（特殊处理）图形
    addParam(int16Reg, static_cast<int>(laserHeadParams.laserSpotPattern), machineResource.laserHeadPatternIdLoc);
    //脉冲频率低地址 (5~100000HZ, 整型)
    addParam(int16Reg, laserHeadParams.laserHeadLowPulseFrequency, machineResource.laserHeadFrequencyLowLoc);
    //写入脉冲频率高地址 (5~100000HZ, 整型)
    addParam(int16Reg, laserHeadParams.laserHeadHighPulseFrequency, machineResource.laserHeadFrequencyHighLoc);

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
    CgxiWireFeederLaserSdk &wireFeederParams = machineResource.wireFeederParams;
    // 存储所有函数定义和调用脚本的数据结构
    std::unordered_map<unsigned int, std::string> writeDataHash;
    auto addParam = [&](CommuRegType regType, double paramValue, unsigned int addr, int receiptNeeded)
    {
        //无效写入地址
        if(addr >= 0xFFFF)
            return LaserSdkResult::PARAMETER_ERROR_LASER_SDK;
        std::string endstr = "\n";
        std::ostringstream useScript;
        switch (regType)
        {
        case float10Reg:
            useScript << std::setprecision(1);//保留小数点后1位
            useScript << "write_value_port(0x0A06, " << addr << "," << paramValue << "*10, 2, SendCommuRegAddr," << receiptNeeded << ")" << endstr;
            break;
        case float100Reg:
            useScript << std::setprecision(2);//保留小数点后2位
            useScript << "write_value_port(0x0A06, " << addr << "," << paramValue << "*100, 2, SendCommuRegAddr," << receiptNeeded << ")" << endstr;
            break;
        case int16Reg:
            useScript << "write_value_port(0x0A06, " << addr << "," << paramValue << ", 2, SendCommuRegAddr," << receiptNeeded << ")" << endstr;
            break;
        }
        writeDataHash.emplace(addr, useScript.str());
        return LaserSdkResult::SUCCESS_LASER_SDK;
    };

    //判断是否需要送丝
    if (machineResource.weldingProcessType == WeldingProcessType::SELF_WELDING_LASERSDK)
    {   //自融焊
        addParam(int16Reg, 0, 0x0115, 0);  //自动送丝暂停
    }
    else
    {   //送丝焊
        addParam(int16Reg, 1, 0x0115, 0);  //自动送丝恢复
    }

    // 0x0100 自动送丝速度
    addParam(int16Reg, wireFeederParams.wireFeederMotorSpeed, machineResource.autoWireFeedSpeedLoc, 1);
    // 0x0101手动送丝速度
    addParam(int16Reg, wireFeederParams.wireFeederManualFeedSpeed, machineResource.manualWireFeedSpeedLoc, 1);
    // 0x0102手动抽丝速度
    addParam(int16Reg, wireFeederParams.wireFeederManualBackSpeed, machineResource.manualWireRetractSpeedLoc, 1);
    // 0x0105 补丝长度
    addParam(int16Reg, wireFeederParams.wireFeederFeedLength, machineResource.wireFeedLengthLoc, 1);
    // 0x0104 抽丝长度
    addParam(int16Reg, wireFeederParams.wireFeederBackLength, machineResource.wireRetractLengthLoc, 1);
    // 0x0106 补丝延时
    addParam(int16Reg, wireFeederParams.wireFeederWaitTime, machineResource.wireFeedBackDelayLoc, 1);
    // 0x010D 断丝模式
    addParam(int16Reg, static_cast<int>(wireFeederParams.craftEndingwireRetraction), machineResource.breakWireFeedingModeLoc, 1);
    // 0x0140 手动送丝开关
    addParam(int16Reg, static_cast<int>(wireFeederParams.isWireFeederManualFeedSwitch), machineResource.manualWireFeedSwitchLoc, 1);
    // 0x0141 手动回抽开关
    addParam(int16Reg, static_cast<int>(wireFeederParams.isWireFeederManualRetractSwitch), machineResource.manualWireRetractSwitchLoc, 1);
    // 0x013F 是否保存当前送丝机数据 地址  → ​​319​
    addParam(int16Reg, wireFeederParams.isWireFeederDataSave, machineResource.saveCurrentWireFeederDataLoc, 1);
    for (unsigned int addr : locVec)
    {
        auto it = writeDataHash.find(addr);
        if (it != writeDataHash.end())
        {
            script += it->second;
        }
        else
        {
            std::string tmp;
            writeLog("craft_wire_feeder_param_set_script_get : Missing Lua script for wire feeder address 0x" + std::to_string(addr));
            std::cout << "Warning: Missing Lua script for wire feeder address 0x"
                      << std::hex << addr << std::dec << std::endl;
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

    CraftInfoLaserSdk curCraftInfo = craftInfo;
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
    std::string write_weld_params = R"(
    comNo = 3
    slave_addr = 0x09
    write_multi_value_port(laserRegisterMap)      --写入激光焊接参数
    write_multi_value_port(laserHeadRegisterMap)  --写入激光头
    write_multi_value_port(spotWeld)              --写入点焊参数
    )";

    ///< 写入焊接模式
    if(curCraftInfo.craftweldMode != machineResource.laserPowerParams.weldMode)
    {
        write_laser_params += writeLuaScript("0x0906", std::to_string(machineResource.weldingModeLoc), std::to_string(static_cast<int>(curCraftInfo.craftweldMode)), "3", "SendCommuRegAddr", "1");
        machineResource.laserPowerParams.weldMode = curCraftInfo.craftweldMode;
    }

    ///< 写入激光头参数
    if(curCraftInfo.graphicsSwitching != machineResource.laserHeadParams.laserSpotPattern)
    {
        // 写入图案
        machineResource.laserHeadParams.laserSpotPattern = curCraftInfo.graphicsSwitching;
        switch (curCraftInfo.graphicsSwitching) {
            case SpotPatternLaserSdk::CIR_LASERSDK:
            case SpotPatternLaserSdk::HORIZON_8_LASERSDK:
            case SpotPatternLaserSdk::LINE_LASERSDK:
            case SpotPatternLaserSdk::STAR_LASERSDK:
                write_laser_params += writeLuaScript("0x0906", std::to_string(machineResource.laserHeadPatternIdLoc), std::to_string(curCraftInfo.graphicsSwitching), "3", "SendCommuRegAddr", "1");
            break;
            case SpotPatternLaserSdk::TRIANGLE_LASERSDK:
                write_laser_params += writeLuaScript("0x0906", std::to_string(machineResource.laserHeadPatternIdLoc), std::to_string(5), "3", "SendCommuRegAddr", "1");
            break;
            case SpotPatternLaserSdk::CROSS_LASERSDK:
                write_laser_params += writeLuaScript("0x0906", std::to_string(machineResource.laserHeadPatternIdLoc), std::to_string(4), "3", "SendCommuRegAddr", "1");
            break;
        }
    }
    bodyScriptText += strWriteInt32RegReplace(821, 0);  //存放超时地址的寄存器
    bodyScriptText += write_laser_params;
    bodyScriptText += write_weld_params;
    // 送丝机参数
    //计算焊丝回抽的距离
    //double backVelocity = (curCraftInfo.craftEndingWireRetraction * 100) / 60; //收尾抽丝速度 cm/min换算成 mm/s
    int distance = curCraftInfo.craftRetractionDistance;            //回抽焊丝的过程距离 mm
    machineResource.wireFeederParams.lengthBack = distance;
    //计算补丝延迟
    int wireDelayTime = static_cast<int>((distance / curCraftInfo.craftWeldingSpeed)*1000); //计算补丝延迟,s换算成ms
    if ( wireDelayTime > 2000)
    {
        writeLog("补丝延迟时间过大");
        //return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }    
    //  回抽长度panduan
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

    machineResource.wireFeederParams.craftEndingwireRetraction = curCraftInfo.craftEndingwireRetraction; ///<断丝模式
    //送丝机地址参数 - 自动送丝速度 - 收尾抽丝
    std::vector<unsigned int> wireFeedVec =
    {
        machineResource.autoWireFeedSpeedLoc,       ///<  自动送丝速度地址 0x0100 256
        machineResource.wireRetractLengthLoc,       ///<  抽丝长度地址 0x0104 260
        machineResource.wireFeedLengthLoc,          ///<  补丝长度地址 0x0105 261
        //machineResource.wireFeedBackDelayLoc,       ///<  补丝延迟时间地址 0x0106 暂时开放
        machineResource.breakWireFeedingModeLoc ,      ///< 断丝模式地址 0x010D  (控制回抽速度)
    };
    craft_wire_feeder_param_set_script_get(laserMachineHandle, wireFeedVec, bodyScriptText);

    return LaserSdkResult::SUCCESS_LASER_SDK;
}

LaserSdkResult CommuRS485::craft_script_laser_value_check_script_get(LaserMachineHandle laserMachineHandle, std::string &script)
{
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    CgxiLaserPowerLaserSdk &laserPowerParams = machineResource.laserPowerParams;

    // 激光器参数校验
    if (laserPowerParams.laserPower < 0 || laserPowerParams.laserPower > 5000)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserPowerParams.laserPowerOpen < 0 || laserPowerParams.laserPowerOpen > 5000)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserPowerParams.laserRampUpTime < 0 || laserPowerParams.laserRampUpTime > 1000)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserPowerParams.laserPowerClose < 0 || laserPowerParams.laserPowerClose > 5000)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserPowerParams.driverTemp < 0 || laserPowerParams.driverTemp > 85)
        return PARAMETER_ERROR_LASER_SDK;//    驱动器温度报警阈值
    if (laserPowerParams.protectiveWindowTemp < 0 || laserPowerParams.protectiveWindowTemp > 85)
        return PARAMETER_ERROR_LASER_SDK;//    保护镜温度报警阈值
    if (laserPowerParams.collimatorTemp < 0 || laserPowerParams.collimatorTemp > 85)
        return PARAMETER_ERROR_LASER_SDK;//    准直镜温度报警阈值
    if (laserPowerParams.laserHeadXOffset < -3 || laserPowerParams.laserHeadXOffset > 3)
        return PARAMETER_ERROR_LASER_SDK;//    X偏移 -3~+3mm
    if (laserPowerParams.laserHeadYOffset < -3 || laserPowerParams.laserHeadYOffset > 3)
        return PARAMETER_ERROR_LASER_SDK;//    Y偏移 -3~+3mm
    if (laserPowerParams.laserHeadXEnlarge < 0.01 || laserPowerParams.laserHeadXEnlarge > 4)
        return PARAMETER_ERROR_LASER_SDK;//    X放大 0.01~4
    if (laserPowerParams.laserHeadYEnlarge < 0.01 || laserPowerParams.laserHeadYEnlarge > 4)
        return PARAMETER_ERROR_LASER_SDK;//    Y放大 0.01~4
    if (laserPowerParams.spotWeldDurationTime < 0 || laserPowerParams.spotWeldDurationTime > 60000)
        return PARAMETER_ERROR_LASER_SDK;//    点焊持续时间 0~60000ms
    if (laserPowerParams.spotWeldIntervalTime < 0 || laserPowerParams.spotWeldIntervalTime > 60000)
        return PARAMETER_ERROR_LASER_SDK;//    点焊间隔时间 0~60000ms
    return SUCCESS_LASER_SDK;
}

LaserSdkResult CommuRS485::craft_script_head_value_check_script_get(LaserMachineHandle laserMachineHandle)
{
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    CgxiLaserHeadLaserSdk &laserHeadParams = machineResource.laserHeadParams;
    // 激光头参数检查
    if (laserHeadParams.laserHeadScanSpeed < 0 || laserHeadParams.laserHeadScanSpeed > 30000)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserHeadParams.laserHeadScanWidth < 0.1 || laserHeadParams.laserHeadScanWidth > 5)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserHeadParams.laserHeadPeakPower < 0 || laserHeadParams.laserHeadPeakPower > 3000)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserHeadParams.laserHeadDutyCycle < 0 || laserHeadParams.laserHeadDutyCycle > 100)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserHeadParams.laserHeadScanHeight < 0.1 || laserHeadParams.laserHeadScanHeight > 5)
        return PARAMETER_ERROR_LASER_SDK;
    if (laserHeadParams.laserHeadSpinAngle < 0 || laserHeadParams.laserHeadSpinAngle > 360)
        return PARAMETER_ERROR_LASER_SDK;

    return SUCCESS_LASER_SDK;
}

LaserSdkResult CommuRS485::craft_script_wire_value_check_script_get(LaserMachineHandle laserMachineHandle, std::string &script)
{
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    CgxiWireFeederLaserSdk &wireFeederParams = machineResource.wireFeederParams;
    //送丝机参数检查
    if (wireFeederParams.wireFeederMotorSpeed < 0 || wireFeederParams.wireFeederMotorSpeed > 600)
    {
        std::cout << "wireFeederParams.wireFeederMotorSpeed wrong" << std::endl;
        return PARAMETER_ERROR_LASER_SDK;
    }
    if (wireFeederParams.wireFeederStartMotorSpeed < 0 || wireFeederParams.wireFeederStartMotorSpeed > 600)
    {
        std::cout << "wireFeederParams.wireFeederStartMotorSpeed wrong" << std::endl;
        return PARAMETER_ERROR_LASER_SDK;
    }
    if (wireFeederParams.wireFeederStopMotorSpeed < 0 || wireFeederParams.wireFeederStopMotorSpeed > 600)
    {
        std::cout << "wireFeederParams.wireFeederStopMotorSpeed wrong" << std::endl;
        return PARAMETER_ERROR_LASER_SDK;
    }
    if (wireFeederParams.wireFeederManualBackSpeed < 0 || wireFeederParams.wireFeederManualBackSpeed > 600)
        return PARAMETER_ERROR_LASER_SDK;
    if (wireFeederParams.wireFeederFeedLength < 0 || wireFeederParams.wireFeederFeedLength > 100)
        return PARAMETER_ERROR_LASER_SDK;
    if (wireFeederParams.wireFeederBackLength < 0 || wireFeederParams.wireFeederBackLength > 100)
        return PARAMETER_ERROR_LASER_SDK;
    if (wireFeederParams.wireFeederWaitTime < 0 || wireFeederParams.wireFeederWaitTime > 2000)
        return PARAMETER_ERROR_LASER_SDK;
    return SUCCESS_LASER_SDK;
}

//单词状态监控
LaserSdkResult CommuRS485::cl_craft_weld_status_monitor_single_execute_script_get(LaserMachineHandle laserMachineHandle,  std::string &script)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    script += "\n status_monitor_single()\n";
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//单词状态监控脚本函数
LaserSdkResult CommuRS485::cl_craft_weld_status_monitor_single_script_get(LaserMachineHandle laserMachineHandle,  std::string &script)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    try
    {
        std::string arcStartProgram1 = "function status_monitor_single()\n";
        std::string scriptGet=
                "    read_value_port(0x0903, " + std::to_string(machineResource.laserHeadStatusLoc) + " , 0x0003, 3, 0, 16)\n"
                "    local report = read_registers_as_hex(30, 60)\n"
                "    print(\"Concatenated message:\", report)\n"
                "    local swapReport = get_swap_report(report)\n"
                "    local val = get_register_value(swapReport,1)\n"
                "    print(\"Laser system not enabled status:\", val)\n"
                "    write_integer_register(801, val)\n"
                "    local val = get_register_value(swapReport,2)\n"
                "    print(\"General alarm status:\", val)\n"
                "    write_integer_register(802, val)\n"
                "    local val = get_register_value(swapReport,3)\n"
                "    parse_alarm_word(val)\n"
                "    print(\"General alarm status 2:\", val)\n"
                "\n"
                "    -- Laser trigger status, Laser alarm status, Safety lock input status, Air pressure alarm input status, Wire feed enable status, PWM output status, Gas valve output status, DAC analog output status\n"
                "    read_value_port(0x0903, " + std::to_string(machineResource.laserTriggerSignalLoc) + " , 0x000a, 3, 0, 16)\n"
                "    local report = read_registers_as_hex(30, 60)\n"
                "    print(\"Concatenated message:\", report)\n"
                "    local swapReport = get_swap_report(report)\n"
                "    local val = get_register_value(swapReport,1)\n"
                "    print(\"Laser trigger status:\", val)\n"
                "    write_integer_register(813, val)\n"
                "    local val = get_register_value(swapReport,3)\n"
                "    print(\"Safety lock input status:\", val)\n"
                "    write_integer_register(814, val)\n"
                "    local val = get_register_value(swapReport,6)\n"
                "    print(\"Wire feed enable status:\", val)\n"
                "    write_integer_register(815, val)\n"
                "    local val = get_register_value(swapReport,7)\n"
                "    print(\"PWM output status:\", val)\n"
                "    write_integer_register(816, val)\n"
                "    local val = get_register_value(swapReport,9)\n"
                "    print(\"Gas valve output status:\", val)\n"
                "    write_integer_register(817, val)\n"
                "    local val = get_register_value(swapReport,10)\n"
                "    print(\"DAC analog output status:\", val)\n"
                "    write_integer_register(818, val)\n"
                "\n"
                "    -- Wire feeder connection status\n"
                "    read_value_port(0x0903, " + std::to_string(machineResource.wireFeederConnectedLoc) + ", 0x0001, 3, 0, 16)\n"
                "    local report = read_registers_as_hex(30, 35)\n"
                "    print(\"Concatenated message:\", report)\n"
                "    local swapReport = get_swap_report(report)\n"
                "    local val = get_register_value(swapReport,1)\n"
                "    print(\"Wire feeder connection status:\", val)\n"
                "    write_integer_register(812, val)\n"
                "\n"
                "    -- Laser status, Air pressure status\n"
                "    read_value_port(0x0903, " + std::to_string(machineResource.laserAlarmCodeLoc) + ", 0x0001, 3, 0, 16)\n"
                "    local report = read_registers_as_hex(30, 40)\n"
                "    print(\"Concatenated message:\", report)\n"
                "    local swapReport = get_swap_report(report)\n"
                "    local val = get_register_value(swapReport,1)\n"
                "    print(\"Laser status:\", val)\n"
                "    write_integer_register(819, val)\n"
                "\n"
                "    -- Wire feeder overcurrent, Wire feeder running status\n"
                "    read_value_port(0x0A03," + std::to_string(machineResource.wireFeederStatusLoc) + ",0x0001, 2,  0, 16)\n"
                "    local report = read_registers_as_hex(30, 40)\n"
                "    print(\"Concatenated message:\", report)\n"
                "    local swapReport = get_swap_report(report)\n"
                "    local val = get_register_value(swapReport,1)\n"
                "    print(\"Wire feeder running status:\", val)\n"
                "    write_integer_register(800, val)\n";
        std::string arcStartProgram2 = "end\n";
        script += arcStartProgram1 + scriptGet + arcStartProgram2;
        return LaserSdkResult::SUCCESS_LASER_SDK;
    }
    catch (const LaserSdkResult& errorResult)
    {
        return errorResult;
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

//缓升缓降 起弧/收弧功能脚本
LaserSdkResult CommuRS485::craft_script_slow_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch, std::string &script)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    std::string writeFunction;
    //获取脚本
    if (laserSwitch == SWITCH_ON_LASER_SDK)
    {
        CgxiLaserHeadLaserSdk &laserHeadParams  = machineResource.laserHeadParams;
        CgxiWireFeederLaserSdk &wireFeederParams  = machineResource.wireFeederParams;
        CgxiLaserPowerLaserSdk &laserPowerParams  = machineResource.laserPowerParams;
        int time = laserPowerParams.gasStartupAdvanceTime;
        try
        {
            std::string arcStartProgram1 = "function slow_arc_start()\n";
            
            std::string arcStartParam =
                    "   write_integer_register(673, 0) \n"
                    "   if read_integer_register(821) ~= 0 then \n"
                    "       print(string.format(\"起弧异常，超时 : 0x%04X\", read_integer_register(821))) \n"
                    "       write_integer_register(673, 1) \n"
                    "       pause()  \n"
                    "       wait(1)  \n"
                    "       return  \n"
                    "   end \n"
                    "   laserPower = " + std::to_string(laserHeadParams.laserHeadPeakPower) + "\n" +
                    "   openPower = " + std::to_string(laserPowerParams.laserPowerOpen) + "\n" +
                    "   openLightTime = " + std::to_string(laserPowerParams.laserRampUpTime) + "\n" +
                    "   wirefeedeOpenSendSpeed = " + std::to_string(wireFeederParams.wireFeederStartMotorSpeed) + "\n" +
                    "   wirefeedeWeldSendSpeed = " + std::to_string(wireFeederParams.wireFeederMotorSpeed) + "\n" +
                    "   write_value_port(0x0906," + std::to_string(machineResource.laserHeadPeakPowerQuickLoc) + ", openPower,3,0,1)" + "\n" +
                    "   write_value_port(0x0A06," + std::to_string(machineResource.autoWireFeedSpeedLoc) + ", wirefeedeOpenSendSpeed,2,0,1000)";
            std::ostringstream oss;
            oss << R"(               
            function slow_ascent()
                -- 计算功率差，送丝速度差
                local v = laserPower - openPower
                local s = wirefeedeWeldSendSpeed - wirefeedeOpenSendSpeed

                -- 计算降几次
                local num = openLightTime // 200
                print("num---------",num)
                if num < 1 then
                    --缓升距离过短直接起弧，设置起弧功率和送丝速度即可
                    write_value_port(0x0906,)" + std::to_string(machineResource.laserHeadPeakPowerQuickLoc) + R"(,laserPower,3,0,1)
                    write_value_port(0x0A06,)" + std::to_string(machineResource.autoWireFeedSpeedLoc) + R"(,wirefeedeWeldSendSpeed,2,0,1)
                    return
                end
                -- 计算每次降多少
                local powerSingle = math.floor(v / num)
                local speedSingle = math.floor(s / num)

                -- 循环逐步调整功率和送丝速度
                for i = 1, num do
                    print("power: ", i)
                    write_value_port(0x0906,)" + std::to_string(machineResource.laserHeadPeakPowerQuickLoc) + R"(,openPower + (powerSingle * i),3,0,1)
                    write_value_port(0x0A06,)" + std::to_string(machineResource.autoWireFeedSpeedLoc) + R"(,wirefeedeOpenSendSpeed + (speedSingle * i),2,0,1)
                end
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
            --开气的延迟等待，不需要继续往下走
            wait()" << time << R"()  -- 替换动态变量
            ask_thread = task_create(slow_ascent)
            )";

            std::string arcStartProgram = oss.str();
            std::string arcStartProgram2 = "end\n";
            script += arcStartProgram1 + arcStartParam + arcStartProgram + arcStartProgram2;
            return LaserSdkResult::SUCCESS_LASER_SDK;
        }
        catch (const LaserSdkResult& errorResult)
        {
            return errorResult;
        }
    }
    else if (laserSwitch == SWITCH_OFF_LASER_SDK)
    {

        CgxiLaserHeadLaserSdk &laserHeadParams  = machineResource.laserHeadParams;
        CgxiWireFeederLaserSdk &wireFeederParams  = machineResource.wireFeederParams;
        CgxiLaserPowerLaserSdk &laserPowerParams  = machineResource.laserPowerParams;

        try
        {
            std::string arcStartProgram1 = "    function slow_arc_stop()\n";
            std::string arcStartParam =
                    "   write_integer_register(675, 0) \n"
                    "   if read_integer_register(821) ~= 0 then \n"
                    "       print(string.format(\"收弧异常，超时 : 0x%04X\", read_integer_register(821))) \n"
                    "       write_integer_register(675, 1) \n"
                    "       return \n"
                    "   end \n"
                    "   laserPower = " + std::to_string(laserHeadParams.laserHeadPeakPower) + "\n" +
                    "   closePower  = " + std::to_string(laserPowerParams.laserPowerClose) + "\n" +
                    "   closeLightTime  = " + std::to_string(laserPowerParams.laserRampDownTime) + "\n" +
                    "   if closeLightTime == 0 then \n"
                    "       print(\"closeLightTime = 0\") \n"
                    "       return \n"
                    "   end \n"
                    "   wirefeedeCloseSendSpeed = " + std::to_string(wireFeederParams.wireFeederStopMotorSpeed) + "\n" +
                    "   wirefeedeWeldSendSpeed = " + std::to_string(wireFeederParams.wireFeederMotorSpeed) + "\n" +
                    "   wireDelayTime = " + std::to_string(wireFeederParams.wireFeederWaitTime) + "\n" +
                    "   flag = 0";

            std::string arcStartProgram =
            R"(
                --laserPower, closePower, wirefeedeCloseSendSpeed, wirefeedeWeldSendSpeed, closeLightTime
                function slow_descent()
                    -- 计算功率差，送丝速度差
                    local v = laserPower - closePower
                    local s = wirefeedeWeldSendSpeed - wirefeedeCloseSendSpeed
                    -- 计算降几次
                    local num = closeLightTime // 200
                    print("num2----------",num)
                    if num < 1 then
                        --缓降距离过短直接收弧，设置收弧功率和送丝速度即可
                        write_value_port(0x0906,)" + std::to_string(machineResource.laserHeadPeakPowerQuickLoc) + R"(,laserPower,3,0,1000)
                        write_value_port(0x0A06,)" + std::to_string(machineResource.autoWireFeedSpeedLoc) + R"(,wirefeedeWeldSendSpeed,2,0,1000)
                        return
                    end
                    -- 计算每次降多少
                    local powerSingle = math.floor(v / num)
                    local speedSingle = math.floor(s / num)
                    -- 循环逐步调整功率和送丝速度
                    for i = 1, num do
                        if flag == 1 then
                            break
                        end
                        write_value_port(0x0906,)" + std::to_string(machineResource.laserHeadPeakPowerQuickLoc) + R"(,(laserPower - (powerSingle * i)),3,0,1)
                        if flag == 1 then
                            break
                        end
                        write_value_port(0x0A06,)" + std::to_string(machineResource.autoWireFeedSpeedLoc) + R"(,wirefeedeWeldSendSpeed - (speedSingle * i),2,0,1)
                    end
                    print("缓降结束！")

                end
                task_thread = task_create(slow_descent)
                function timeController()
                    wait(closeLightTime)
                    flag = 1
                    -- 出发送丝机信号开关
                    write_value_port(0x0906,50, 0,3,0,1500)
                end
                task_thread2 = task_create(timeController)
            )";
            std::string arcStartProgram2 = "    end\n";
            script += arcStartProgram1 + arcStartParam + arcStartProgram + arcStartProgram2;
            return LaserSdkResult::SUCCESS_LASER_SDK;
        }
        catch (const LaserSdkResult& errorResult)
        {
            return errorResult;
        }
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
        script += "slow_arc_start()\n";
    }
    else if (startSwitch == SWITCH_OFF_LASER_SDK)
    {
        script += "slow_arc_stop()\n";
    }
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

LaserSdkResult CommuRS485::craft_script_write_craft_resource_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo, std::string &script)
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
    write_laser_params += "    laserHeadRegisterMap[3][2] = "+ std::to_string(curCraftInfo.craftLaserPower)+"\n";  //  峰值功率从开光功率开始升
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
    std::string write_weld_params = R"(
    comNo = 3
    slave_addr = 0x09
    write_multi_value_port(laserRegisterMap)      --写入激光焊接参数
    write_multi_value_port(laserHeadRegisterMap)  --写入激光头
    write_multi_value_port(spotWeld)              --写入点焊参数
    )";

    ///< 写入焊接模式
    if(true)
    {
        write_laser_params += writeLuaScript("0x0906", std::to_string(machineResource.weldingModeLoc), std::to_string(static_cast<int>(curCraftInfo.craftweldMode)), "3", "SendCommuRegAddr", "1");
        machineResource.laserPowerParams.weldMode = curCraftInfo.craftweldMode;
    }

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
                write_laser_params += writeLuaScript("0x0906", std::to_string(machineResource.laserHeadPatternIdLoc), std::to_string(curCraftInfo.graphicsSwitching), "3", "SendCommuRegAddr", "1");
            break;
            case SpotPatternLaserSdk::TRIANGLE_LASERSDK:
                write_laser_params += writeLuaScript("0x0906", std::to_string(machineResource.laserHeadPatternIdLoc), std::to_string(5), "3", "SendCommuRegAddr", "1");
            break;
            case SpotPatternLaserSdk::CROSS_LASERSDK:
                write_laser_params += writeLuaScript("0x0906", std::to_string(machineResource.laserHeadPatternIdLoc), std::to_string(4), "3", "SendCommuRegAddr", "1");
            break;
        }
    }
    script += strWriteInt32RegReplace(821, 0);  //存放超时地址的寄存器
    script += write_laser_params;
    script += write_weld_params;
    // 送丝机参数
    //计算焊丝回抽的距离
    //double backVelocity = (curCraftInfo.craftEndingWireRetraction * 100) / 60; //收尾抽丝速度 cm/min换算成 mm/s
    int distance = curCraftInfo.craftRetractionDistance;            //回抽焊丝的过程距离 mm
    machineResource.wireFeederParams.lengthBack = distance;
    //计算补丝延迟
    int wireDelayTime = static_cast<int>((distance / curCraftInfo.craftWeldingSpeed)*1000); //计算补丝延迟,s换算成ms
    if ( wireDelayTime > 2000)
    {
        writeLog("补丝延迟时间过大");
        //return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }
    //  回抽长度panduan
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

    machineResource.wireFeederParams.craftEndingwireRetraction = curCraftInfo.craftEndingwireRetraction; ///<断丝模式
    //送丝机地址参数 - 自动送丝速度 - 收尾抽丝
    std::vector<unsigned int> wireFeedVec =
    {
        machineResource.autoWireFeedSpeedLoc,       ///<  自动送丝速度地址 0x0100 256
        machineResource.wireRetractLengthLoc,       ///<  抽丝长度地址 0x0104 260
        machineResource.wireFeedLengthLoc,          ///<  补丝长度地址 0x0105 261
        //machineResource.wireFeedBackDelayLoc,       ///<  补丝延迟时间地址 0x0106 暂时开放
        machineResource.breakWireFeedingModeLoc ,      ///< 断丝模式地址 0x010D  (控制回抽速度)
    };
    craft_wire_feeder_param_set_script_get(laserMachineHandle, wireFeedVec, script);

    return LaserSdkResult::SUCCESS_LASER_SDK;
}
