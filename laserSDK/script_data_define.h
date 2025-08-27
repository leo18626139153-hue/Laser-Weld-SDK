#ifndef CGXILUAUSEPUBLIC_H
#define CGXILUAUSEPUBLIC_H

#include "cr_sdk/windows/include/robotapi.h"

//using namespace CGXiRobotPluginSDK;
#include<iostream>
#include <string>
#include <vector>

using namespace std;

//lua脚本运行  script = mainFuncProgram + 运行前序列函数 + RobotProgram1 + 主函数 + RobotProgram2;
extern  string mainFuncProgram;
extern  string RobotProgram1;
extern  string RobotProgram2;

extern  string Lua_connect_laser;//激光器连接函数
extern  string Lua_connect_Wire;//送丝机连接函数
extern  string read_switch_wc;//读取水冷机开关

//数据转化类函数
extern string Lua_data_get;
extern string Lua_crc16;
extern string Lua_split;
extern string Lua_get_message;
extern string Lua_data_combin;

extern string write_ospri_io;

//参数设置-激光头-超强伟业通讯脚本
extern string swap_bytes_for_word;
extern string write_laser_graphics;//写入光斑图案
extern string write_scanning_speed;//写入扫描速度
extern string write_rotation_angle;//写入旋转角度
extern string write_scanning_width;//写入扫描宽度
extern string write_scanning_height;//写入扫描高度
extern string write_XaxisCenter_offset;//写入X偏移
extern string write_YaxisCenter_offset;//写入Y偏移
extern string readRespondOfWrite;//接收写入报文的应答报文

extern string read_scanningCorrectionCoefficient_YXaxis;//发送请求读取X，Y轴校正系数的数据报文
extern string LaserHeadGetDataXYaxisCorrection;//接收应答读取X，Y轴校正系数的数据报文

//参数设置-激光器-BWT
extern string write_laser_power; //写入激光器功率（%）
extern string read_laser_power; //读取激光器功率（W）

//送丝机
extern string wireSenderPowerOn; //送丝机开关机
extern string wireEnable_Lua; //送丝机使能
extern string manualWireSend; //手动送丝
extern string manualWireBack; //手动退丝
#endif // CGXILUAUSEPUBLIC_H
