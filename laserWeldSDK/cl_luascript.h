#ifndef CL_LUASCRIPT_H
#define CL_LUASCRIPT_H

#include "robotapi.h"
#include <string>

//using namespace CGXiRobotPluginSDK;

//lua脚本运行  script = 运行前序列函数 + mainFuncProgram +  RobotProgram1 + 主函数 + RobotProgram2;
extern  std::string mainFuncProgram;
extern  std::string RobotProgram1;
extern  std::string RobotProgram2;
extern  std::string mainFuncProgramEnd;
extern  std::string Lua_connect_laser;  //激光器连接函数
extern  std::string Lua_connect_Wire;   //送丝机连接函数
extern  std::string read_switch_wc;     //读取水冷机开关
//数据转化类函数
extern  std::string Lua_data_get;    ///< 获取数据函数
extern  std::string Lua_crc16;       ///< CRC16校验函数
extern  std::string Lua_split;       ///< 数据拆分函数
extern  std::string Lua_get_message; ///< 获取报文函数
extern  std::string Lua_data_combin; ///< 数据组合类
extern std::string swap_bytes_for_word; ///< 交换字节的函数
extern std::string readRespondOfWrite;  ///< 接收写入报文的应答报文

extern std::string modbus_crc16;    ///< CRC-16 (Modbus) 计算函数 无高低字节互转
extern std::string Lua_split;       ///< 高低字节拆分 如0x1234 输出{0x12, 0x34}
extern std::string register_clear;  ///< 定义寄存器清空函数(起始地址，清空位数)
extern std::string to_hex_string;   ///< 将十进制数转换为十六进制，并以两位字符表示
extern std::string concatenate_crc; ///< 遍历表中单字节数据拼接对应的十六进制字符串，输入 {0x09, 0x10, {0x1234}, {0x56}}，输出 "0910123456"
extern std::string parse_little_endian_bytes;   ///< 每2个字符为一组（1字节），转换为整数
extern std::string WriteHex2Commu_reg;  ///< 将数据写入通讯变量
extern std::string read_value_port;     ///< 单参数写入 (功能码(2 Byte), 寄存器地址(2 Bytes), 寄存器值(2 Bytes), 端口号， 起始寄存器), 接受回执
extern std::string hex_to_bytes;        ///< 把十六进制字符串转为字节数组
extern std::string swap_bytes_to_table; ///< 高低位交换：每两个字节交换一次
extern std::string get_swap_report;     ///< 获取第N个寄存器的值
extern std::string int16_to_hex;        ///< 十进制值转2字节十六进制（大端：高字节在前，低字节在后）
extern std::string read_registers_as_hex;   ///< 读取寄存器范围并拼接报文
extern std::string get_register_value;  ///< 获取对应寄存器的存储值
extern std::string parse_alarm_word;    ///< 告警信息的种类解析函数


#endif // CL_LUASCRIPT_H
