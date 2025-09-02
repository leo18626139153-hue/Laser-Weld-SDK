#ifndef CL_LUASCRIPT_H
#define CL_LUASCRIPT_H

#include "robotapi.h"
#include <string>

//using namespace CGXiRobotPluginSDK;

//lua脚本运行  script = mainFuncProgram + 运行前序列函数 + RobotProgram1 + 主函数 + RobotProgram2;
extern  std::string mainFuncProgram;
extern  std::string RobotProgram1;
extern  std::string RobotProgram2;

extern  std::string Lua_connect_laser;  //激光器连接函数
extern  std::string Lua_connect_Wire;   //送丝机连接函数
extern  std::string read_switch_wc;     //读取水冷机开关
//数据转化类函数
extern  std::string Lua_data_get;
extern  std::string Lua_crc16;
extern  std::string Lua_split;
extern  std::string Lua_get_message;
extern  std::string Lua_data_combin;

extern std::string swap_bytes_for_word;

extern std::string write_ospri_io;

extern std::string readRespondOfWrite;//接收写入报文的应答报文
extern std::string read_scanningCorrectionCoefficient_YXaxis;//发送请求读取X，Y轴校正系数的数据报文
extern std::string LaserHeadGetDataXYaxisCorrection;//接收应答读取X，Y轴校正系数的数据报文

//送丝机
extern std::string wireSenderPowerOn; //送丝机开关机
extern std::string wireEnable_Lua; //送丝机使能
extern std::string manualWireSend; //手动送丝
extern std::string manualWireBack; //手动退丝

#endif // CL_LUASCRIPT_H
