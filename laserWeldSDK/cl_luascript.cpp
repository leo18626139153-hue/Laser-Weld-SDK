#include "cl_luascript.h"

std::string mainFuncProgram = "function mainFuncProgram()\n";

std::string RobotProgram1 = "function RobotProgram()\n";

std::string RobotProgram2 = "sync()\n"
                            "halt()\n"
                            "end\n"
                            "-----RobotProgramEnd----\n"
                            "RobotProgram_Result=task_create(RobotProgram)\n"
                            "-----PauseFuncProgram----\n"
                            "function PauseFuncProgram()\n"
                            "\twhile(true)\n"
                            "\tdo\n"
                            "\t\twait(100)\n"
                            "\tend\n"
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
