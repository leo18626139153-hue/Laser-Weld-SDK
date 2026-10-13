#ifndef LASER_PARAMETER_COMPILER_H
#define LASER_PARAMETER_COMPILER_H

#include "parameterdefine.h"
#include <stdint.h>
#include <vector>

/**
 * @file parametercompiler.h
 * @brief 将参数描述、地址绑定和物理值编译成不依赖通讯的寄存器写入计划。
 */

namespace laser_parameters
{

/**
 * @brief 按参数语义编号关联的描述与寄存器绑定集合。
 * @note 编译器只读使用本配置，不持有其中元素的长期指针，也不修改设备已确认状态。
 */
struct Configuration
{
    std::vector<LaserParameterDescriptorSdk> descriptors; ///< 设备支持的参数描述
    std::vector<LaserParameterBindingSdk> bindings;       ///< 参数到寄存器的绑定
};

/**
 * @brief 单个语义参数的已编码寄存器写操作。
 * @note words 保留整数补码或浮点原始位；三十二位参数的两字顺序已在编译时确定。
 */
struct WriteOperation
{
    unsigned parameterId;         ///< 稳定的参数语义编号
    unsigned address;             ///< 起始寄存器地址
    std::vector<uint16_t> words;   ///< 按描述字序排列的一或两个十六位原始值
    bool signedSingleWord;        ///< 仅十六位有符号参数为真，渲染时显示带符号数值
};

/** @brief 保留调用方值列表顺序的写入计划，不持有设备或通讯状态。 */
struct WritePlan
{
    std::vector<WriteOperation> operations; ///< 已完成校验及编码的写操作
};

/**
 * @brief 校验参数描述、绑定、地址范围及同方向寄存器重叠。
 * @param[in] configuration 参数描述与绑定，允许空配置，不修改其内容。
 * @param[in,out] diagnostic 可选诊断缓冲区；非空时 structSize 至少为 sizeof 本结构。
 * @retval SUCCESS_LASER_SDK 配置有效，成功诊断清空参数编号及消息。
 * @retval PARAMETER_ERROR_LASER_SDK 结构、编号、比例、文本、绑定能力或地址重叠无效。
 * @retval PARAM_TYPE_ERROR_LASER_SDK 编码枚举无效，或绑定没有对应的参数描述。
 * @retval PARAMETER_OUT_OF_RANGE_LASER_SDK 寄存器地址或多字占用范围越界。
 * @retval MEMORY_ERROR_LASER_SDK 校验索引分配内存失败。
 * @note 允许描述尚未绑定，也允许读写方向保留无效地址；实际写入时再检查写地址是否存在。
 *       可读和可写方向分别检查重叠，不要求配置中的所有参数都在本次写入。
 */
LaserSdkResult ValidateConfiguration(const Configuration& configuration,
                                    LaserParameterDiagnosticSdk* diagnostic = nullptr);

/**
 * @brief 将物理值列表校验并编码为寄存器写入计划。
 * @param[in] configuration 参数描述及绑定；函数仍完整校验配置，不修改其内容。
 * @param[in] values 本次需要写入的物理值，允许为空，不要求写入全部可写参数。
 * @param[in,out] plan 成功时整体替换并保持输入值顺序；任何失败均保持原计划不变。
 * @param[in,out] diagnostic 可选诊断缓冲区；非空时 structSize 至少为 sizeof 本结构。
 * @retval SUCCESS_LASER_SDK 编码成功；空值列表生成空计划并清空成功诊断。
 * @retval PARAM_WRITE_ERROR_LASER_SDK 所请求参数不支持写入。
 * @retval PARAM_TYPE_ERROR_LASER_SDK 设备未定义所请求参数，或配置编码类型无效。
 * @retval PARAMETER_ERROR_LASER_SDK 配置、结构或值编号无效，参数重复，或写地址未绑定。
 * @retval PARAMETER_OUT_OF_RANGE_LASER_SDK 地址、物理值或编码值越界，或精确整数编码失败。
 * @retval MEMORY_ERROR_LASER_SDK 计划或校验索引分配内存失败。
 * @details 先检查物理范围，再计算原始值并按整数舍入或浮点位表示编码。
 *          三十二位值按描述拆成两字；十六位有符号值保留补码并设置渲染标志。
 * @note 此接口只生成计划，不执行寄存器写入；成功不表示设备已接收参数。
 */
LaserSdkResult CompileWritePlan(const Configuration& configuration,
                               const std::vector<LaserParameterValueSdk>& values,
                               WritePlan& plan,
                               LaserParameterDiagnosticSdk* diagnostic = nullptr);

} // namespace laser_parameters

#endif // LASER_PARAMETER_COMPILER_H
