#ifndef CL_API_H
#define CL_API_H

#include "cl_basedefine.h"
#include "parameterdefine.h"
#include "robotapi.h"
#include <stdint.h>

#ifdef WIN32
#if defined(LASERWELDSDK_LIBRARY)
#  define LASERWELDSDK_EXPORT __declspec(dllexport)
#else
#  define LASERWELDSDK_EXPORT __declspec(dllimport)
#endif
#else
#ifdef  LASERWELDSDK_LIBRARY
#define LASERWELDSDK_EXPORT __attribute__((visibility("default")))
#else
#define LASERWELDSDK_EXPORT __attribute__((visibility("default")))
#endif
#endif

#define LASERWELDSDK_VERSION "v1.2.3-6"

/**
 * @warning SDK当前不支持同一进程内并发调用。调用方必须串行化所有接口，
 * 包括使用不同LaserMachineHandle的调用；并发调用属于未定义行为。
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LaserSdkResult     获取当前焊接SDK版本号
 * @param[out] version            焊接SDK版本号 传出参数
 * @param[out] versionLen         焊接SDK版本号字符长度 传入时表示version参数申请的空间大小，传出时表示焊接SDK版本号长度
 * @return WELDSDK_EXPORT    处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_sdk_version_get(char* version, int* versionLen);

/**
 * @brief initMachineResource    注册焊机资源
 * @param[in] laserMachineResource   焊机资源结构体
 * @param[out] laserMachineHandle     焊机资源句柄 传出参数
 * @return LaserSdkResult        处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_machine_resource_init(const MachineResource* laserMachineResource, LaserMachineHandle* laserMachineHandle);

/**
 * @brief updateMachineResource  更新焊机资源
 * @param[in] laserMachineHandle     焊机资源句柄
 * @param[in] laserMachineResource   焊机资源结构体
 * @return LaserSdkResult        处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_machine_resource_update(const LaserMachineHandle laserMachineHandle, const MachineResource* laserMachineResource);

/**
 * @brief getMachineResource     获取焊机资源句柄对应的数据
 * @param[in] laserMachineHandle     焊机资源句柄
 * @param[out] laserMachineResource   焊机资源结构体 传出参数
 * @return LaserSdkResult        处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_machine_resource_get(const LaserMachineHandle laserMachineHandle, MachineResource* laserMachineResource);

/**
 * @brief 释放焊机资源
 * @param[in] laserMachineHandle             焊机资源句柄
 * @return LaserSdkResult                处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_machine_resource_release(const LaserMachineHandle laserMachineHandle);

/**
 * @brief 设置参数描述和控制器本地地址绑定，SDK深复制配置并按句柄独立保存。
 * @param[in] laserMachineHandle 已注册的焊机句柄。
 * @param[in] descriptors 参数描述数组，物理值编码为value*scale+offset。
 * @param[in] descriptorCount 参数描述个数。
 * @param[in] bindings 本地读写地址数组，地址0合法，未配置方向使用无效地址常量。
 * @param[in] bindingCount 地址绑定个数。
 * @param[out] diagnostic 可为空；非空时必须先设置structSize。
 * @warning 显式配置启用本地映射输出。上位机必须预先配置Modbus RTU主从站及远端地址映射。
 * @return 配置校验失败时保留该句柄原配置，成功不产生通讯或设备动作。
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_parameters_configuration_set(
    LaserMachineHandle laserMachineHandle,
    const LaserParameterDescriptorSdk* descriptors, unsigned int descriptorCount,
    const LaserParameterBindingSdk* bindings, unsigned int bindingCount,
    LaserParameterDiagnosticSdk* diagnostic);

/**
 * @brief 获取有效参数配置；未设置时返回当前型号的兼容默认描述与绑定。
 * @param[in] laserMachineHandle 已注册的焊机句柄。
 * @param[out] descriptors 参数描述数组；为空且bindings为空时仅查询所需个数。
 * @param[in,out] descriptorCount 输入描述数组容量，输出所需个数。
 * @param[out] bindings 参数地址绑定数组，语义与当前输出模式一致。
 * @param[in,out] bindingCount 输入绑定数组容量，输出所需个数。
 * @note 两个数组均为空时只查询个数；任一容量不足时两个数组均不写入。
 * @return 成功、无效句柄、参数错误、内存不足或输出容量不足。
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_parameters_configuration_get(
    LaserMachineHandle laserMachineHandle,
    LaserParameterDescriptorSdk* descriptors, unsigned int* descriptorCount,
    LaserParameterBindingSdk* bindings, unsigned int* bindingCount);

/**
 * @brief 清除显式参数配置并恢复型号默认行为，不修改控制器主从站配置。
 * @param[in] laserMachineHandle 已注册的焊机句柄。
 * @return 成功、无效句柄或内部异常；原始焊机资源不变。
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_parameters_configuration_reset(LaserMachineHandle laserMachineHandle);

/**
 * @brief 按有效描述及绑定生成参数脚本，不修改焊机资源、不执行通讯、不触发出光。
 * @param[in] laserMachineHandle 已注册的焊机句柄。
 * @param[in] values 物理单位参数值数组，允许客户扩展参数ID。
 * @param[in] valueCount 参数值个数，0表示空写入计划。
 * @param[out] scripts 输出Lua；为空时仅查询所需字节数。
 * @param[in,out] scriptLen 输入缓冲区容量，输出所需字节数，包含结尾空字符。
 * @param[out] diagnostic 可为空；非空时必须先设置structSize。
 * @note 未显式配置的旧超强保留自由通讯兼容输出；显式配置后三品牌均输出本地寄存器操作。
 * @return 成功、参数配置或编码错误、无效句柄、内存不足或输出容量不足。
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_parameters_script_get(
    LaserMachineHandle laserMachineHandle,
    const LaserParameterValueSdk* values, unsigned int valueCount,
    char* scripts, int* scriptLen, LaserParameterDiagnosticSdk* diagnostic);

/*
*************激光焊功能接口*************
*/

/**
 * @brief  获取起弧脚本
 * @param[in] laserMachineHandle 焊机资源句柄
 * @param[out] scripts           起弧LUA脚本 传出参数
 * @param[in,out] scriptLen      脚本长度 传入时为scripts参数申请的空间大小，传出时为生成的起弧LUA脚本大小
 * @return LaserSdkResult    处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_get_arc_start_script(const LaserMachineHandle laserMachineHandle, char* scripts, int* scriptLen);

/**
 * @brief  获取收弧脚本脚本
 * @param[in] laserMachineHandle 焊机资源句柄
 * @param[out] scripts           收弧LUA脚本 传出参数
 * @param[in,out] scriptLen      脚本长度 传入时为scripts参数申请的空间大小，传出时为生成的收弧LUA脚本大小
 * @return LaserSdkResult    处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_get_arc_stop_script(const LaserMachineHandle laserMachineHandle, char* scripts, int* scriptLen);

/**
 * @brief     初始化所有焊接相关状态
 * @param[in] laserMachineHandle          焊机资源句柄
 * @return LaserSdkResult                处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_craft_weld_status_init(const LaserMachineHandle laserMachineHandle);

/**
 * @brief     更改焊接设备状态
 * @param[in] laserMachineHandle             焊机资源句柄
 * @return LaserSdkResult                处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_craft_device_status_modify(const LaserMachineHandle laserMachineHandle, char* scripts, int* scriptLen);

/**
 * @brief     获取指令列表转换后脚本
 * @param[in] laserMachineHandle    焊机资源句柄
 * @param[in] weldStartData         指令列表数据结构体
 * @param[in] isWeld                是否进行焊接 false-轨迹模拟 true-开始焊接
 * @param[out] scripts              指令列表LUA脚本。传入nullptr时仅查询所需长度，不生成外部副作用。
 * @param[in,out] scriptLen         不可为nullptr。scripts非空时传入缓冲区容量；传出脚本所需字节数，包含结尾'\0'。
 *                                  缓冲区不足时返回BUFFER_TOO_SMALL_LASER_SDK且不写入scripts。
 * @warning SDK当前不支持并发调用，包括不同焊机句柄之间的并发；调用方必须串行化。
 * @return LaserSdkResult        处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_craft_inst_list_script_get(const LaserMachineHandle laserMachineHandle, LaserWeldStartData weldStartData, bool isWeld, char* scripts, int* scriptLen);

/**
 * @brief     获取修改工艺参数生效后脚本
 * @param[in] laserMachineHandle    焊机资源句柄
 * @param[in] craftInfo             指令列表数据结构体
 * @param[out] scripts               指令列表LUA脚本 传出参数
 * @param[out] scriptLen             脚本长度 传入时为scripts参数申请的空间大小，传出时为生成的指令列表LUA脚本大小
 * @return LaserSdkResult        处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_craft_inst_devices_params_script_get(const LaserMachineHandle laserMachineHandle, CraftInfoLaserSdk craftInfo, char* scripts, int* scriptLen);

/**
 * @brief     获取收弧子程序脚本
 * @param[in] laserMachineHandle        焊机资源句柄
 * @param[out] scripts                  ArcEndSubProgramScript 的LUA脚本 传出参数
 * @param[out] scriptLen                脚本长度 传入时为scripts参数申请的空间大小，传出时为生成的ArcSet的LUA脚本大小
 * @return LaserSdkResult               处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_craft_end_arc_subprogram_script_get(const LaserMachineHandle laserMachineHandle, char* scripts, int* scriptLen);

/**
 * @brief   获取焊接结束后操作的脚本
 * @param[in] laserMachineHandle            焊机资源句柄
 * @param[out] scripts                       生成的LUA脚本 传出参数
 * @param[out] scriptLen                     脚本长度 传入时为scripts参数申请的空间大小，传出时为生成的LUA脚本大小
 * @return LaserSdkResult                处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_craft_after_weld_end_operate_script_get(const LaserMachineHandle laserMachineHandle,char* scripts, int* scriptLen);

/**
 * @brief getLapRollbackCirclePoint     缓升缓降 起弧
 * @param laserMachineHandle            激光器句柄
 * @param scripts                       起弧LUA脚本 传出参数
 * @param scriptLen                     脚本长度 传入时为scripts参数申请的空间大小，传出时为生成的起弧LUA脚本大小
 * @return LaserSdkResult                处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_get_arc_start_slow_script(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen);

/**
 * @brief getLapRollbackCirclePoint     缓升缓降收弧
 * @param laserMachineHandle            激光器句柄
 * @param scripts                       起弧LUA脚本 传出参数
 * @param scriptLen                     脚本长度 传入时为scripts参数申请的空间大小，传出时为生成的起弧LUA脚本大小
 * @return LaserSdkResult                处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_get_arc_stop_slow_script(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen);

/**
 * @brief         获取焊接状态监控脚本
 * @param laserMachineHandle                 焊接资源句柄
 * @param scripts                           焊接状态监控的LUA脚本 传出参数
 * @param scriptLen                         脚本长度 传入时为scripts参数申请的空间大小，传出时为生成的LUA脚本大小
 * @return LaserSdkResult                    处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_craft_weld_status_monitor_script_get(const LaserMachineHandle laserMachineHandle, char *scripts, int *scriptLen);

/*
*************机器人控制*************
*/

/**
 * @brief 计算抬枪后的姿态
 * @param laserMachineHandle        焊机资源句柄
 * @param pose                      抬枪前的姿态(位置+轴角(角度))
 * @param joint                     抬枪前参考关节角
 * @param liftDistance              抬枪距离
 * @param liftPose                  抬枪后的姿态(位置+轴角(角度))
 * @param liftJoint                 抬枪后参考关节角
 * @return LaserSdkResult           处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult  cl_craft_lift_pose_cal(const LaserMachineHandle laserMachineHandle, const double* pose, const double* joint, const double liftDistance, double* liftPose, double* liftJoint);

/*
*************Modbus报文相关*************
*/

/**
 * @brief     根据对应的地址以及对应参数值，生成出对应的报文
 * @param[in] laserMachineHandle             焊接资源句柄
 * @param[in] firstAddr                      激光焊资源地址
 * @param[in] parameter                      参数值
 * @param[in] slaveAddr                      从站地址
 * @param[in] funcCode                       功能码;暂时只支持06
 * @param[out] message                       生成的报文
 * @param[out] length                        报文长度
 * @return LaserSdkResult                    处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_message_generate(const LaserMachineHandle laserMachineHandle, uint16_t firstAddr, uint16_t parameter, uint8_t slaveAddr, uint8_t funcCode, uint8_t *message, int *length);

/**
 * @brief     将变量写入bool寄存器(暂不支持)
 * @param[in] laserMachineHandle             焊接资源句柄
 * @param[in] firstRegister                  要写入的起始地址
 * @param[in] registerNums                   要写入的寄存器个数
 * @param[in] message                        报文列表
 * @return LaserSdkResult                    处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_write_message_to_bool_register(const LaserMachineHandle laserMachineHandle, unsigned int firstRegister, unsigned int registerNums, uint8_t *message);

/**
 * @brief     将报文写入Int16寄存器
 * @param[in] laserMachineHandle             焊接资源句柄
 * @param[in] firstRegister                  要写入的起始地址
 * @param[in] registerNums                   要写入的寄存器个数,报文长度除以2
 * @param[in] message                        报文列表，单字节
 * @return LaserSdkResult                    处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_write_message_to_Int16_register(const LaserMachineHandle laserMachineHandle, unsigned int firstRegister, unsigned int registerNums, uint8_t *message);

/**
 * @brief     将状态监控报文写入Int32寄存器,占用24个内存
 * @param[in] laserMachineHandle             焊接资源句柄
 * @param[in] firstRegister                  要写入的起始地址
 * @return LaserSdkResult                    处理结果
 */
LASERWELDSDK_EXPORT LaserSdkResult cl_write_state_monitor_message_to_Int32_register(const LaserMachineHandle laserMachineHandle, unsigned int firstRegister);

  /**
 * @brief cl_parse_modbus_to_int16_register modbus报文解析
 * @param laserMachineHandle                焊机资源句柄
 * @param hexResponse                       返回报文
 * @param regIndex                          解析数据索引
 * @param regLen                            解析个数
 * @param value                             返回值
 * @return LaserSdkResult
 */
LASERWELDSDK_EXPORT LaserSdkResult  cl_parse_modbus_to_int16_register(const LaserMachineHandle laserMachineHandle,const char* hexResponse, const int regIndex, const int regLen,  int16_t* value);

/**
* @brief cl_parse_modbus_to_int16_register modbus报文解析
* @param laserMachineHandle                焊机资源句柄
* @param hexResponse                       返回报文
* @param value                             返回值
* @return LaserSdkResult
*/
LASERWELDSDK_EXPORT LaserSdkResult  cl_parse_modbus_to_int32_register(const LaserMachineHandle laserMachineHandle,const char* hexResponse, int32_t &value);

/**
* @brief cl_get_instance_function_script 获取指令函数脚本
* @param funcName                       函数名称
* @param funcParam                      函数参数
* @param addr                           地址码
* @param coAddr                         从站号
* @param comNo                          com口号
* @param scripts                        输出的脚本
* @param scriptLen                      脚本长度
* @return LaserSdkResult
*/
LASERWELDSDK_EXPORT LaserSdkResult cl_get_instance_function_script(const char* funcName, const char* funcParam, const unsigned int addr, const int regType,unsigned int coAddr, int comNo, char *scripts, int *scriptLen);

#ifdef __cplusplus
}
#endif

#endif // CL_API_H

