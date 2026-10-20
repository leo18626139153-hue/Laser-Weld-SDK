#include "rampcontrol.h"
#include "parametercontext.h"
#include "deviceparameters.h"
#include "brand/brandadapter.h"
#include "internaldatadefine.h"
#include <cmath>
#include <cstring>
#include <iomanip>
#include <locale>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <sstream>

namespace ramp_control
{
namespace
{
std::map<LaserMachineHandle, LaserRampControlConfigSdk> configurations;
std::mutex configurationMutex;
const unsigned kLegacyFrameSettleMs = 50;

struct Context
{
    LaserRampControlConfigSdk config;
    laser_parameters::Configuration parameters;
    LaserParameterDescriptorSdk power;
    LaserParameterDescriptorSdk wire;
    unsigned powerAddress;
    unsigned wireAddress;
    bool mapped;
    bool wireWelding;
    bool explicitlyConfigured;
};

LaserRampControlConfigSdk defaults()
{
    LaserRampControlConfigSdk result = {};
    result.structSize = sizeof(result);
    result.version = LASER_RAMP_CONTROL_CONFIG_VERSION_SDK;
    result.updatePeriodMs = 100;
    result.settleTimeMs = 50;
    result.timeoutMs = 2000;
    return result;
}

bool exists(LaserMachineHandle handle)
{
    return machineResourcesMap.find(handle) != machineResourcesMap.end();
}

bool wireParameter(unsigned id)
{
    return (id >= LASER_PARAM_WIRE_FEED_SPEED_SDK && id <= LASER_PARAM_MANUAL_RETRACT_SWITCH_SDK)
            || id == LASER_PARAM_WIRE_FEED_SWITCH_SDK || id == LASER_PARAM_WIRE_OUTPUT_DELAY_SDK
            || id == LASER_PARAM_WIRE_RETRACT_SPEED_SDK || id == LASER_PARAM_SINGLE_STEP_DISTANCE_SDK;
}

LaserSdkResult descriptor(const laser_parameters::Configuration& config, unsigned id,
                          LaserParameterDescriptorSdk& result, unsigned& address)
{
    bool found = false;
    for (std::size_t i = 0; i < config.descriptors.size(); ++i)
        if (config.descriptors[i].parameterId == id)
        {
            result = config.descriptors[i];
            found = true;
            break;
        }
    if (!found) return PARAM_TYPE_ERROR_LASER_SDK;
    if ((result.flags & LASER_PARAMETER_WRITABLE_SDK) == 0) return PARAM_WRITE_ERROR_LASER_SDK;
    // 动态逐周期只支持已验证的线性十六位无符号寄存器，拒绝猜测多字浮点原子性。
    if (result.dataType != LASER_PARAMETER_U16_SDK || result.scale <= 0.0)
        return PARAM_TYPE_ERROR_LASER_SDK;
    for (std::size_t i = 0; i < config.bindings.size(); ++i)
        if (config.bindings[i].parameterId == id)
        {
            address = config.bindings[i].writeAddress;
            return address <= 65535u ? SUCCESS_LASER_SDK : PARAMETER_ERROR_LASER_SDK;
        }
    return PARAMETER_ERROR_LASER_SDK;
}

LaserSdkResult resolve(LaserMachineHandle handle, const MachineResource& resource, Context& context,
                        const LaserRampControlConfigSdk* candidate = nullptr)
{
    context.config.structSize = sizeof(context.config);
    LaserSdkResult result = SUCCESS_LASER_SDK;
    if (candidate != nullptr) context.config = *candidate;
    else result = GetConfig(handle, &context.config);
    if (result != SUCCESS_LASER_SDK) return result;
    if (context.config.enabled == 0) return PARAMETER_ERROR_LASER_SDK;
    if (resource.laserHeadBrand != CQLASERPOWER_SDK && resource.laserHeadBrand != BCXLASERPOWER_SDK
            && resource.laserHeadBrand != CQ_SUP26AD_LASERPOWER_SDK)
        return PARAM_TYPE_ERROR_LASER_SDK;
    context.mapped = laser_parameters::UsesMappedParameters(handle, resource.laserHeadBrand);
    context.wireWelding = resource.weldingProcessType == WIRE_WELDING_LASERSDK;
    result = laser_parameters::GetMachineConfiguration(handle, context.parameters, &context.explicitlyConfigured);
    if (result != SUCCESS_LASER_SDK) return result;
    if (!context.mapped && resource.laserHeadBrand == CQLASERPOWER_SDK)
    {
        // 旧协议的动态目标是快速峰值地址，绝不把激光器额定功率地址用于渐变。
        LaserParameterDescriptorSdk power = {};
        power.structSize = sizeof(power);
        power.parameterId = LASER_PARAM_FAST_PEAK_POWER_SDK;
        power.dataType = LASER_PARAMETER_U16_SDK;
        power.wordOrder = LASER_PARAMETER_HIGH_FIRST_SDK;
        power.rounding = LASER_PARAMETER_NEAREST_SDK;
        power.scale = 1;
        power.minimum = 0;
        power.maximum = 65535;
        power.flags = LASER_PARAMETER_WRITABLE_SDK;
        std::strcpy(power.name, "快速峰值功率");
        std::strcpy(power.unit, "W");
        LaserParameterBindingSdk binding = {};
        binding.structSize = sizeof(binding);
        binding.parameterId = power.parameterId;
        binding.writeAddress = resource.laserHeadPeakPowerQuickLoc;
        binding.readAddress = LASER_PARAMETER_INVALID_ADDRESS_SDK;
        context.parameters.descriptors.push_back(power);
        context.parameters.bindings.push_back(binding);
    }
    const unsigned powerId = resource.laserHeadBrand == CQLASERPOWER_SDK
            ? LASER_PARAM_FAST_PEAK_POWER_SDK : LASER_PARAM_WELD_POWER_SDK;
    result = descriptor(context.parameters, powerId, context.power, context.powerAddress);
    if (result != SUCCESS_LASER_SDK) return result;
    if (std::strcmp(context.power.unit, "W") != 0) return PARAM_TYPE_ERROR_LASER_SDK;
    if (context.wireWelding)
    {
        result = descriptor(context.parameters, LASER_PARAM_WIRE_FEED_SPEED_SDK, context.wire, context.wireAddress);
        if (result != SUCCESS_LASER_SDK) return result;
        if (std::strcmp(context.wire.unit, "mm/s") != 0 && std::strcmp(context.wire.unit, "cm/min") != 0)
            return PARAM_TYPE_ERROR_LASER_SDK;
    }
    if (!context.mapped && (context.config.settleTimeMs < kLegacyFrameSettleMs
            || context.config.updatePeriodMs < context.config.settleTimeMs * (context.wireWelding ? 2u : 1u)))
        return PARAMETER_OUT_OF_RANGE_LASER_SDK;
    return laser_parameters::ValidateConfiguration(context.parameters);
}

double nativeWire(const Context& context, double millimetersPerSecond)
{
    return std::strcmp(context.wire.unit, "cm/min") == 0 ? millimetersPerSecond * 6.0 : millimetersPerSecond;
}

LaserSdkResult encode(const Context& context, unsigned id, double value, unsigned& raw)
{
    LaserParameterValueSdk parameter = {};
    parameter.structSize = sizeof(parameter);
    parameter.parameterId = id;
    parameter.value = value;
    std::vector<LaserParameterValueSdk> values(1, parameter);
    laser_parameters::WritePlan plan;
    const LaserSdkResult result = laser_parameters::CompileWritePlan(context.parameters, values, plan);
    if (result != SUCCESS_LASER_SDK) return result;
    if (plan.operations.size() != 1 || plan.operations[0].words.size() != 1)
        return PARAM_TYPE_ERROR_LASER_SDK;
    raw = plan.operations[0].words[0];
    return SUCCESS_LASER_SDK;
}

/** @brief SDK 外控保留原配置时间能力，另外限制单阶段最多一分钟，避免巨大输入无限等待。 */
LaserSdkResult checkTime(const Context& context, unsigned id, double milliseconds)
{
    if (!std::isfinite(milliseconds) || milliseconds < 0 || milliseconds > 60000.0)
        return PARAMETER_OUT_OF_RANGE_LASER_SDK;
    for (std::size_t i = 0; i < context.parameters.descriptors.size(); ++i)
    {
        const LaserParameterDescriptorSdk& item = context.parameters.descriptors[i];
        if (item.parameterId == id)
        {
            if (std::strcmp(item.unit, "ms") != 0) return PARAM_TYPE_ERROR_LASER_SDK;
            return milliseconds >= item.minimum && milliseconds <= item.maximum
                    ? SUCCESS_LASER_SDK : PARAMETER_OUT_OF_RANGE_LASER_SDK;
        }
    }
    return PARAM_TYPE_ERROR_LASER_SDK;
}

LaserSdkResult checkTimes(const Context& context, const CgxiLaserPowerLaserSdk& parameters)
{
    const unsigned ids[] = {LASER_PARAM_RAMP_UP_TIME_SDK, LASER_PARAM_RAMP_DOWN_TIME_SDK};
    const double values[] = {static_cast<double>(parameters.laserRampUpTime), parameters.laserRampDownTime};
    for (unsigned i = 0; i < 2; ++i)
    {
        const LaserSdkResult result = checkTime(context, ids[i], values[i]);
        if (result != SUCCESS_LASER_SDK) return result;
    }
    return SUCCESS_LASER_SDK;
}

/** @brief 编译同一阶段的三个精确端点；自熔焊在此之前就屏蔽所有送丝字段读取。 */
LaserSdkResult endpoints(const Context& context, const MachineResource& resource,
                         unsigned (&power)[3], unsigned (&wire)[3])
{
    const double powers[] = {static_cast<double>(resource.laserPowerParams.laserPowerOpen),
                             static_cast<double>(resource.laserHeadParams.laserHeadPeakPower),
                             static_cast<double>(resource.laserPowerParams.laserPowerClose)};
    for (unsigned i = 0; i < 3; ++i)
    {
        const LaserSdkResult result = encode(context, context.power.parameterId, powers[i], power[i]);
        if (result != SUCCESS_LASER_SDK) return result;
    }
    if (context.wireWelding)
    {
        const CgxiWireFeederLaserSdk& wireParameters = resource.wireFeederParams;
        const double speeds[] = {static_cast<double>(wireParameters.wireFeederStartMotorSpeed),
                                  static_cast<double>(wireParameters.wireFeederMotorSpeed),
                                  static_cast<double>(wireParameters.wireFeederStopMotorSpeed)};
        for (unsigned i = 0; i < 3; ++i)
        {
            const LaserSdkResult result = encode(context, LASER_PARAM_WIRE_FEED_SPEED_SDK,
                                                 nativeWire(context, speeds[i]), wire[i]);
            if (result != SUCCESS_LASER_SDK) return result;
        }
    }
    return SUCCESS_LASER_SDK;
}

/** @brief 仅在串口发送前生成一帧；等待点在带代次校验的调用方，避免取消后旧函数重试。 */
std::string serialWriter()
{
    return R"(
local function cl_ramp_xor(a, b)
    local result, bit = 0, 1
    for i = 1, 16 do
        if a % 2 ~= b % 2 then result = result + bit end
        a = math.floor(a / 2)
        b = math.floor(b / 2)
        bit = bit * 2
    end
    return result
end
local function cl_ramp_wait_bus(token, port)
    local now = cl_ramp_now()
    local due = math.max((port and cl_ramp_ctx.bus[port]) or now, cl_ramp_ctx.sendDue or now)
    while due > now do
        -- 旧代码每帧等待50ms；取消任务不能证明物理端口空闲，必须保留截止时间。
        wait(due-now)
        if token ~= cl_ramp_ctx.token then return false end
        now = cl_ramp_now()
        due = math.max((port and cl_ramp_ctx.bus[port]) or now, cl_ramp_ctx.sendDue or now)
    end
    return token == cl_ramp_ctx.token
end
function cl_ramp_serial(code, address, value, port, receipt, deadline)
    local token = cl_ramp_ctx.token
    if not cl_ramp_wait_bus(token, port) then return false end
    if not cl_ramp_valid(token, deadline) then return false end
    local bytes = {math.floor(code/256), code%256, math.floor(address/256), address%256,
                   math.floor(value/256), value%256}
    local crc = 65535
    for i = 1, 6 do
        crc = cl_ramp_xor(crc, bytes[i])
        for j = 1, 8 do
            local low = crc % 2
            crc = math.floor(crc/2)
            if low == 1 then crc = cl_ramp_xor(crc, 40961) end
        end
    end
    bytes[7] = crc % 256
    bytes[8] = math.floor(crc/256)
    -- 沿用旧自由通讯的发送0、接收30、十六位接收区，整帧在唯一阶段任务内生成。
    for i = 1, 8 do write_char_register(i-1, bytes[i]) end
    serial_send_receive(port, 8, 0, 0, 1, 30, receipt or 0)
    cl_ramp_ctx.bus[port] = cl_ramp_now() + cl_ramp_settle
    -- 两个物理端口仍复用发送0缓冲，所以跨端口也必须保留上一帧稳定窗口。
    cl_ramp_ctx.sendDue = cl_ramp_ctx.bus[port]
    return true
end
)";
}

std::string writer(const Context& context, const MachineResource& resource, bool wire, const std::string& value)
{
    std::ostringstream output;
    if (context.mapped)
        output << "write_int16_register(" << (wire ? context.wireAddress : context.powerAddress) << ", " << value << ")\n";
    else
    {
        const BrandProtocolProfile profile = GetBrandProtocolAdapter(resource.laserHeadBrand).profile();
        output << "if not cl_ramp_serial(" << (wire ? profile.wireFeederWriteSingleScriptCode : profile.laserWriteSingleScriptCode)
               << ", " << (wire ? context.wireAddress : context.powerAddress) << ", " << value << ", " << (wire ? 2 : 3)
               << ", 0, deadline) then return false end\n";
    }
    return output.str();
}

/** @brief 关丝只负责协议触发，抽补丝由板卡已配置的长度、延时和断丝模式执行。 */
LaserSdkResult wireSwitch(const Context& context, const MachineResource& resource, bool enabled, std::string& script)
{
    if (!context.wireWelding) { script.clear(); return SUCCESS_LASER_SDK; }
    const BrandProtocolAdapter& adapter = GetBrandProtocolAdapter(resource.laserHeadBrand);
    if (adapter.controlsWireThroughDigitalOutput())
    {
        script = enabled ? "set_configurable_digital_out(2, true)\n" : "set_configurable_digital_out(2, false)\n";
        return SUCCESS_LASER_SDK;
    }
    if (!context.mapped)
    {
        std::ostringstream output;
        output << "if not cl_ramp_serial(" << adapter.profile().laserWriteSingleScriptCode << ", "
               << resource.wireFeedSignalLoc << ", " << (enabled ? 1 : 0) << ", 3) then return end\n";
        script = output.str();
        return SUCCESS_LASER_SDK;
    }
    LaserParameterDescriptorSdk signal = {};
    unsigned address = 0;
    const LaserSdkResult result = descriptor(context.parameters, LASER_PARAM_WIRE_FEED_SWITCH_SDK, signal, address);
    if (result != SUCCESS_LASER_SDK) return result;
    unsigned raw = 0;
    const LaserSdkResult encoded = encode(context, LASER_PARAM_WIRE_FEED_SWITCH_SDK, enabled ? 1 : 0, raw);
    if (encoded != SUCCESS_LASER_SDK) return encoded;
    script = "write_int16_register(" + std::to_string(address) + ", " + std::to_string(raw) + ")\n";
    return SUCCESS_LASER_SDK;
}
} // 匿名命名空间

LaserSdkResult GetConfig(LaserMachineHandle handle, LaserRampControlConfigSdk* config)
{
    if (!exists(handle)) return HANDLE_ERROR_LASER_SDK;
    if (config == nullptr || config->structSize < sizeof(*config)) return PARAMETER_ERROR_LASER_SDK;
    std::lock_guard<std::mutex> lock(configurationMutex);
    const std::map<LaserMachineHandle, LaserRampControlConfigSdk>::const_iterator it = configurations.find(handle);
    *config = it == configurations.end() ? defaults() : it->second;
    return SUCCESS_LASER_SDK;
}

LaserSdkResult SetConfig(LaserMachineHandle handle, const LaserRampControlConfigSdk* config)
{
    if (!exists(handle)) return HANDLE_ERROR_LASER_SDK;
    if (config == nullptr || config->structSize < sizeof(*config)
            || config->version != LASER_RAMP_CONTROL_CONFIG_VERSION_SDK || config->enabled > 1)
        return PARAMETER_ERROR_LASER_SDK;
    if (config->updatePeriodMs < 1 || config->updatePeriodMs > 1000 || config->settleTimeMs > 1000
            || config->timeoutMs < config->updatePeriodMs || config->timeoutMs < config->settleTimeMs
            || config->timeoutMs > 60000)
        return PARAMETER_OUT_OF_RANGE_LASER_SDK;
    // 安装之前验证型号和动态目标，失败时不污染既有配置。
    const MachineResource& resource = machineResourcesMap.find(handle)->second;
    if (config->enabled)
    {
        if (resource.laserHeadBrand != CQLASERPOWER_SDK && resource.laserHeadBrand != BCXLASERPOWER_SDK
                && resource.laserHeadBrand != CQ_SUP26AD_LASERPOWER_SDK) return PARAM_TYPE_ERROR_LASER_SDK;
        if (!laser_parameters::UsesMappedParameters(handle, resource.laserHeadBrand)
                && (config->settleTimeMs < kLegacyFrameSettleMs
                    || config->updatePeriodMs < config->settleTimeMs
                       * (resource.weldingProcessType == WIRE_WELDING_LASERSDK ? 2u : 1u)))
            return PARAMETER_OUT_OF_RANGE_LASER_SDK;
        Context context = {};
        const LaserSdkResult result = resolve(handle, resource, context, config);
        if (result != SUCCESS_LASER_SDK) return result;
    }
    LaserRampControlConfigSdk copy = *config;
    copy.structSize = sizeof(copy);
    std::lock_guard<std::mutex> lock(configurationMutex);
    configurations[handle] = copy;
    return SUCCESS_LASER_SDK;
}

void ReleaseConfig(LaserMachineHandle handle)
{
    std::lock_guard<std::mutex> lock(configurationMutex);
    configurations.erase(handle);
}

LaserSdkResult ResetConfig(LaserMachineHandle handle)
{
    if (!exists(handle)) return HANDLE_ERROR_LASER_SDK;
    ReleaseConfig(handle);
    return SUCCESS_LASER_SDK;
}

bool Enabled(LaserMachineHandle handle)
{
    std::lock_guard<std::mutex> lock(configurationMutex);
    const std::map<LaserMachineHandle, LaserRampControlConfigSdk>::const_iterator it = configurations.find(handle);
    return it != configurations.end() && it->second.enabled != 0;
}

LaserSdkResult BuildRuntimeScript(LaserMachineHandle handle, const MachineResource& resource, std::string& script)
{
    Context context = {};
    context.config.structSize = sizeof(context.config);
    LaserSdkResult result = resolve(handle, resource, context);
    if (result != SUCCESS_LASER_SDK) return result;
    std::string wireOn, wireOff;
    result = wireSwitch(context, resource, true, wireOn);
    if (result != SUCCESS_LASER_SDK) return result;
    result = wireSwitch(context, resource, false, wireOff);
    if (result != SUCCESS_LASER_SDK) return result;
    std::ostringstream output;
    output.imbue(std::locale::classic());
    // 同一控制器重新加载运行时，先使旧代次无效并删除旧任务，避免全局覆盖留下孤儿。
    output << R"(
-- SDK 外控阶段：控制器上电时间是秒和微秒；不采用 CPU 时间或固定步数估算。
if cl_ramp_ctx ~= nil then
    cl_ramp_ctx.token = cl_ramp_ctx.token + 1
    if cl_ramp_ctx.task ~= nil then task_delete(cl_ramp_ctx.task) end
    cl_ramp_ctx.task = nil
else
    cl_ramp_ctx = {token=0, task=nil, on=false, stopped=true, power=nil, bus={}}
end
if cl_ramp_ctx.bus == nil then cl_ramp_ctx.bus = {} end
local function cl_ramp_now()
    local t = get_systime()
    return t[1] * 1000 + t[2] / 1000
end
function cl_ramp_cancel()
    cl_ramp_ctx.token = cl_ramp_ctx.token + 1
    local old = cl_ramp_ctx.task
    cl_ramp_ctx.task = nil
    if old ~= nil then task_delete(old) end
end
)";
    output << "local cl_ramp_period = " << context.config.updatePeriodMs << "\n"
           << "local cl_ramp_settle = " << context.config.settleTimeMs << "\n"
           << "local cl_ramp_timeout = " << context.config.timeoutMs << "\n";
    output << R"(
local function cl_ramp_valid(token, deadline)
    if token ~= cl_ramp_ctx.token then return false end
    if deadline ~= nil and cl_ramp_now() > deadline then
        write_integer_register(675, 1)
        -- 此检查可能在阶段任务内部运行，先移除自身句柄再复用取消清理，避免删除自己中断关光。
        cl_ramp_ctx.task = nil
        cl_ramp_abort()
        return false
    end
    return true
end
)";
    if (!context.mapped) output << serialWriter();
    if (!context.mapped)
    {
        output << R"(
function cl_ramp_prepare(code, address, value, port)
    local token = cl_ramp_ctx.token
    local started = cl_ramp_now()
    write_int16_register(30, 0)
    if not cl_ramp_serial(code, address, value, port, 1, started+cl_ramp_timeout) then return false end
    wait(cl_ramp_settle)
    while token == cl_ramp_ctx.token do
        local elapsed = cl_ramp_now() - started
        if elapsed < 0 or elapsed >= cl_ramp_timeout then
            write_integer_register(821, address)
            write_integer_register(673, 1)
            cl_ramp_abort()
            return false
        end
        if read_int16_register(30) ~= 0 then return true end
        wait(math.min(cl_ramp_period, cl_ramp_timeout-elapsed))
    end
    return token == cl_ramp_ctx.token
end
)";
    }
    output << R"(
local function cl_ramp_write(token, p, w, immediate, deadline)
    if not cl_ramp_valid(token, deadline) then return false end
    p = math.floor(p + 0.5)
)" << writer(context, resource, false, "p")
           << "    cl_ramp_ctx.power = p\n";
    if (!context.mapped)
        output << "    if not immediate then wait(cl_ramp_settle) end\n    if not cl_ramp_valid(token, deadline) then return false end\n";
    if (context.wireWelding)
    {
        output << "    if w ~= nil then\n        w = math.floor(w + 0.5)\n"
               << writer(context, resource, true, "w")
               << "        cl_ramp_ctx.wire = w\n    end\n";
    }
    // 自由通讯自融只有功率一帧，上方已等待稳定；映射初值及填丝第二帧才在此等待。
    if (context.mapped || context.wireWelding)
        output << "    if not immediate then wait(cl_ramp_settle) end\n";
    output << "    return cl_ramp_valid(token, deadline)\nend\n";
    output << R"(
local function cl_ramp_stop_outputs(token)
    if token ~= cl_ramp_ctx.token then return false end
    -- 正常终点和异常均先关光，再按协议安全提交一次停丝，禁止终点保持出光等待串口。
    set_configurable_digital_out(1, false)
    if not cl_ramp_ctx.stopped then
)" << wireOff << R"(
        if token ~= cl_ramp_ctx.token then return false end
        -- stopped 表示停丝已经提交；排队等待不能冒充完成，新代次必须接管未提交的停丝。
        cl_ramp_ctx.stopped = true
    end
    return token == cl_ramp_ctx.token
end
function cl_ramp_abort()
    cl_ramp_ctx.resume = nil
    cl_ramp_ctx.request = nil
    cl_ramp_ctx.phase = nil
    cl_ramp_ctx.phaseOwner = nil
    cl_ramp_ctx.resumePhaseOwner = nil
    cl_ramp_ctx.resuming = false
    cl_ramp_ctx.startAction = nil
    cl_ramp_ctx.preparing = false
    write_boolean_register(611, false)
    cl_ramp_cancel()
    local token = cl_ramp_ctx.token
    cl_ramp_ctx.on = false
    if not cl_ramp_stop_outputs(token) then return end
end
function cl_ramp_prepare_arc(pendingAction)
    cl_ramp_ctx.resume = nil
    cl_ramp_ctx.request = nil
    cl_ramp_ctx.phase = nil
    cl_ramp_ctx.phaseOwner = nil
    cl_ramp_ctx.resumePhaseOwner = nil
    cl_ramp_ctx.startAction = nil
    cl_ramp_ctx.preparing = false
    cl_ramp_cancel()
    local token = cl_ramp_ctx.token
    -- 尚在接管旧停丝队列时也拥有新的起弧请求，暂停必须能取消并保存它。
    cl_ramp_ctx.on = pendingAction ~= nil
    cl_ramp_ctx.preparing = pendingAction ~= nil
    cl_ramp_ctx.startAction = pendingAction
    if pendingAction ~= nil then write_boolean_register(611, true) end
    if not cl_ramp_stop_outputs(token) then return false end
    -- CO0是红光，焊接阶段保持其原状态；正式出光只用CO1，设备自动送气。
    return token == cl_ramp_ctx.token
end
function cl_ramp_submit_start(action)
    if not cl_ramp_prepare_arc(action) then return end
    cl_ramp_ctx.startAction = action
    cl_ramp_ctx.preparing = true
    cl_ramp_ctx.on = true
    write_boolean_register(611, true)
    action()
end
local function cl_ramp_record_phase(p0, p1, w0, w1, duration, started, completed)
    local owner = cl_ramp_ctx.resumePhaseOwner
    cl_ramp_ctx.resumePhaseOwner = nil
    if owner == nil then
        cl_ramp_ctx.nextPhaseOwner = (cl_ramp_ctx.nextPhaseOwner or 0) + 1
        owner = cl_ramp_ctx.nextPhaseOwner
    end
    cl_ramp_ctx.phaseOwner = owner
    local phase = {owner=owner, p0=p0, p1=p1, w0=w0, w1=w1,
                   duration=duration, started=started, completed=completed}
    cl_ramp_ctx.phase = phase
    return phase
end
local function cl_ramp_phase(p0, p1, w0, w1, duration)
    cl_ramp_cancel()
    local token = cl_ramp_ctx.token
    if cl_ramp_ctx.power ~= nil then p0 = cl_ramp_ctx.power end
)";
    if (context.wireWelding)
        output << "    if cl_ramp_ctx.wire ~= nil then w0 = cl_ramp_ctx.wire end\n";
    output << R"(
    local started = cl_ramp_now()
    local phase = cl_ramp_record_phase(p0, p1, w0, w1, duration, started, false)
    if duration <= 0 then
        if cl_ramp_write(token, p1, w1, true) then phase.completed = true end
        return
    end
    local function worker()
        while token == cl_ramp_ctx.token and cl_ramp_ctx.on do
)";
    if (!context.mapped)
        output << "            if not cl_ramp_wait_bus(token) then return end\n";
    output << R"(
            local elapsed = cl_ramp_now() - started
            if elapsed < 0 or elapsed > duration + cl_ramp_timeout then
                write_integer_register(675, 1)
                cl_ramp_ctx.task = nil
                cl_ramp_abort()
                return
            end
            local ratio = math.min(1, elapsed / duration)
            local p = p0 + (p1-p0) * ratio
            local w = nil
)";
    if (context.wireWelding)
        output << "            w = w0 + (w1-w0) * ratio\n";
    output << R"(
)";
    // 映射更新由主站周期传播，稳定等待只放在开光准备；周期任务不能额外拖长短坡度。
    output << (context.mapped ? "            if not cl_ramp_write(token, p, w, true, started+duration+cl_ramp_timeout) then return end\n"
                             : "            if not cl_ramp_write(token, p, w, false, started+duration+cl_ramp_timeout) then return end\n");
    output << R"(
            if ratio >= 1 then
                if token == cl_ramp_ctx.token and cl_ramp_ctx.phase == phase then
                    phase.completed = true
                    cl_ramp_ctx.task = nil
                end
                return
            end
            local nextTick = math.min(duration, elapsed + cl_ramp_period)
            local delay = nextTick - (cl_ramp_now() - started)
            if delay > 0 then wait(delay) end
        end
    end
    cl_ramp_ctx.task = task_create(worker)
end
function cl_ramp_retarget(p, w)
    cl_ramp_cancel()
    cl_ramp_ctx.power = p
    cl_ramp_ctx.phase = nil
    cl_ramp_ctx.phaseOwner = nil
    cl_ramp_ctx.resumePhaseOwner = nil
    cl_ramp_ctx.resume = nil
    if not cl_ramp_ctx.preparing then
        cl_ramp_ctx.request = {p0=p, p1=p, w0=w, w1=w, duration=0}
    end
)";
    if (context.wireWelding) output << "    cl_ramp_ctx.wire = w\n";
    output << R"(
end
function cl_ramp_start(p0, p1, w0, w1, duration, preGas)
    cl_ramp_ctx.resume = nil
    cl_ramp_ctx.request = {p0=p0, p1=p1, w0=w0, w1=w1, duration=duration}
    cl_ramp_ctx.phase = nil
    cl_ramp_ctx.phaseOwner = cl_ramp_ctx.resumePhaseOwner
    cl_ramp_ctx.preparing = true
    write_boolean_register(611, true)
    cl_ramp_cancel()
    local token = cl_ramp_ctx.token
    cl_ramp_ctx.on = true
    if not cl_ramp_stop_outputs(token) then return end
    -- 准备期没有开过送丝，取消此阶段不得产生停丝下降沿并触发不必要的回抽。
    cl_ramp_ctx.stopped = true
    local preparationStarted = cl_ramp_now()
    -- 零时长起弧在开光前就准备焊接终值，不能让串口排队把零坡度拖成出光后的台阶。
    if duration <= 0 then p0 = p1 w0 = w1 end
    if not cl_ramp_write(token, p0, w0, false, preparationStarted+cl_ramp_timeout) then return end
)";
    if (context.wireWelding && !context.mapped)
    {
        const BrandProtocolAdapter& adapter = GetBrandProtocolAdapter(resource.laserHeadBrand);
        output << "    if not cl_ramp_prepare(" << adapter.profile().wireFeederWriteSingleScriptCode
               << ", " << resource.wireFeederStatusLoc << ", "
               << adapter.wireFeederRunStatusValue(WIRE_WELDING_LASERSDK) << ", 2) then return end\n";
    }
    output << R"(
    if token ~= cl_ramp_ctx.token then return end
)" << wireOn << R"(
    if token ~= cl_ramp_ctx.token then return end
    cl_ramp_ctx.stopped = false
    cl_ramp_ctx.preparing = false
    cl_ramp_ctx.startAction = nil
    set_configurable_digital_out(1, true)
    if duration <= 0 then
        cl_ramp_record_phase(p1, p1, w1, w1, 0, cl_ramp_now(), true)
        return
    end
    cl_ramp_phase(p0, p1, w0, w1, duration)
end
function cl_ramp_down(p0, p1, w0, w1, duration)
    if not cl_ramp_ctx.on then return end
    cl_ramp_ctx.resume = nil
)";
    if (!context.mapped)
        output << "    if duration <= 0 then\n        cl_ramp_cancel()\n"
                  "        cl_ramp_record_phase(p1, p1, w1, w1, 0, cl_ramp_now(), true)\n        return\n    end\n";
    // 零距离的降功率点就是终点，自由通讯不得同步补帧阻塞随后的硬截止。
    output << R"(
    cl_ramp_phase(p0, p1, w0, w1, duration)
end
function cl_ramp_finish(p, w, postGas)
    cl_ramp_ctx.resume = nil
    cl_ramp_ctx.request = nil
    cl_ramp_ctx.phase = nil
    cl_ramp_ctx.phaseOwner = nil
    cl_ramp_ctx.resumePhaseOwner = nil
    cl_ramp_ctx.resuming = false
    cl_ramp_ctx.startAction = nil
    cl_ramp_ctx.preparing = false
    write_boolean_register(611, false)
    -- 已关光但停丝仍排队时，新终点调用必须接管停丝，不能把未提交的请求当作完成。
    if not cl_ramp_ctx.on and cl_ramp_ctx.stopped then return end
    local wasOn = cl_ramp_ctx.on
    cl_ramp_cancel()
    local token = cl_ramp_ctx.token
    cl_ramp_ctx.on = false
)";
    if (context.mapped)
        output << "    if wasOn and not cl_ramp_write(token, p, w, true) then return end\n";
    // 自由通讯终点必须先截止出光，不能把三个帧压进同一个发送缓冲或等待完整剩余坡度。
    output << R"(
    -- 正常停丝仅在轨迹终点触发一次；板卡负责其已配置的回抽和补丝。
    if not cl_ramp_stop_outputs(token) then return end
    if token ~= cl_ramp_ctx.token then return end
end
function cl_ramp_pause()
    if not cl_ramp_ctx.on then return end
    local snapshot = nil
    if cl_ramp_ctx.preparing and cl_ramp_ctx.startAction ~= nil then
        snapshot = {action=cl_ramp_ctx.startAction}
    elseif cl_ramp_ctx.request ~= nil then
        local target = cl_ramp_ctx.phase or cl_ramp_ctx.request
        local remaining = target.duration
        if cl_ramp_ctx.phase ~= nil then
            remaining = math.max(0, target.duration-(cl_ramp_now()-target.started))
        end
        snapshot = {p0=cl_ramp_ctx.power or target.p0, p1=target.p1,
                    w0=target.w0, w1=target.w1, duration=remaining, phaseOwner=cl_ramp_ctx.phaseOwner}
)";
    if (context.wireWelding)
        output << "        snapshot.w0 = cl_ramp_ctx.wire or target.w0\n";
    output << R"(
    end
    cl_ramp_ctx.resume = snapshot
    cl_ramp_cancel()
    local token = cl_ramp_ctx.token
    cl_ramp_ctx.on = false
    if not cl_ramp_stop_outputs(token) then return end
    if token ~= cl_ramp_ctx.token then return end
end
function cl_ramp_resume()
    local snapshot = cl_ramp_ctx.resume
    if snapshot == nil then return end
    cl_ramp_ctx.resuming = true
    cl_ramp_ctx.resume = nil
    if not read_boolean_register(611) then cl_ramp_ctx.resuming = false return end
    cl_ramp_ctx.resumePhaseOwner = snapshot.phaseOwner
    -- 恢复重新领取一个代次；暂停墙钟不计入剩余坡度，不让旧worker复活。
    if snapshot.action ~= nil then cl_ramp_submit_start(snapshot.action)
    else cl_ramp_start(snapshot.p0, snapshot.p1, snapshot.w0, snapshot.w1, snapshot.duration) end
    cl_ramp_ctx.resuming = false
end
function cl_ramp_wait_phase()
    local owner = cl_ramp_ctx.phaseOwner
    if owner == nil then return false end
    -- 逻辑阶段身份跨暂停恢复保持，新的起弧/工艺/阶段会替换身份，旧等待不得跟随新周期。
    while cl_ramp_ctx.phaseOwner == owner do
        local phase = cl_ramp_ctx.phase
        if phase ~= nil and phase.owner == owner and phase.completed then return true end
        if not cl_ramp_ctx.on and cl_ramp_ctx.resume == nil
                and not cl_ramp_ctx.resuming and not cl_ramp_ctx.preparing then return false end
        -- 只在未完成时让出调度；完成后没有固定的收尾等待，也不采用旧nominal墙钟。
        wait(1)
    end
    return false
end
)";
    script = output.str();
    return SUCCESS_LASER_SDK;
}

LaserSdkResult BuildStageScript(LaserMachineHandle handle, const MachineResource& resource,
                               SwitchTypeLaserSdk arcSwitch, bool rampDownOnly, std::string& script)
{
    Context context = {};
    context.config.structSize = sizeof(context.config);
    LaserSdkResult result = resolve(handle, resource, context);
    if (result != SUCCESS_LASER_SDK) return result;
    unsigned power[3] = {}, wire[3] = {};
    result = endpoints(context, resource, power, wire);
    if (result != SUCCESS_LASER_SDK) return result;
    const CgxiLaserPowerLaserSdk& parameters = resource.laserPowerParams;
    result = checkTimes(context, parameters);
    if (result != SUCCESS_LASER_SDK) return result;
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(17);
    const std::string startWire = context.wireWelding ? std::to_string(wire[0]) : "nil";
    const std::string weldWire = context.wireWelding ? std::to_string(wire[1]) : "nil";
    const std::string endWire = context.wireWelding ? std::to_string(wire[2]) : "nil";
    if (arcSwitch == SWITCH_ON_LASER_SDK)
    {
        output << "do\nlocal function requestedArcStart()\n";
        // 独立普通/慢速起弧也必须真实屏蔽板卡时间渐变，不能只依赖工艺下发的先后顺序。
        std::vector<LaserParameterValueSdk> envelope(4);
        const unsigned ids[] = {LASER_PARAM_LASER_ON_POWER_SDK, LASER_PARAM_LASER_OFF_POWER_SDK,
                                 LASER_PARAM_RAMP_UP_TIME_SDK, LASER_PARAM_RAMP_DOWN_TIME_SDK};
        const double values[] = {resource.laserHeadBrand == BCXLASERPOWER_SDK
                                 ? static_cast<double>(parameters.laserPowerOpen) : 100.0,
                                 resource.laserHeadBrand == BCXLASERPOWER_SDK
                                 ? static_cast<double>(parameters.laserPowerClose) : 100.0, 0.0, 0.0};
        for (unsigned i = 0; i < 4; ++i)
        {
            envelope[i].structSize = sizeof(LaserParameterValueSdk);
            envelope[i].parameterId = ids[i];
            envelope[i].value = values[i];
        }
        laser_parameters::WritePlan plan;
        result = laser_parameters::CompileWritePlan(context.parameters, envelope, plan);
        if (result != SUCCESS_LASER_SDK) return result;
        if (context.mapped)
        {
            for (std::size_t i = 0; i < plan.operations.size(); ++i)
                for (std::size_t j = 0; j < plan.operations[i].words.size(); ++j)
                    output << "write_int16_register(" << plan.operations[i].address + j << ", "
                           << plan.operations[i].words[j] << ")\n";
        }
        else
        {
            output << "do\nlocal preparationToken = cl_ramp_ctx.token\n";
            const unsigned code = GetBrandProtocolAdapter(resource.laserHeadBrand).profile().laserWriteSingleScriptCode;
            for (std::size_t i = 0; i < plan.operations.size(); ++i)
                for (std::size_t j = 0; j < plan.operations[i].words.size(); ++j)
                    output << "if preparationToken ~= cl_ramp_ctx.token then return end\n"
                           << "if not cl_ramp_prepare(" << code << ", " << plan.operations[i].address + j << ", "
                           << plan.operations[i].words[j] << ", 3) then return end\n";
            output << "if preparationToken ~= cl_ramp_ctx.token then return end\nend\n";
        }
        output << "cl_ramp_start(" << power[0] << ", " << power[1] << ", " << startWire << ", " << weldWire
               << ", " << parameters.laserRampUpTime << ")\n";
        output << "end\ncl_ramp_submit_start(requestedArcStart)\nend\n";
    }
    else if (arcSwitch != SWITCH_OFF_LASER_SDK) return PARAMETER_ERROR_LASER_SDK;
    else if (rampDownOnly)
        output << "cl_ramp_down(" << power[1] << ", " << power[2] << ", " << weldWire << ", " << endWire
               << ", " << parameters.laserRampDownTime << ")\n";
    else output << "cl_ramp_finish(" << power[2] << ", " << endWire << ")\n";
    script = output.str();
    return SUCCESS_LASER_SDK;
}

LaserSdkResult BuildCraftScript(LaserMachineHandle handle, const MachineResource& resource,
                               const CraftInfoLaserSdk& craft, std::string& script)
{
    Context context = {};
    context.config.structSize = sizeof(context.config);
    LaserSdkResult result = resolve(handle, resource, context);
    if (result != SUCCESS_LASER_SDK) return result;
    CraftInfoLaserSdk prepared = craft;
    // 自熔焊只使用无丝副本，后续兼容转换函数不会读取原工艺未初始化的送丝字段。
    if (!context.wireWelding)
    {
        prepared.craftWeldingWireFeed = 0;
        prepared.craftLeadInWireFeed = 0;
        prepared.craftLeadOutWireFeed = 0;
        prepared.craftEndingWireRetraction = 0;
        prepared.craftRetractionLength = 0;
        prepared.craftEndLoopbackLength = 0;
        prepared.wireDelayTime = 0;
        prepared.craftWireCutMode = NORMAL;
        prepared.craftStepEquivalent = 0;
    }
    if (!context.explicitlyConfigured)
    {
        // 显式外控不能绕过原默认型号的额定功率、图形、扫描及时间校验。
        CraftInfoLaserSdk validation = prepared;
        if (context.wireWelding)
        {
            if (nativeWire(context, craft.craftWeldingWireFeed) > std::numeric_limits<int>::max()
                    || nativeWire(context, craft.craftLeadInWireFeed) > std::numeric_limits<int>::max()
                    || nativeWire(context, craft.craftLeadOutWireFeed) > std::numeric_limits<int>::max())
                return PARAMETER_OUT_OF_RANGE_LASER_SDK;
            validation.craftWeldingWireFeed = static_cast<int>(nativeWire(context, craft.craftWeldingWireFeed));
            validation.craftLeadInWireFeed = static_cast<int>(nativeWire(context, craft.craftLeadInWireFeed));
            validation.craftLeadOutWireFeed = static_cast<int>(nativeWire(context, craft.craftLeadOutWireFeed));
        }
        result = GetBrandProtocolAdapter(resource.laserHeadBrand).checkCraft(validation);
        if (result != SUCCESS_LASER_SDK) return result;
    }
    std::vector<LaserParameterValueSdk> converted;
    result = laser_parameters::BuildCraftValues(resource, prepared, converted);
    if (result != SUCCESS_LASER_SDK) return result;
    std::vector<LaserParameterValueSdk> selected;
    const bool legacy = resource.laserHeadBrand == CQLASERPOWER_SDK;
    for (std::size_t i = 0; i < converted.size(); ++i)
    {
        LaserParameterValueSdk parameter = converted[i];
        if (parameter.parameterId == LASER_PARAM_RATED_POWER_SDK
                || (legacy && parameter.parameterId == LASER_PARAM_WELD_POWER_SDK)) continue;
        if (wireParameter(parameter.parameterId))
        {
            if (!context.wireWelding) continue;
            if (parameter.parameterId == LASER_PARAM_WIRE_FEED_SWITCH_SDK
                    || parameter.parameterId == LASER_PARAM_WIRE_RUN_STATE_SDK)
            {
                if (resource.laserHeadBrand != CQ_SUP26AD_LASERPOWER_SDK
                        || parameter.parameterId != LASER_PARAM_WIRE_FEED_SWITCH_SDK) continue;
                // 完整首站准备保持停丝，起弧阶段仍按原控制流程打开送丝。
                parameter.value = 0;
            }
            if (parameter.parameterId == LASER_PARAM_WIRE_FEED_SPEED_SDK
                    || parameter.parameterId == LASER_PARAM_START_WIRE_FEED_SPEED_SDK
                    || parameter.parameterId == LASER_PARAM_STOP_WIRE_FEED_SPEED_SDK)
                parameter.value = nativeWire(context, parameter.value);
        }
        // 外控只关闭板卡时间渐变，功率端点保留品牌协议语义，不能把四个字段一律清零。
        if (parameter.parameterId == LASER_PARAM_RAMP_UP_TIME_SDK
                || parameter.parameterId == LASER_PARAM_RAMP_DOWN_TIME_SDK) parameter.value = 0;
        if (resource.laserHeadBrand != BCXLASERPOWER_SDK
                && (parameter.parameterId == LASER_PARAM_LASER_ON_POWER_SDK
                    || parameter.parameterId == LASER_PARAM_LASER_OFF_POWER_SDK)) parameter.value = 100;
        bool writable = false;
        for (std::size_t j = 0; j < context.parameters.descriptors.size(); ++j)
            if (context.parameters.descriptors[j].parameterId == parameter.parameterId)
                writable = (context.parameters.descriptors[j].flags & LASER_PARAMETER_WRITABLE_SDK) != 0;
        if (writable) selected.push_back(parameter);
    }
    laser_parameters::CompleteSup26SelfWeldCraftValues(resource, selected);
    // 旧超强工艺仍设置初始峰值，但新实际运行功率只写快速峰值，保持额定功率不动。
    if (legacy)
    {
        LaserParameterValueSdk power = {};
        power.structSize = sizeof(power);
        power.parameterId = LASER_PARAM_FAST_PEAK_POWER_SDK;
        power.value = craft.craftLaserPower;
        selected.push_back(power);
    }
    MachineResource snapshot = resource;
    laser_parameters::UpdateCompilationResource(snapshot, prepared);
    result = checkTimes(context, snapshot.laserPowerParams);
    if (result != SUCCESS_LASER_SDK) return result;
    unsigned powers[3] = {}, wires[3] = {};
    result = endpoints(context, snapshot, powers, wires);
    if (result != SUCCESS_LASER_SDK) return result;
    // 使用本次已经扩充的配置；自由通讯仍走品牌协议渲染，映射仅写本地槽。
    std::string body;
    if (context.mapped)
        result = laser_parameters::BuildDeviceParametersScript(resource, &context.parameters, selected, body);
    else
    {
        laser_parameters::WritePlan plan;
        result = laser_parameters::CompileWritePlan(context.parameters, selected, plan);
        if (result == SUCCESS_LASER_SDK)
        {
            // 新外控脚本使用同一无后台重试的帧发送器，保持 Lua5.1 兼容和取消边界。
            const BrandProtocolProfile profile = GetBrandProtocolAdapter(resource.laserHeadBrand).profile();
            std::ostringstream emitted;
            emitted << "do\nlocal preparationToken = cl_ramp_ctx.token\n";
            for (std::size_t i = 0; i < plan.operations.size(); ++i)
            {
                const laser_parameters::WriteOperation& operation = plan.operations[i];
                const bool directWire = operation.parameterId >= LASER_PARAM_WIRE_FEED_SPEED_SDK
                        && operation.parameterId <= LASER_PARAM_MANUAL_RETRACT_SWITCH_SDK;
                for (std::size_t word = 0; word < operation.words.size(); ++word)
                    emitted << "if preparationToken ~= cl_ramp_ctx.token then return end\n"
                            << "if not cl_ramp_prepare(" << (directWire ? profile.wireFeederWriteSingleScriptCode
                                                                    : profile.laserWriteSingleScriptCode)
                            << ", " << operation.address + word << ", " << operation.words[word]
                            << ", " << (directWire ? 2 : 3) << ") then return end\n";
            }
            emitted << "if preparationToken ~= cl_ramp_ctx.token then return end\nend\n";
            body = emitted.str();
        }
    }
    if (result != SUCCESS_LASER_SDK) return result;
    std::ostringstream output;
    output << "if cl_ramp_cancel ~= nil then cl_ramp_cancel() end\n" << body
           << "if cl_ramp_retarget ~= nil then cl_ramp_retarget(" << powers[1] << ", "
           << (context.wireWelding ? std::to_string(wires[1]) : "nil") << ") end\n";
    script = output.str();
    return SUCCESS_LASER_SDK;
}
} // 命名空间 ramp_control
