#include "trmcontrol.h"
#include "cl_api.h"
#include "internaldatadefine.h"
#include "parametercompiler.h"
#include "utils.h"
#include <cmath>
#include <cstring>
#include <locale>
#include <sstream>

namespace {
constexpr unsigned kControlEnable = 1u;
constexpr unsigned kControlGas = 2u;
constexpr unsigned kControlLaser = 4u;
constexpr unsigned kControlKnown = kControlEnable | kControlGas | kControlLaser;
constexpr unsigned kAllCapabilities = TRM_CAP_AUTOMATIC_WELD_SDK | TRM_CAP_WIRE_WELD_SDK | TRM_CAP_GAS_CHECK_SDK | TRM_CAP_POINT_WELD_SDK |
    TRM_CAP_CRAFT_APPLY_SDK;
constexpr unsigned kAllConfirmations = TRM_CONF_ORDERED_FRESH_FEEDBACK_SDK | TRM_CONF_SAFE_STOP_NO_REPLAY_SDK | TRM_CONF_LIVE_POWER_SDK |
    TRM_CONF_LIVE_WIRE_SDK | TRM_CONF_GAS_TIMING_SDK | TRM_CONF_ZERO_WIRE_SAFE_SDK;
constexpr unsigned kRealtimeCount = 8u;
constexpr unsigned kStatusCount = 3u;
constexpr unsigned kRegisterLimit = 65535u;
constexpr unsigned kProcessNumber = 1u;
constexpr unsigned kMaximumUpdatePeriodMs = 1000u;
constexpr unsigned kMaximumTimeoutMs = 60000u;
constexpr int kMaximumGasMs = 3000;
constexpr int kMaximumRampMs = 60000;
constexpr double kMillisecondsPerSecond = 1000.0;
constexpr double kMaximumWireMPerMin = 6.0;
constexpr double kWireScale = 100.0;
constexpr double kMaximumScanWidthMm = 15.0;
constexpr double kWidthScale = 10.0;
constexpr unsigned kPowerLowerBoundDivisor = 10u;

// 公共SDK入口要求进程内串行；档案只储存连接数据，工艺总是来自当前请求。
std::map<LaserMachineHandle, LaserTrmProfileSdk> profiles;

bool has(unsigned mask, unsigned bits) {
    return (mask & bits) == bits;
}

bool overlaps(unsigned first, unsigned count, unsigned other, unsigned otherCount) {
    return first < other + otherCount && other < first + count;
}

bool validProfile(const LaserTrmProfileSdk &profile) {
    if (profile.transport != TRM_TRANSPORT_MAPPED_RTU_SDK || (profile.capabilities & ~kAllCapabilities) != 0 ||
        (profile.commissioningFlags & ~kAllConfirmations) != 0 ||
        !has(profile.commissioningFlags, TRM_CONF_ORDERED_FRESH_FEEDBACK_SDK | TRM_CONF_SAFE_STOP_NO_REPLAY_SDK)) {
        return false;
    }
    if (profile.realtimeWriteBase > kRegisterLimit - (kRealtimeCount - 1) || profile.realtimeReadBase > kRegisterLimit - (kRealtimeCount - 1) ||
        profile.statusReadBase > kRegisterLimit - (kStatusCount - 1) || profile.feedbackSequenceAddress > kRegisterLimit ||
        profile.runtimeStateAddress > kRegisterLimit || profile.feedbackSequenceAddress == profile.runtimeStateAddress ||
        overlaps(profile.realtimeWriteBase, kRealtimeCount, profile.realtimeReadBase, kRealtimeCount) ||
        overlaps(profile.realtimeWriteBase, kRealtimeCount, profile.statusReadBase, kStatusCount) ||
        overlaps(profile.realtimeReadBase, kRealtimeCount, profile.statusReadBase, kStatusCount)) {
        return false;
    }
    // 运行时同时保留上位机通用脚本结果寄存器，档案不能覆盖这些信号。
    const unsigned signals[] = {INT32_REG_WELD_STATUS_MONITOR_613, INT32_REG_PROGRAM_STOP_ARC_END_614, INT32_REG_SINGLE_STEP_RUN_616,
        INT32_REG_RUN_BREAK_POINT_617, INT32_REG_WELD_MODE_660, INT32_REG_WELD_PATH_662, INT32_REG_ARRIVE_POINT_LOC_663, INT32_REG_PRESENT_CRAFT_LOC_664,
        INT32_REG_ARC_START_FAIL_673, INT32_REG_WELD_INTERRUPT_674, INT32_REG_ARC_END_FAIL_675, INT32_REG_WELD_STATE_799, INT32_REG_NORMAL_RUN_ALL_SCRIPT_LOC,
        INT32_REG_COMMU_TIMEOUT_821};
    for (unsigned signal : signals) {
        if (profile.feedbackSequenceAddress == signal || profile.runtimeStateAddress == signal) {
            return false;
        }
    }
    if (profile.updatePeriodMs == 0 || profile.updatePeriodMs > kMaximumUpdatePeriodMs || profile.timeoutMs <= profile.updatePeriodMs ||
        profile.timeoutMs > kMaximumTimeoutMs || profile.ratedPowerW == 0 || profile.ratedPowerW > kRegisterLimit ||
        profile.minimumPowerW > profile.ratedPowerW || profile.minimumPowerW < (profile.ratedPowerW + kPowerLowerBoundDivisor - 1) / kPowerLowerBoundDivisor ||
        (profile.offControlWord & ~kControlEnable) != 0) {
        return false;
    }
    if ((has(profile.capabilities, TRM_CAP_POINT_WELD_SDK) && (profile.maximumPointWeldMs == 0 || profile.maximumPointWeldMs > kMaximumTimeoutMs)) ||
        (!has(profile.capabilities, TRM_CAP_POINT_WELD_SDK) && profile.maximumPointWeldMs != 0)) {
        return false;
    }
    if (has(profile.capabilities, TRM_CAP_GAS_CHECK_SDK) && profile.gasCheckControlWord != kControlGas) {
        return false;
    }
    const bool weld = (profile.capabilities & (TRM_CAP_AUTOMATIC_WELD_SDK | TRM_CAP_POINT_WELD_SDK)) != 0;
    if (weld &&
        (!has(profile.commissioningFlags, TRM_CONF_LIVE_POWER_SDK | TRM_CONF_GAS_TIMING_SDK) ||
            (profile.preGasControlWord & ~(kControlEnable | kControlGas)) != 0 || !has(profile.preGasControlWord, kControlGas) ||
            (profile.postGasControlWord & ~(kControlEnable | kControlGas)) != 0 || !has(profile.postGasControlWord, kControlGas) ||
            (profile.enableControlWord & ~(kControlEnable | kControlGas)) != 0 || !has(profile.enableControlWord, kControlEnable) ||
            (profile.laserOnControlWord & ~kControlKnown) != 0 || !has(profile.laserOnControlWord, kControlEnable | kControlLaser))) {
        return false;
    }
    return !has(profile.capabilities, TRM_CAP_WIRE_WELD_SDK) || (weld && has(profile.commissioningFlags, TRM_CONF_LIVE_WIRE_SDK));
}

bool inRange(double value, double maximum) {
    return std::isfinite(value) && value >= 0.0 && value <= maximum;
}

laser_parameters::Configuration parameterConfiguration(const MachineResource &resource, const LaserTrmProfileSdk &profile, bool offlineEncoding) {
    struct Parameter {
        unsigned id;
        unsigned offset;
        double min;
        double max;
        double scale;
        const char *name;
        const char *unit;
    };
    const unsigned rated = resource.laserPowerParams.laserPower > 0 ? static_cast<unsigned>(resource.laserPowerParams.laserPower) : 0u;
    const unsigned minimum = rated > 0 ? (rated + kPowerLowerBoundDivisor - 1) / kPowerLowerBoundDivisor : 1u;
    const double minPower = profile.capabilities != 0 ? profile.minimumPowerW : minimum;
    const double maxPower = profile.capabilities != 0 ? profile.ratedPowerW : rated > 0 ? rated : kRegisterLimit;
    const Parameter parameters[] = {{LASER_PARAM_WELD_POWER_SDK, 2, minPower, maxPower, 1, "激光功率", "W"},
        {LASER_PARAM_PWM_FREQUENCY_SDK, 3, 2, 5000, 1, "激光频率", "Hz"}, {LASER_PARAM_DUTY_CYCLE_SDK, 4, 0, 100, 1, "激光占空比", "%"},
        {LASER_PARAM_WIRE_FEED_SPEED_SDK, 5, 0, kMaximumWireMPerMin, kWireScale, "送丝速度", "m/min"},
        {LASER_PARAM_SCAN_FREQUENCY_SDK, 6, 0, 500, 1, "扫描频率", "Hz"},
        {LASER_PARAM_SCAN_WIDTH_SDK, 7, 0, kMaximumScanWidthMm, kWidthScale, "扫描宽度", "mm"}};
    laser_parameters::Configuration result;
    for (const auto &parameter : parameters) {
        LaserParameterDescriptorSdk descriptor = {};
        descriptor.structSize = sizeof(descriptor);
        descriptor.parameterId = parameter.id;
        descriptor.dataType = LASER_PARAMETER_U16_SDK;
        descriptor.wordOrder = LASER_PARAMETER_HIGH_FIRST_SDK;
        descriptor.rounding = LASER_PARAMETER_EXACT_SDK;
        descriptor.minimum = parameter.min;
        descriptor.maximum = parameter.max;
        descriptor.scale = parameter.scale;
        descriptor.flags = LASER_PARAMETER_WRITABLE_SDK | LASER_PARAMETER_READABLE_SDK;
        std::strcpy(descriptor.name, parameter.name);
        std::strcpy(descriptor.unit, parameter.unit);
        result.descriptors.push_back(descriptor);
        LaserParameterBindingSdk binding = {};
        binding.structSize = sizeof(binding);
        binding.parameterId = parameter.id;
        binding.writeAddress = LASER_PARAMETER_INVALID_ADDRESS_SDK;
        binding.readAddress = LASER_PARAMETER_INVALID_ADDRESS_SDK;
        if (profile.capabilities != 0) {
            binding.writeAddress = profile.realtimeWriteBase + parameter.offset;
            binding.readAddress = profile.realtimeReadBase + parameter.offset;
        } else if (offlineEncoding) {
            // 离线仅用偏移量做数值校验，未配置时绝不作为控制器写地址发布。
            binding.writeAddress = parameter.offset;
        }
        result.bindings.push_back(binding);
    }
    return result;
}

LaserParameterValueSdk parameterValue(unsigned id, double value) {
    LaserParameterValueSdk parameter = {};
    parameter.structSize = sizeof(parameter);
    parameter.parameterId = id;
    parameter.value = value;
    return parameter;
}

std::string command(const char *name) {
    return std::string("if not ") + name + " then trm_fault() return end\n";
}
}

namespace trm_control {
void Release(LaserMachineHandle handle) {
    profiles.erase(handle);
}

LaserSdkResult Snapshot(LaserMachineHandle handle, MachineResource &resource, LaserTrmProfileSdk &profile) {
    const auto it = machineResourcesMap.find(handle);
    if (it == machineResourcesMap.end()) {
        return HANDLE_ERROR_LASER_SDK;
    }
    if (it->second.laserHeadBrand != TRM_LASERPOWER_SDK || it->second.commuType != RS485_2_LASER_SDK) {
        return PARAM_TYPE_ERROR_LASER_SDK;
    }
    resource = it->second;
    const auto configured = profiles.find(handle);
    profile = configured != profiles.end() ? configured->second : LaserTrmProfileSdk{};
    profile.structSize = sizeof(profile);
    profile.version = LASER_TRM_PROFILE_VERSION_SDK;
    return SUCCESS_LASER_SDK;
}

bool CanWeld(const MachineResource &resource, const LaserTrmProfileSdk &profile, unsigned capability) {
    if (!has(profile.capabilities, capability)) {
        return false;
    }
    if (resource.weldingProcessType == WIRE_WELDING_LASERSDK) {
        return has(profile.capabilities, TRM_CAP_WIRE_WELD_SDK);
    }
    return resource.weldingProcessType == SELF_WELDING_LASERSDK && has(profile.commissioningFlags, TRM_CONF_ZERO_WIRE_SAFE_SDK);
}

LaserSdkResult BuildCraft(const MachineResource &resource, const LaserTrmProfileSdk &profile, const CraftInfoLaserSdk &craft, const LaserTrmCraftSdk &values,
    std::string &definition) {
    if (values.structSize != sizeof(values) || values.version != LASER_TRM_CRAFT_VERSION_SDK) {
        return PARAMETER_ERROR_LASER_SDK;
    }
    if (resource.weldingProcessType != SELF_WELDING_LASERSDK && resource.weldingProcessType != WIRE_WELDING_LASERSDK) {
        return WELD_TYPE_ERROR_LASER_SDK;
    }
    if (!std::isfinite(craft.craftWeldingSpeed) || craft.craftWeldingSpeed <= 0 || craft.craftLeadInRampTime < 0 ||
        craft.craftLeadInRampTime > kMaximumRampMs || craft.craftLeadOutRampDistance < 0 || craft.craftPreGasTime < 0 ||
        craft.craftPreGasTime > kMaximumGasMs || craft.craftPostGasTime < 0 || craft.craftPostGasTime > kMaximumGasMs ||
        craft.craftLeadInPower > craft.craftLaserPower || craft.craftLeadOutPower > craft.craftLaserPower) {
        return PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }
    const double rampDown = craft.craftLeadOutRampDistance * kMillisecondsPerSecond / craft.craftWeldingSpeed;
    if (!inRange(rampDown, kMaximumRampMs)) {
        return PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }
    const bool wire = resource.weldingProcessType == WIRE_WELDING_LASERSDK;
    const auto config = parameterConfiguration(resource, profile, true);
    const std::vector<LaserParameterValueSdk> weld = {parameterValue(LASER_PARAM_WELD_POWER_SDK, craft.craftLaserPower),
        parameterValue(LASER_PARAM_WIRE_FEED_SPEED_SDK, wire ? values.wireWeldMPerMin : 0.0),
        parameterValue(LASER_PARAM_PWM_FREQUENCY_SDK, values.laserFrequencyHz), parameterValue(LASER_PARAM_DUTY_CYCLE_SDK, values.dutyCyclePercent),
        parameterValue(LASER_PARAM_SCAN_FREQUENCY_SDK, values.scanFrequencyHz), parameterValue(LASER_PARAM_SCAN_WIDTH_SDK, values.scanWidthMm)};
    laser_parameters::WritePlan nominal, leadIn, leadOut;
    LaserSdkResult result = laser_parameters::CompileWritePlan(config, weld, nominal);
    const auto phase = [&config, wire](int power, double speed, laser_parameters::WritePlan &plan) {
        return laser_parameters::CompileWritePlan(config,
            {parameterValue(LASER_PARAM_WELD_POWER_SDK, power), parameterValue(LASER_PARAM_WIRE_FEED_SPEED_SDK, wire ? speed : 0.0)}, plan);
    };
    if (result == SUCCESS_LASER_SDK) {
        result = phase(craft.craftLeadInPower, wire ? values.wireLeadInMPerMin : 0.0, leadIn);
    }
    if (result == SUCCESS_LASER_SDK) {
        result = phase(craft.craftLeadOutPower, wire ? values.wireLeadOutMPerMin : 0.0, leadOut);
    }
    if (result != SUCCESS_LASER_SDK) {
        return result;
    }
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "{process=" << kProcessNumber << ", p0=" << leadIn.operations[0].words[0] << ", p1=" << nominal.operations[0].words[0]
           << ", p2=" << leadOut.operations[0].words[0] << ", w0=" << leadIn.operations[1].words[0] << ", w1=" << nominal.operations[1].words[0]
           << ", w2=" << leadOut.operations[1].words[0] << ", wire=" << (wire ? "true" : "false") << ", hz=" << nominal.operations[2].words[0]
           << ", duty=" << nominal.operations[3].words[0] << ", scan=" << nominal.operations[4].words[0] << ", width=" << nominal.operations[5].words[0]
           << ", up=" << craft.craftLeadInRampTime << ", down=" << std::lround(rampDown) << ", pre=" << craft.craftPreGasTime
           << ", post=" << craft.craftPostGasTime << "}";
    definition = output.str();
    return SUCCESS_LASER_SDK;
}
} // namespace trm_control

LaserSdkResult cl_trm_profile_set(LaserMachineHandle handle, const LaserTrmProfileSdk *profile) {
    if (profile == nullptr || profile->structSize != sizeof(*profile) || profile->version != LASER_TRM_PROFILE_VERSION_SDK) {
        return PARAMETER_ERROR_LASER_SDK;
    }
    MachineResource resource;
    LaserTrmProfileSdk previous;
    const LaserSdkResult result = trm_control::Snapshot(handle, resource, previous);
    if (result != SUCCESS_LASER_SDK) {
        return result;
    }
    if (profile->capabilities == 0) {
        trm_control::Release(handle);
        return SUCCESS_LASER_SDK;
    }
    const int resourcePower = resource.laserPowerParams.laserPower;
    if (!validProfile(*profile) || (resourcePower > 0 && static_cast<unsigned>(resourcePower) != profile->ratedPowerW)) {
        return PARAMETER_ERROR_LASER_SDK;
    }
    try {
        profiles[handle] = *profile;
        return SUCCESS_LASER_SDK;
    } catch (const std::bad_alloc &) {
        return MEMORY_ERROR_LASER_SDK;
    }
}

LaserSdkResult cl_trm_profile_get(LaserMachineHandle handle, LaserTrmProfileSdk *profile) {
    if (profile == nullptr || profile->structSize != sizeof(*profile)) {
        return PARAMETER_ERROR_LASER_SDK;
    }
    MachineResource resource;
    return trm_control::Snapshot(handle, resource, *profile);
}

LaserSdkResult cl_trm_craft_validate(LaserMachineHandle handle, const CraftInfoLaserSdk *craft, const LaserTrmCraftSdk *values) {
    if (craft == nullptr || values == nullptr) {
        return PARAMETER_ERROR_LASER_SDK;
    }
    try {
        MachineResource resource;
        LaserTrmProfileSdk profile;
        const LaserSdkResult result = trm_control::Snapshot(handle, resource, profile);
        if (result != SUCCESS_LASER_SDK) {
            return result;
        }
        std::string ignored;
        return trm_control::BuildCraft(resource, profile, *craft, *values, ignored);
    } catch (const std::bad_alloc &) {
        return MEMORY_ERROR_LASER_SDK;
    } catch (...) {
        return OTHER_ERROR_LASER_SDK;
    }
}

namespace trm_control {
LaserSdkResult GetParameterConfiguration(LaserMachineHandle handle, laser_parameters::Configuration &configuration) {
    MachineResource resource;
    LaserTrmProfileSdk profile;
    const LaserSdkResult result = Snapshot(handle, resource, profile);
    if (result != SUCCESS_LASER_SDK) {
        return result;
    }
    configuration = parameterConfiguration(resource, profile, false);
    return SUCCESS_LASER_SDK;
}

LaserSdkResult BuildParameterScript(LaserMachineHandle handle, const std::vector<LaserParameterValueSdk> &values, std::string &script,
    LaserParameterDiagnosticSdk *diagnostic) {
    MachineResource resource;
    LaserTrmProfileSdk profile;
    const LaserSdkResult result = Snapshot(handle, resource, profile);
    if (result != SUCCESS_LASER_SDK) {
        return result;
    }
    if (values.empty()) {
        script.clear();
        return SUCCESS_LASER_SDK;
    }
    if (!has(profile.capabilities, TRM_CAP_CRAFT_APPLY_SDK)) {
        return COMMU_ERROR_LASER_SDK;
    }
    for (const auto &value : values) {
        if (value.parameterId == LASER_PARAM_WIRE_FEED_SPEED_SDK && resource.weldingProcessType == SELF_WELDING_LASERSDK &&
            (value.value != 0.0 || !has(profile.commissioningFlags, TRM_CONF_ZERO_WIRE_SAFE_SDK))) {
            if (diagnostic != nullptr) {
                diagnostic->parameterId = value.parameterId;
                diagnostic->result = PARAM_WRITE_ERROR_LASER_SDK;
            }
            return PARAM_WRITE_ERROR_LASER_SDK;
        }
    }
    laser_parameters::WritePlan plan;
    const LaserSdkResult compiled = laser_parameters::CompileWritePlan(parameterConfiguration(resource, profile, false), values, plan, diagnostic);
    if (compiled != SUCCESS_LASER_SDK) {
        return compiled;
    }
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << BuildRuntime(profile, true) << "local fields = {";
    for (const auto &operation : plan.operations) {
        output << "[" << operation.address - profile.realtimeWriteBase << "]=" << operation.words[0] << ",";
    }
    output << "}\n"
           << "local previous = read_integer_register(trm_seq)\n"
           << "if not trm_wait_feedback(previous, {[0]=" << kProcessNumber << ", [1]=trm_off}, 0, true, trm.token, false) or\n"
           << "    not trm_set(fields, 0, true, trm.token, false) then trm_publish(TRM_FAULT) return end\n"
           << "trm_publish(TRM_APPLIED)\n";
    script = output.str();
    return SUCCESS_LASER_SDK;
}

std::string BuildRuntime(const LaserTrmProfileSdk &p, bool parametersOnly) {
    std::ostringstream out;
    out << "local trm_tx, trm_rx = " << p.realtimeWriteBase << ", " << p.realtimeReadBase << "\n";
    out << "local trm_status, trm_seq, trm_state = " << p.statusReadBase << ", " << p.feedbackSequenceAddress << ", " << p.runtimeStateAddress << "\n";
    out << "local trm_period, trm_timeout, trm_maxPoint = " << p.updatePeriodMs << ", " << p.timeoutMs << ", " << p.maximumPointWeldMs << "\n";
    out << "local trm_pre, trm_enable, trm_on, trm_post, trm_off, trm_gas = " << p.preGasControlWord << ", " << p.enableControlWord << ", "
        << p.laserOnControlWord << ", " << p.postGasControlWord << ", " << p.offControlWord << ", " << p.gasCheckControlWord << "\n";
    out << "local TRM_IDLE, TRM_PREPARING, TRM_EMITTING, TRM_PAUSED, TRM_STOPPING, TRM_FAULT, TRM_GAS, TRM_APPLIED = " << TRM_STATE_IDLE_SDK << ", "
        << TRM_STATE_PREPARING_SDK << ", " << TRM_STATE_EMITTING_SDK << ", " << TRM_STATE_PAUSED_SDK << ", " << TRM_STATE_STOPPING_SDK << ", "
        << TRM_STATE_FAULT_SDK << ", " << TRM_STATE_GAS_SDK << ", " << TRM_STATE_CRAFT_APPLIED_SDK << "\n";
    out << R"(
local trm = {token=0, state=0}
local function trm_now()
    local t = get_systime()
    return t[1] * 1000 + t[2] / 1000
end
local function trm_publish(state)
    trm.state = state
    write_integer_register(trm_state, state)
end
local function trm_cancel()
    trm.token = trm.token + 1
    trm.phase = nil
    return trm.token
end
local function trm_u16(value)
    return value % 65536
end
local function trm_feedback(previous, fields, emission, healthy, strict)
    local observedAt = trm_now()
    local seq = read_integer_register(trm_seq)
    if seq == previous then return nil, seq, observedAt end
    local light = trm_u16(read_int16_register(trm_status))
    local low = trm_u16(read_int16_register(trm_status + 1))
    local high = trm_u16(read_int16_register(trm_status + 2))
    local matches = light == emission
    for offset, expected in pairs(fields) do
        if trm_u16(read_int16_register(trm_rx + offset)) ~= expected then matches = false end
    end
    if seq ~= read_integer_register(trm_seq) then return nil end
    if healthy and (low ~= 0 or high ~= 0) then return false end
    if matches then return true, seq, observedAt end
    if strict then return false end
    return nil
end
local function trm_valid(token, held)
    return token == trm.token and (not held or (read_boolean_register(653) and not read_boolean_register(650)))
end
local function trm_wait_feedback(previous, fields, emission, healthy, token, held)
    local deadline = trm_now() + trm_timeout
    while trm_valid(token, held) and trm_now() <= deadline do
        local ok = trm_feedback(previous, fields, emission, healthy)
        if ok ~= nil then return ok and trm_valid(token, held) and trm_now() <= deadline end
        wait(1)
    end
    return false
end
local function trm_set(fields, emission, healthy, token, held)
    if not trm_valid(token, held) then return false end
    local previous = read_integer_register(trm_seq)
    -- 每次提交使用同一组目标，等待远端新鲜读回，不使用本地写槽确认。
    for offset, value in pairs(fields) do
        write_int16_register(trm_tx + offset, value)
    end
    return trm_wait_feedback(previous, fields, emission, healthy, token, held)
end
local function trm_apply(craft)
    local token = trm.token
    local previous = read_integer_register(trm_seq)
    if not trm_wait_feedback(previous, {[1]=trm_off}, 0, true, token, false) then return false end
    -- 先选择流程，收到远端确认后再下发工艺，避免流程覆盖刚写入的工艺值。
    if not trm_set({[0]=craft.process}, 0, true, token, false) then return false end
    if not trm_set({[2]=craft.p1, [3]=craft.hz, [4]=craft.duty, [5]=craft.w1, [6]=craft.scan, [7]=craft.width}, 0, true, token, false) then
        return false
    end
    trm.craft = craft
    trm_publish(TRM_APPLIED)
    return true
end
)";
    if (parametersOnly) {
        return out.str();
    }
    out << R"(
local function trm_control(value, emission, healthy, token, held)
    return trm_set({[1]=value}, emission, healthy, token, held)
end
local function trm_feedback_window()
    local observedAt = trm_now()
    return {sequence=read_integer_register(trm_seq), observedAt=observedAt, freshBy=observedAt+trm_timeout}
end
local function trm_check_fresh(window, word, emission, strict)
    if trm_now() > window.freshBy then return false end
    local ok, sequence, observedAt = trm_feedback(window.sequence, {[1]=word}, emission, true, strict)
    local completedAt = trm_now()
    if ok == false or completedAt > window.freshBy then return false end
    if ok == true then
        -- 本次新采样的最早时刻按上次看到旧序号的时间估计，读回耗时不能延长证明窗口。
        local freshBy = window.observedAt + trm_timeout
        if completedAt > freshBy then return false end
        window.sequence = sequence
        window.freshBy = freshBy
    end
    if sequence ~= nil then window.observedAt = observedAt end
    return true
end
local function trm_delay(duration, token, held, word, emission)
    local deadline = trm_now() + duration
    local window = trm_feedback_window()
    while trm_now() < deadline do
        if not trm_valid(token, held) or not trm_check_fresh(window, word, emission, false) then return false end
        wait(math.max(1, math.min(trm_period, deadline - trm_now(), window.freshBy - trm_now())))
    end
    return trm_valid(token, held) and trm_now() <= window.freshBy
end
local function trm_stop_outputs(nextState, post)
    local token = trm_cancel()
    trm.pointDeadline = nil
    trm_publish(TRM_STOPPING)
    write_boolean_register(611, false)
    write_integer_register(614, 0)
    -- 终点优先提交关光。只有普通收尾且配置延时才保留气体。
    local offWord = post > 0 and trm_post or trm_off
    local ok = trm_control(offWord, 0, false, token, false)
    if post > 0 and token == trm.token then
        local held = ok and trm_delay(post, token, false, trm_post, 0)
        if token ~= trm.token then return false end
        ok = trm_control(trm_off, 0, false, token, false) and held
    end
    if token ~= trm.token then return false end
    if ok then
        trm_publish(nextState)
    else
        write_integer_register(675, 1)
        trm_publish(TRM_FAULT)
    end
    return ok
end
local function trm_abort()
    if trm.faulting then
        -- 若固件在halt后调度停止事件，补充确认已提交的关光；故障状态保持。
        if trm.faultSequence ~= nil and trm_wait_feedback(trm.faultSequence, {[1]=trm_off}, 0, false, trm.token, false) then
            write_integer_register(675, 0)
        end
        return false
    end
    local interrupted = trm.state == TRM_PREPARING or trm.state == TRM_EMITTING or trm.state == TRM_PAUSED
    trm.resume = nil
    if interrupted then write_integer_register(674, 1) end
    return trm_stop_outputs(TRM_IDLE, 0)
end
local function trm_fault()
    if trm.faulting then return end
    trm.faulting = true
    trm.resume = nil
    trm.pointDeadline = nil
    trm_cancel()
    trm.faultSequence = read_integer_register(trm_seq)
    -- 先提交安全字，再立即停止运动；远端确认不能阻塞急停路径。
    write_int16_register(trm_tx + 1, trm_off)
    write_boolean_register(611, false)
    write_integer_register(614, 0)
    write_integer_register(675, 1)
    trm_publish(TRM_FAULT)
    halt()
end
local function trm_session_init()
    write_integer_register(673, 0)
    write_integer_register(674, 0)
    write_integer_register(675, 0)
    return trm_abort()
end
local function trm_targets(p, w, emission, token, held)
    local fields = {[2]=math.floor(p + 0.5)}
    if trm.craft.wire or emission == 0 then fields[5] = math.floor(w + 0.5) end
    if not trm_set(fields, emission, true, token, held) then return false end
    trm.power = fields[2]
    trm.wire = fields[5] or 0
    return true
end
local function trm_phase(p, w, duration)
    local token = trm_cancel()
    local p0, w0, started = trm.power, trm.wire, trm_now()
    local phase = {p=p, w=w, deadline=started+duration, complete=false}
    trm.phase = phase
    task_create(function()
        while token == trm.token and trm.state == TRM_EMITTING do
            local elapsed = math.max(0, trm_now() - started)
            local ratio = duration == 0 and 1 or math.min(1, elapsed / duration)
            if not trm_targets(p0 + (p-p0)*ratio, w0 + (w-w0)*ratio, 1, token, false) then
                if token == trm.token then trm_fault() end
                return
            end
            if ratio >= 1 then phase.complete = true return end
            wait(math.max(1, math.min(trm_period, phase.deadline - trm_now())))
        end
    end)
end
local function trm_watch()
    task_create(function()
        local window = trm_feedback_window()
        while true do
            if trm.pointDeadline ~= nil and trm_now() >= trm.pointDeadline then trm_fault() return end
            local sleep = math.min(trm_period, 10)
            if trm.state == TRM_EMITTING or trm.state == TRM_GAS then
                local emission = trm.state == TRM_EMITTING and 1 or 0
                local word = emission == 1 and trm_on or trm_gas
                if not trm_check_fresh(window, word, emission, true) then trm_fault() return end
                sleep = math.min(sleep, math.max(1, window.freshBy - trm_now()))
            else
                window = trm_feedback_window()
            end
            if trm.pointDeadline ~= nil then sleep = math.min(sleep, math.max(1, trm.pointDeadline - trm_now())) end
            wait(sleep)
        end
    end)
end
local function trm_begin(p0, w0, p, w, duration, held)
    if trm.craft == nil then return false end
    local token = trm_cancel()
    trm_publish(TRM_PREPARING)
    local initialP = duration == 0 and p or p0
    local initialW = duration == 0 and w or w0
    if not trm_targets(initialP, initialW, 0, token, held) then return false end
    if trm.craft.pre > 0 then
        if not trm_control(trm_pre, 0, true, token, held) or not trm_delay(trm.craft.pre, token, held, trm_pre, 0) then return false end
    end
    if not trm_control(trm_enable, 0, true, token, held) then return false end
    if held then trm.pointDeadline = trm_now() + trm_maxPoint end
    if not trm_control(trm_on, 1, true, token, held) then return false end
    if not trm_valid(token, held) then return false end
    trm_publish(TRM_EMITTING)
    write_boolean_register(611, true)
    write_integer_register(614, 1)
    trm_phase(p, w, duration)
    return true
end
local function trm_start_selected(held)
    if trm.craft == nil or (trm.state ~= TRM_APPLIED and trm.state ~= TRM_IDLE) then return false end
    write_integer_register(673, 0)
    local c = trm.craft
    if trm_begin(c.p0, c.w0, c.p1, c.w1, c.up, held) then return true end
    write_integer_register(673, 1)
    return false
end
local function trm_motion_ready()
    local preGas = trm.craft and trm.craft.pre or 0
    local deadline = trm_now() + preGas + trm_timeout * 6
    while (trm.state == TRM_PAUSED or trm.state == TRM_PREPARING or trm.pausing) and trm_now() <= deadline do
        wait(1)
    end
    return trm.state == TRM_EMITTING and not trm.faulting
end
local function trm_down()
    if trm.state ~= TRM_EMITTING then return false end
    trm_phase(trm.craft.p2, trm.craft.w2, trm.craft.down)
    return true
end
local function trm_finish()
    trm.resume = nil
    return trm_stop_outputs(TRM_IDLE, trm.craft and trm.craft.post or 0)
end
local function trm_point_release()
    if not trm_down() then return false end
    local phase = trm.phase
    while trm.phase == phase and not phase.complete do wait(1) end
    if trm.phase ~= phase then return false end
    return trm_finish()
end
local function trm_gas_start()
    if trm.state ~= TRM_IDLE and trm.state ~= TRM_APPLIED then return false end
    if not trm_control(trm_gas, 0, true, trm.token, false) then return false end
    trm_publish(TRM_GAS)
    return true
end
)";
    return out.str();
}

std::string BuildProtection() {
    return R"(
local function trm_pause()
    if trm.state ~= TRM_EMITTING or trm.phase == nil then
        if trm.state == TRM_PREPARING or trm.state == TRM_STOPPING or trm.state == TRM_GAS then trm_abort() end
        return
    end
    local phase = trm.phase
    local resume = {p0=trm.power, w0=trm.wire, p=phase.p, w=phase.w, duration=math.max(0, phase.deadline-trm_now())}
    trm.pausing = true
    local stopped = trm_stop_outputs(TRM_PAUSED, 0)
    trm.pausing = false
    if stopped then trm.resume = resume elseif trm.state == TRM_FAULT then trm_fault() end
end
local function trm_resume()
    local resume = trm.resume
    trm.resume = nil
    if trm.state == TRM_PAUSED and resume ~= nil then
        trm_publish(TRM_PREPARING)
        local previous = read_integer_register(trm_seq)
        if not trm_wait_feedback(previous, {[1]=trm_off}, 0, true, trm.token, false) or
            not trm_begin(resume.p0, resume.w0, resume.p, resume.w, resume.duration, false) then trm_fault() end
    end
end
sys_event_register(1, trm_abort)
sys_event_register(2, trm_pause)
sys_event_register(3, trm_resume)
)";
}
} // namespace trm_control

LaserSdkResult cl_trm_craft_script_get(LaserMachineHandle handle, const CraftInfoLaserSdk *craft, const LaserTrmCraftSdk *values, char *scripts,
    int *scriptLen) {
    if (craft == nullptr || values == nullptr || scriptLen == nullptr) {
        return PARAMETER_ERROR_LASER_SDK;
    }
    try {
        MachineResource resource;
        LaserTrmProfileSdk profile;
        LaserSdkResult result = trm_control::Snapshot(handle, resource, profile);
        if (result != SUCCESS_LASER_SDK) {
            return result;
        }
        if (!has(profile.capabilities, TRM_CAP_CRAFT_APPLY_SDK) ||
            (resource.weldingProcessType == SELF_WELDING_LASERSDK && !has(profile.commissioningFlags, TRM_CONF_ZERO_WIRE_SAFE_SDK))) {
            return COMMU_ERROR_LASER_SDK;
        }
        std::string definition;
        result = trm_control::BuildCraft(resource, profile, *craft, *values, definition);
        if (result != SUCCESS_LASER_SDK) {
            return result;
        }
        // 工艺下发保持关光，不注册设备动作或写控制字。失败只发布结果。
        return strToChar(scripts, scriptLen,
            trm_control::BuildRuntime(profile, true) + "if not trm_apply(" + definition + ") then trm_publish(TRM_FAULT) return end\n");
    } catch (const std::bad_alloc &) {
        return MEMORY_ERROR_LASER_SDK;
    } catch (...) {
        return OTHER_ERROR_LASER_SDK;
    }
}

LaserSdkResult cl_trm_action_script_get(LaserMachineHandle handle, LaserTrmActionSdk action, const CraftInfoLaserSdk *craft, const LaserTrmCraftSdk *values,
    char *scripts, int *scriptLen) {
    if (scriptLen == nullptr || (craft == nullptr) != (values == nullptr)) {
        return PARAMETER_ERROR_LASER_SDK;
    }
    try {
        MachineResource resource;
        LaserTrmProfileSdk profile;
        LaserSdkResult result = trm_control::Snapshot(handle, resource, profile);
        if (result != SUCCESS_LASER_SDK) {
            return result;
        }
        if (profile.capabilities == 0) {
            return COMMU_ERROR_LASER_SDK;
        }
        if (action == TRM_ACTION_ABORT_SDK || (action == TRM_ACTION_GAS_STOP_SDK && has(profile.capabilities, TRM_CAP_GAS_CHECK_SDK))) {
            return strToChar(scripts, scriptLen, trm_control::BuildRuntime(profile) + "trm_abort()\n");
        }
        if (action != TRM_ACTION_MANUAL_SESSION_SDK && action != TRM_ACTION_GAS_START_SDK && action != TRM_ACTION_POINT_START_SDK) {
            return PARAM_TYPE_ERROR_LASER_SDK;
        }
        const bool gas = action != TRM_ACTION_POINT_START_SDK && has(profile.capabilities, TRM_CAP_GAS_CHECK_SDK);
        const bool point = action != TRM_ACTION_GAS_START_SDK && craft != nullptr && trm_control::CanWeld(resource, profile, TRM_CAP_POINT_WELD_SDK);
        if (!gas && !point) {
            return COMMU_ERROR_LASER_SDK;
        }
        std::string definition;
        if (point) {
            result = trm_control::BuildCraft(resource, profile, *craft, *values, definition);
            if (result != SUCCESS_LASER_SDK) {
                return result;
            }
        }
        std::string body = trm_control::BuildRuntime(profile);
        // 手动动作按键松开由同一会话处理。暂停取消本次操作，继续也不会复活已按住的按键。
        body += R"(
local trm_manualRevision = 0
local function trm_manual_pause()
    trm_manualRevision = trm_manualRevision + 1
    trm_abort()
end
sys_event_register(1, trm_abort)
sys_event_register(2, trm_manual_pause)
)";
        body += command("trm_session_init()");
        body += "trm_watch()\n";
        body += gas ? "local trm_hasGas = true\n" : "local trm_hasGas = false\n";
        body += point ? "local trm_manualCraft = " + definition + "\n" : "local trm_manualCraft = nil\n";
        body += R"(
local priorGas = true
local priorPoint = true
local previousRevision = trm_manualRevision
while true do
    if previousRevision ~= trm_manualRevision then
        priorGas = true
        priorPoint = true
        previousRevision = trm_manualRevision
    end
    local gas = trm_hasGas and read_boolean_register(650)
    local point = trm_manualCraft ~= nil and read_boolean_register(653)
    if gas and point then
        if trm.state == TRM_GAS or trm.state == TRM_EMITTING then
            if not trm_abort() then trm_fault() return end
        end
    elseif trm.state == TRM_GAS then
        if not gas then
            if not trm_abort() then trm_fault() return end
        end
    elseif trm.state == TRM_EMITTING then
        if not point then
            if not trm_point_release() and previousRevision == trm_manualRevision then trm_fault() return end
        end
    elseif gas and not priorGas then
        if not trm_gas_start() then trm_fault() return end
    elseif point and not priorPoint then
        if not trm_apply(trm_manualCraft) then trm_fault() return end
        if read_boolean_register(653) and previousRevision == trm_manualRevision and not trm_start_selected(true) then
            if not read_boolean_register(653) or previousRevision ~= trm_manualRevision then
                if not trm_abort() then trm_fault() return end
            else
                trm_fault()
                return
            end
        end
    end
    priorGas = gas
    priorPoint = point
    wait(10)
end
)";
        return strToChar(scripts, scriptLen, body);
    } catch (const std::bad_alloc &) {
        return MEMORY_ERROR_LASER_SDK;
    } catch (...) {
        return OTHER_ERROR_LASER_SDK;
    }
}
