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

std::string swap_bytes_for_word = "function swap_bytes(data)\n"
                              "local low_byte = data & 0xFF   \n"
                              "local high_byte = data >> 8    \n"
                              "return (low_byte << 8) | high_byte"
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

std::string Lua_crc16 = "function modbus_crc16(data)\n"
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


std::string Lua_get_message = "function Lua_get_message(data5)\n"
                            "\tresult5_1=Lua_split(data5[1])\n"
                            "\tresult5_2=Lua_split(data5[2])\n"
                            "\tresult5_3=Lua_split(data5[3])\n"
                            "\tresult5={result5_1[1],result5_1[2],result5_2[1],result5_2[2],result5_3[1],result5_3[2]}\n"
                            "\treturn result5\n"
                            "end\n";

std::string Lua_data_combin = "function Lua_data_combin(data1,data2)\n"
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



std::string modbus_crc16 = "function modbus_crc16(data)\n"
                           "    local crc = 0xFFFF  \n"
                           "    local polynomial = 0xA001   \n"
                           "    for _, byte in ipairs(data) do\n"
                           "        crc = crc ~ byte\n"
                           "        for i = 1, 8 do\n"
                           "            if (crc & 0x0001) ~= 0 then\n"
                           "                crc = (crc >> 1) ~ polynomial\n"
                           "            else\n"
                           "                crc = crc >> 1\n"
                           "            end\n"
                           "        end\n"
                           "    end\n"
                           "    return crc\n"
                           "end\n";



std::string Lua_split = "function Lua_split(dataSplit)\n"
                        "    local low_byte = dataSplit & 0xFF    \n"
                        "    local high_byte = dataSplit >> 8    \n"
                        "    return {high_byte, low_byte}\n"
                        "end\n";


std::string register_clear =
    "function register_clear(firstAddr, cleanNums)\n"
    "	for i=0,cleanNums-1,1 do\n"
    "		write_int16_register(i + firstAddr, 0) \n"
    "	end\n"
    "end\n"
    "function bool_register_clear(firstAddr, cleanNums)\n"
    "	for i=0,cleanNums-1,1 do\n"
    "		write_bool_register(i + firstAddr, 0) \n"
    "	end\n"
    "end\n";


std::string to_hex_string =
    "function to_hex_string(number)\n"
    "    return string.format(\"%02X\", number)\n"
    "end\n";


std::string concatenate_crc =
    "function concatenate_crc(crc_mes)\n"
    "    local result = \"\"\n"
    "    for _, value in ipairs(crc_mes) do\n"
    "        if type(value) == \"number\" then\n"
    "            local hex_value = to_hex_string(value)\n"
    "            result = result .. hex_value\n"
    "        elseif type(value) == \"table\" then\n"
    "            local high, low = split_to_bytes(value[1])\n"
    "            local high_hex = to_hex_string(high)\n"
    "            local low_hex = to_hex_string(low)\n"
    "            result = result .. high_hex .. low_hex\n"
    "        end\n"
    "    end\n"
    "    return result\n"
    "end\n";


std::string parse_little_endian_bytes =
    "function parse_little_endian_bytes(hex_str)\n"
    "    local bytes = {}\n"
    "    for i = 1, #hex_str, 2 do\n"
    "        local byte = hex_str:sub(i, i + 1)\n"
    "        if #byte == 2 then\n"
    "            local value = tonumber(byte, 16)\n"
    "            table.insert(bytes, value)\n"
    "        end\n"
    "    end\n"
    "    return bytes\n"
    "end\n";


std::string WriteHex2Commu_reg =
    "function WriteHex2Commu_reg(hex)\n"
    "	local byte_values = parse_little_endian_bytes(hex)\n"
    "	local cnt = 0;\n"
    "	for i, byte in ipairs(byte_values) do\n"
    "		write_char_register(SendCommuRegAddr+i-1,byte)\n"
    "		cnt = i\n"
    "	end\n"
    "	return cnt\n"
    "end\n";


std::string read_value_port =
        "--Single parameter write (Function code(2 Byte), Register address(2 Bytes), Register value(2 Bytes), Port number, Start register), Receive receipt\n"
        "function read_value_port(function_code, port_address, parameter_value, comNo)\n"
        "	--------Clear communication variables--------\n"
        "	bool_register_clear(SendCommuRegAddr, SendCommuReg_n)\n"
        "	register_clear(RecvCommuRegAddr, RecvCommuReg_n)\n"
        "	----------Split high/low bytes and calculate checksum---------    \n"
        "	local data1=Lua_split(function_code)    \n"
        "	local data2=Lua_split(port_address)     \n"
        "    local data3=Lua_split(parameter_value)  \n"
        "	local mes_1={data1[1], data1[2], data2[1], data2[2], data3[1], data3[2]}\n"
        "	local mes_2=Lua_split(modbus_crc16(mes_1))    --Calculate CRC16 checksum without inversion    \n"
        "    print(string.format(\"CRC checksum value: 0x%02X\", mes_2[1], mes_2[2])) \n"
        "    --Construct message, invert high/low bytes for sending\n"
        "	mes_3 = {data1[1], data1[2],  data2[1], data2[2], data3[1], data3[2], mes_2[2], mes_2[1]}	\n"
        "    local tmp_string = concatenate_crc(mes_3)  --Concatenate data into string\n"
        "	print(\"Sent message:\",tmp_string)\n"
        "    local CommuReg_n =  WriteHex2Commu_reg(tmp_string)  --Write data to communication variables, return byte count\n"
        "    local timeout = 1   --Timeout time\n"
        "    --Port--Write byte count--Write type--Write start address--Read byte count--Read type--Read start address--Timeout time\n"
        "	serial_send_receive(comNo, CommuReg_n, 0, SendCommuRegAddr, RecvCommuReg_n,RecvDataType,RecvCommuRegAddr,timeout)\n"
        "    --Wait for receive result\n"
        "    local timeout = 500\n"
        "	print(\"Waiting for receive result...\",read_int16_register(RecvCommuRegAddr))\n"
        "    while(read_int16_register(RecvCommuRegAddr) == 0 and timeout > 0) do\n"
        "		print(\"Waiting for receive result...\")\n"
        "        wait(10)\n"
        "        timeout = timeout - 10\n"
        "    end\n"
        "    print(\"Receive result completed\")\n"
        "end\n";


std::string hex_to_bytes =
    "local function hex_to_bytes(hex_str)\n"
    "    local bytes = {}\n"
    "    for byte in hex_str:gmatch(\"%x%x\") do\n"
    "        table.insert(bytes, tonumber(byte, 16))\n"
    "    end\n"
    "    return bytes\n"
    "end\n";


std::string swap_bytes_to_table =
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


std::string get_swap_report =
    "local function get_swap_report(hex_str)\n"
    "    local bytes = hex_to_bytes(hex_str)\n"
    "    local swapped = swap_bytes_to_table(bytes)\n"
    "    local data = {}\n"
    "    for i = 4, #swapped do\n"
    "        table.insert(data, swapped[i])\n"
    "    end\n"
    "	return data\n"
    "end\n";


std::string int16_to_hex =
    "local function int16_to_hex(val)\n"
    "    if val < 0 then\n"
    "        val = val + 0x10000\n"
    "    end\n"
    "    local high = (val >> 8) & 0xFF\n"
    "    local low  = val & 0xFF\n"
    "    return string.format(\"%02X%02X\", high, low)\n"
    "end\n";


std::string read_registers_as_hex =
    "local function read_registers_as_hex(start_reg, end_reg)\n"
    "    local report = {}\n"
    "    for i = start_reg, end_reg do\n"
    "        local value = read_int16_register(i)\n"
    "        local hex_str = int16_to_hex(value)\n"
    "        table.insert(report, hex_str)\n"
    "    end\n"
    "    return table.concat(report)\n"
    "end\n";


std::string get_register_value =
    "function get_register_value(data, reg_index)\n"
    "    local pos = (reg_index - 1) * 2 + 1\n"
    "    if pos + 1 > #data then\n"
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


std::string parse_alarm_word =
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
