#include "parametercontext.h"
#include "deviceparameters.h"
#include "internaldatadefine.h"
#include <map>
#include <mutex>

namespace laser_parameters {
namespace {

std::map<LaserMachineHandle, Configuration> configurations;
std::mutex configurationMutex;

/** @brief 在配置锁内生成独立副本，编译过程不持有内部容器引用或长时间占锁。 */
bool copyOverride(LaserMachineHandle handle, Configuration& configuration)
{
    std::lock_guard<std::mutex> lock(configurationMutex);
    const std::map<LaserMachineHandle, Configuration>::const_iterator it = configurations.find(handle);
    if (it == configurations.end()) return false;
    configuration = it->second;
    return true;
}

} // namespace

LaserSdkResult SetMachineConfiguration(LaserMachineHandle handle,
                                      const Configuration& configuration,
                                      LaserParameterDiagnosticSdk* diagnostic)
{
    if (machineResourcesMap.find(handle) == machineResourcesMap.end())
        return HANDLE_ERROR_LASER_SDK;
    // 第一阶段：先校验候选配置，失败不改变现有句柄配置。
    const LaserSdkResult result = ValidateConfiguration(configuration, diagnostic);
    if (result != SUCCESS_LASER_SDK) return result;
    Configuration copy(configuration);
    // 第二阶段：完整副本准备好后才进入配置锁，原子替换该句柄的编译配置。
    std::lock_guard<std::mutex> lock(configurationMutex);
    configurations[handle] = std::move(copy);
    return SUCCESS_LASER_SDK;
}

LaserSdkResult GetMachineConfiguration(LaserMachineHandle handle,
                                      Configuration& configuration,
                                      bool* explicitlyConfigured,
                                      LaserParameterDiagnosticSdk* diagnostic)
{
    const std::unordered_map<int, MachineResource>::const_iterator it = machineResourcesMap.find(handle);
    if (it == machineResourcesMap.end()) return HANDLE_ERROR_LASER_SDK;
    Configuration candidate;
    // 配置来源只选择一次：优先显式绑定，缺省时才从设备档案构建。
    const bool hasOverride = copyOverride(handle, candidate);
    if (!hasOverride)
    {
        const LaserSdkResult result = BuildDefaultConfiguration(it->second, candidate, diagnostic);
        if (result != SUCCESS_LASER_SDK) return result;
    }
    else
    {
        const LaserSdkResult result = ValidateConfiguration(candidate, diagnostic);
        if (result != SUCCESS_LASER_SDK) return result;
    }
    // 全部校验成功后统一交付，避免失败调用暴露半份配置。
    configuration = std::move(candidate);
    if (explicitlyConfigured != nullptr) *explicitlyConfigured = hasOverride;
    return SUCCESS_LASER_SDK;
}

LaserSdkResult ResetMachineConfiguration(LaserMachineHandle handle)
{
    if (machineResourcesMap.find(handle) == machineResourcesMap.end())
        return HANDLE_ERROR_LASER_SDK;
    ReleaseMachineConfiguration(handle);
    return SUCCESS_LASER_SDK;
}

void ReleaseMachineConfiguration(LaserMachineHandle handle)
{
    std::lock_guard<std::mutex> lock(configurationMutex);
    configurations.erase(handle);
}

bool UsesMappedParameters(LaserMachineHandle handle, LaserHeadBrandSdk brand)
{
    {
        std::lock_guard<std::mutex> lock(configurationMutex);
        if (configurations.find(handle) != configurations.end()) return true;
    }
    return brand == BCXLASERPOWER_SDK || brand == CQ_SUP26AD_LASERPOWER_SDK;
}

LaserSdkResult BuildMachineCraftScript(LaserMachineHandle handle,
                                      const MachineResource& resource,
                                      const CraftInfoLaserSdk& craft,
                                      std::string& script,
                                      LaserParameterDiagnosticSdk* diagnostic)
{
    if (machineResourcesMap.find(handle) == machineResourcesMap.end())
        return HANDLE_ERROR_LASER_SDK;
    Configuration configured;
    const bool hasOverride = copyOverride(handle, configured);
    // 品牌换算、参数编码和脚本输出均只使用本次局部副本。
    return BuildDeviceCraftScript(resource, craft, hasOverride ? &configured : nullptr, script, diagnostic);
}

LaserSdkResult BuildMachineParameterScript(LaserMachineHandle handle,
                                          const MachineResource& resource,
                                          const std::vector<LaserParameterValueSdk>& values,
                                          std::string& script,
                                          LaserParameterDiagnosticSdk* diagnostic)
{
    if (machineResourcesMap.find(handle) == machineResourcesMap.end())
        return HANDLE_ERROR_LASER_SDK;
    Configuration configured;
    const bool hasOverride = copyOverride(handle, configured);
    return BuildDeviceParametersScript(resource, hasOverride ? &configured : nullptr, values, script, diagnostic);
}

void UpdateCompilationResource(MachineResource& resource, const CraftInfoLaserSdk& craft)
{
    // 兼容旧控制生成器：只更新本次编译的工作快照，不表示板卡已经执行。
    CgxiLaserPowerLaserSdk& power = resource.laserPowerParams;
    power.laserPower = craft.craftLaserPower;
    power.laserPowerOpen = craft.craftLeadInPower;
    power.laserPowerClose = craft.craftLeadOutPower;
    power.laserRampUpTime = craft.craftLeadInRampTime;
    power.laserRampDownTime = craft.craftLeadOutRampDistance * 1000.0 / craft.craftWeldingSpeed;
    power.gasStartupAdvanceTime = craft.craftPreGasTime;
    power.gasShutoffDelayTime = craft.craftPostGasTime;
    power.laserHeadYEnlarge = craft.craftYAxisZoom;
    power.laserHeadXEnlarge = craft.craftXAxisZoom;
    power.laserHeadYOffset = craft.craftYAxisOffset;
    power.laserHeadXOffset = craft.craftXAxisOffset;
    power.spotWeldType = craft.craftWeldType;
    power.spotWeldDurationTime = craft.craftSpotWeldDuration;
    power.spotWeldIntervalTime = craft.craftSpotWeldInterval;
    power.weldMode = craft.craftweldMode;
    CgxiLaserHeadLaserSdk& head = resource.laserHeadParams;
    // 旧缓升缓降生成器把此字段作为目标功率；额定限制已由参数编译器单独校验。
    head.laserHeadPeakPower = craft.craftLaserPower;
    head.laserHeadScanSpeed = craft.craftScanSpeed;
    head.laserHeadScanWidth = craft.craftScanWidth;
    head.laserHeadScanHeight = craft.craftScanHeight;
    head.laserHeadSpinAngle = craft.craftRotationAngle;
    head.laserHeadDutyCycle = craft.laserHeadDutyCycle;
    head.laserHeadLowPulseFrequency = craft.laserHeadLowPulseFrequency;
    head.laserHeadHighPulseFrequency = craft.laserHeadHighPulseFrequency;
    head.laserSpotPattern = craft.graphicsSwitching;
    CgxiWireFeederLaserSdk& wire = resource.wireFeederParams;
    wire.lengthBack = 0.0;
    wire.wireFeederWaitTime = craft.wireDelayTime;
    wire.wireFeederBackLength = craft.craftRetractionLength;
    wire.wireFeederFeedLength = craft.craftEndLoopbackLength;
    wire.wireFeederMotorSpeed = craft.craftWeldingWireFeed;
    wire.wireFeederStartMotorSpeed = craft.craftLeadInWireFeed;
    wire.wireFeederStopMotorSpeed = craft.craftLeadOutWireFeed;
    wire.craftWireCutMode = craft.craftWireCutMode;
}

} // namespace laser_parameters
