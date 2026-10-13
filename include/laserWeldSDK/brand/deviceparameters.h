#ifndef LASER_DEVICEPARAMETERS_H
#define LASER_DEVICEPARAMETERS_H

#include "cl_basedefine.h"
#include "parameterdefine.h"
#include "common/parametercompiler.h"
#include <string>
#include <vector>

namespace laser_parameters
{

/**
 * @brief 构建指定设备的兼容参数描述与地址绑定。
 * 宝辰鑫和 SUP26AD 的默认绑定是机器人本地映射槽，不是设备远端寄存器。
 * 旧超强严格读取工艺实际使用的资源地址；手动控制地址不参与默认工艺配置。
 * 欧斯睿普仅保留原 SDK 的超强兼容别名，不代表其独立协议已验证。
 * @param[in] resource 设备资源；地址零合法，自熔焊不要求配置未使用的送丝机地址。
 * @param[out] configuration 成功时整体替换配置；失败时保持原值。
 * @param[out] diagnostic 可选错误诊断，非空时 structSize 至少为当前结构大小。
 * @return 成功、配置错误或范围错误；不修改资源或连接状态。
 */
LaserSdkResult BuildDefaultConfiguration(const MachineResource& resource,
                                        Configuration& configuration,
                                        LaserParameterDiagnosticSdk* diagnostic = nullptr);

/**
 * @brief 将工艺转换为品牌规定的业务参数值，不参与串口或从站配置。
 * 工艺值仍使用描述规定的物理单位，缩放和整数舍入由后续参数编译器执行。
 * 超强开关光瓦数转换为焊接功率百分比；宝辰鑫保留瓦特值。
 * @param[in] resource 设备型号与焊接类型，不依赖提前写入资源参数缓存。
 * @param[in] craft 工艺原始值；回抽长度保持原值，不作公共二十毫米限幅。
 * @param[out] values 成功时整体替换业务值列表；失败时保持原值。
 * @param[out] diagnostic 可选错误诊断，非空时须提供有效 structSize。
 * @return 成功或换算前提错误；具体参数的可写性、范围和编码由编译器校验。
 */
LaserSdkResult BuildCraftValues(const MachineResource& resource,
                               const CraftInfoLaserSdk& craft,
                               std::vector<LaserParameterValueSdk>& values,
                               LaserParameterDiagnosticSdk* diagnostic = nullptr);

/**
 * @brief 根据参数写计划生成旧超强自由通讯兼容脚本。
 * 仅输出调用语句，不包含初始化函数定义；调用方负责加载旧通讯函数。
 * 站号、串口和回执策略仅为原自由通讯路径保留，不进入公共参数描述。
 * @param[in] plan 已完成编码与地址校验的写计划。
 * @param[in] resource 旧超强通信路由或原 SDK 已有欧斯睿普兼容别名。
 * @param[out] script 成功时整体替换脚本体；失败时保持原值。
 * @param[out] diagnostic 可选错误诊断，非空时须提供有效 structSize。
 * @return 成功、设备路由错误或输出内存错误；不产生数字输出启停动作。
 */
LaserSdkResult EmitLegacySupLua(const WritePlan& plan,
                               const MachineResource& resource,
                               std::string& script,
                               LaserParameterDiagnosticSdk* diagnostic = nullptr);

/**
 * @brief 编译业务参数并生成设备脚本，显式配置统一使用本地映射寄存器。
 * 显式配置只产生本地寄存器写入，由上位机配置串口、主从站和远端映射。
 * 未提供显式配置时，旧超强和欧斯睿普兼容别名保留自由通讯，其余使用本地映射。
 * @param[in] resource 设备资源。
 * @param[in] overrideConfiguration 上位机提供的有效配置；空指针使用兼容默认配置。
 * @param[in] values 按参数语义提供的业务值，包括调用方声明的自定义参数。
 * @param[out] script 成功时整体替换脚本体，不包含函数定义；失败时保持原值。
 * @param[out] diagnostic 可选错误诊断，非空时须提供有效 structSize。
 * @return 成功、配置、范围、能力或编码错误；不改变资源或注册配置。
 */
LaserSdkResult BuildDeviceParametersScript(const MachineResource& resource,
                                          const Configuration* overrideConfiguration,
                                          const std::vector<LaserParameterValueSdk>& values,
                                          std::string& script,
                                          LaserParameterDiagnosticSdk* diagnostic = nullptr);

/**
 * @brief 将工艺转换、能力校验、地址绑定与脚本输出组成无副作用流程。
 * 必须具备可写的焊接功率、占空比和扫描宽度；其余工艺值仅选择配置中的可写项。
 * 默认配置按原型号检查工艺；显式配置使用调用方描述的范围，保留必要的业务换算前提。
 * @param[in] resource 设备资源，不修改全局设备参数缓存。
 * @param[in] craft 工艺原始值。
 * @param[in] overrideConfiguration 可选上位机配置，允许调整地址和增减可选参数。
 * @param[out] script 成功时整体替换脚本体；失败时保持原值。
 * @param[out] diagnostic 可选错误诊断，非空时须提供有效 structSize。
 * @return 成功或工艺编译错误；工艺配置和起收弧控制分别生成。
 */
LaserSdkResult BuildDeviceCraftScript(const MachineResource& resource,
                                     const CraftInfoLaserSdk& craft,
                                     const Configuration* overrideConfiguration,
                                     std::string& script,
                                     LaserParameterDiagnosticSdk* diagnostic = nullptr);

} // 参数编译命名空间 laser_parameters

#endif // LASER_DEVICEPARAMETERS_H
