#ifndef LASER_PARAMETER_CONTEXT_H
#define LASER_PARAMETER_CONTEXT_H

#include "parametercompiler.h"
#include <string>

namespace laser_parameters {

/**
 * @brief 以深复制方式设置指定焊机的参数描述和本地地址绑定。
 * @param[in] handle 已注册的焊机句柄。
 * @param[in] configuration 完整配置，调用返回后调用方可修改或销毁原数组。
 * @param[out] diagnostic 可为空；非空时需先填写structSize。
 * @return 成功、无效句柄或配置校验错误；失败不替换该句柄的旧配置。
 * @note 仅安装编译配置，不执行通讯；整库仍遵循公开API的串行调用要求。
 */
LaserSdkResult SetMachineConfiguration(LaserMachineHandle handle,
                                      const Configuration& configuration,
                                      LaserParameterDiagnosticSdk* diagnostic = nullptr);

/**
 * @brief 获取当前有效配置，未显式配置时从型号兼容档案生成默认配置。
 * @param[in] handle 已注册的焊机句柄。
 * @param[out] configuration 成功时整体替换为配置副本，失败时不修改。
 * @param[out] explicitlyConfigured 可为空；成功时指明是否存在显式地址绑定。
 * @param[out] diagnostic 可为空的参数级诊断。
 * @return 成功、无效句柄或型号档案及配置错误。
 */
LaserSdkResult GetMachineConfiguration(LaserMachineHandle handle,
                                      Configuration& configuration,
                                      bool* explicitlyConfigured = nullptr,
                                      LaserParameterDiagnosticSdk* diagnostic = nullptr);

/**
 * @brief 删除显式绑定并恢复型号默认配置，不修改上位机或控制器的通讯配置。
 * @param[in] handle 已注册的焊机句柄。
 * @return 成功或无效句柄；仅清理参数编译状态。
 */
LaserSdkResult ResetMachineConfiguration(LaserMachineHandle handle);

/**
 * @brief 释放句柄或切换型号时清除该句柄的配置。
 * @param[in] handle 待清理的句柄；尚未配置的句柄可重复清理。
 */
void ReleaseMachineConfiguration(LaserMachineHandle handle);

/**
 * @brief 判断当前句柄是否采用由上位机配置的本地映射寄存器通道。
 * @param[in] handle 待判断的焊机句柄。
 * @param[in] brand 已验证的设备品牌，仅在未显式配置时选择默认通道。
 * @return 显式配置或宝辰鑫、SUP26AD默认档案返回true；旧兼容通道返回false。
 */
bool UsesMappedParameters(LaserMachineHandle handle, LaserHeadBrandSdk brand);

/**
 * @brief 纯编译工艺参数脚本体，不修改资源、不执行通讯、不触发出光。
 * @param[in] handle 已注册的焊机句柄，用于获取本次调用的配置副本。
 * @param[in] resource 当前资源的只读快照，提供型号及焊接类型。
 * @param[in] craft 待转换的工艺参数；额定功率与实际焊接功率分别处理。
 * @param[out] script 成功时替换为脚本体，失败保持原字符串。
 * @param[out] diagnostic 可为空的参数转换或编码诊断。
 * @return 成功、无效句柄或工艺转换及配置错误。
 */
LaserSdkResult BuildMachineCraftScript(LaserMachineHandle handle,
                                      const MachineResource& resource,
                                      const CraftInfoLaserSdk& craft,
                                      std::string& script,
                                      LaserParameterDiagnosticSdk* diagnostic = nullptr);

/**
 * @brief 将显式参数值编译成脚本体，默认旧超强保留其兼容输出通道。
 * @param[in] handle 已注册的焊机句柄。
 * @param[in] resource 当前资源的只读快照，不作为设备已执行状态。
 * @param[in] values 本次写入的物理单位参数值，仅编译列表内的参数。
 * @param[out] script 成功时整体替换，失败不产生部分脚本。
 * @param[out] diagnostic 可为空的参数级诊断。
 * @return 成功、无效句柄或参数校验及编码错误。
 */
LaserSdkResult BuildMachineParameterScript(LaserMachineHandle handle,
                                          const MachineResource& resource,
                                          const std::vector<LaserParameterValueSdk>& values,
                                          std::string& script,
                                          LaserParameterDiagnosticSdk* diagnostic = nullptr);

/**
 * @brief 更新完整指令编译期间的兼容资源快照，供旧缓升缓降生成器读取。
 * @param[in,out] resource 外层生成作用域保护的工作快照，结束后必须恢复。
 * @param[in] craft 已完成校验的工艺参数，焊接速度必须为有限正数。
 * @warning 仅在外层资源快照保护下调用；该资源不是设备已确认状态。
 */
void UpdateCompilationResource(MachineResource& resource, const CraftInfoLaserSdk& craft);

} // namespace laser_parameters

#endif
