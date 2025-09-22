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



#endif // CL_LUASCRIPT_H
