#include "cl_luascript.h"

std::string mainFuncProgram = "function mainFuncProgram()\n";

std::string RobotProgram1 = "function RobotProgram()\n";
std::string RobotProgram2 = "end\n"
                            "-----RobotProgramEnd----\n"
                            "RobotProgram_Result=task_create(RobotProgram)\n";

std::string mainFuncProgramEnd = "function PauseFuncProgram()\n"
                                "   while(true)\n"
                                "   do\n"
                                "       wait(100) --sync\n"
                                "   end\n"
                                "end\n"
                                "PauseFuncProgram_Result=task_create(PauseFuncProgram)\n"
                                "end\n"
                                "mainFuncProgram_Result=task_create(mainFuncProgram)\n";
//连接激光
std::string Lua_connect_laser = "connect_id=8\n"
                                "::reconnect_label::\n"
                                "socket_disconnect(connect_id)\n"
                                "wait(100)\n"
                                "socket_connect('udp','192.168.16.254',connect_id,8080,8080)\n"
                                "wait(100)\n"
                                "connect_status = get_socket_connect_status(connect_id)\n"
                                "if(connect_status~=0) then\n"
                                "\tif(connect_id<=16) then\n"
                                "\t\tgoto reconnect_label\n"
                                "\tend\n"
                                "\tprint('连接失败,请检查设备')\n"
                                "\thalt()\n"
                                "end\n"
                                "print('udp连接成功,连接ID:'..connect_id)\n"
                                "connect_id=8\n";
//送丝机连接函数
std::string Lua_connect_Wire = "exj_disable(0)\n"
                            "wait(200)\n"
                            "write_ect_pdo(904,8)\n"
                            "wait(200)\n"
                            "exj_enable(0)\n"
                            "actual_modu  lefeed=8\n";
//读水冷机开关状态
std::string read_switch_wc = "function read_switch_wc()\n"
                         "for i=0,9,1 do\n"
                         "write_int16_register(i,0)\n"
                         "end\n"
                         "mes={0x03FE,0x3D00,0x0100,0xC901}\n"
                         "for i=1,4,1 do\n"
                         "write_int16_register(i-1,mes[i])\n"
                         "end\n"
                         "serial_send(2,4,1,0)\n"
                         "serial_receive(2,5,1,10,0)\n"
                         "if(read_int16_register(10)==0x03FE) then\n"
                         "result_wc=Lua_data_get(read_int16_register(11),read_int16_register(12))\n"
                         "write_double_register(11,result_wc)\n"
                         "return result_wc\n"
                         "else\n"
                         "return -1\n"
                         "end\n"
                         "end\n";


std::string Lua_data_get = "function Lua_data_get(data1,data2)\n"
                       "\tif(data1<0) then\n"
                       "\t\tdata1=65536+data1\n"
                       "\tend\n"
                       "\tif(data2<0) then\n"
                       "\t\tdata2=65536+data2\n"
                       "\tend\n"
                       "\tdata_1={}\n"
                       "\tdata_2={}\n"
                       "\tdata_1[1]=data1//(16^3)\n"
                       "\tdata_1[2]=(data1-data_1[1]*(16^3))//(16^2)\n"
                       "\tdata_2[1]=data2//(16^3)\n"
                       "\tdata_2[2]=(data2-data_2[1]*(16^3))//(16^2)\n"
                       "\tdata_2[3]=(data2-data_2[1]*(16^3)-data_2[2]*(16^2))//16\n"
                       "\tdata_2[4]=data2%16\n"
                       "\tresult1=(data_1[1]*(16^3))+(data_1[2]*(16^2))+(data_2[3]*16+data_2[4])\n"
                       "\treturn result1\n"
                       "end\n";

std::string Lua_crc16 = "function Lua_crc16(data)\n"
                    "\ttmp=0xFFFF\n"
                    "\tfor i=1,6,1 do\n"
                    "\t\ttmp=(data[i])~tmp\n"
                    "\t\tfor j=1,8,1 do\n"
                    "\t\t\tif(tmp&1==1) then\n"
                    "\t\t\t\ttmp=tmp>>1 \n"
                    "\t\t\t\ttmp=40961~tmp\n"
                    "\t\t\telse\n"
                    "\t\t\t\ttmp=tmp>>1\n"
                    "\t\t\tend\n"
                    "\t\tend\n"
                    "\tend\n"
                    "\treturn tmp\n"
                    "end\n";

std::string Lua_split = "function Lua_split(data3)\n"
                    "\tif(data3<0) then\n"
                    "\t\tdata3=65536+data3\n"
                    "\tend\n"
                    "\tdata_3={}\n"
                    "\tresult4={}\n"
                    "\tdata_3[1]=data3//(16^3)\n"
                    "\tdata_3[2]=(data3-data_3[1]*(16^3))//(16^2)\n"
                    "\tdata_3[3]=(data3-data_3[1]*(16^3)-data_3[2]*(16^2))//16\n"
                    "\tdata_3[4]=data3%16\n"
                    "\tsplit1=16*data_3[1]+data_3[2]\n"
                    "\tsplit2=16*data_3[3]+data_3[4]\n"
                    "\tresult4={split1,split2}\n"
                    "\treturn result4\n"
                    "end\n";

std::string Lua_get_message = "function Lua_get_message(data5)\n"
                          "\tresult5_1=Lua_split(data5[1])\n"
                          "\tresult5_2=Lua_split(data5[2])\n"
                          "\tresult5_3=Lua_split(data5[3])\n"
                          "\tresult5={result5_1[1],result5_1[2],result5_2[1],result5_2[2],result5_3[1],result5_3[2]}\n"
                          "\treturn result5\n"
                          "end\n";

std::string Lua_data_combin = "connect_id=8\n"
                         "function Lua_data_combin(data1,data2)\n"
                       "\tif(data1<0) then\n"
                       "\t\tdata1=65536+data1\n"
                       "\tend\n"
                       "\tif(data2<0) then\n"
                       "\t\tdata2=65536+data2\n"
                       "\tend\n"
                       "\tdata_1={}\n"
                       "\tdata_2={}\n"
                       "\tdata_1[1]=data1//(16^3)\n"
                       "\tdata_1[2]=(data1-data_1[1]*(16^3))//(16^2)\n"
                       "\tdata_1[3]=(data1-data_1[1]*(16^3)-data_1[2]*(16^2))//16\n"
                       "\tdata_1[4]=data1%16\n"
                       "\tdata_2[1]=data2//(16^3)\n"
                       "\tdata_2[2]=(data2-data_2[1]*(16^3))//(16^2)\n"
                       "\tdata_2[3]=(data2-data_2[1]*(16^3)-data_2[2]*(16^2))//16\n"
                       "\tdata_2[4]=data2%16\n"
                       "\tresult1=data_1[3]*16+data_1[4]+data_2[1]*(16^3)+data_2[2]*(16^2)\n"
                       "\treturn result1\n"
                       "end\n";

std::string swap_bytes_for_word = "function swap_bytes_for_word(data4)\n"
                              "if(data4<0) then\n"
                              "data4=65536+data4\n"
                              "end\n"
                              "data_4={}\n"
                              "data_4[1]=data4//(16^3)\n"
                              "data_4[2]=(data4-data_4[1]*(16^3))//(16^2)\n"
                              "data_4[3]=(data4-data_4[1]*(16^3)-data_4[2]*(16^2))//16\n"
                              "data_4[4]=data4%16\n"
                              "result1=(data_4[3]*(16^3))+(data_4[4]*(16^2))+(data_4[1]*16+data_4[2])\n"
                              "return result1\n"
                              "end\n";

std::string readRespondOfWrite = "function readRespondOfWrite()\n"
                             "print(\"enter readRespondOfWrite\")\n"
                             "while(true)\n"
                             "do\n"
                             "isSendData=read_bool_register(33)\n"
                             "if(isSendData==1)\n"
                             "then\n"
                             "print(\"read Respond Of Write\")\n"
                             "serial_receive(3,4,1,66,300)\n"
                             "end\n"
                             "sync()  --sync()\n"
                             "end\n"
                             "end\n";

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
    for i=62,69,1 do
        write_int16_register(i,0)
    end
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

std::string write_function =
R"(
local laserHeadRegisterMap = {
    -- 激光头控制参数
    laserHeadPatternIdLoc = { 0x0030, 0},     -- 激光头图形切换
    laserHeadScanSpeedLoc = { 0x0000, 1000}, -- 激光头扫描速度
    laserHeadScanWidthLoc = { 0x0001, 0 }, -- 激光头扫描宽度
    laserHeadScanHeightLoc = { 0x0006, 0 },    -- 激光头扫描高度
    laserHeadRotationAngleLoc = { 0x0007, 0 }, -- 激光头旋转角度

}

local locationCalibration = {
    -- X Y 偏移
    laserHeadYScaleLoc = { 0x0238, 0},  -- 激光头Y轴缩放
    laserHeadYOffsetLoc = { 0x0239, 0}, -- 激光头Y轴偏移
    laserHeadXScaleLoc = { 0x023A, 0 }, -- 激光头X轴缩放
    laserHeadXOffsetLoc = {0x023B, 0 }, -- 激光头X轴偏移
}
--焊接模式
local weldingMode = {
    weldingModeLoc = {0x0202, 0} --焊接模式 1：连续 0：点焊
}
--点焊工艺
local spotWeld = {
    spotWeldTypeLoc = {0x0242, 0}, --点焊类型 1：鱼鳞焊 0：间断
    spotWeldDurationLoc = {0x0243, 0}, --点焊持续时间  0 ~ 60000 ms
    spotWeldIntervalLoc = {0x0244, 0} --点焊间隔时间   0 ~ 60000 ms
}
--激光器工艺
local laserRegisterMap = {
    gasONDelayLoc = {0x0235, 0},     -- 气体打开延时 --提前送气时间
    gasOffDelayLoc = {0x0236, 0},    -- 气体关闭延时 --滞后送气时间
    laserOnPowerLoc = {0x0231, 1200},  -- 开光功率
    laserOnRampTimeLoc = {0x0232, 50},   -- 开光功率上升时间
    -- 焊接激光功率
    laserPowerLoc = {0x0230, 2000},-- 激光功率
    --焊接速度--

    laserOffPowerLoc = {0x0233, 0},  -- 关光功率
    laserOffRampTimeLoc = {0x0234, nil},   -- 关光功率下降时间

    --提前回抽距离--

}

--送丝工艺
local wire_feeder_registers = {
    operationModeLoc = {0x010C, 0},  -- 运行模式 0：连续模式 1：脉冲模式
    -- 0：连续模式
    smoothnessLoc = { 0x0108, 50},     -- 平滑度 25 %~80 %
    --1：脉冲模式
    pulsePeriodLoc = {0x0107, 100},    -- 脉冲周期 100~1000 ms
    -- 自动送丝速度
    autoWireFeedSpeed = {0x0100, 100},  -- 15~600 cm/min
    --收尾回送长度（mm）
    wireFeedLength = {0x0105, 100} --补丝长度 0~100 mm
}

--计算光光功率下降时间
laserOffRampDistance = 100 -- 关光功率下降距离
speed = 200  -- 送丝速度
laserRegisterMap.laserOffRampTimeLoc[2] = laserOffRampDistance / speed * 1000 -- 关光功率下降时间（ms）

-- 开光送丝速度（cm/min）
wireOutSpeed = 0
if (laserRegisterMap > 0) then
    wire_feeder_registers.autoWireFeedSpeedLoc[2] = wireOutSpeed  -- 自动送丝速度为开光送丝速度
end
-- 关光送丝（cm/min）
wireInSpeed = 0

--收尾抽丝（cm/min）
wireRetractLength = 0

function Lua_split(data3)
    -- 处理负数：转换为无符号16位整数
    local value = data3 < 0 and (65536 + data3) or data3
    -- 直接提取高低字节（避免复杂的数学运算）
    local high_byte = (value >> 8) & 0xFF  -- 右移8位取高字节
    local low_byte = value & 0xFF         -- 掩码取低字节
    return {high_byte, low_byte}
end

function Lua_get_message(data5)
    result5_1=Lua_split(data5[1])
    result5_2=Lua_split(data5[2])
    result5_3=Lua_split(data5[3])
    result5={result5_1[1],result5_1[2],result5_2[1],result5_2[2],result5_3[1],result5_3[2]}
    return result5
end

function Lua_crc16(data)
    tmp=0xFFFF
    for i=1,6,1 do
        tmp=(data[i])~tmp
        for j=1,8,1 do
            if(tmp&1==1) then
                tmp=tmp>>1
                tmp=40961~tmp
            else
                tmp=tmp>>1
            end
        end
    end
    return tmp
end
function swap_bytes_for_word(data4)
if(data4<0) then
data4=65536+data4
end
data_4={}
data_4[1]=data4//(16^3)
data_4[2]=(data4-data_4[1]*(16^3))//(16^2)
data_4[3]=(data4-data_4[1]*(16^3)-data_4[2]*(16^2))//16
data_4[4]=data4%16
result1=(data_4[3]*(16^3))+(data_4[4]*(16^2))+(data_4[1]*16+data_4[2])
return result1
end

----------定义写入函数-----------
function write_value_port(parameter_value, port_address, function_code, port_num)
    --------将通讯变量清零--------
    for i=62,69,1 do
        write_int16_register(i,0)
    end
    --------定义写入报文---------
    mes_1=Lua_get_message({function_code,port_address,parameter_value})            --------整合设定报文(无校验码)，并处理为6维数组
    mes_2=Lua_crc16(mes_1)											        --------计算CRC16校验码
    parameter_value=swap_bytes_for_word(parameter_value)				    --------参数高低位互换
    mes_3={swap_bytes_for_word(function_code),swap_bytes_for_word(port_address),parameter_value,mes_2}
    --------写入报文写入通讯变量---------
    for i=1,4,1 do
        write_int16_register(i+61,mes_3[i])
    end
    --------发送报文------

    serial_send(port_num,4,1,62)
end

--function code : 0x0609
function laser_write_register(dataX)
    local laser_function_code = 0x0609
    local laser_port_num = 3
    for param_name, param_value in pairs(dataX) do
        write_value_port(param_value[2], param_value[1], laser_function_code, laser_port_num)
        print(param_name, param_value[1]) -- 打印参数名称和地址
        if param_value[1] == param_value then
            wait(200)
        end
    end
end

--function code : 0x0A09
function wireFeeder_write_register()
    local wireFeeder_function_code = 0x0A09
    local wireFeeder_port_num = 2
    for param_name, param_value in pairs(wire_feeder_registers) do
        write_value_port(param_value[2], param_value[1], wireFeeder_function_code, wireFeeder_port_num)
        print(param_name, param_value[1]) -- 打印参数名称和地址
    end
end

)";

std::string write_laser_params =
R"(
--激光器
laser_write_register(laserRegisterMap)
--激光头
laser_write_register(laserHeadRegisterMap)
--YXYX
laser_write_register(locationCalibration)
--
laser_write_register(weldingMode)
--
laser_write_register(spotWeld)
--
wireFeeder_write_register()
)"  ;

std::string writeFunction =
R"(
----------定义写入函数-----------
function write_value_port(parameter_value, port_address, function_code, port_num)
    --------将通讯变量清零--------
    for i=62,69,1 do
        write_int16_register(i,0)
    end
    --------定义写入报文---------
    mes_1=Lua_get_message({function_code,port_address,parameter_value})     --------整合设定报文(无校验码)，并处理为6维数组
    mes_2=Lua_crc16(mes_1)											        --------计算CRC16校验码
    parameter_value=swap_bytes_for_word(parameter_value)				    --------参数高低位互换
    mes_3={swap_bytes_for_word(function_code),swap_bytes_for_word(port_address),parameter_value,mes_2}
    --------写入报文写入通讯变量---------
    for i=1,4,1 do
        write_int16_register(i+61,mes_3[i])
    end
    --------发送报文------
    serial_send(port_num,4,1,62)
end
)";
