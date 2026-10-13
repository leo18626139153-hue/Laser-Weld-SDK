#include "common/parametercompiler.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <new>
#include <set>
#include <utility>

namespace laser_parameters
{
namespace
{

const unsigned MAX_REGISTER_ADDRESS = 0xFFFFu; ///< 十六位寄存器地址上限
const unsigned ACCESS_FLAGS = LASER_PARAMETER_READABLE_SDK | LASER_PARAMETER_WRITABLE_SDK; ///< 合法访问能力位

typedef std::map<unsigned, const LaserParameterDescriptorSdk*> DescriptorMap;
typedef std::map<unsigned, const LaserParameterBindingSdk*> BindingMap;

/**
 * @brief 在完整诊断缓冲区中记录结果，不写入调用方的扩展尾部。
 * @param[in,out] diagnostic 可为空；结构大小不足时不写入。
 * @param[in] parameterId 错误所属参数，整体错误或成功时为零。
 * @param[in] result 需要返回给调用方的结果码。
 * @param[in] message 以零字符结束的诊断，复制时保留结尾零字符。
 * @return 原样返回 result，便于错误分支统一填写诊断后返回。
 */
LaserSdkResult setDiagnostic(LaserParameterDiagnosticSdk* diagnostic,
                            unsigned parameterId,
                            LaserSdkResult result,
                            const char* message)
{
    if (diagnostic != nullptr && diagnostic->structSize >= sizeof(LaserParameterDiagnosticSdk))
    {
        LaserParameterDiagnosticSdk value = {};
        value.structSize = diagnostic->structSize;
        value.parameterId = parameterId;
        value.result = result;
        std::strncpy(value.message, message, sizeof(value.message) - 1);
        *diagnostic = value;
    }
    return result;
}

/**
 * @brief 获取参数编码占用的连续十六位寄存器数。
 * @param[in] dataType 参数描述中的编码类型。
 * @return 单字整数返回一，多字整数或浮点返回二，未知类型返回零。
 */
unsigned wordCount(LaserParameterDataTypeSdk dataType)
{
    switch (dataType)
    {
    case LASER_PARAMETER_U16_SDK:
    case LASER_PARAMETER_I16_SDK:
        return 1;
    case LASER_PARAMETER_U32_SDK:
    case LASER_PARAMETER_I32_SDK:
    case LASER_PARAMETER_F32_SDK:
        return 2;
    default:
        return 0;
    }
}

/**
 * @brief 校验同一读写方向的寄存器范围及占用冲突。
 * @param[in] parameterId 当前参数语义编号，用于定位错误。
 * @param[in] address 起始地址；无效地址表示该方向尚未配置。
 * @param[in] width 经编码类型校验得到的字数，只能为一或二。
 * @param[in,out] occupied 本次校验中该方向的地址占用表。
 * @param[in,out] diagnostic 可选错误诊断。
 * @return 未配置或无冲突时成功，否则返回地址越界或参数配置错误。
 * @note 失败时占用表可能已有当前参数的部分地址，调用方应丢弃本次临时索引。
 */
LaserSdkResult validateAddress(unsigned parameterId,
                               unsigned address,
                               unsigned width,
                               std::map<unsigned, unsigned>& occupied,
                               LaserParameterDiagnosticSdk* diagnostic)
{
    if (address == LASER_PARAMETER_INVALID_ADDRESS_SDK)
        return SUCCESS_LASER_SDK;
    // 使用差值判断多字占用范围，避免先做 address + width 后发生无符号溢出。
    if (address > MAX_REGISTER_ADDRESS || width - 1 > MAX_REGISTER_ADDRESS - address)
    {
        return setDiagnostic(diagnostic, parameterId, PARAMETER_OUT_OF_RANGE_LASER_SDK,
                             "参数寄存器范围超出十六位地址空间");
    }
    for (unsigned index = 0; index < width; ++index)
    {
        const unsigned currentAddress = address + index;
        if (!occupied.insert(std::make_pair(currentAddress, parameterId)).second)
        {
            return setDiagnostic(diagnostic, parameterId, PARAMETER_ERROR_LASER_SDK,
                                 "不同参数的寄存器范围发生重叠");
        }
    }
    return SUCCESS_LASER_SDK;
}

/**
 * @brief 校验配置并建立只在本次调用中使用的索引。
 * @param[in] configuration 在本次校验期间保持有效且不修改的配置。
 * @param[out] descriptors 输出语义编号到描述的临时指针索引。
 * @param[out] bindings 输出语义编号到绑定的临时指针索引。
 * @param[in,out] diagnostic 可选诊断；缓冲区过小时不写入。
 * @return 配置有效时成功，否则返回首个检测到的配置错误。
 * @note 输出指针只借用 configuration 中的元素，失败时不得使用部分索引。
 */
LaserSdkResult validateAndIndex(const Configuration& configuration,
                                DescriptorMap& descriptors,
                                BindingMap& bindings,
                                LaserParameterDiagnosticSdk* diagnostic)
{
    if (diagnostic != nullptr && diagnostic->structSize < sizeof(LaserParameterDiagnosticSdk))
        return PARAMETER_ERROR_LASER_SDK;

    // 先验证语义与编码元数据，使后续地址校验能够可靠确定占用字数。
    for (size_t index = 0; index < configuration.descriptors.size(); ++index)
    {
        const LaserParameterDescriptorSdk& descriptor = configuration.descriptors[index];
        if (descriptor.structSize < sizeof(LaserParameterDescriptorSdk)
                || descriptor.parameterId == 0)
        {
            return setDiagnostic(diagnostic, descriptor.parameterId, PARAMETER_ERROR_LASER_SDK,
                                 "参数描述结构大小或语义编号无效");
        }
        if (!descriptors.insert(std::make_pair(descriptor.parameterId, &descriptor)).second)
        {
            return setDiagnostic(diagnostic, descriptor.parameterId, PARAMETER_ERROR_LASER_SDK,
                                 "参数描述的语义编号重复");
        }
        if (wordCount(descriptor.dataType) == 0
                || (descriptor.wordOrder != LASER_PARAMETER_HIGH_FIRST_SDK
                    && descriptor.wordOrder != LASER_PARAMETER_LOW_FIRST_SDK)
                || (descriptor.rounding != LASER_PARAMETER_NEAREST_SDK
                    && descriptor.rounding != LASER_PARAMETER_TRUNCATE_SDK
                    && descriptor.rounding != LASER_PARAMETER_EXACT_SDK)
                || (descriptor.flags & ~ACCESS_FLAGS) != 0)
        {
            return setDiagnostic(diagnostic, descriptor.parameterId, PARAM_TYPE_ERROR_LASER_SDK,
                                 "参数编码、字序、舍入或能力位无效");
        }
        if (!std::isfinite(descriptor.scale) || descriptor.scale == 0.0
                || !std::isfinite(descriptor.offset)
                || !std::isfinite(descriptor.minimum) || !std::isfinite(descriptor.maximum)
                || descriptor.minimum > descriptor.maximum)
        {
            return setDiagnostic(diagnostic, descriptor.parameterId, PARAMETER_ERROR_LASER_SDK,
                                 "参数比例、偏移或物理值范围无效");
        }
        if (std::memchr(descriptor.name, '\0', sizeof(descriptor.name)) == nullptr
                || std::memchr(descriptor.unit, '\0', sizeof(descriptor.unit)) == nullptr)
        {
            return setDiagnostic(diagnostic, descriptor.parameterId, PARAMETER_ERROR_LASER_SDK,
                                 "参数名称或单位未以零字符结束");
        }
    }

    // 读写方向分别建立占用表，允许同一参数的读写地址相同。
    // 描述可以暂时没有绑定，但每个已提供的绑定必须指向合法描述并满足访问能力。
    std::map<unsigned, unsigned> writeOccupied;
    std::map<unsigned, unsigned> readOccupied;
    for (size_t index = 0; index < configuration.bindings.size(); ++index)
    {
        const LaserParameterBindingSdk& binding = configuration.bindings[index];
        if (binding.structSize < sizeof(LaserParameterBindingSdk) || binding.parameterId == 0)
        {
            return setDiagnostic(diagnostic, binding.parameterId, PARAMETER_ERROR_LASER_SDK,
                                 "参数绑定结构大小或语义编号无效");
        }
        if (!bindings.insert(std::make_pair(binding.parameterId, &binding)).second)
        {
            return setDiagnostic(diagnostic, binding.parameterId, PARAMETER_ERROR_LASER_SDK,
                                 "参数绑定的语义编号重复");
        }
        DescriptorMap::const_iterator descriptorIt = descriptors.find(binding.parameterId);
        if (descriptorIt == descriptors.end())
        {
            return setDiagnostic(diagnostic, binding.parameterId, PARAM_TYPE_ERROR_LASER_SDK,
                                 "参数绑定没有对应的参数描述");
        }
        const LaserParameterDescriptorSdk& descriptor = *descriptorIt->second;
        if ((binding.writeAddress != LASER_PARAMETER_INVALID_ADDRESS_SDK
             && (descriptor.flags & LASER_PARAMETER_WRITABLE_SDK) == 0)
                || (binding.readAddress != LASER_PARAMETER_INVALID_ADDRESS_SDK
                    && (descriptor.flags & LASER_PARAMETER_READABLE_SDK) == 0))
        {
            return setDiagnostic(diagnostic, binding.parameterId, PARAMETER_ERROR_LASER_SDK,
                                 "参数地址绑定与读写能力不匹配");
        }
        const unsigned width = wordCount(descriptor.dataType);
        LaserSdkResult result = validateAddress(binding.parameterId, binding.writeAddress,
                                               width, writeOccupied, diagnostic);
        if (result != SUCCESS_LASER_SDK)
            return result;
        result = validateAddress(binding.parameterId, binding.readAddress,
                                 width, readOccupied, diagnostic);
        if (result != SUCCESS_LASER_SDK)
            return result;
    }
    return SUCCESS_LASER_SDK;
}

/**
 * @brief 以约定字序拆分三十二位原始编码，不依赖运行主机字节序。
 * @param[in] encoded 已完成数值转换的整数或浮点原始位。
 * @param[in] wordOrder 已校验的寄存器字序。
 * @param[in,out] words 依次追加两个十六位字，不交换单个寄存器内部的字节。
 */
void append32BitWords(uint32_t encoded,
                      LaserParameterWordOrderSdk wordOrder,
                      std::vector<uint16_t>& words)
{
    const uint16_t high = static_cast<uint16_t>((encoded >> 16) & 0xFFFFu);
    const uint16_t low = static_cast<uint16_t>(encoded & 0xFFFFu);
    if (wordOrder == LASER_PARAMETER_HIGH_FIRST_SDK)
    {
        words.push_back(high);
        words.push_back(low);
    }
    else
    {
        words.push_back(low);
        words.push_back(high);
    }
}

/**
 * @brief 将单个物理值编码为临时写操作的寄存器原始字。
 * @param[in] descriptor 已通过配置校验的类型、比例、范围和舍入规则。
 * @param[in] physicalValue 与描述单位一致的物理值，必须为有限数且在闭区间范围内。
 * @param[in,out] operation 本次临时操作，追加编码字并设置单字符号标志。
 * @param[in,out] diagnostic 可选参数级诊断。
 * @return 编码成功或物理范围、编码溢出、精确整数转换、平台浮点类型错误。
 * @note 失败时临时操作不得提交；整数范围检查在有符号或无符号强制转换之前完成。
 */
LaserSdkResult encodeValue(const LaserParameterDescriptorSdk& descriptor,
                           double physicalValue,
                           WriteOperation& operation,
                           LaserParameterDiagnosticSdk* diagnostic)
{
    // 第一阶段检查物理值，再计算原始值；两处均拒绝非有限数。
    if (!std::isfinite(physicalValue)
            || physicalValue < descriptor.minimum || physicalValue > descriptor.maximum)
    {
        return setDiagnostic(diagnostic, descriptor.parameterId, PARAMETER_OUT_OF_RANGE_LASER_SDK,
                             "参数物理值不是有限数或超出允许范围");
    }
    const double rawValue = physicalValue * descriptor.scale + descriptor.offset;
    if (!std::isfinite(rawValue))
    {
        return setDiagnostic(diagnostic, descriptor.parameterId, PARAMETER_OUT_OF_RANGE_LASER_SDK,
                             "参数缩放后的编码值溢出");
    }
    operation.signedSingleWord = descriptor.dataType == LASER_PARAMETER_I16_SDK;
    if (descriptor.dataType == LASER_PARAMETER_F32_SDK)
    {
        // 浮点编码使用单精度位表示，不经过整数舍入；按原始位拆字以保留符号和小数。
        if (!std::numeric_limits<float>::is_iec559 || sizeof(float) != sizeof(uint32_t))
        {
            return setDiagnostic(diagnostic, descriptor.parameterId, PARAM_TYPE_ERROR_LASER_SDK,
                                 "当前平台不支持三十二位 IEEE 浮点编码");
        }
        if (std::fabs(rawValue) > static_cast<double>((std::numeric_limits<float>::max)()))
        {
            return setDiagnostic(diagnostic, descriptor.parameterId, PARAMETER_OUT_OF_RANGE_LASER_SDK,
                                 "参数超出三十二位浮点数编码范围");
        }
        const float floatValue = static_cast<float>(rawValue);
        if (!std::isfinite(floatValue))
        {
            return setDiagnostic(diagnostic, descriptor.parameterId, PARAMETER_OUT_OF_RANGE_LASER_SDK,
                                 "浮点数编码结果无效");
        }
        uint32_t encoded = 0;
        std::memcpy(&encoded, &floatValue, sizeof(encoded));
        append32BitWords(encoded, descriptor.wordOrder, operation.words);
        return SUCCESS_LASER_SDK;
    }

    // 整数编码先按描述取整，再检查该类型的可表示范围，禁止溢出后截断。
    double encodedValue = 0.0;
    switch (descriptor.rounding)
    {
    case LASER_PARAMETER_NEAREST_SDK:
        encodedValue = std::round(rawValue);
        break;
    case LASER_PARAMETER_TRUNCATE_SDK:
        encodedValue = std::trunc(rawValue);
        break;
    case LASER_PARAMETER_EXACT_SDK:
    {
        encodedValue = std::round(rawValue);
        // 仅容忍比例运算带来的机器精度误差，不接受有业务含义的小数截断。
        const double tolerance = std::numeric_limits<double>::epsilon()
                * (std::max)(1.0, std::fabs(rawValue)) * 4.0;
        if (std::fabs(rawValue - encodedValue) > tolerance)
        {
            return setDiagnostic(diagnostic, descriptor.parameterId, PARAMETER_OUT_OF_RANGE_LASER_SDK,
                                 "参数无法按精确整数策略编码");
        }
        break;
    }
    default:
        return setDiagnostic(diagnostic, descriptor.parameterId, PARAM_TYPE_ERROR_LASER_SDK,
                             "参数整数舍入策略无效");
    }

    double lower = 0.0;
    double upper = 0.0;
    switch (descriptor.dataType)
    {
    case LASER_PARAMETER_U16_SDK:
        upper = 65535.0;
        break;
    case LASER_PARAMETER_I16_SDK:
        lower = -32768.0;
        upper = 32767.0;
        break;
    case LASER_PARAMETER_U32_SDK:
        upper = 4294967295.0;
        break;
    case LASER_PARAMETER_I32_SDK:
        lower = -2147483648.0;
        upper = 2147483647.0;
        break;
    default:
        return setDiagnostic(diagnostic, descriptor.parameterId, PARAM_TYPE_ERROR_LASER_SDK,
                             "参数整数编码类型无效");
    }
    if (encodedValue < lower || encodedValue > upper)
    {
        return setDiagnostic(diagnostic, descriptor.parameterId, PARAMETER_OUT_OF_RANGE_LASER_SDK,
                             "参数超出指定整数编码范围");
    }

    if (descriptor.dataType == LASER_PARAMETER_U16_SDK)
        operation.words.push_back(static_cast<uint16_t>(encodedValue));
    else if (descriptor.dataType == LASER_PARAMETER_I16_SDK)
        operation.words.push_back(static_cast<uint16_t>(static_cast<int16_t>(encodedValue)));
    else if (descriptor.dataType == LASER_PARAMETER_U32_SDK)
        append32BitWords(static_cast<uint32_t>(encodedValue), descriptor.wordOrder, operation.words);
    else
        append32BitWords(static_cast<uint32_t>(static_cast<int32_t>(encodedValue)),
                         descriptor.wordOrder, operation.words);
    return SUCCESS_LASER_SDK;
}

} // namespace

/** @brief 使用局部索引验证配置，成功时清空旧诊断，不保留任何设备或调用状态。 */
LaserSdkResult ValidateConfiguration(const Configuration& configuration,
                                    LaserParameterDiagnosticSdk* diagnostic)
{
    try
    {
        DescriptorMap descriptors;
        BindingMap bindings;
        const LaserSdkResult result = validateAndIndex(configuration, descriptors, bindings, diagnostic);
        if (result != SUCCESS_LASER_SDK)
            return result;
        return setDiagnostic(diagnostic, 0, SUCCESS_LASER_SDK, "");
    }
    catch (const std::bad_alloc&)
    {
        return setDiagnostic(diagnostic, 0, MEMORY_ERROR_LASER_SDK, "参数配置校验内存不足");
    }
}

/** @brief 通过候选计划完成整批参数编译；接口参数与错误契约见 parametercompiler.h。 */
LaserSdkResult CompileWritePlan(const Configuration& configuration,
                               const std::vector<LaserParameterValueSdk>& values,
                               WritePlan& plan,
                               LaserParameterDiagnosticSdk* diagnostic)
{
    try
    {
        // 第一阶段验证完整配置；只读配置和未绑定描述不会在此阶段被强制写入。
        DescriptorMap descriptors;
        BindingMap bindings;
        const LaserSdkResult configurationResult =
                validateAndIndex(configuration, descriptors, bindings, diagnostic);
        if (configurationResult != SUCCESS_LASER_SDK)
            return configurationResult;

        // 第二阶段按本次值列表选择参数，校验能力和绑定后编码，所有结果只进入候选计划。
        WritePlan pendingPlan;
        std::set<unsigned> valueIds;
        pendingPlan.operations.reserve(values.size());
        for (size_t index = 0; index < values.size(); ++index)
        {
            const LaserParameterValueSdk& value = values[index];
            if (value.structSize < sizeof(LaserParameterValueSdk) || value.parameterId == 0)
            {
                return setDiagnostic(diagnostic, value.parameterId, PARAMETER_ERROR_LASER_SDK,
                                     "参数值结构大小或语义编号无效");
            }
            if (!valueIds.insert(value.parameterId).second)
            {
                return setDiagnostic(diagnostic, value.parameterId, PARAMETER_ERROR_LASER_SDK,
                                     "本次写入列表含重复参数");
            }
            DescriptorMap::const_iterator descriptorIt = descriptors.find(value.parameterId);
            if (descriptorIt == descriptors.end())
            {
                return setDiagnostic(diagnostic, value.parameterId, PARAM_TYPE_ERROR_LASER_SDK,
                                     "当前设备未定义该参数");
            }
            const LaserParameterDescriptorSdk& descriptor = *descriptorIt->second;
            if ((descriptor.flags & LASER_PARAMETER_WRITABLE_SDK) == 0)
            {
                return setDiagnostic(diagnostic, value.parameterId, PARAM_WRITE_ERROR_LASER_SDK,
                                     "当前设备参数不支持写入");
            }
            BindingMap::const_iterator bindingIt = bindings.find(value.parameterId);
            if (bindingIt == bindings.end()
                    || bindingIt->second->writeAddress == LASER_PARAMETER_INVALID_ADDRESS_SDK)
            {
                return setDiagnostic(diagnostic, value.parameterId, PARAMETER_ERROR_LASER_SDK,
                                     "本次写入参数尚未配置写地址");
            }
            WriteOperation operation = {};
            operation.parameterId = value.parameterId;
            operation.address = bindingIt->second->writeAddress;
            const LaserSdkResult result = encodeValue(descriptor, value.value, operation, diagnostic);
            if (result != SUCCESS_LASER_SDK)
                return result;
            pendingPlan.operations.push_back(operation);
        }
        // 第三阶段全部校验通过后一次性交付；此前任一错误均不会覆盖调用方原计划。
        plan.operations.swap(pendingPlan.operations);
        return setDiagnostic(diagnostic, 0, SUCCESS_LASER_SDK, "");
    }
    catch (const std::bad_alloc&)
    {
        return setDiagnostic(diagnostic, 0, MEMORY_ERROR_LASER_SDK, "参数写入计划生成内存不足");
    }
}

} // namespace laser_parameters
