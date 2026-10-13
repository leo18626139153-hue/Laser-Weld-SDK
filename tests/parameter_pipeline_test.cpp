#include "cl_api.h"
#include "parameterdefine.h"
#include "common/parametercompiler.h"
#include "common/parameterscript.h"
#include "common/rs485_commu.h"
#include "brand/deviceparameters.h"
#include "craftscript.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

extern int parameter_test_robot_calls;
namespace lp = laser_parameters;

namespace
{
unsigned checks = 0;
void Check(bool condition, const char* expression, int line)
{
    ++checks;
    if (!condition)
    {
        std::ostringstream message;
        message << "line " << line << ": " << expression;
        throw std::runtime_error(message.str());
    }
}
#define CHECK(expression) Check((expression), #expression, __LINE__)

LaserParameterDescriptorSdk Descriptor(unsigned id, LaserParameterDataTypeSdk type,
                                      double minimum, double maximum)
{
    LaserParameterDescriptorSdk value = {};
    value.structSize = sizeof(value);
    value.parameterId = id;
    value.dataType = type;
    value.wordOrder = LASER_PARAMETER_HIGH_FIRST_SDK;
    value.rounding = LASER_PARAMETER_EXACT_SDK;
    value.scale = 1;
    value.minimum = minimum;
    value.maximum = maximum;
    value.flags = LASER_PARAMETER_WRITABLE_SDK;
    return value;
}

LaserParameterBindingSdk Binding(unsigned id, unsigned address)
{
    LaserParameterBindingSdk value = {};
    value.structSize = sizeof(value);
    value.parameterId = id;
    value.writeAddress = address;
    value.readAddress = LASER_PARAMETER_INVALID_ADDRESS_SDK;
    return value;
}

LaserParameterValueSdk Value(unsigned id, double physicalValue)
{
    LaserParameterValueSdk value = {};
    value.structSize = sizeof(value);
    value.parameterId = id;
    value.value = physicalValue;
    return value;
}

lp::Configuration OneParameter(LaserParameterDataTypeSdk type = LASER_PARAMETER_U16_SDK)
{
    lp::Configuration result;
    result.descriptors.push_back(Descriptor(1001, type, -1e12, 1e12));
    result.bindings.push_back(Binding(1001, 0));
    return result;
}

lp::WritePlan Compile(const lp::Configuration& config,
                      const std::vector<LaserParameterValueSdk>& values)
{
    lp::WritePlan plan;
    LaserParameterDiagnosticSdk diagnostic = {};
    diagnostic.structSize = sizeof(diagnostic);
    LaserSdkResult result = lp::CompileWritePlan(config, values, plan, &diagnostic);
    if (result != SUCCESS_LASER_SDK)
    {
        std::ostringstream error;
        error << "CompileWritePlan result=" << result << " id="
              << diagnostic.parameterId << " message=" << diagnostic.message;
        throw std::runtime_error(error.str());
    }
    return plan;
}

// 同时检查数值、位模式和地址，避免仅验证脚本含有某个函数名。
std::map<unsigned, int> ParseWrites(const std::string& script)
{
    std::map<unsigned, int> writes;
    const std::regex pattern("write_int16_register\\(\\s*(0x[0-9a-fA-F]+|[0-9]+)\\s*,\\s*(-?[0-9]+)\\s*\\)");
    for (std::sregex_iterator it(script.begin(), script.end(), pattern), end; it != end; ++it)
    {
        const unsigned address = static_cast<unsigned>(std::stoul((*it)[1].str(), nullptr, 0));
        CHECK(writes.count(address) == 0);
        writes[address] = std::stoi((*it)[2].str());
    }
    return writes;
}

void CheckMappedTransport(const std::string& script)
{
    CHECK(script.find("serial_send_receive") == std::string::npos);
    CHECK(script.find("write_value_port") == std::string::npos);
    CHECK(script.find("crc16") == std::string::npos);
    CHECK(script.find("COM") == std::string::npos);
}

void CheckMappedScript(const std::string& script)
{
    CheckMappedTransport(script);
    CHECK(script.find("set_configurable_digital_out") == std::string::npos);
}

void CheckPlanScript(const lp::WritePlan& plan, const std::string& script)
{
    CheckMappedScript(script);
    const std::map<unsigned, int> actual = ParseWrites(script);
    unsigned wordCount = 0;
    for (std::size_t index = 0; index < plan.operations.size(); ++index)
    {
        const lp::WriteOperation& op = plan.operations[index];
        for (std::size_t word = 0; word < op.words.size(); ++word)
        {
            const unsigned address = op.address + static_cast<unsigned>(word);
            CHECK(actual.count(address) == 1);
            int expected = op.words[word];
            if (op.signedSingleWord && expected >= 32768) expected -= 65536;
            CHECK(actual.at(address) == expected);
            ++wordCount;
        }
    }
    CHECK(actual.size() == wordCount);
}

/** @brief 校验物理值缩放、带符号位模式、三十二位字序与整数舍入。 */
void TestIntegerAndFloatEncoding()
{
    lp::Configuration config;
    config.descriptors.push_back(Descriptor(101, LASER_PARAMETER_I16_SDK, -100, 100));
    config.descriptors.back().scale = 10;
    config.descriptors.back().offset = -5;
    config.bindings.push_back(Binding(101, 0));
    config.descriptors.push_back(Descriptor(102, LASER_PARAMETER_U32_SDK, 0, 4294967295.0));
    config.bindings.push_back(Binding(102, 2));
    config.descriptors.push_back(config.descriptors.back());
    config.descriptors.back().parameterId = 103;
    config.descriptors.back().wordOrder = LASER_PARAMETER_LOW_FIRST_SDK;
    config.bindings.push_back(Binding(103, 4));
    config.descriptors.push_back(Descriptor(104, LASER_PARAMETER_I32_SDK, -2147483648.0, 2147483647.0));
    config.bindings.push_back(Binding(104, 6));
    config.descriptors.push_back(Descriptor(105, LASER_PARAMETER_F32_SDK, -100, 100));
    config.bindings.push_back(Binding(105, 8));
    config.descriptors.push_back(config.descriptors.back());
    config.descriptors.back().parameterId = 106;
    config.descriptors.back().wordOrder = LASER_PARAMETER_LOW_FIRST_SDK;
    config.bindings.push_back(Binding(106, 10));
    std::vector<LaserParameterValueSdk> values;
    values.push_back(Value(101, -1.2));
    values.push_back(Value(102, 305419896.0));
    values.push_back(Value(103, 305419896.0));
    values.push_back(Value(104, -2));
    values.push_back(Value(105, -2.5));
    values.push_back(Value(106, -2.5));
    const lp::WritePlan plan = Compile(config, values);
    CHECK(plan.operations.size() == 6);
    CHECK(plan.operations[0].address == 0);
    CHECK(plan.operations[0].words[0] == 65519);
    CHECK(plan.operations[0].signedSingleWord);
    CHECK(plan.operations[1].words[0] == 0x1234 && plan.operations[1].words[1] == 0x5678);
    CHECK(plan.operations[2].words[0] == 0x5678 && plan.operations[2].words[1] == 0x1234);
    CHECK(plan.operations[3].words[0] == 0xFFFF && plan.operations[3].words[1] == 0xFFFE);
    CHECK(plan.operations[4].words[0] == 0xC020 && plan.operations[4].words[1] == 0);
    CHECK(plan.operations[5].words[0] == 0 && plan.operations[5].words[1] == 0xC020);
    std::string script;
    CHECK(lp::EmitMappedLua(plan, script) == SUCCESS_LASER_SDK);
    CheckPlanScript(plan, script);

    config = OneParameter(LASER_PARAMETER_I16_SDK);
    config.descriptors[0].rounding = LASER_PARAMETER_NEAREST_SDK;
    CHECK(Compile(config, {Value(1001, -1.5)}).operations[0].words[0] == 65534);
    config.descriptors[0].rounding = LASER_PARAMETER_TRUNCATE_SDK;
    CHECK(Compile(config, {Value(1001, -1.5)}).operations[0].words[0] == 65535);
    config.descriptors[0].rounding = LASER_PARAMETER_EXACT_SDK;
    lp::WritePlan unchanged = plan;
    CHECK(lp::CompileWritePlan(config, {Value(1001, 1.1)}, unchanged) != SUCCESS_LASER_SDK);
    CHECK(unchanged.operations.size() == plan.operations.size());
    CHECK(unchanged.operations[0].words == plan.operations[0].words);
}

/** @brief 无效配置或写入失败时保留旧计划，覆盖能力、绑定、范围及溢出。 */
void TestValidationAndTransactionalFailure()
{
    lp::Configuration valid = OneParameter();
    CHECK(lp::ValidateConfiguration(valid) == SUCCESS_LASER_SDK);
    const lp::WritePlan baseline = Compile(valid, {Value(1001, 12)});
    lp::WritePlan output = baseline;
    LaserParameterDiagnosticSdk diagnostic = {};
    diagnostic.structSize = sizeof(diagnostic);
    const double failures[] = {-1, 65536, std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity()};
    for (unsigned index = 0; index < sizeof(failures) / sizeof(failures[0]); ++index)
    {
        CHECK(lp::CompileWritePlan(valid, {Value(1001, failures[index])}, output, &diagnostic)
              == PARAMETER_OUT_OF_RANGE_LASER_SDK);
        CHECK(diagnostic.parameterId == 1001);
        CHECK(output.operations[0].words[0] == 12);
    }
    lp::Configuration range = valid;
    range.descriptors[0].minimum = 10;
    range.descriptors[0].maximum = 20;
    CHECK(lp::CompileWritePlan(range, {Value(1001, 9)}, output) != SUCCESS_LASER_SDK);
    CHECK(lp::CompileWritePlan(range, {Value(1001, 21)}, output) != SUCCESS_LASER_SDK);
    CHECK(Compile(range, {Value(1001, 10)}).operations[0].words[0] == 10);
    CHECK(Compile(range, {Value(1001, 20)}).operations[0].words[0] == 20);
    CHECK(lp::CompileWritePlan(valid, {Value(9999, 1)}, output) == PARAM_TYPE_ERROR_LASER_SDK);
    CHECK(lp::CompileWritePlan(valid, {Value(1001, 1), Value(1001, 2)}, output) != SUCCESS_LASER_SDK);
    CHECK(output.operations[0].words[0] == 12);

    lp::Configuration bad = valid;
    bad.descriptors.push_back(bad.descriptors[0]);
    CHECK(lp::ValidateConfiguration(bad) != SUCCESS_LASER_SDK);
    bad = valid;
    bad.bindings.push_back(bad.bindings[0]);
    CHECK(lp::ValidateConfiguration(bad) != SUCCESS_LASER_SDK);
    bad = valid;
    bad.bindings[0].parameterId = 9999;
    CHECK(lp::ValidateConfiguration(bad) != SUCCESS_LASER_SDK);
    bad = valid;
    bad.descriptors.push_back(Descriptor(1002, LASER_PARAMETER_U32_SDK, 0, 1e12));
    bad.bindings.push_back(Binding(1002, 0));
    CHECK(lp::ValidateConfiguration(bad) != SUCCESS_LASER_SDK);
    bad.bindings[1].writeAddress = 65535;
    CHECK(lp::ValidateConfiguration(bad) != SUCCESS_LASER_SDK);
    bad = valid;
    bad.descriptors[0].dataType = static_cast<LaserParameterDataTypeSdk>(99);
    CHECK(lp::ValidateConfiguration(bad) != SUCCESS_LASER_SDK);
    bad = valid;
    bad.descriptors[0].scale = std::numeric_limits<double>::quiet_NaN();
    CHECK(lp::ValidateConfiguration(bad) != SUCCESS_LASER_SDK);
    bad = valid;
    bad.descriptors[0].structSize = 0;
    CHECK(lp::ValidateConfiguration(bad) != SUCCESS_LASER_SDK);

    // 未绑定参数可保留在能力列表中，但本次请求写入必须明确失败。
    bad = valid;
    bad.bindings.clear();
    CHECK(lp::ValidateConfiguration(bad) == SUCCESS_LASER_SDK);
    CHECK(lp::CompileWritePlan(bad, {Value(1001, 1)}, output) != SUCCESS_LASER_SDK);
    bad = valid;
    bad.bindings[0].writeAddress = LASER_PARAMETER_INVALID_ADDRESS_SDK;
    CHECK(lp::CompileWritePlan(bad, {Value(1001, 1)}, output) != SUCCESS_LASER_SDK);
    bad.descriptors[0].flags = LASER_PARAMETER_READABLE_SDK;
    bad.bindings[0].readAddress = 0;
    CHECK(lp::ValidateConfiguration(bad) == SUCCESS_LASER_SDK);
    CHECK(lp::CompileWritePlan(bad, {Value(1001, 1)}, output) == PARAM_WRITE_ERROR_LASER_SDK);
    CHECK(output.operations[0].words[0] == 12);
    diagnostic.structSize = 0;
    diagnostic.parameterId = 54321;
    CHECK(lp::CompileWritePlan(valid, {Value(1001, 1)}, output, &diagnostic) != SUCCESS_LASER_SDK);
    CHECK(diagnostic.parameterId == 54321);

    // 三十二位编码独立检查范围，而不是先窄化后截断高位。
    bad = OneParameter(LASER_PARAMETER_U32_SDK);
    CHECK(lp::CompileWritePlan(bad, {Value(1001, 4294967296.0)}, output) != SUCCESS_LASER_SDK);
    bad.descriptors[0].dataType = LASER_PARAMETER_I32_SDK;
    CHECK(lp::CompileWritePlan(bad, {Value(1001, -2147483649.0)}, output) != SUCCESS_LASER_SDK);
    CHECK(lp::CompileWritePlan(bad, {Value(1001, 2147483648.0)}, output) != SUCCESS_LASER_SDK);
    bad.descriptors[0].dataType = LASER_PARAMETER_F32_SDK;
    bad.descriptors[0].minimum = -1e300;
    bad.descriptors[0].maximum = 1e300;
    CHECK(lp::CompileWritePlan(bad, {Value(1001, 1e100)}, output) != SUCCESS_LASER_SDK);
    CHECK(output.operations[0].words[0] == 12);
    std::string preserved("previous script");
    lp::WritePlan malformed = baseline;
    malformed.operations[0].words.clear();
    CHECK(lp::EmitMappedLua(malformed, preserved) != SUCCESS_LASER_SDK);
    CHECK(preserved == "previous script");
}

MachineResource Resource(LaserHeadBrandSdk brand)
{
    MachineResource r;
    std::memset(&r, 0, sizeof(r));
    r.commuType = RS485_2_LASER_SDK;
    r.laserHeadBrand = brand;
    r.robotHandle = -1;
    r.weldingProcessType = WIRE_WELDING_LASERSDK;
    r.laserHeadScanSpeedLoc = 0;
    r.laserHeadScanWidthLoc = 1;
    r.laserHeadPeakPowerLoc = 2;
    r.laserHeadDutyCycleLoc = 3;
    r.laserHeadFrequencyHighLoc = 4;
    r.laserHeadFrequencyLowLoc = 5;
    r.laserHeadScanHeightLoc = 6;
    r.laserHeadRotationAngleLoc = 7;
    r.laserHeadPatternIdLoc = 48;
    r.gasValveSignalLoc = 49;
    r.wireFeedSignalLoc = 50;
    r.laserHeadPeakPowerQuickLoc = 51;
    r.weldingModeLoc = 514;
    r.laserPowerLoc = 560;
    r.laserOnPowerLoc = 561;
    r.laserOnRampTimeLoc = 562;
    r.laserOffPowerLoc = 563;
    r.laserOffRampTimeLoc = 564;
    r.gasONDelayLoc = 565;
    r.gasOffDelayLoc = 566;
    r.wireOutDelayLoc = 567;
    r.laserHeadYScaleLoc = 568;
    r.laserHeadYOffsetLoc = 569;
    r.laserHeadXScaleLoc = 570;
    r.laserHeadXOffsetLoc = 571;
    r.spotWeldTypeLoc = 578;
    r.spotWeldDurationLoc = 579;
    r.spotWeldIntervalLoc = 580;
    r.autoWireFeedSpeedLoc = 256;
    r.manualWireFeedSpeedLoc = 257;
    r.manualWireRetractSpeedLoc = 258;
    r.wireRetractLengthLoc = 260;
    r.wireFeedLengthLoc = 261;
    r.wireFeedBackDelayLoc = 262;
    r.manualWireFeedSwitchLoc = 320;
    r.manualWireRetractSwitchLoc = 321;
    r.breakWireFeedingModeLoc = 269;
    r.delayWireFeedingLoc = 277;
    r.wireFeederStatusLoc = 277;
    r.laserHeadParams.laserHeadPeakPower = 3000;
    r.laserPowerParams.laserPower = 2000;
    r.wireFeederParams.wireFeederManualFeedSpeed = 40;
    r.wireFeederParams.wireFeederManualBackSpeed = 30;
    return r;
}

CraftInfoLaserSdk Craft()
{
    CraftInfoLaserSdk c = {};
    c.craftweldMode = CONTINUOUS_LASER_SDK;
    c.laserHeadPeakPower = 3000;
    c.craftLaserPower = 2000;
    c.craftLeadInPower = 1000;
    c.craftLeadOutPower = 1500;
    c.craftScanWidth = 2;
    c.craftScanHeight = 4;
    c.craftScanSpeed = 200;
    c.graphicsSwitching = CIR_LASERSDK;
    c.craftYAxisZoom = 0.1;
    c.craftXAxisZoom = 0.1;
    c.craftYAxisOffset = -1.25;
    c.craftXAxisOffset = 0.75;
    c.craftWeldingSpeed = 10;
    c.craftLeadOutRampDistance = 10;
    c.craftLeadInRampTime = 200;
    c.craftWeldingWireFeed = 60;
    c.craftLeadInWireFeed = 30;
    c.craftLeadOutWireFeed = 20;
    c.craftRetractionLength = 10;
    c.craftEndLoopbackLength = 5;
    c.craftPreGasTime = 100;
    c.craftPostGasTime = 200;
    c.craftStepEquivalent = 100;
    c.laserHeadDutyCycle = 100;
    c.laserHeadLowPulseFrequency = 2100;
    return c;
}

LaserParameterBindingSdk& FindBinding(lp::Configuration& configuration, unsigned id)
{
    for (std::size_t index = 0; index < configuration.bindings.size(); ++index)
        if (configuration.bindings[index].parameterId == id) return configuration.bindings[index];
    throw std::runtime_error("parameter binding missing");
}

LaserParameterDescriptorSdk& FindDescriptor(lp::Configuration& configuration, unsigned id)
{
    for (std::size_t index = 0; index < configuration.descriptors.size(); ++index)
        if (configuration.descriptors[index].parameterId == id) return configuration.descriptors[index];
    throw std::runtime_error("parameter descriptor missing");
}

std::string OldCraftScript(LaserMachineHandle handle, const CraftInfoLaserSdk& craft)
{
    std::vector<char> output(1024 * 1024, '\0');
    int size = static_cast<int>(output.size());
    LaserSdkResult result = cl_craft_inst_devices_params_script_get(handle, craft, output.data(), &size);
    if (result != SUCCESS_LASER_SDK)
        throw std::runtime_error("old craft API result=" + std::to_string(static_cast<int>(result)));
    return std::string(output.data());
}

void SetConfiguration(LaserMachineHandle handle, const lp::Configuration& configuration)
{
    CHECK(cl_parameters_configuration_set(handle, configuration.descriptors.data(),
          static_cast<unsigned>(configuration.descriptors.size()), configuration.bindings.data(),
          static_cast<unsigned>(configuration.bindings.size()), nullptr) == SUCCESS_LASER_SDK);
}

std::vector<LaserParameterValueSdk> SupportedValues(const lp::Configuration& config,
                                                   const std::vector<LaserParameterValueSdk>& values)
{
    std::vector<LaserParameterValueSdk> selected;
    for (std::size_t i = 0; i < values.size(); ++i)
        for (std::size_t j = 0; j < config.descriptors.size(); ++j)
            if (config.descriptors[j].parameterId == values[i].parameterId
                    && (config.descriptors[j].flags & LASER_PARAMETER_WRITABLE_SDK))
                selected.push_back(values[i]);
    return selected;
}

/** @brief 用独立寄存器预期表验证默认工艺，并确认原接口使用重新配置的地址。 */
void TestBrandProfilesAndOldEntryPoint()
{
    const LaserHeadBrandSdk brands[] = {BCXLASERPOWER_SDK, CQ_SUP26AD_LASERPOWER_SDK, CQLASERPOWER_SDK};
    for (unsigned index = 0; index < sizeof(brands) / sizeof(brands[0]); ++index)
    {
        MachineResource resource = Resource(brands[index]);
        const CraftInfoLaserSdk craft = Craft();
        lp::Configuration config;
        CHECK(lp::BuildDefaultConfiguration(resource, config) == SUCCESS_LASER_SDK);
        CHECK(lp::ValidateConfiguration(config) == SUCCESS_LASER_SDK);
        std::vector<LaserParameterValueSdk> values;
        CHECK(lp::BuildCraftValues(resource, craft, values) == SUCCESS_LASER_SDK);
        CHECK(!values.empty());
        values = SupportedValues(config, values);
        lp::WritePlan plan = Compile(config, values);
        std::string defaultScript;
        CHECK(lp::BuildDeviceCraftScript(resource, craft, nullptr, defaultScript) == SUCCESS_LASER_SDK);
        LaserMachineHandle handle = -1;
        CHECK(cl_machine_resource_init(&resource, &handle) == SUCCESS_LASER_SDK);
        const std::string oldEntryScript = OldCraftScript(handle, craft);
        CHECK(oldEntryScript.size() >= defaultScript.size());
        CHECK(oldEntryScript.compare(oldEntryScript.size() - defaultScript.size(),
                                     defaultScript.size(), defaultScript) == 0);
        CHECK(oldEntryScript.find("set_configurable_digital_out") == std::string::npos);
        if (brands[index] != CQLASERPOWER_SDK)
        {
            CheckPlanScript(plan, defaultScript);
            // 固定工艺的独立预期表覆盖板卡地址、单位、瓦/百分比与缩放差异。
            const int expectedBcx[] = {3000, 75, -125, 2000, 2100, 100, 1000, 1500,
                                      200, 1000, 100, 200, 100, 60, 0, 60, 10, 5,
                                      0, 200, 0, 200, 10, 10};
            const int expectedSup[] = {10, -125, 10, 75, 0, 100, 200, 0, 0, 1,
                                      0, 32, 20, 40, 0, 2000, 100, 0, 2100,
                                      60, 1, 10, 5, 50, 200, 75, 1000};
            const int* expected = brands[index] == BCXLASERPOWER_SDK ? expectedBcx : expectedSup;
            const unsigned count = brands[index] == BCXLASERPOWER_SDK ? 24 : 27;
            const std::map<unsigned, int> writes = ParseWrites(defaultScript);
            CHECK(writes.size() == count);
            for (unsigned slot = 0; slot < count; ++slot)
            {
                if (writes.at(60 + slot) != expected[slot])
                {
                    std::ostringstream mismatch;
                    mismatch << "brand=" << brands[index] << " address=" << (60 + slot)
                             << " expected=" << expected[slot] << " actual=" << writes.at(60 + slot);
                    throw std::runtime_error(mismatch.str());
                }
                CHECK(writes.at(60 + slot) == expected[slot]);
            }
        }
        else
        {
            CHECK(defaultScript.find("write_value_port") != std::string::npos
                  || defaultScript.find("write_multi_value_port") != std::string::npos);
            CHECK(defaultScript.find("set_configurable_digital_out") == std::string::npos);
            CHECK(FindBinding(config, LASER_PARAM_SCAN_SPEED_SDK).writeAddress == 0);
        }

        // 同一工艺换本地地址后，数值及位模式必须完全保持，旧入口同样使用新绑定。
        for (std::size_t binding = 0; binding < config.bindings.size(); ++binding)
        {
            config.bindings[binding].writeAddress = 300 + static_cast<unsigned>(binding * 3);
            if (config.bindings[binding].readAddress != LASER_PARAMETER_INVALID_ADDRESS_SDK)
                config.bindings[binding].readAddress = 600 + static_cast<unsigned>(binding * 3);
        }
        SetConfiguration(handle, config);
        const lp::WritePlan relocated = Compile(config, values);
        CHECK(relocated.operations.size() == plan.operations.size());
        for (std::size_t operation = 0; operation < plan.operations.size(); ++operation)
        {
            CHECK(relocated.operations[operation].words == plan.operations[operation].words);
            CHECK(relocated.operations[operation].address != plan.operations[operation].address);
        }
        const std::string mapped = OldCraftScript(handle, craft);
        CheckPlanScript(relocated, mapped);
        CHECK(cl_parameters_configuration_reset(handle) == SUCCESS_LASER_SDK);
        CHECK(OldCraftScript(handle, craft) == oldEntryScript);
        CHECK(cl_machine_resource_release(handle) == SUCCESS_LASER_SDK);
    }

    // SUP26AD 回抽长度由自己的板卡范围约束，合法的 100 不应被通用 20 限幅。
    MachineResource sup = Resource(CQ_SUP26AD_LASERPOWER_SDK);
    CraftInfoLaserSdk craft = Craft();
    craft.craftRetractionLength = 100;
    std::string script;
    CHECK(lp::BuildDeviceCraftScript(sup, craft, nullptr, script) == SUCCESS_LASER_SDK);
    CHECK(ParseWrites(script).at(81) == 100);
    CHECK(ParseWrites(script).at(61) == -125);
    CHECK(ParseWrites(script).at(84) == 200);
    CHECK(ParseWrites(script).at(86) == 1000);
}

/** @brief 验证公开接口的容量查询、深复制、句柄隔离和失败不改资源。 */
void TestPublicApiBuffersIsolationAndPurity()
{
    MachineResource resource = Resource(CQ_SUP26AD_LASERPOWER_SDK);
    LaserMachineHandle first = -1, second = -1;
    CHECK(cl_machine_resource_init(&resource, &first) == SUCCESS_LASER_SDK);
    CHECK(cl_machine_resource_init(&resource, &second) == SUCCESS_LASER_SDK);
    CHECK(first != second);
    lp::Configuration config = OneParameter();
    config.descriptors[0].minimum = 0;
    config.descriptors[0].maximum = 100;
    SetConfiguration(first, config);
    config.bindings[0].writeAddress = 200;
    SetConfiguration(second, config);
    config.bindings[0].writeAddress = 999;

    unsigned descriptorCount = 0, bindingCount = 0;
    CHECK(cl_parameters_configuration_get(first, nullptr, &descriptorCount, nullptr, &bindingCount)
          == SUCCESS_LASER_SDK);
    CHECK(descriptorCount == 1 && bindingCount == 1);
    LaserParameterDescriptorSdk descriptor = {};
    LaserParameterBindingSdk binding = {};
    CHECK(cl_parameters_configuration_get(first, &descriptor, &descriptorCount, &binding, &bindingCount)
          == SUCCESS_LASER_SDK);
    CHECK(binding.writeAddress == 0);
    binding.writeAddress = 700;
    descriptor.maximum = 1;
    descriptorCount = 0;
    bindingCount = 1;
    CHECK(cl_parameters_configuration_get(first, &descriptor, &descriptorCount, &binding, &bindingCount)
          == BUFFER_TOO_SMALL_LASER_SDK);
    CHECK(descriptorCount == 1 && bindingCount == 1);
    CHECK(descriptor.maximum == 1 && binding.writeAddress == 700);

    LaserParameterValueSdk value = Value(1001, 42);
    int size = 0;
    CHECK(cl_parameters_script_get(first, &value, 1, nullptr, &size, nullptr) == SUCCESS_LASER_SDK);
    CHECK(size > 1);
    const int required = size;
    std::vector<char> output(static_cast<std::size_t>(required), '#');
    size = required - 1;
    CHECK(cl_parameters_script_get(first, &value, 1, output.data(), &size, nullptr)
          == BUFFER_TOO_SMALL_LASER_SDK);
    CHECK(size == required);
    CHECK(output[0] == '#' && output.back() == '#');
    size = required;
    CHECK(cl_parameters_script_get(first, &value, 1, output.data(), &size, nullptr) == SUCCESS_LASER_SDK);
    CHECK(size == required && output.back() == '\0');
    CHECK(ParseWrites(output.data()).at(0) == 42);
    size = required;
    CHECK(cl_parameters_script_get(first, &value, 1, output.data(), nullptr, nullptr)
          == PARAMETER_ERROR_LASER_SDK);
    std::vector<char> other(1024, '#');
    size = static_cast<int>(other.size());
    CHECK(cl_parameters_script_get(second, &value, 1, other.data(), &size, nullptr) == SUCCESS_LASER_SDK);
    CHECK(ParseWrites(other.data()).at(200) == 42);

    // 校验失败不更换已安装配置，不覆盖输出，也不更新机器资源。
    MachineResource before;
    CHECK(cl_machine_resource_get(first, &before) == SUCCESS_LASER_SDK);
    lp::Configuration invalid = config;
    invalid.descriptors.push_back(invalid.descriptors[0]);
    CHECK(cl_parameters_configuration_set(first, invalid.descriptors.data(),
          static_cast<unsigned>(invalid.descriptors.size()), invalid.bindings.data(),
          static_cast<unsigned>(invalid.bindings.size()), nullptr) != SUCCESS_LASER_SDK);
    value.value = 101;
    output.assign(static_cast<std::size_t>(required), '#');
    size = required;
    CHECK(cl_parameters_script_get(first, &value, 1, output.data(), &size, nullptr)
          == PARAMETER_OUT_OF_RANGE_LASER_SDK);
    CHECK(output[0] == '#');
    MachineResource after;
    CHECK(cl_machine_resource_get(first, &after) == SUCCESS_LASER_SDK);
    CHECK(std::memcmp(&before, &after, sizeof(before)) == 0);
    value.value = 42;
    size = required;
    CHECK(cl_parameters_script_get(first, &value, 1, output.data(), &size, nullptr) == SUCCESS_LASER_SDK);
    CHECK(ParseWrites(output.data()).at(0) == 42);

    // 品牌切换清理原参数上下文，不让上一板卡绑定沿用到下一板卡。
    resource.laserHeadBrand = BCXLASERPOWER_SDK;
    CHECK(cl_machine_resource_update(first, &resource) == SUCCESS_LASER_SDK);
    descriptorCount = bindingCount = 0;
    CHECK(cl_parameters_configuration_get(first, nullptr, &descriptorCount, nullptr, &bindingCount)
          == SUCCESS_LASER_SDK);
    CHECK(descriptorCount > 1 && bindingCount > 1);
    CHECK(cl_machine_resource_release(first) == SUCCESS_LASER_SDK);
    CHECK(cl_parameters_configuration_reset(first) == HANDLE_ERROR_LASER_SDK);
    CHECK(cl_machine_resource_release(second) == SUCCESS_LASER_SDK);
}

/** @brief 可选只读能力可跳过，必要工艺能力与自定义范围必须严格执行。 */
void TestOptionalCapabilitiesAndCustomRanges()
{
    MachineResource resource = Resource(BCXLASERPOWER_SDK);
    const CraftInfoLaserSdk craft = Craft();
    lp::Configuration config;
    CHECK(lp::BuildDefaultConfiguration(resource, config) == SUCCESS_LASER_SDK);
    FindDescriptor(config, LASER_PARAM_RATED_POWER_SDK).flags = LASER_PARAMETER_READABLE_SDK;
    FindBinding(config, LASER_PARAM_RATED_POWER_SDK).writeAddress = LASER_PARAMETER_INVALID_ADDRESS_SDK;
    std::string script;
    CHECK(lp::BuildDeviceCraftScript(resource, craft, &config, script) == SUCCESS_LASER_SDK);
    CHECK(ParseWrites(script).count(60) == 0);
    CHECK(ParseWrites(script).at(63) == 2000);
    lp::WritePlan plan;
    CHECK(lp::CompileWritePlan(config, {Value(LASER_PARAM_RATED_POWER_SDK, 3000)}, plan)
          == PARAM_WRITE_ERROR_LASER_SDK);
    FindDescriptor(config, LASER_PARAM_WELD_POWER_SDK).flags = LASER_PARAMETER_READABLE_SDK;
    FindBinding(config, LASER_PARAM_WELD_POWER_SDK).writeAddress = LASER_PARAMETER_INVALID_ADDRESS_SDK;
    CHECK(lp::BuildDeviceCraftScript(resource, craft, &config, script) != SUCCESS_LASER_SDK);

    // 配置明确扩大板卡能力后，应使用配置范围，不再套用内置型号的宽度上限。
    resource = Resource(CQ_SUP26AD_LASERPOWER_SDK);
    CHECK(lp::BuildDefaultConfiguration(resource, config) == SUCCESS_LASER_SDK);
    CraftInfoLaserSdk widerCraft = craft;
    widerCraft.craftScanWidth = 6;
    CHECK(lp::BuildDeviceCraftScript(resource, widerCraft, nullptr, script) != SUCCESS_LASER_SDK);
    FindDescriptor(config, LASER_PARAM_SCAN_WIDTH_SDK).maximum = 10;
    CHECK(lp::BuildDeviceCraftScript(resource, widerCraft, &config, script) == SUCCESS_LASER_SDK);
    CHECK(ParseWrites(script).at(72) == 60);

    // 三十二位完整频率按高低字先校验再组合，禁止负数或窄化截断。
    resource = Resource(CQLASERPOWER_SDK);
    CHECK(lp::BuildDefaultConfiguration(resource, config) == SUCCESS_LASER_SDK);
    config.descriptors.push_back(Descriptor(LASER_PARAM_PWM_FREQUENCY_SDK,
                                LASER_PARAMETER_U32_SDK, 0, 4294967295.0));
    config.bindings.push_back(Binding(LASER_PARAM_PWM_FREQUENCY_SDK, 500));
    CraftInfoLaserSdk pwmCraft = craft;
    pwmCraft.laserHeadHighPulseFrequency = 0x1234;
    pwmCraft.laserHeadLowPulseFrequency = 0x5678;
    CHECK(lp::BuildDeviceCraftScript(resource, pwmCraft, &config, script) == SUCCESS_LASER_SDK);
    CHECK(ParseWrites(script).at(500) == 0x1234);
    CHECK(ParseWrites(script).at(501) == 0x5678);
    script = "unchanged";
    pwmCraft.laserHeadHighPulseFrequency = 70000;
    CHECK(lp::BuildDeviceCraftScript(resource, pwmCraft, &config, script) != SUCCESS_LASER_SDK);
    CHECK(script == "unchanged");
    pwmCraft.laserHeadHighPulseFrequency = 0;
    pwmCraft.laserHeadLowPulseFrequency = -1;
    CHECK(lp::BuildDeviceCraftScript(resource, pwmCraft, &config, script) != SUCCESS_LASER_SDK);
    CHECK(script == "unchanged");
}

/** @brief 保留板卡既有舍入方式及旧客户端未使用地址、自熔焊断丝机行为。 */
void TestDefaultRoundingCompatibility()
{
    MachineResource resource = Resource(BCXLASERPOWER_SDK);
    CraftInfoLaserSdk craft = Craft();
    craft.craftScanWidth = 1.239;
    craft.craftYAxisZoom = 1.239;
    craft.craftYAxisOffset = -1.235;
    craft.craftLeadOutRampDistance = 1;
    craft.craftWeldingSpeed = 3;
    std::string script;
    CHECK(lp::BuildDeviceCraftScript(resource, craft, nullptr, script) == SUCCESS_LASER_SDK);
    const std::map<unsigned, int> bcx = ParseWrites(script);
    CHECK(bcx.at(79) == 123);
    CHECK(bcx.at(83) == 123);
    CHECK(bcx.at(62) == -124);
    CHECK(bcx.at(69) == 333);
    resource = Resource(CQ_SUP26AD_LASERPOWER_SDK);
    craft = Craft();
    craft.craftLaserPower = 300;
    craft.craftLeadInPower = 101;
    craft.craftLeadOutPower = 100;
    craft.craftScanWidth = 1.26;
    craft.craftRetractionLength = 100;
    CHECK(lp::BuildDeviceCraftScript(resource, craft, nullptr, script) == SUCCESS_LASER_SDK);
    const std::map<unsigned, int> sup = ParseWrites(script);
    CHECK(sup.at(83) == 33);
    CHECK(sup.at(72) == 13);
    CHECK(sup.at(81) == 100);

    // 旧接口未使用的设备页/手动地址不应阻塞仅工艺下发的老客户端。
    resource = Resource(CQLASERPOWER_SDK);
    resource.manualWireFeedSpeedLoc = 0;
    resource.manualWireRetractSpeedLoc = 0;
    resource.manualWireFeedSwitchLoc = 0;
    resource.manualWireRetractSwitchLoc = 0;
    resource.gasValveSignalLoc = 0;
    resource.wireFeedSignalLoc = 0;
    resource.laserHeadPeakPowerQuickLoc = 0;
    craft = Craft();
    CHECK(lp::BuildDeviceCraftScript(resource, craft, nullptr, script) == SUCCESS_LASER_SDK);
    resource.weldingProcessType = SELF_WELDING_LASERSDK;
    resource.autoWireFeedSpeedLoc = 0;
    resource.wireRetractLengthLoc = 0;
    resource.wireFeedLengthLoc = 0;
    resource.wireFeedBackDelayLoc = 0;
    resource.breakWireFeedingModeLoc = 0;
    resource.wireFeederStatusLoc = 0;
    CHECK(lp::BuildDeviceCraftScript(resource, craft, nullptr, script) == SUCCESS_LASER_SDK);
    CHECK(script.find("0x0A06") == std::string::npos);
}

/** @brief 原工艺下发入口成功、失败或重试均不得改变持久机器资源。 */
void TestOldCraftFailureDoesNotMutateResource()
{
    MachineResource resource = Resource(CQ_SUP26AD_LASERPOWER_SDK);
    LaserMachineHandle handle = -1;
    CHECK(cl_machine_resource_init(&resource, &handle) == SUCCESS_LASER_SDK);
    MachineResource before;
    CHECK(cl_machine_resource_get(handle, &before) == SUCCESS_LASER_SDK);
    CraftInfoLaserSdk craft = Craft();
    const std::string baseline = OldCraftScript(handle, craft);
    MachineResource after;
    CHECK(cl_machine_resource_get(handle, &after) == SUCCESS_LASER_SDK);
    CHECK(std::memcmp(&before, &after, sizeof(before)) == 0);
    craft.craftXAxisOffset = std::numeric_limits<double>::quiet_NaN();
    std::vector<char> output(1024 * 1024, '#');
    int size = static_cast<int>(output.size());
    CHECK(cl_craft_inst_devices_params_script_get(handle, craft, output.data(), &size)
          != SUCCESS_LASER_SDK);
    CHECK(output[0] == '#');
    CHECK(cl_machine_resource_get(handle, &after) == SUCCESS_LASER_SDK);
    CHECK(std::memcmp(&before, &after, sizeof(before)) == 0);
    craft = Craft();
    CHECK(OldCraftScript(handle, craft) == baseline);
    CHECK(cl_machine_resource_release(handle) == SUCCESS_LASER_SDK);
}

/** @brief 原完整程序和起收弧入口必须贯通映射参数链，选工艺不能隐式出光。 */
void TestFullInstructionAndControlPaths()
{
    const LaserHeadBrandSdk brands[] = {BCXLASERPOWER_SDK, CQ_SUP26AD_LASERPOWER_SDK, CQLASERPOWER_SDK};
    const std::regex laserOn("set_configurable_digital_out\\(\\s*1\\s*,\\s*true\\s*\\)");
    typedef LaserSdkResult (*ScriptFunction)(LaserMachineHandle, char*, int*);
    const ScriptFunction controls[] = {cl_get_arc_start_script, cl_get_arc_stop_script,
                                      cl_get_arc_start_slow_script, cl_get_arc_stop_slow_script,
                                      cl_craft_device_status_modify};
    for (unsigned board = 0; board < sizeof(brands) / sizeof(brands[0]); ++board)
    {
        MachineResource resource = Resource(brands[board]);
        resource.laserPowerParams.gasStartupAdvanceTime = 5000;
        LaserMachineHandle handle = -1;
        CHECK(cl_machine_resource_init(&resource, &handle) == SUCCESS_LASER_SDK);
        lp::Configuration config;
        CHECK(lp::BuildDefaultConfiguration(resource, config) == SUCCESS_LASER_SDK);
        FindDescriptor(config, LASER_PARAM_PRE_GAS_TIME_SDK).maximum = 10000;
        for (std::size_t i = 0; i < config.bindings.size(); ++i)
        {
            if (config.bindings[i].writeAddress != LASER_PARAMETER_INVALID_ADDRESS_SDK)
                config.bindings[i].writeAddress = 300 + static_cast<unsigned>(i * 3);
            if (config.bindings[i].readAddress != LASER_PARAMETER_INVALID_ADDRESS_SDK)
                config.bindings[i].readAddress = 600 + static_cast<unsigned>(i * 3);
        }
        SetConfiguration(handle, config);
        CraftInfoLaserSdk craft = Craft();
        craft.craftPreGasTime = 5000;
        InstListData list;
        list.listLocToTypeID[0] = std::make_pair(CRAFT_SELECT_INST_LASER_WELD_SDK, 0);
        list.craftsMap[0] = craft;
        InstListTransEnvironmentData environment = {};
        environment.craftID = -1;
        environment.prePointLoc = -1;
        environment.policy.emitDeviceParameters = true;
        environment.policy.emitArcControl = true;
        environment.policy.emitRampDownControl = true;
        environment.policy.useWeldMotionParameters = true;
        std::string body;
        CHECK(GetBasicCraftScript(handle, list, environment, body) == SUCCESS_LASER_SDK);
        CheckMappedScript(body);
        CHECK(ParseWrites(body).at(FindBinding(config, LASER_PARAM_WELD_POWER_SDK).writeAddress) == 2000);
        CHECK(cl_machine_resource_update(handle, &resource) == SUCCESS_LASER_SDK);

        // 只选工艺不包含起弧指令时，完整实焊程序也不能隐式开启 CO1。
        int instructions[] = {CRAFT_SELECT_INST_LASER_WELD_SDK, 0};
        LaserWeldStartData start = {};
        start.instList = instructions;
        start.instNum = 1;
        start.craftInfoList = &craft;
        start.craftNum = 1;
        MachineResource before;
        CHECK(cl_machine_resource_get(handle, &before) == SUCCESS_LASER_SDK);
        std::vector<char> output(1024 * 1024, '#');
        int size = static_cast<int>(output.size());
        CHECK(cl_craft_inst_list_script_get(handle, start, true, output.data(), &size) == SUCCESS_LASER_SDK);
        const std::string script(output.data());
        CheckMappedTransport(script);
        CHECK(!std::regex_search(script, laserOn));
        CHECK(script.find(body) != std::string::npos);
        MachineResource after;
        CHECK(cl_machine_resource_get(handle, &after) == SUCCESS_LASER_SDK);
        CHECK(std::memcmp(&before, &after, sizeof(before)) == 0);
        // 同一入口长度查询、容量失败也必须恢复临时编译资源。
        size = 0;
        CHECK(cl_craft_inst_list_script_get(handle, start, true, nullptr, &size) == SUCCESS_LASER_SDK);
        CHECK(size == static_cast<int>(script.size()) + 1);
        output.assign(1, '#');
        size = 1;
        CHECK(cl_craft_inst_list_script_get(handle, start, true, output.data(), &size) == BUFFER_TOO_SMALL_LASER_SDK);
        CHECK(output[0] == '#');
        CHECK(cl_machine_resource_get(handle, &after) == SUCCESS_LASER_SDK);
        CHECK(std::memcmp(&before, &after, sizeof(before)) == 0);

        // 动作接口允许数字输出，但不应夹带串口工具定义或未定义自由通讯调用。
        for (unsigned control = 0; control < sizeof(controls) / sizeof(controls[0]); ++control)
        {
            output.assign(1024 * 1024, '#');
            size = static_cast<int>(output.size());
            CHECK(controls[control](handle, output.data(), &size) == SUCCESS_LASER_SDK);
            CheckMappedTransport(output.data());
            const std::regex legacyTimeoutGate("read_integer_register\\(\\s*821\\s*\\)");
            CHECK(!std::regex_search(output.data(), legacyTimeoutGate));
        }
        CHECK(cl_machine_resource_release(handle) == SUCCESS_LASER_SDK);
    }
}

/** @brief 继承旧超强功率、超时清零和不隐式操作 CO 的行为，并检查 Lua 行边界。 */
void TestLegacyFullProgramPowerAndTimeoutReset()
{
    MachineResource resource = Resource(CQLASERPOWER_SDK);
    resource.robotParams.emptyRunTCPSpeed = 100;
    resource.robotParams.emptyRunTCPAcc = 1000;
    resource.robotParams.weldTCPAcc = 1000;
    LaserMachineHandle handle = -1;
    CHECK(cl_machine_resource_init(&resource, &handle) == SUCCESS_LASER_SDK);
    CraftInfoLaserSdk craft = Craft();
    const std::string parameters = OldCraftScript(handle, craft);
    const std::regex clearTimeout("write_integer_register\\(\\s*821\\s*,\\s*0\\s*\\)");
    CHECK(std::regex_search(parameters, clearTimeout));
    // 默认旧超强保留手组 RTU 路径，完整缓降目标应为工艺焊接功率。
    PointDataLaserSdk points[2] = {};
    points[0].pointType = LINE_POINT_WELD_SDK;
    points[1].pointType = LINE_POINT_WELD_SDK;
    points[1].pose[0] = 100;
    int instructions[] = {CRAFT_SELECT_INST_LASER_WELD_SDK, 0,
                          LINE_POINT_INST_LASER_WELD_SDK, 0,
                          ARC_START_INST_LASER_WELD_SDK, 0,
                          LINE_POINT_INST_LASER_WELD_SDK, 1,
                          SLOW_DOWN_LASER_WELD_SDK, 0,
                          ARC_END_INST_LASER_WELD_SDK, 0};
    LaserWeldStartData start = {};
    start.instList = instructions;
    start.instNum = 6;
    start.craftInfoList = &craft;
    start.craftNum = 1;
    start.pointList = points;
    start.pointsNum = 2;
    MachineResource before;
    CHECK(cl_machine_resource_get(handle, &before) == SUCCESS_LASER_SDK);
    std::vector<char> output(1024 * 1024, '#');
    int size = static_cast<int>(output.size());
    CHECK(cl_craft_inst_list_script_get(handle, start, true, output.data(), &size) == SUCCESS_LASER_SDK);
    const std::string script(output.data());
    const std::regex correctPower("laserPower\\s*=\\s*2000([^0-9]|$)");
    const std::regex wrongPower("laserPower\\s*=\\s*3000([^0-9]|$)");
    CHECK(std::regex_search(script, correctPower));
    CHECK(!std::regex_search(script, wrongPower));
    CHECK(script.find("--SLOW_DOWN--") != std::string::npos);
    MachineResource after;
    CHECK(cl_machine_resource_get(handle, &after) == SUCCESS_LASER_SDK);
    CHECK(std::memcmp(&before, &after, sizeof(before)) == 0);

    // 已起弧后切换工艺也只下发参数，继承原仓库取消隐式 CO 操作的修复。
    InstListData changingCraft;
    changingCraft.listLocToTypeID[0] = std::make_pair(CRAFT_SELECT_INST_LASER_WELD_SDK, 0);
    // 解释器要求列表结束时焊接状态闭合；关闭控制输出策略，仅用收弧指令闭合状态。
    changingCraft.listLocToTypeID[1] = std::make_pair(ARC_END_INST_LASER_WELD_SDK, 0);
    changingCraft.craftsMap[0] = craft;
    InstListTransEnvironmentData activeEnvironment = {};
    activeEnvironment.craftID = 0;
    activeEnvironment.isWelding = true;
    activeEnvironment.policy.emitDeviceParameters = true;
    std::string changeBody;
    CHECK(GetBasicCraftScript(handle, changingCraft, activeEnvironment, changeBody) == SUCCESS_LASER_SDK);
    CHECK(changeBody.find("set_configurable_digital_out") == std::string::npos);
    CHECK(cl_machine_resource_update(handle, &resource) == SUCCESS_LASER_SDK);

    // 旧超强不通过 CO2 送丝，空拼接片段不能把 CO1 指令接到上一行 Lua 注释内。
    // 公有旧起弧接口基线误将气/丝地址列入激光头参数，此处直接验证同一个 Lua renderer。
    CommuRS485* const rawRenderer = CommuRS485::GetInstance();
    for (unsigned action = 0; action < 2; ++action)
    {
        rawRenderer->resetPendingScriptFragments();
        const SwitchTypeLaserSdk arcSwitch = action == 0 ? SWITCH_ON_LASER_SDK : SWITCH_OFF_LASER_SDK;
        CHECK(rawRenderer->craft_script_arc_function_switch_script_get(handle, arcSwitch) == SUCCESS_LASER_SDK);
        std::string rawBody;
        CHECK(rawRenderer->craft_script_arc_program_run_script_get(handle, arcSwitch, rawBody) == SUCCESS_LASER_SDK);
        const std::string rawScript = rawRenderer->loadLuaFunctionScriptOut(resource.laserHeadBrand) + rawBody;
        CHECK(rawBody.find(action == 0 ? "arc_start()" : "arc_stop()") != std::string::npos);
        std::istringstream lines(rawScript);
        std::string line;
        unsigned activeCo1Statements = 0;
        const std::regex co1(action == 0
                ? "set_configurable_digital_out\\(\\s*1\\s*,\\s*true\\s*\\)"
                : "set_configurable_digital_out\\(\\s*1\\s*,\\s*false\\s*\\)");
        while (std::getline(lines, line))
        {
            // 单行注释后的文本不会被 Lua 执行，只统计注释前的有效语句。
            const std::string active = line.substr(0, line.find("--"));
            if (std::regex_search(active, co1)) ++activeCo1Statements;
        }
        CHECK(activeCo1Statements > 0);
        CHECK(rawScript.find("set_configurable_digital_out(2,") == std::string::npos);
    }
    CHECK(cl_machine_resource_release(handle) == SUCCESS_LASER_SDK);

    // 欧斯睿普保留原 SDK 已有的旧超强兼容别名，不增加独立协议承诺。
    std::string supScript, aliasScript;
    CHECK(lp::BuildDeviceCraftScript(resource, craft, nullptr, supScript) == SUCCESS_LASER_SDK);
    resource.laserHeadBrand = OSRPLASERPOWER_SDK;
    CHECK(lp::BuildDeviceCraftScript(resource, craft, nullptr, aliasScript) == SUCCESS_LASER_SDK);
    CHECK(supScript == aliasScript);
}
} // namespace

int main()
{
    try
    {
        TestIntegerAndFloatEncoding();
        TestValidationAndTransactionalFailure();
        TestBrandProfilesAndOldEntryPoint();
        TestPublicApiBuffersIsolationAndPurity();
        TestOptionalCapabilitiesAndCustomRanges();
        TestDefaultRoundingCompatibility();
        TestOldCraftFailureDoesNotMutateResource();
        TestFullInstructionAndControlPaths();
        TestLegacyFullProgramPowerAndTimeoutReset();
        CHECK(parameter_test_robot_calls == 0);
        std::cout << "PASS parameter pipeline: " << checks
                  << " checks, 9 scenario groups, zero robot SDK calls" << std::endl;
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL parameter pipeline: " << error.what() << std::endl;
        return 1;
    }
}
