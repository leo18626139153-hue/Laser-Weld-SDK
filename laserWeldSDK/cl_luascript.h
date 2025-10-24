#ifndef CL_LUASCRIPT_H
#define CL_LUASCRIPT_H

#include "robotapi.h"
#include "cl_basedefine.h"
#include "cl_utils.h"
#include <string>
#include <vector>
#include <map>

/**
 * @brief modbusCRC16Calculate     查表法计算CRC16校验码;C风格
 * @param nData              用于crc16计算的列表，按字节顺序存放
 * @param wLength            crc16计算的列表changdu
 * @return uint16_t          处理结果
 */
uint16_t modbusCRC16Table(uint8_t *nData, uint16_t wLength);

/**
 * @brief crc16Modbus     计算CRC16校验码
 * @param data              用于crc16计算的列表，按字节顺序存放
 * @return uint16_t          处理结果
 */
uint16_t crc16Modbus(const std::vector<uint8_t> &data);

/**
 * @brief putByte     将一个字节按顺序放入列表中
 * @param CalcList        用于crc16计算的列表，按字节顺序存放
 * @param byteSingle      要放入列表的字节
 * @return void
 */
inline void putByte(std::vector<uint8_t>& CalcList, uint8_t byteSingle)  
{ 
    CalcList.push_back(byteSingle); 
}

/**
 * @brief putDoubleBytes     将两个字节拆开，按顺序放入列表中,高字节在前，低字节在后
 * @param CalcList   用于crc16计算的列表，按字节顺序存放
 * @param bytesDouble            
 */
inline void putDoubleBytes(std::vector<uint8_t>& CalcList, uint16_t bytesDouble)
{
    CalcList.push_back(static_cast<uint8_t>((bytesDouble >> 8) & 0xFF));   //高字节在前
    CalcList.push_back(static_cast<uint8_t>(bytesDouble & 0xFF));         //低字节在后 
}   

/**
 * @brief  buildMessageVector        
 * @param  locVec                    map中存放的modbus帧信息 06/10 : <地址,   值> || 03: <地址,   长度>
 * @param  funcCode                  modbus帧的指令码 06 写单个寄存器 10 写多个寄存器 03 读单个寄存器
 * @param  slave                     modbus帧的从机地址
 * @return std::vector<uint8_t>     处理结果
 */
std::vector<uint8_t> buildMessageVector(const std::map<uint16_t, uint16_t>& locVec, uint8_t slave, uint8_t funcCode);

/**
 * @brief  writeMessageToRegisters        将报文写入寄存器，bool寄存器报文正常顺序||int16寄存器报文高低字节颠倒顺序
 * @param  message                     要写入的报文
 * @param  registers                   寄存器列表类型
 * @param  firstRegister               要写入的起始地址
 * @param  registerNums                要写入的寄存器个数
 * @return LaserSdkResult              处理结果
 */
LaserSdkResult writeMessageToRegisters(LaserMachineHandle laserMachineHandle, const std::vector<uint8_t>& message, int registerType, unsigned int firstRegister, unsigned int registerNums);

#endif // CL_LUASCRIPT_H
