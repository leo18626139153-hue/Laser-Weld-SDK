#include "brand/deviceparameters.h"
#include "brand/brandadapter.h"
#include "common/parameterscript.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <locale>
#include <sstream>

namespace laser_parameters
{
namespace
{

/**
 * @brief 在调用方诊断结构容量有效时填写结果，不越界访问旧结构。
 * @param diagnostic 可选诊断缓冲区。
 * @param id 失败参数编号；零表示整体结果。
 * @param message 以零字符结尾的中文诊断。
 * @param result 原始结果码，也用于清空成功诊断。
 * @return 原始结果码，不改变脚本或写计划。
 */
LaserSdkResult fail(LaserParameterDiagnosticSdk* diagnostic, unsigned id,
                    const char* message, LaserSdkResult result = PARAMETER_ERROR_LASER_SDK)
{
    if (diagnostic && diagnostic->structSize >= sizeof(LaserParameterDiagnosticSdk))
    {
        diagnostic->parameterId = id;
        diagnostic->result = result;
        std::strncpy(diagnostic->message, message, sizeof(diagnostic->message) - 1);
        diagnostic->message[sizeof(diagnostic->message) - 1] = '\0';
    }
    return result;
}

/** @brief 欧斯睿普沿用原 SDK 的超强兼容映射，不代表其独立协议已验证。 */
bool isLegacySupBrand(LaserHeadBrandSdk brand)
{
    return brand == CQLASERPOWER_SDK || brand == OSRPLASERPOWER_SDK;
}

/** @brief 仅接纳已有设备策略和原有兼容别名，未知品牌不猜测地址。 */
bool supportedBrand(LaserHeadBrandSdk brand)
{
    return isLegacySupBrand(brand) || brand == BCXLASERPOWER_SDK
            || brand == CQ_SUP26AD_LASERPOWER_SDK;
}

/**
 * @brief 同时建立物理参数描述和写地址绑定，地址零是合法寄存器。
 * @param configuration 当前工厂构建的临时配置。
 * @param id 稳定的参数语义编号，不作为寄存器地址使用。
 * @param name 中文参数名。
 * @param unit 业务物理单位。
 * @param address 实际写地址；是否为本地槽由工厂对应的输出方式确定。
 * @param minimum 最小物理值。
 * @param maximum 最大物理值。
 * @param scale 物理值到寄存器原始值的放大比例。
 * @param type 数据编码类型。
 * @param rounding 默认整数舍入规则，可由显式配置覆盖。
 * @return 无；初始只声明写能力，确有回读映射时再补读地址。
 */
void add(Configuration& configuration, unsigned id, const char* name, const char* unit,
         unsigned address, double minimum, double maximum, double scale = 1.0,
         LaserParameterDataTypeSdk type = LASER_PARAMETER_U16_SDK,
         LaserParameterRoundingSdk rounding = LASER_PARAMETER_NEAREST_SDK)
{
    LaserParameterDescriptorSdk descriptor = {};
    descriptor.structSize = sizeof(descriptor);
    descriptor.parameterId = id;
    descriptor.dataType = type;
    descriptor.wordOrder = LASER_PARAMETER_HIGH_FIRST_SDK;
    descriptor.rounding = rounding;
    descriptor.scale = scale;
    descriptor.minimum = minimum;
    descriptor.maximum = maximum;
    descriptor.flags = LASER_PARAMETER_WRITABLE_SDK;
    std::strncpy(descriptor.name, name, sizeof(descriptor.name) - 1);
    std::strncpy(descriptor.unit, unit, sizeof(descriptor.unit) - 1);
    configuration.descriptors.push_back(descriptor);

    LaserParameterBindingSdk binding = {};
    binding.structSize = sizeof(binding);
    binding.parameterId = id;
    binding.writeAddress = address;
    binding.readAddress = LASER_PARAMETER_INVALID_ADDRESS_SDK;
    configuration.bindings.push_back(binding);
}

/** @brief 保存未编码的物理值，避免业务换算提前截断成十六位整数。 */
void value(std::vector<LaserParameterValueSdk>& values, unsigned id, double physicalValue)
{
    LaserParameterValueSdk parameter = {};
    parameter.structSize = sizeof(parameter);
    parameter.parameterId = id;
    parameter.value = physicalValue;
    values.push_back(parameter);
}

/** @brief 工艺自动下发只选择配置声明可写的参数，保留可选只读参数。 */
bool writable(const Configuration& configuration, unsigned id)
{
    for (std::size_t i = 0; i < configuration.descriptors.size(); ++i)
        if (configuration.descriptors[i].parameterId == id)
            return (configuration.descriptors[i].flags & LASER_PARAMETER_WRITABLE_SDK) != 0;
    return false;
}

/**
 * @brief 构建已兼容宝辰鑫应用的本地映射槽，语义编号不依赖槽地址。
 * @param configuration 追加二十四个工艺项及已有布尔控制项的临时配置。
 * @return 无；主从站、轮询及设备远端寄存器配置由上位机负责。
 */
void buildBcx(Configuration& configuration)
{
    add(configuration, LASER_PARAM_RATED_POWER_SDK, "额定功率", "W", 60, 1, 65535);
    add(configuration, LASER_PARAM_X_OFFSET_SDK, "横向偏移", "mm", 61, -15, 15, 100, LASER_PARAMETER_I16_SDK);
    add(configuration, LASER_PARAM_Y_OFFSET_SDK, "纵向偏移", "mm", 62, -15, 15, 100, LASER_PARAMETER_I16_SDK);
    add(configuration, LASER_PARAM_WELD_POWER_SDK, "焊接功率", "W", 63, 0, 65535);
    add(configuration, LASER_PARAM_PWM_LOW_WORD_SDK, "脉冲频率低字", "Hz", 64, 0, 65535);
    add(configuration, LASER_PARAM_DUTY_CYCLE_SDK, "占空比", "%", 65, 0, 100);
    add(configuration, LASER_PARAM_LASER_ON_POWER_SDK, "开光功率", "W", 66, 0, 65535);
    add(configuration, LASER_PARAM_LASER_OFF_POWER_SDK, "关光功率", "W", 67, 0, 65535);
    add(configuration, LASER_PARAM_RAMP_UP_TIME_SDK, "缓升时间", "ms", 68, 0, 9999);
    add(configuration, LASER_PARAM_RAMP_DOWN_TIME_SDK, "缓降时间", "ms", 69, 0, 9999);
    add(configuration, LASER_PARAM_PRE_GAS_TIME_SDK, "提前送气", "ms", 70, 0, 65535);
    add(configuration, LASER_PARAM_POST_GAS_TIME_SDK, "滞后送气", "ms", 71, 0, 65535);
    add(configuration, LASER_PARAM_STEP_EQUIVALENT_SDK, "步进当量", "P/mm", 72, 0, 65535);
    add(configuration, LASER_PARAM_WIRE_FEED_SPEED_SDK, "送丝速度", "mm/s", 73, 0, 65535);
    add(configuration, LASER_PARAM_WIRE_FEED_DELAY_SDK, "补丝延时", "ms", 74, 0, 2000);
    add(configuration, LASER_PARAM_WIRE_RETRACT_SPEED_SDK, "抽补丝速度", "mm/s", 75, 0, 65535);
    add(configuration, LASER_PARAM_WIRE_RETRACT_LENGTH_SDK, "回抽长度", "mm", 76, 0, 65535);
    add(configuration, LASER_PARAM_WIRE_FEED_LENGTH_SDK, "补丝长度", "mm", 77, 0, 65535);
    add(configuration, LASER_PARAM_SINGLE_STEP_DISTANCE_SDK, "单步距离", "mm", 78, 0, 65535);
    add(configuration, LASER_PARAM_SCAN_WIDTH_SDK, "摆动幅度", "mm", 79, 0, 655.35, 100);
    add(configuration, LASER_PARAM_SPOT_PATTERN_SDK, "摆动图形", "", 80, 0, 6);
    add(configuration, LASER_PARAM_SCAN_FREQUENCY_SDK, "摆动频率", "Hz", 81, 0, 65535);
    add(configuration, LASER_PARAM_X_SCALE_SDK, "横向比例", "", 82, 0, 10, 100);
    add(configuration, LASER_PARAM_Y_SCALE_SDK, "纵向比例", "", 83, 0, 10, 100);
    // 工艺回读槽由上位机现有映射提供；不得把远端设备地址当作本地槽。
    for (std::size_t i = 0; i < configuration.bindings.size(); ++i)
    {
        configuration.bindings[i].readAddress = configuration.bindings[i].writeAddress + 47;
        configuration.descriptors[i].flags |= LASER_PARAMETER_READABLE_SDK;
        // 宝辰鑫历史偏移使用四舍五入，其它整数及缩放值沿用向零截断。
        const unsigned id = configuration.descriptors[i].parameterId;
        if (id != LASER_PARAM_X_OFFSET_SDK && id != LASER_PARAM_Y_OFFSET_SDK)
            configuration.descriptors[i].rounding = LASER_PARAMETER_TRUNCATE_SDK;
    }
    add(configuration, LASER_PARAM_GAS_VALVE_SWITCH_SDK, "气阀开关", "", 94, 0, 1);
    add(configuration, LASER_PARAM_LASER_ENABLE_SWITCH_SDK, "激光使能", "", 92, 0, 1);
}

/**
 * @brief 构建已接入 SUP26AD 应用的本地写槽和回读槽。
 * @param configuration 追加二十七个工艺项的临时配置。
 * @return 无；手动气光的私有命令值不推测为通用布尔开关。
 */
void buildSup26Ad(Configuration& configuration)
{
    add(configuration, LASER_PARAM_Y_SCALE_SDK, "纵向比例", "", 60, 0.01, 4, 100);
    add(configuration, LASER_PARAM_Y_OFFSET_SDK, "纵向偏移", "mm", 61, -3, 3, 100, LASER_PARAMETER_I16_SDK);
    add(configuration, LASER_PARAM_X_SCALE_SDK, "横向比例", "", 62, 0.01, 4, 100);
    add(configuration, LASER_PARAM_X_OFFSET_SDK, "横向偏移", "mm", 63, -3, 3, 100, LASER_PARAMETER_I16_SDK);
    add(configuration, LASER_PARAM_WELD_MODE_SDK, "焊接模式", "", 64, 0, 1);
    add(configuration, LASER_PARAM_PRE_GAS_TIME_SDK, "提前送气", "ms", 65, 0, 3000);
    add(configuration, LASER_PARAM_POST_GAS_TIME_SDK, "滞后送气", "ms", 66, 0, 3000);
    add(configuration, LASER_PARAM_SPOT_DURATION_SDK, "点焊持续", "ms", 67, 0, 60000);
    add(configuration, LASER_PARAM_SPOT_INTERVAL_SDK, "点焊间隔", "ms", 68, 0, 60000);
    add(configuration, LASER_PARAM_EXPERT_MODE_SDK, "专家模式", "", 69, 0, 1);
    add(configuration, LASER_PARAM_SPOT_PATTERN_SDK, "光斑图形", "", 70, 0, 6);
    add(configuration, LASER_PARAM_SCAN_FREQUENCY_SDK, "扫描频率", "Hz", 71, 10, 500);
    add(configuration, LASER_PARAM_SCAN_WIDTH_SDK, "扫描宽度", "mm", 72, 0.1, 5, 10);
    add(configuration, LASER_PARAM_SCAN_HEIGHT_SDK, "扫描高度", "mm", 73, 0.1, 5, 10);
    add(configuration, LASER_PARAM_ROTATION_ANGLE_SDK, "旋转角度", "deg", 74, 0, 360);
    add(configuration, LASER_PARAM_WELD_POWER_SDK, "焊接功率", "W", 75, 1, 65535);
    add(configuration, LASER_PARAM_DUTY_CYCLE_SDK, "占空比", "%", 76, 0, 100);
    add(configuration, LASER_PARAM_PWM_HIGH_WORD_SDK, "脉冲频率高字", "", 77, 0, 65535);
    add(configuration, LASER_PARAM_PWM_LOW_WORD_SDK, "脉冲频率低字", "", 78, 0, 65535);
    add(configuration, LASER_PARAM_WIRE_FEED_SPEED_SDK, "送丝速度", "cm/min", 79, 0, 600);
    add(configuration, LASER_PARAM_WIRE_FEED_SWITCH_SDK, "送丝开关", "", 80, 0, 1);
    add(configuration, LASER_PARAM_WIRE_RETRACT_LENGTH_SDK, "回抽长度", "mm", 81, 0, 200);
    add(configuration, LASER_PARAM_WIRE_FEED_LENGTH_SDK, "补丝长度", "mm", 82, 0, 200);
    add(configuration, LASER_PARAM_LASER_ON_POWER_SDK, "开光百分比", "%", 83, 0, 100, 1,
        LASER_PARAMETER_U16_SDK, LASER_PARAMETER_TRUNCATE_SDK);
    add(configuration, LASER_PARAM_RAMP_UP_TIME_SDK, "缓升时间", "ms", 84, 0, 2000);
    add(configuration, LASER_PARAM_LASER_OFF_POWER_SDK, "关光百分比", "%", 85, 0, 100, 1,
        LASER_PARAMETER_U16_SDK, LASER_PARAMETER_TRUNCATE_SDK);
    add(configuration, LASER_PARAM_RAMP_DOWN_TIME_SDK, "缓降时间", "ms", 86, 0, 2000);
    for (std::size_t i = 0; i < configuration.bindings.size(); ++i)
    {
        configuration.bindings[i].readAddress = configuration.bindings[i].writeAddress + 90;
        configuration.descriptors[i].flags |= LASER_PARAMETER_READABLE_SDK;
    }
    // 手动气、光属于命令枚举，未提供已验证的统一布尔语义，不纳入默认工艺配置。
}

/**
 * @brief 构建旧超强及兼容别名的实际工艺地址，不根据地址零猜测缺失。
 * @param resource 原调用方填写的设备地址和焊接类型。
 * @param configuration 只追加当前工艺会访问的寄存器描述及绑定。
 * @return 无；自熔焊不读取未使用的送丝机地址，手动控制保持原接口路径。
 */
void buildLegacySup(const MachineResource& resource, Configuration& configuration)
{
    add(configuration, LASER_PARAM_WELD_POWER_SDK, "焊接参考功率", "W", resource.laserPowerLoc, 0, 65535);
    add(configuration, LASER_PARAM_LASER_ON_POWER_SDK, "开光百分比", "%", resource.laserOnPowerLoc, 0, 100, 1,
        LASER_PARAMETER_U16_SDK, LASER_PARAMETER_TRUNCATE_SDK);
    add(configuration, LASER_PARAM_LASER_OFF_POWER_SDK, "关光百分比", "%", resource.laserOffPowerLoc, 0, 100, 1,
        LASER_PARAMETER_U16_SDK, LASER_PARAMETER_TRUNCATE_SDK);
    add(configuration, LASER_PARAM_RAMP_UP_TIME_SDK, "缓升时间", "ms", resource.laserOnRampTimeLoc, 0, 3000);
    add(configuration, LASER_PARAM_RAMP_DOWN_TIME_SDK, "缓降时间", "ms", resource.laserOffRampTimeLoc, 0, 3000);
    add(configuration, LASER_PARAM_PRE_GAS_TIME_SDK, "提前送气", "ms", resource.gasONDelayLoc, 0, 3000);
    add(configuration, LASER_PARAM_POST_GAS_TIME_SDK, "滞后送气", "ms", resource.gasOffDelayLoc, 0, 3000);
    add(configuration, LASER_PARAM_WIRE_OUTPUT_DELAY_SDK, "延时送丝", "ms", resource.wireOutDelayLoc, 0, 3000);
    add(configuration, LASER_PARAM_Y_SCALE_SDK, "纵向比例", "", resource.laserHeadYScaleLoc, 0.01, 4, 100);
    add(configuration, LASER_PARAM_Y_OFFSET_SDK, "纵向偏移", "mm", resource.laserHeadYOffsetLoc, -3, 3, 100, LASER_PARAMETER_I16_SDK);
    add(configuration, LASER_PARAM_X_SCALE_SDK, "横向比例", "", resource.laserHeadXScaleLoc, 0.01, 4, 100);
    add(configuration, LASER_PARAM_X_OFFSET_SDK, "横向偏移", "mm", resource.laserHeadXOffsetLoc, -3, 3, 100, LASER_PARAMETER_I16_SDK);
    add(configuration, LASER_PARAM_WELD_MODE_SDK, "焊接模式", "", resource.weldingModeLoc, 0, 1);
    add(configuration, LASER_PARAM_SPOT_WELD_TYPE_SDK, "点焊类型", "", resource.spotWeldTypeLoc, 0, 1);
    add(configuration, LASER_PARAM_SPOT_DURATION_SDK, "点焊持续", "ms", resource.spotWeldDurationLoc, 0, 60000);
    add(configuration, LASER_PARAM_SPOT_INTERVAL_SDK, "点焊间隔", "ms", resource.spotWeldIntervalLoc, 0, 60000);
    add(configuration, LASER_PARAM_SCAN_SPEED_SDK, "扫描速度", "mm/s", resource.laserHeadScanSpeedLoc, 0, 3000);
    add(configuration, LASER_PARAM_SCAN_WIDTH_SDK, "扫描宽度", "mm", resource.laserHeadScanWidthLoc, 0.1, 5, 10);
    add(configuration, LASER_PARAM_HEAD_PEAK_POWER_SDK, "初始峰值功率", "W", resource.laserHeadPeakPowerLoc, 0, 65535);
    add(configuration, LASER_PARAM_DUTY_CYCLE_SDK, "占空比", "%", resource.laserHeadDutyCycleLoc, 0, 100);
    // 部分旧客户端只有一个频率寄存器；相同地址时只绑定低字。
    if (resource.laserHeadFrequencyHighLoc != resource.laserHeadFrequencyLowLoc)
        add(configuration, LASER_PARAM_PWM_HIGH_WORD_SDK, "脉冲频率高字", "", resource.laserHeadFrequencyHighLoc, 0, 65535);
    add(configuration, LASER_PARAM_PWM_LOW_WORD_SDK, "脉冲频率低字", "", resource.laserHeadFrequencyLowLoc, 0, 65535);
    add(configuration, LASER_PARAM_SCAN_HEIGHT_SDK, "扫描高度", "mm", resource.laserHeadScanHeightLoc, 0.1, 5, 10);
    add(configuration, LASER_PARAM_ROTATION_ANGLE_SDK, "旋转角度", "deg", resource.laserHeadRotationAngleLoc, 0, 360);
    add(configuration, LASER_PARAM_SPOT_PATTERN_SDK, "光斑图形", "", resource.laserHeadPatternIdLoc, 0, 5);
    // 默认工艺仅描述实际需要写入的参数，不要求手动控制与快速功率地址。
    // 自熔焊旧工艺路径不下発送丝机参数或暂停命令，关闭设备时无需提供其地址。
    if (resource.weldingProcessType == WIRE_WELDING_LASERSDK)
    {
        add(configuration, LASER_PARAM_WIRE_FEED_SPEED_SDK, "送丝速度", "cm/min", resource.autoWireFeedSpeedLoc, 15, 600);
        add(configuration, LASER_PARAM_WIRE_RETRACT_LENGTH_SDK, "回抽长度", "mm", resource.wireRetractLengthLoc, 0, 100);
        add(configuration, LASER_PARAM_WIRE_FEED_LENGTH_SDK, "补丝长度", "mm", resource.wireFeedLengthLoc, 0, 100);
        add(configuration, LASER_PARAM_WIRE_FEED_DELAY_SDK, "补丝延时", "ms", resource.wireFeedBackDelayLoc, 0, 2000);
        add(configuration, LASER_PARAM_WIRE_CUT_MODE_SDK, "断丝模式", "", resource.breakWireFeedingModeLoc, 0, 1);
        add(configuration, LASER_PARAM_WIRE_RUN_STATE_SDK, "送丝运行状态", "", resource.wireFeederStatusLoc, 0, 1);
    }
    for (std::size_t i = 0; i < configuration.bindings.size(); ++i)
    {
        configuration.bindings[i].readAddress = configuration.bindings[i].writeAddress;
        configuration.descriptors[i].flags |= LASER_PARAMETER_READABLE_SDK;
    }
}

/** @brief 旧自由通讯的送丝机路由语义集合。 */
bool directWireFeeder(unsigned id)
{
    switch (id)
    {
    case LASER_PARAM_WIRE_FEED_SPEED_SDK:
    case LASER_PARAM_WIRE_RETRACT_LENGTH_SDK:
    case LASER_PARAM_WIRE_FEED_LENGTH_SDK:
    case LASER_PARAM_WIRE_FEED_DELAY_SDK:
    case LASER_PARAM_WIRE_CUT_MODE_SDK:
    case LASER_PARAM_WIRE_RUN_STATE_SDK:
    case LASER_PARAM_MANUAL_FEED_SPEED_SDK:
    case LASER_PARAM_MANUAL_RETRACT_SPEED_SDK:
    case LASER_PARAM_MANUAL_FEED_SWITCH_SDK:
    case LASER_PARAM_MANUAL_RETRACT_SWITCH_SDK:
        return true;
    default:
        return false;
    }
}

/** @brief 超强协议中三角和十字的编码与 SDK 枚举相反，集中转换避免散落常量。 */
int patternValue(SpotPatternLaserSdk pattern)
{
    if (pattern == TRIANGLE_LASERSDK) return 4;
    if (pattern == CROSS_LASERSDK) return 5;
    return static_cast<int>(pattern);
}

/**
 * @brief 根据超强图形的单次扫描长度，将扫描速度换算为频率。
 * @param craft 原始速度和图形尺寸；最终范围及整数舍入在参数描述中执行。
 * @return 尚未舍入的频率，单位 Hz；未知图形返回零并交给后续范围校验。
 */
double scanFrequency(const CraftInfoLaserSdk& craft)
{
    const double width = craft.craftScanWidth;
    const double height = craft.craftScanHeight;
    const double speed = craft.craftScanSpeed;
    switch (craft.graphicsSwitching)
    {
    case CIR_LASERSDK:
    case DOUBLE_CIRCLE_LASERSDK: return speed / (PI * width);
    case LINE_LASERSDK: return speed / (2.0 * width);
    case STAR_LASERSDK: return speed * 2.0 / (3.0 * (width + height));
    case HORIZON_8_LASERSDK: return speed * 2.0 / (PI * (width + height));
    case CROSS_LASERSDK: return speed / (2.0 * (width + height));
    case TRIANGLE_LASERSDK: return speed / (3.0 * width);
    default: return 0;
    }
}

} // 匿名命名空间

LaserSdkResult BuildDefaultConfiguration(const MachineResource& resource,
                                        Configuration& configuration,
                                        LaserParameterDiagnosticSdk* diagnostic)
{
    if (!supportedBrand(resource.laserHeadBrand))
        return fail(diagnostic, 0, "设备品牌未提供已验证的参数配置");
    Configuration candidate;
    // 先构建临时配置并完整校验，避免错误地址表替换调用方正在使用的配置。
    if (resource.laserHeadBrand == BCXLASERPOWER_SDK) buildBcx(candidate);
    else if (resource.laserHeadBrand == CQ_SUP26AD_LASERPOWER_SDK) buildSup26Ad(candidate);
    else buildLegacySup(resource, candidate);
    const LaserSdkResult result = ValidateConfiguration(candidate, diagnostic);
    if (result == SUCCESS_LASER_SDK) configuration = candidate;
    return result;
}

LaserSdkResult BuildCraftValues(const MachineResource& resource,
                               const CraftInfoLaserSdk& craft,
                               std::vector<LaserParameterValueSdk>& values,
                               LaserParameterDiagnosticSdk* diagnostic)
{
    if (!supportedBrand(resource.laserHeadBrand))
        return fail(diagnostic, 0, "设备品牌未提供已验证的工艺转换");
    if (!std::isfinite(craft.craftWeldingSpeed) || craft.craftWeldingSpeed <= 0.0
            || craft.craftLeadOutRampDistance < 0)
        return fail(diagnostic, LASER_PARAM_RAMP_DOWN_TIME_SDK, "缓降计算需要正焊接速度及非负距离", PARAMETER_OUT_OF_RANGE_LASER_SDK);
    const double rampDown = craft.craftLeadOutRampDistance * 1000.0 / craft.craftWeldingSpeed;
    if (!std::isfinite(rampDown))
        return fail(diagnostic, LASER_PARAM_RAMP_DOWN_TIME_SDK, "缓降时间不是有限数", PARAMETER_OUT_OF_RANGE_LASER_SDK);
    const bool bcx = resource.laserHeadBrand == BCXLASERPOWER_SDK;
    const bool sup26 = resource.laserHeadBrand == CQ_SUP26AD_LASERPOWER_SDK;
    // 超强以当前焊接功率作为百分比基准；宝辰鑫开关光参数直接使用瓦特。
    if (!bcx && (craft.craftLaserPower <= 0 || craft.craftLeadInPower < 0
            || craft.craftLeadInPower > craft.craftLaserPower || craft.craftLeadOutPower < 0
            || craft.craftLeadOutPower > craft.craftLaserPower))
        return fail(diagnostic, LASER_PARAM_WELD_POWER_SDK, "超强开关光功率必须在正焊接功率范围内", PARAMETER_OUT_OF_RANGE_LASER_SDK);
    if (craft.laserHeadPeakPower > 0 && craft.craftLaserPower > craft.laserHeadPeakPower)
        return fail(diagnostic, LASER_PARAM_WELD_POWER_SDK, "焊接功率超过设备额定功率", PARAMETER_OUT_OF_RANGE_LASER_SDK);

    std::vector<LaserParameterValueSdk> candidate;
    // 专家模式必须先下发，后续图形及渐进参数才按专家模式生效。
    if (sup26) value(candidate, LASER_PARAM_EXPERT_MODE_SDK, 1);
    value(candidate, LASER_PARAM_RATED_POWER_SDK, craft.laserHeadPeakPower);
    value(candidate, LASER_PARAM_WELD_POWER_SDK, craft.craftLaserPower);
    value(candidate, LASER_PARAM_HEAD_PEAK_POWER_SDK, craft.craftLeadInPower);
    value(candidate, LASER_PARAM_LASER_ON_POWER_SDK, bcx ? craft.craftLeadInPower
          : craft.craftLeadInPower * 100.0 / craft.craftLaserPower);
    value(candidate, LASER_PARAM_LASER_OFF_POWER_SDK, bcx ? craft.craftLeadOutPower
          : craft.craftLeadOutPower * 100.0 / craft.craftLaserPower);
    value(candidate, LASER_PARAM_RAMP_UP_TIME_SDK, craft.craftLeadInRampTime);
    // 完整关光渐进距离除以焊接速度得到时间，不扣除兼容字段 craftRetractionDistance。
    value(candidate, LASER_PARAM_RAMP_DOWN_TIME_SDK, rampDown);
    value(candidate, LASER_PARAM_PRE_GAS_TIME_SDK, craft.craftPreGasTime);
    value(candidate, LASER_PARAM_POST_GAS_TIME_SDK, craft.craftPostGasTime);
    value(candidate, LASER_PARAM_WIRE_OUTPUT_DELAY_SDK, 0);
    value(candidate, LASER_PARAM_WELD_MODE_SDK, sup26
          ? (craft.craftweldMode == SPOT_LASER_SDK ? 1 : 0) : static_cast<int>(craft.craftweldMode));
    value(candidate, LASER_PARAM_SPOT_WELD_TYPE_SDK, craft.craftWeldType);
    value(candidate, LASER_PARAM_SPOT_DURATION_SDK, craft.craftSpotWeldDuration);
    value(candidate, LASER_PARAM_SPOT_INTERVAL_SDK, craft.craftSpotWeldInterval);
    value(candidate, LASER_PARAM_SPOT_PATTERN_SDK, bcx
          ? static_cast<int>(craft.graphicsSwitching) : patternValue(craft.graphicsSwitching));
    value(candidate, LASER_PARAM_SCAN_SPEED_SDK, craft.craftScanSpeed);
    value(candidate, LASER_PARAM_SCAN_FREQUENCY_SDK, bcx ? craft.craftScanSpeed : scanFrequency(craft));
    value(candidate, LASER_PARAM_SCAN_WIDTH_SDK, craft.craftScanWidth);
    value(candidate, LASER_PARAM_SCAN_HEIGHT_SDK, craft.craftScanHeight);
    value(candidate, LASER_PARAM_ROTATION_ANGLE_SDK, craft.craftRotationAngle);
    value(candidate, LASER_PARAM_DUTY_CYCLE_SDK, craft.laserHeadDutyCycle);
    value(candidate, LASER_PARAM_PWM_HIGH_WORD_SDK, craft.laserHeadHighPulseFrequency);
    value(candidate, LASER_PARAM_PWM_LOW_WORD_SDK, craft.laserHeadLowPulseFrequency);
    if (craft.laserHeadHighPulseFrequency < 0 || craft.laserHeadHighPulseFrequency > 65535
            || craft.laserHeadLowPulseFrequency < 0 || craft.laserHeadLowPulseFrequency > 65535)
        return fail(diagnostic, LASER_PARAM_PWM_FREQUENCY_SDK, "脉冲频率高低字必须在十六位无符号范围内", PARAMETER_OUT_OF_RANGE_LASER_SDK);
    // 先检查原始高低字，再使用固定三十二位合成，避免平台字长差异掩盖越界。
    const uint32_t frequency = (static_cast<uint32_t>(craft.laserHeadHighPulseFrequency) << 16)
            | static_cast<uint32_t>(craft.laserHeadLowPulseFrequency);
    value(candidate, LASER_PARAM_PWM_FREQUENCY_SDK, static_cast<double>(frequency));
    value(candidate, LASER_PARAM_Y_SCALE_SDK, craft.craftYAxisZoom);
    value(candidate, LASER_PARAM_X_SCALE_SDK, craft.craftXAxisZoom);
    value(candidate, LASER_PARAM_Y_OFFSET_SDK, craft.craftYAxisOffset);
    value(candidate, LASER_PARAM_X_OFFSET_SDK, craft.craftXAxisOffset);
    value(candidate, LASER_PARAM_STEP_EQUIVALENT_SDK, craft.craftStepEquivalent);
    value(candidate, LASER_PARAM_WIRE_FEED_SPEED_SDK, craft.craftWeldingWireFeed);
    value(candidate, LASER_PARAM_START_WIRE_FEED_SPEED_SDK, craft.craftLeadInWireFeed);
    value(candidate, LASER_PARAM_STOP_WIRE_FEED_SPEED_SDK, craft.craftLeadOutWireFeed);
    // 宝辰鑫旧板卡的抽补丝速度与自动送丝速度使用同一业务值。
    value(candidate, LASER_PARAM_WIRE_RETRACT_SPEED_SDK, bcx ? craft.craftWeldingWireFeed : craft.craftEndingWireRetraction);
    value(candidate, LASER_PARAM_WIRE_RETRACT_LENGTH_SDK, craft.craftRetractionLength);
    value(candidate, LASER_PARAM_WIRE_FEED_LENGTH_SDK, craft.craftEndLoopbackLength);
    value(candidate, LASER_PARAM_WIRE_FEED_DELAY_SDK, craft.wireDelayTime);
    value(candidate, LASER_PARAM_WIRE_CUT_MODE_SDK, craft.craftWireCutMode);
    value(candidate, LASER_PARAM_SINGLE_STEP_DISTANCE_SDK, 0);
    if (!bcx)
    {
        value(candidate, LASER_PARAM_WIRE_FEED_SWITCH_SDK, resource.weldingProcessType == WIRE_WELDING_LASERSDK ? 1 : 0);
        if (!sup26) value(candidate, LASER_PARAM_WIRE_RUN_STATE_SDK, resource.weldingProcessType == WIRE_WELDING_LASERSDK ? 1 : 0);
    }
    values.swap(candidate);
    return fail(diagnostic, 0, "", SUCCESS_LASER_SDK);
}

LaserSdkResult EmitLegacySupLua(const WritePlan& plan, const MachineResource& resource,
                               std::string& script, LaserParameterDiagnosticSdk* diagnostic)
{
    if (!isLegacySupBrand(resource.laserHeadBrand))
        return fail(diagnostic, 0, "自由通讯输出仅支持旧超强及原有兼容别名");
    const BrandProtocolProfile profile = GetBrandProtocolAdapter(resource.laserHeadBrand).profile();
    // 仅旧自由通讯渲染保留端口和复合脚本码，参数计划本身不携带站号或 CRC。
    std::ostringstream candidate;
    candidate.imbue(std::locale::classic());
    for (std::size_t i = 0; i < plan.operations.size(); ++i)
    {
        const WriteOperation& operation = plan.operations[i];
        const bool wire = directWireFeeder(operation.parameterId);
        const unsigned code = wire ? profile.wireFeederWriteSingleScriptCode : profile.laserWriteSingleScriptCode;
        // 无回执运行状态和手动开关沿用直连送丝机协议能力。
        const int receipt = operation.parameterId == LASER_PARAM_WIRE_RUN_STATE_SDK
                || operation.parameterId == LASER_PARAM_MANUAL_FEED_SWITCH_SDK
                || operation.parameterId == LASER_PARAM_MANUAL_RETRACT_SWITCH_SDK ? 0 : 1;
        for (std::size_t word = 0; word < operation.words.size(); ++word)
        {
            candidate << "    write_value_port(0x" << std::uppercase << std::hex << std::setw(4)
                      << std::setfill('0') << code << ", " << std::dec << operation.address + word
                      << ", " << operation.words[word] << ", " << (wire ? 2 : 3)
                      << ", SendCommuRegAddr, " << receipt << ")\n";
        }
    }
    if (!candidate.good())
        return fail(diagnostic, 0, "参数脚本输出内存不足", MEMORY_ERROR_LASER_SDK);
    script = candidate.str();
    return fail(diagnostic, 0, "", SUCCESS_LASER_SDK);
}

LaserSdkResult BuildDeviceParametersScript(const MachineResource& resource,
                                          const Configuration* overrideConfiguration,
                                          const std::vector<LaserParameterValueSdk>& values,
                                          std::string& script,
                                          LaserParameterDiagnosticSdk* diagnostic)
{
    if (!supportedBrand(resource.laserHeadBrand))
        return fail(diagnostic, 0, "设备品牌未提供已验证的参数写入流程");
    Configuration defaults;
    const Configuration* active = overrideConfiguration;
    if (!active)
    {
        const LaserSdkResult result = BuildDefaultConfiguration(resource, defaults, diagnostic);
        if (result != SUCCESS_LASER_SDK) return result;
        active = &defaults;
    }
    WritePlan plan;
    // 编译器统一完成能力、范围、缩放、舍入及地址重叠校验，输出层不再解释业务值。
    const LaserSdkResult result = CompileWritePlan(*active, values, plan, diagnostic);
    if (result != SUCCESS_LASER_SDK) return result;
    // 显式配置表示上位机已建立有效的本地映射，禁止再落入旧自由通讯分支。
    if (!overrideConfiguration && isLegacySupBrand(resource.laserHeadBrand))
        return EmitLegacySupLua(plan, resource, script, diagnostic);
    const LaserSdkResult emitted = EmitMappedLua(plan, script);
    if (emitted != SUCCESS_LASER_SDK)
        return fail(diagnostic, 0, "参数映射脚本输出失败", emitted);
    return emitted;
}

LaserSdkResult BuildDeviceCraftScript(const MachineResource& resource,
                                     const CraftInfoLaserSdk& craft,
                                     const Configuration* overrideConfiguration,
                                     std::string& script,
                                     LaserParameterDiagnosticSdk* diagnostic)
{
    Configuration defaults;
    const Configuration* active = overrideConfiguration;
    if (!active)
    {
        const LaserSdkResult result = BuildDefaultConfiguration(resource, defaults, diagnostic);
        if (result != SUCCESS_LASER_SDK) return result;
        active = &defaults;
    }
    // 基础工艺项必须可写；型号不具备的可选项或可选只读项不会被自动下发。
    const unsigned required[] = {LASER_PARAM_WELD_POWER_SDK, LASER_PARAM_DUTY_CYCLE_SDK, LASER_PARAM_SCAN_WIDTH_SDK};
    for (std::size_t i = 0; i < sizeof(required) / sizeof(required[0]); ++i)
        if (!writable(*active, required[i]))
            return fail(diagnostic, required[i], "工艺配置缺少必要的功率、占空比或扫描宽度参数");
    if (!overrideConfiguration)
    {
        // 默认型号保留原校验；显式配置允许调用方以描述定义新的地址、范围和编码。
        const LaserSdkResult result = GetBrandProtocolAdapter(resource.laserHeadBrand).checkCraft(craft);
        if (result != SUCCESS_LASER_SDK)
            return fail(diagnostic, 0, "工艺超出默认设备型号的能力范围", result);
    }
    std::vector<LaserParameterValueSdk> converted;
    LaserSdkResult result = BuildCraftValues(resource, craft, converted, diagnostic);
    if (result != SUCCESS_LASER_SDK) return result;
    std::vector<LaserParameterValueSdk> selected;
    for (std::size_t i = 0; i < converted.size(); ++i)
    {
        LaserParameterValueSdk parameter = converted[i];
        if (!writable(*active, parameter.parameterId)) continue;
        if (!overrideConfiguration && isLegacySupBrand(resource.laserHeadBrand))
        {
            // 旧超强缓升降由 SDK 轨迹执行，禁止同时启用板卡渐进时间。
            if (parameter.parameterId == LASER_PARAM_RAMP_UP_TIME_SDK
                    || parameter.parameterId == LASER_PARAM_RAMP_DOWN_TIME_SDK) parameter.value = 0;
            // 工艺下发只配置参数；送丝信号与使能开关由起收弧动作独立管理。
            if (parameter.parameterId == LASER_PARAM_WIRE_FEED_SWITCH_SDK) continue;
            if (resource.weldingProcessType == SELF_WELDING_LASERSDK
                    && directWireFeeder(parameter.parameterId)
                    && parameter.parameterId != LASER_PARAM_WIRE_RUN_STATE_SDK) continue;
        }
        selected.push_back(parameter);
    }
    return BuildDeviceParametersScript(resource, overrideConfiguration, selected, script, diagnostic);
}

} // 参数编译命名空间 laser_parameters
