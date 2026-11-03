#include "cl_api.h"
#include "common/parametercompiler.h"
#include "common/rampcontrol.h"
#include "brand/deviceparameters.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

extern int parameter_test_robot_calls;
namespace lp = laser_parameters;

namespace {
unsigned checks = 0;
void Check(bool condition, const char *expression, int line) {
    ++checks;
    if (!condition) {
        std::ostringstream message;
        message << "line " << line << ": " << expression;
        throw std::runtime_error(message.str());
    }
}
#define CHECK(expression) Check((expression), #expression, __LINE__)

/** @brief 构造纯编译资源，所有实际机器人 API 均由失败桩阻止。 */
MachineResource Resource(LaserHeadBrandSdk brand, bool wire) {
    MachineResource r;
    std::memset(&r, 0, sizeof(r));
    r.commuType = RS485_2_LASER_SDK;
    r.laserHeadBrand = brand;
    r.robotHandle = -1;
    r.weldingProcessType = wire ? WIRE_WELDING_LASERSDK : SELF_WELDING_LASERSDK;
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
    r.breakWireFeedingModeLoc = 269;
    r.wireFeederStatusLoc = 277;
    r.laserHeadParams.laserHeadPeakPower = 2000;
    r.laserPowerParams.laserPower = 3000;
    r.laserPowerParams.laserPowerOpen = 1000;
    r.laserPowerParams.laserPowerClose = 500;
    r.laserPowerParams.laserRampUpTime = 200;
    r.laserPowerParams.laserRampDownTime = 500;
    r.laserPowerParams.gasStartupAdvanceTime = 100;
    r.laserPowerParams.gasShutoffDelayTime = 200;
    r.wireFeederParams.wireFeederStartMotorSpeed = 30;
    r.wireFeederParams.wireFeederMotorSpeed = 60;
    r.wireFeederParams.wireFeederStopMotorSpeed = 20;
    r.robotParams.emptyRunTCPSpeed = 100;
    r.robotParams.emptyRunTCPAcc = 1000;
    r.robotParams.weldTCPAcc = 1000;
    if (!wire) {
        // 自熔焊必须允许送丝设备未配置；这些哨兵地址不得被解析或下发。
        r.autoWireFeedSpeedLoc = std::numeric_limits<unsigned>::max();
        r.wireRetractLengthLoc = std::numeric_limits<unsigned>::max();
        r.wireFeedLengthLoc = std::numeric_limits<unsigned>::max();
        r.wireFeedBackDelayLoc = std::numeric_limits<unsigned>::max();
        r.wireFeederStatusLoc = std::numeric_limits<unsigned>::max();
    }
    return r;
}

/** @brief 新外控模式输入统一为 mm/s，额定功率不作为动态输出。 */
CraftInfoLaserSdk Craft() {
    CraftInfoLaserSdk c = {};
    c.craftweldMode = CONTINUOUS_LASER_SDK;
    c.laserHeadPeakPower = 3000;
    c.craftLaserPower = 2000;
    c.craftLeadInPower = 1000;
    c.craftLeadOutPower = 500;
    c.craftScanWidth = 2;
    c.craftScanHeight = 4;
    c.craftScanSpeed = 200;
    c.graphicsSwitching = CIR_LASERSDK;
    c.craftYAxisZoom = 1;
    c.craftXAxisZoom = 1;
    c.craftWeldingSpeed = 10;
    c.craftLeadOutRampDistance = 5;
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

LaserRampControlConfigSdk Enabled() {
    LaserRampControlConfigSdk config = {};
    config.structSize = sizeof(config);
    config.version = 1;
    config.enabled = 1;
    config.updatePeriodMs = 100;
    config.settleTimeMs = 50;
    config.timeoutMs = 2000;
    return config;
}

std::string CraftScript(LaserMachineHandle handle, const CraftInfoLaserSdk &craft) {
    std::vector<char> buffer(1024 * 1024, '#');
    int length = static_cast<int>(buffer.size());
    CHECK(cl_craft_inst_devices_params_script_get(handle, craft, buffer.data(), &length) == SUCCESS_LASER_SDK);
    return std::string(buffer.data());
}

/** @brief 使用公共程序入口生成含真实收弧轨迹的实际 Lua。 */
std::string Program(LaserMachineHandle handle, const CraftInfoLaserSdk &craft, bool weld) {
    PointDataLaserSdk points[2] = {};
    points[0].pointType = LINE_POINT_WELD_SDK;
    points[1].pointType = LINE_POINT_WELD_SDK;
    points[1].pose[0] = 10;
    int instructions[] = {CRAFT_SELECT_INST_LASER_WELD_SDK, 0, LINE_POINT_INST_LASER_WELD_SDK, 0, ARC_START_INST_LASER_WELD_SDK, 0,
        LINE_POINT_INST_LASER_WELD_SDK, 1, ARC_END_INST_LASER_WELD_SDK, 0};
    LaserWeldStartData start = {};
    start.instList = instructions;
    start.instNum = 5;
    start.craftInfoList = const_cast<CraftInfoLaserSdk *>(&craft);
    start.craftNum = 1;
    start.pointList = points;
    start.pointsNum = 2;
    std::vector<char> buffer(1024 * 1024, '#');
    int length = static_cast<int>(buffer.size());
    CHECK(cl_craft_inst_list_script_get(handle, start, weld, buffer.data(), &length) == SUCCESS_LASER_SDK);
    int required = 0;
    CHECK(cl_craft_inst_list_script_get(handle, start, weld, nullptr, &required) == SUCCESS_LASER_SDK);
    CHECK(required == static_cast<int>(std::strlen(buffer.data())) + 1);
    return std::string(buffer.data());
}

void Literal(std::ostream &output, const char *key, const std::string &value) {
    CHECK(value.find("]====]") == std::string::npos);
    output << key << "=[====[\n" << value << "\n]====],\n";
}

/** @brief 公有独立动作入口应与完整程序复用同一运行时和阶段。 */
std::string Action(LaserMachineHandle handle, LaserSdkResult (*function)(LaserMachineHandle, char *, int *)) {
    std::vector<char> buffer(1024 * 1024, '#');
    int length = static_cast<int>(buffer.size());
    CHECK(function(handle, buffer.data(), &length) == SUCCESS_LASER_SDK);
    return std::string(buffer.data());
}

/** @brief 新工艺动作接口必须直接消费调用方快照，并保持资源不变。 */
std::string CraftAction(LaserMachineHandle handle, const CraftInfoLaserSdk &craft, SwitchTypeLaserSdk action, bool downOnly) {
    const CraftInfoLaserSdk craftBefore = craft;
    MachineResource before, after;
    CHECK(cl_machine_resource_get(handle, &before) == SUCCESS_LASER_SDK);
    std::vector<char> buffer(1024 * 1024, '#');
    int length = static_cast<int>(buffer.size());
    CHECK(cl_craft_arc_script_get(handle, &craft, action, downOnly, buffer.data(), &length) == SUCCESS_LASER_SDK);
    int required = 0;
    CHECK(cl_craft_arc_script_get(handle, &craft, action, downOnly, nullptr, &required) == SUCCESS_LASER_SDK);
    CHECK(required == static_cast<int>(std::strlen(buffer.data())) + 1);
    CHECK(cl_machine_resource_get(handle, &after) == SUCCESS_LASER_SDK);
    CHECK(std::memcmp(&before, &after, sizeof(before)) == 0);
    CHECK(std::memcmp(&craftBefore, &craft, sizeof(craft)) == 0);
    return std::string(buffer.data());
}

/** @brief 校验配置事务性、默认禁用兼容及句柄隔离。 */
void TestConfiguration() {
    MachineResource resource = Resource(BCXLASERPOWER_SDK, true);
    LaserMachineHandle first = -1, second = -1;
    CHECK(cl_machine_resource_init(&resource, &first) == SUCCESS_LASER_SDK);
    CHECK(cl_machine_resource_init(&resource, &second) == SUCCESS_LASER_SDK);
    LaserRampControlConfigSdk initial = {};
    initial.structSize = sizeof(initial);
    CHECK(cl_ramp_control_config_get(first, &initial) == SUCCESS_LASER_SDK);
    CHECK(initial.version == 1 && initial.enabled == 0);
    CHECK(initial.updatePeriodMs == 100 && initial.settleTimeMs == 50 && initial.timeoutMs == 2000);
    const std::string oldProgram = Program(first, Craft(), true);
    LaserRampControlConfigSdk enabled = Enabled();
    CHECK(cl_ramp_control_config_set(first, &enabled) == SUCCESS_LASER_SDK);
    LaserRampControlConfigSdk observed = {};
    observed.structSize = sizeof(observed);
    CHECK(cl_ramp_control_config_get(second, &observed) == SUCCESS_LASER_SDK);
    CHECK(observed.enabled == 0);
    const unsigned invalidValues[] = {0, 1001};
    for (unsigned i = 0; i < 2; ++i) {
        LaserRampControlConfigSdk invalid = enabled;
        invalid.updatePeriodMs = invalidValues[i];
        CHECK(cl_ramp_control_config_set(first, &invalid) != SUCCESS_LASER_SDK);
    }
    LaserRampControlConfigSdk invalid = enabled;
    invalid.version = 99;
    CHECK(cl_ramp_control_config_set(first, &invalid) != SUCCESS_LASER_SDK);
    invalid = enabled;
    invalid.timeoutMs = 49;
    CHECK(cl_ramp_control_config_set(first, &invalid) != SUCCESS_LASER_SDK);
    observed.structSize = sizeof(observed);
    CHECK(cl_ramp_control_config_get(first, &observed) == SUCCESS_LASER_SDK);
    CHECK(observed.enabled == 1 && observed.updatePeriodMs == 100);
    CHECK(cl_ramp_control_config_reset(first) == SUCCESS_LASER_SDK);
    CHECK(Program(first, Craft(), true) == oldProgram);
    CHECK(cl_machine_resource_release(first) == SUCCESS_LASER_SDK);
    CHECK(cl_ramp_control_config_get(first, &observed) == HANDLE_ERROR_LASER_SDK);
    CHECK(cl_machine_resource_release(second) == SUCCESS_LASER_SDK);
}

/** @brief 导出真正编译生成的运行时和完整程序，交由 Lua 解释器执行。 */
void EmitFixture(std::ostream &output, LaserHeadBrandSdk brand, const char *name, bool wire, bool scaled = false) {
    MachineResource resource = Resource(brand, wire);
    LaserMachineHandle handle = -1;
    CHECK(cl_machine_resource_init(&resource, &handle) == SUCCESS_LASER_SDK);
    LaserRampControlConfigSdk enabled = Enabled();
    CHECK(cl_ramp_control_config_set(handle, &enabled) == SUCCESS_LASER_SDK);
    if (brand == CQLASERPOWER_SDK) {
        CHECK(cl_machine_device_power_managed_set(handle, true) == SUCCESS_LASER_SDK);
    }
    if (scaled) {
        lp::Configuration configuration;
        CHECK(lp::BuildDefaultConfiguration(resource, configuration) == SUCCESS_LASER_SDK);
        for (std::size_t i = 0; i < configuration.descriptors.size(); ++i) {
            if (configuration.descriptors[i].parameterId == LASER_PARAM_WIRE_FEED_SPEED_SDK) {
                configuration.descriptors[i].scale = 2;
            }
        }
        CHECK(cl_parameters_configuration_set(handle, configuration.descriptors.data(), static_cast<unsigned>(configuration.descriptors.size()),
                  configuration.bindings.data(), static_cast<unsigned>(configuration.bindings.size()), nullptr) == SUCCESS_LASER_SDK);
    }
    MachineResource before, after;
    CHECK(cl_machine_resource_get(handle, &before) == SUCCESS_LASER_SDK);
    std::string runtime;
    CHECK(ramp_control::BuildRuntimeScript(handle, resource, runtime) == SUCCESS_LASER_SDK);
    CHECK(!runtime.empty());
    const CraftInfoLaserSdk craft = Craft();
    const std::string parameters = CraftScript(handle, craft);
    if (brand == CQ_SUP26AD_LASERPOWER_SDK) {
        CHECK(parameters.find(wire ? "-- SUP26AD_CRAFT_BANK_V2:WIRE:" : "-- SUP26AD_CRAFT_BANK_V2:SELF:") != std::string::npos);
        CHECK(parameters.find("write_int16_register(69,") == std::string::npos);
        CHECK(parameters.find("write_int16_register(98,") == std::string::npos);
        CHECK(parameters.find("write_int16_register(99,") == std::string::npos);
        CHECK(parameters.find("write_int16_register(100,") == std::string::npos);
        CHECK(parameters.find("write_int16_register(101,") == std::string::npos);
        CHECK(!wire || parameters.find(",95=0,") != std::string::npos);
        if (!wire) {
            CHECK(parameters.find("94=") == std::string::npos);
            CHECK(parameters.find("96=") == std::string::npos);
            CHECK(parameters.find("97=") == std::string::npos);
        }
    }
    const std::string program = Program(handle, craft, true);
    const std::string simulation = Program(handle, craft, false);
    CHECK(cl_machine_resource_get(handle, &after) == SUCCESS_LASER_SDK);
    CHECK(std::memcmp(&before, &after, sizeof(before)) == 0);
    if (wire && brand != BCXLASERPOWER_SDK) {
        CraftInfoLaserSdk over = craft;
        over.craftWeldingWireFeed = 101;
        std::vector<char> buffer(1024 * 1024, '#');
        int length = static_cast<int>(buffer.size());
        CHECK(cl_craft_inst_devices_params_script_get(handle, over, buffer.data(), &length) != SUCCESS_LASER_SDK);
        CHECK(buffer[0] == '#');
    }
    const int wireMultiplier = brand == BCXLASERPOWER_SDK ? 1 : (scaled ? 12 : 6);
    output << "{name='" << name << (wire ? "_wire" : "_self") << (scaled ? "_scaled" : "") << "',wire=" << (wire ? "true" : "false")
           << ",legacy=" << (brand == CQLASERPOWER_SDK ? "true" : "false")
           << ",powerAddress=" << (brand == BCXLASERPOWER_SDK ? 63 : (brand == CQ_SUP26AD_LASERPOWER_SDK ? 90 : 51))
           << ",wireAddress=" << (brand == BCXLASERPOWER_SDK ? 73 : (brand == CQ_SUP26AD_LASERPOWER_SDK ? 94 : 256)) << ",wireMultiplier=" << wireMultiplier
           << ",period=100,settle=50,timeout=2000,\n";
    Literal(output, "runtime", runtime);
    Literal(output, "parameters", parameters);
    Literal(output, "program", program);
    CraftInfoLaserSdk shortCraft = craft;
    shortCraft.craftLeadOutRampDistance = 1;
    Literal(output, "programShort", Program(handle, shortCraft, true));
    shortCraft.craftLeadOutRampDistance = 0;
    Literal(output, "programZero", Program(handle, shortCraft, true));
    Literal(output, "simulation", simulation);
    Literal(output, "publicStart", Action(handle, cl_get_arc_start_script));
    Literal(output, "publicDown", Action(handle, cl_get_arc_stop_slow_script));
    Literal(output, "publicFinish", Action(handle, cl_get_arc_stop_script));
    Literal(output, "protection", Action(handle, cl_craft_end_arc_subprogram_script_get));
    Literal(output, "pureStart", CraftAction(handle, craft, SWITCH_ON_LASER_SDK, false));
    Literal(output, "pureDown", CraftAction(handle, craft, SWITCH_OFF_LASER_SDK, true));
    Literal(output, "pureFinish", CraftAction(handle, craft, SWITCH_OFF_LASER_SDK, false));
    output << "},\n";
    CHECK(cl_machine_resource_release(handle) == SUCCESS_LASER_SDK);
}
}

int main() {
    try {
        TestConfiguration();
        std::ofstream fixtures("generated_sequence_fixtures.lua.md", std::ios::binary);
        CHECK(fixtures.good());
        fixtures << "return {\n";
        const LaserHeadBrandSdk brands[] = {BCXLASERPOWER_SDK, CQ_SUP26AD_LASERPOWER_SDK, CQLASERPOWER_SDK};
        const char *names[] = {"bcx", "sup26", "sup25"};
        for (unsigned index = 0; index < 3; ++index) {
            EmitFixture(fixtures, brands[index], names[index], true);
            EmitFixture(fixtures, brands[index], names[index], false);
        }
        EmitFixture(fixtures, CQ_SUP26AD_LASERPOWER_SDK, "sup26", true, true);
        fixtures << "}\n";
        fixtures.close();
        CHECK(fixtures.good());
        CHECK(parameter_test_robot_calls == 0);
        std::cout << "PASS sequence generation: " << checks << " checks, seven executable fixtures, zero robot SDK calls" << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL sequence generation: " << error.what() << std::endl;
        return 1;
    }
}
