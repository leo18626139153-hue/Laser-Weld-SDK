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

std::string Lua_split = "function Lua_split(data3)\n"
                        "   local value = dataSplit < 0 and (65536 + dataSplit) or dataSplit\n"
                        "   local low_byte = dataSplit & 0xFF    \n"
                        "   local high_byte = dataSplit >> 8    \n"
                        "   return {high_byte, low_byte}   \n"
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






