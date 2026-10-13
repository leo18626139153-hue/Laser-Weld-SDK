#include "cl_api.h"
#include "parametercontext.h"
#include "internaldatadefine.h"
#include "brandadapter.h"
#include "utils.h"
#include <cstring>
#include <new>

namespace {

bool validDiagnostic(LaserParameterDiagnosticSdk* diagnostic)
{
    return diagnostic == nullptr || diagnostic->structSize >= sizeof(LaserParameterDiagnosticSdk);
}

void setDiagnostic(LaserParameterDiagnosticSdk* diagnostic, LaserSdkResult result)
{
    if (diagnostic == nullptr) return;
    diagnostic->parameterId = 0;
    diagnostic->result = result;
    diagnostic->message[0] = '\0';
}

std::string standaloneParameterScript(LaserMachineHandle handle,
                                      const MachineResource& resource,
                                      const std::string& body)
{
    if (laser_parameters::UsesMappedParameters(handle, resource.laserHeadBrand)) return body;
    return GetBrandProtocolAdapter(resource.laserHeadBrand).buildInitLuaFunc() + body;
}

} // namespace

LaserSdkResult cl_parameters_configuration_set(LaserMachineHandle handle,
    const LaserParameterDescriptorSdk* descriptors, unsigned int descriptorCount,
    const LaserParameterBindingSdk* bindings, unsigned int bindingCount,
    LaserParameterDiagnosticSdk* diagnostic)
{
    if (!validDiagnostic(diagnostic)) return PARAMETER_ERROR_LASER_SDK;
    // 第一阶段：检查C接口指针及结构大小，避免读取不完整的调用方数组。
    setDiagnostic(diagnostic, SUCCESS_LASER_SDK);
    if ((descriptorCount != 0 && descriptors == nullptr)
            || (bindingCount != 0 && bindings == nullptr))
    {
        setDiagnostic(diagnostic, PARAMETER_ERROR_LASER_SDK);
        return PARAMETER_ERROR_LASER_SDK;
    }
    try
    {
        if (machineResourcesMap.find(handle) == machineResourcesMap.end())
        {
            setDiagnostic(diagnostic, HANDLE_ERROR_LASER_SDK);
            return HANDLE_ERROR_LASER_SDK;
        }
        // 先检查元素头，拒绝布局过小的调用方，避免复制后才发现 ABI 不匹配。
        for (unsigned int i = 0; i < descriptorCount; ++i)
            if (descriptors[i].structSize < sizeof(LaserParameterDescriptorSdk))
            {
                setDiagnostic(diagnostic, PARAMETER_ERROR_LASER_SDK);
                return PARAMETER_ERROR_LASER_SDK;
            }
        for (unsigned int i = 0; i < bindingCount; ++i)
            if (bindings[i].structSize < sizeof(LaserParameterBindingSdk))
            {
                setDiagnostic(diagnostic, PARAMETER_ERROR_LASER_SDK);
                return PARAMETER_ERROR_LASER_SDK;
            }
        laser_parameters::Configuration configuration;
        // 第二阶段：建立候选副本；配置内核校验成功后才安装到指定句柄。
        if (descriptorCount != 0) configuration.descriptors.assign(descriptors, descriptors + descriptorCount);
        if (bindingCount != 0) configuration.bindings.assign(bindings, bindings + bindingCount);
        return laser_parameters::SetMachineConfiguration(handle, configuration, diagnostic);
    }
    catch (const std::bad_alloc&)
    {
        setDiagnostic(diagnostic, MEMORY_ERROR_LASER_SDK);
        return MEMORY_ERROR_LASER_SDK;
    }
    catch (...)
    {
        setDiagnostic(diagnostic, OTHER_ERROR_LASER_SDK);
        return OTHER_ERROR_LASER_SDK;
    }
}

LaserSdkResult cl_parameters_configuration_get(LaserMachineHandle handle,
    LaserParameterDescriptorSdk* descriptors, unsigned int* descriptorCount,
    LaserParameterBindingSdk* bindings, unsigned int* bindingCount)
{
    if (descriptorCount == nullptr || bindingCount == nullptr) return PARAMETER_ERROR_LASER_SDK;
    try
    {
        laser_parameters::Configuration configuration;
        // 先取得完整有效配置，再统一返回两个输出数组的容量要求。
        const LaserSdkResult result = laser_parameters::GetMachineConfiguration(handle, configuration);
        if (result != SUCCESS_LASER_SDK) return result;
        const unsigned int requiredDescriptors = static_cast<unsigned int>(configuration.descriptors.size());
        const unsigned int requiredBindings = static_cast<unsigned int>(configuration.bindings.size());
        const unsigned int descriptorCapacity = descriptors == nullptr ? 0 : *descriptorCount;
        const unsigned int bindingCapacity = bindings == nullptr ? 0 : *bindingCount;
        *descriptorCount = requiredDescriptors;
        *bindingCount = requiredBindings;
        // 双空指针仅查询长度，容量检查通过前不写入任何输出数组。
        if (descriptors == nullptr && bindings == nullptr) return SUCCESS_LASER_SDK;
        if (descriptorCapacity < requiredDescriptors || bindingCapacity < requiredBindings)
            return BUFFER_TOO_SMALL_LASER_SDK;
        if (requiredDescriptors != 0)
            std::memcpy(descriptors, configuration.descriptors.data(), requiredDescriptors * sizeof(*descriptors));
        if (requiredBindings != 0)
            std::memcpy(bindings, configuration.bindings.data(), requiredBindings * sizeof(*bindings));
        return SUCCESS_LASER_SDK;
    }
    catch (const std::bad_alloc&) { return MEMORY_ERROR_LASER_SDK; }
    catch (...) { return OTHER_ERROR_LASER_SDK; }
}

LaserSdkResult cl_parameters_configuration_reset(LaserMachineHandle handle)
{
    try { return laser_parameters::ResetMachineConfiguration(handle); }
    catch (...) { return OTHER_ERROR_LASER_SDK; }
}

LaserSdkResult cl_parameters_script_get(LaserMachineHandle handle,
    const LaserParameterValueSdk* values, unsigned int valueCount,
    char* scripts, int* scriptLen, LaserParameterDiagnosticSdk* diagnostic)
{
    if (!validDiagnostic(diagnostic)) return PARAMETER_ERROR_LASER_SDK;
    setDiagnostic(diagnostic, SUCCESS_LASER_SDK);
    if (scriptLen == nullptr || (valueCount != 0 && values == nullptr))
    {
        setDiagnostic(diagnostic, PARAMETER_ERROR_LASER_SDK);
        return PARAMETER_ERROR_LASER_SDK;
    }
    const std::unordered_map<int, MachineResource>::const_iterator it = machineResourcesMap.find(handle);
    if (it == machineResourcesMap.end())
    {
        setDiagnostic(diagnostic, HANDLE_ERROR_LASER_SDK);
        return HANDLE_ERROR_LASER_SDK;
    }
    try
    {
        const MachineResource resource = it->second;
        // 资源只读快照与值数组副本保证编译失败、长度查询均不修改设备资源。
        for (unsigned int i = 0; i < valueCount; ++i)
            if (values[i].structSize < sizeof(LaserParameterValueSdk))
            {
                setDiagnostic(diagnostic, PARAMETER_ERROR_LASER_SDK);
                return PARAMETER_ERROR_LASER_SDK;
            }
        std::vector<LaserParameterValueSdk> parameterValues;
        if (valueCount != 0) parameterValues.assign(values, values + valueCount);
        std::string body;
        const LaserSdkResult result = laser_parameters::BuildMachineParameterScript(
                    handle, resource, parameterValues, body, diagnostic);
        if (result != SUCCESS_LASER_SDK) return result;
        // 类型及地址校验成功后才装配兼容初始化；最后一步才写调用方缓冲区。
        const std::string script = standaloneParameterScript(handle, resource, body);
        const LaserSdkResult copied = strToChar(scripts, scriptLen, script);
        if (copied != SUCCESS_LASER_SDK) setDiagnostic(diagnostic, copied);
        return copied;
    }
    catch (const std::bad_alloc&)
    {
        setDiagnostic(diagnostic, MEMORY_ERROR_LASER_SDK);
        return MEMORY_ERROR_LASER_SDK;
    }
    catch (...)
    {
        setDiagnostic(diagnostic, OTHER_ERROR_LASER_SDK);
        return OTHER_ERROR_LASER_SDK;
    }
}
