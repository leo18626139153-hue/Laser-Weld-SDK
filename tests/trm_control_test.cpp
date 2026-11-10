#include "cl_api.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
void require(bool passed, const char *check, int line) {
    if (!passed) {
        std::cerr << "FAIL " << line << ": " << check << "\n";
        std::exit(1);
    }
}
#define CHECK(condition) require((condition), #condition, __LINE__)

LaserTrmProfileSdk fixture() {
    LaserTrmProfileSdk profile = {};
    profile.structSize = sizeof(profile);
    profile.version = LASER_TRM_PROFILE_VERSION_SDK;
    profile.transport = TRM_TRANSPORT_MAPPED_RTU_SDK;
    profile.capabilities = TRM_CAP_AUTOMATIC_WELD_SDK | TRM_CAP_WIRE_WELD_SDK | TRM_CAP_GAS_CHECK_SDK | TRM_CAP_POINT_WELD_SDK | TRM_CAP_CRAFT_APPLY_SDK;
    profile.commissioningFlags = TRM_CONF_ORDERED_FRESH_FEEDBACK_SDK | TRM_CONF_SAFE_STOP_NO_REPLAY_SDK | TRM_CONF_LIVE_POWER_SDK | TRM_CONF_LIVE_WIRE_SDK |
        TRM_CONF_GAS_TIMING_SDK | TRM_CONF_ZERO_WIRE_SAFE_SDK;
    profile.realtimeWriteBase = 200;
    profile.realtimeReadBase = 220;
    profile.statusReadBase = 240;
    profile.feedbackSequenceAddress = 250;
    profile.runtimeStateAddress = 251;
    profile.ratedPowerW = 2000;
    profile.minimumPowerW = 200;
    profile.updatePeriodMs = 10;
    profile.timeoutMs = 50;
    profile.maximumPointWeldMs = 120;
    // 这些字仅用于虚拟控制器测试，不代表厂家已经确认组合。
    profile.preGasControlWord = 2;
    profile.enableControlWord = 3;
    profile.laserOnControlWord = 7;
    profile.postGasControlWord = 2;
    profile.gasCheckControlWord = 2;
    return profile;
}

template <typename Generate> std::string generate(Generate generator) {
    int length = 0;
    CHECK(generator(nullptr, &length) == SUCCESS_LASER_SDK && length > 1);
    char marker = 'x';
    int small = 1;
    CHECK(generator(&marker, &small) == BUFFER_TOO_SMALL_LASER_SDK && marker == 'x' && small == length);
    std::vector<char> output(length);
    CHECK(generator(output.data(), &length) == SUCCESS_LASER_SDK && output.back() == '\0');
    return std::string(output.data());
}

void save(const char *path, const std::string &script) {
    std::ofstream out(path, std::ios::binary);
    out << script;
    CHECK(out.good());
}
}

int main() {
    MachineResource machine = {};
    machine.laserHeadBrand = TRM_LASERPOWER_SDK;
    machine.commuType = RS485_2_LASER_SDK;
    machine.weldingProcessType = WIRE_WELDING_LASERSDK;
    machine.robotParams.emptyRunTCPSpeed = 10;
    machine.robotParams.emptyRunTCPAcc = 100;
    machine.robotParams.weldTCPAcc = 100;
    machine.laserPowerParams.laserPower = 2000;
    LaserMachineHandle handle = -1;
    CHECK(cl_machine_resource_init(&machine, &handle) == SUCCESS_LASER_SDK);

    CraftInfoLaserSdk craft = {};
    craft.craftLaserPower = 800;
    craft.craftLeadInPower = 400;
    craft.craftLeadOutPower = 200;
    craft.craftWeldingSpeed = 10;
    craft.craftLeadInRampTime = 40;
    craft.craftLeadOutRampDistance = 1;
    craft.craftPreGasTime = 15;
    craft.craftPostGasTime = 15;
    LaserTrmCraftSdk values = {};
    values.structSize = sizeof(values);
    values.version = LASER_TRM_CRAFT_VERSION_SDK;
    values.wireLeadInMPerMin = 1.23;
    values.wireWeldMPerMin = 3.45;
    values.wireLeadOutMPerMin = 2.56;
    values.laserFrequencyHz = 2500;
    values.dutyCyclePercent = 90;
    values.scanFrequencyHz = 150;
    values.scanWidthMm = 2.3;

    const auto point = [&](LaserTrmActionSdk action, char *buffer, int *len) {
        return cl_trm_action_script_get(handle, action, &craft, &values, buffer, len);
    };
    int length = 0;
    CHECK(cl_trm_craft_validate(handle, &craft, &values) == SUCCESS_LASER_SDK);
    CraftInfoLaserSdk overRated = craft;
    overRated.craftLaserPower = 2001;
    CHECK(cl_trm_craft_validate(handle, &overRated, &values) == PARAMETER_OUT_OF_RANGE_LASER_SDK);
    overRated = craft;
    overRated.craftLeadInPower = 199;
    CHECK(cl_trm_craft_validate(handle, &overRated, &values) == PARAMETER_OUT_OF_RANGE_LASER_SDK);
    unsigned descriptorCount = 0, bindingCount = 0;
    CHECK(cl_parameters_configuration_get(handle, nullptr, &descriptorCount, nullptr, &bindingCount) == SUCCESS_LASER_SDK && descriptorCount == 6 &&
        bindingCount == 6);
    std::vector<LaserParameterDescriptorSdk> descriptors(descriptorCount);
    std::vector<LaserParameterBindingSdk> bindings(bindingCount);
    CHECK(cl_parameters_configuration_get(handle, descriptors.data(), &descriptorCount, bindings.data(), &bindingCount) == SUCCESS_LASER_SDK);
    CHECK(bindings[0].writeAddress == LASER_PARAMETER_INVALID_ADDRESS_SDK);
    double LaserTrmCraftSdk::*const wireValues[] = {
        &LaserTrmCraftSdk::wireLeadInMPerMin, &LaserTrmCraftSdk::wireWeldMPerMin, &LaserTrmCraftSdk::wireLeadOutMPerMin};
    for (auto field : wireValues) {
        LaserTrmCraftSdk offGrid = values;
        offGrid.*field = 2.371;
        CHECK(cl_trm_craft_validate(handle, &craft, &offGrid) == PARAMETER_OUT_OF_RANGE_LASER_SDK);
    }
    LaserTrmCraftSdk offGrid = values;
    offGrid.scanWidthMm = 0.15;
    CHECK(cl_trm_craft_validate(handle, &craft, &offGrid) == PARAMETER_OUT_OF_RANGE_LASER_SDK);
    CHECK(cl_trm_craft_script_get(handle, &craft, &values, nullptr, &length) == COMMU_ERROR_LASER_SDK);
    CHECK(point(TRM_ACTION_MANUAL_SESSION_SDK, nullptr, &length) == COMMU_ERROR_LASER_SDK);
    CHECK(cl_get_arc_start_script(handle, nullptr, &length) == PARAM_TYPE_ERROR_LASER_SDK);
    CHECK(cl_craft_end_arc_subprogram_script_get(handle, nullptr, &length) == PARAM_TYPE_ERROR_LASER_SDK);

    PointDataLaserSdk points[2] = {};
    points[1].pose[0] = 10;
    int instructions[] = {CRAFT_SELECT_INST_LASER_WELD_SDK, 0, LINE_POINT_INST_LASER_WELD_SDK, 0, ARC_START_INST_LASER_WELD_SDK, 0,
        LINE_POINT_INST_LASER_WELD_SDK, 1, ARC_END_INST_LASER_WELD_SDK, 0};
    LaserWeldStartData data = {};
    data.instList = instructions;
    data.instNum = 5;
    data.craftInfoList = &craft;
    data.craftNum = 1;
    data.pointList = points;
    data.pointsNum = 2;
    CHECK(cl_trm_program_script_get(handle, &data, &values, 1, true, nullptr, &length) == COMMU_ERROR_LASER_SDK);
    const std::string sim = generate([&](char *buffer, int *len) {
        return cl_trm_program_script_get(handle, &data, nullptr, 0, false, buffer, len);
    });
    CHECK(sim.find("movel(") != std::string::npos && sim.find("write_int16_register") == std::string::npos &&
        sim.find("set_configurable_digital_out") == std::string::npos && sim.find("sys_event_register") == std::string::npos);

    LaserTrmProfileSdk profile = fixture();
    LaserTrmProfileSdk bad = profile;
    bad.commissioningFlags &= ~TRM_CONF_ORDERED_FRESH_FEEDBACK_SDK;
    CHECK(cl_trm_profile_set(handle, &bad) == PARAMETER_ERROR_LASER_SDK);
    bad = profile;
    bad.maximumPointWeldMs = 0;
    CHECK(cl_trm_profile_set(handle, &bad) == PARAMETER_ERROR_LASER_SDK);
    bad = profile;
    bad.ratedPowerW = 1900;
    CHECK(cl_trm_profile_set(handle, &bad) == PARAMETER_ERROR_LASER_SDK);
    bad = profile;
    bad.feedbackSequenceAddress = INT32_REG_RUN_BREAK_POINT_617;
    CHECK(cl_trm_profile_set(handle, &bad) == PARAMETER_ERROR_LASER_SDK);
    CHECK(cl_trm_profile_set(handle, &profile) == SUCCESS_LASER_SDK);
    CHECK(cl_parameters_configuration_get(handle, descriptors.data(), &descriptorCount, bindings.data(), &bindingCount) == SUCCESS_LASER_SDK);
    CHECK(bindings[0].writeAddress == 202 && bindings[0].readAddress == 222);
    LaserParameterValueSdk ordinary = {};
    ordinary.structSize = sizeof(ordinary);
    ordinary.parameterId = LASER_PARAM_LASER_ENABLE_SWITCH_SDK;
    ordinary.value = 1;
    CHECK(cl_parameters_script_get(handle, &ordinary, 1, nullptr, &length, nullptr) == PARAM_TYPE_ERROR_LASER_SDK);
    ordinary.parameterId = LASER_PARAM_SCAN_WIDTH_SDK;
    ordinary.value = 0.15;
    CHECK(cl_parameters_script_get(handle, &ordinary, 1, nullptr, &length, nullptr) == PARAMETER_OUT_OF_RANGE_LASER_SDK);
    ordinary.value = 2.3;
    const std::string ordinaryScript = generate([&](char *buffer, int *len) {
        return cl_parameters_script_get(handle, &ordinary, 1, buffer, len, nullptr);
    });
    CHECK(ordinaryScript.find("local fields = {[7]=23,}") != std::string::npos && ordinaryScript.find("trm_control(") == std::string::npos);
    bad = profile;
    bad.realtimeReadBase = bad.realtimeWriteBase;
    CHECK(cl_trm_profile_set(handle, &bad) == PARAMETER_ERROR_LASER_SDK);
    CHECK(cl_trm_profile_get(handle, &bad) == SUCCESS_LASER_SDK && bad.realtimeReadBase == profile.realtimeReadBase);
    craft.craftLeadOutPower = 199;
    CHECK(cl_trm_craft_validate(handle, &craft, &values) == PARAMETER_OUT_OF_RANGE_LASER_SDK);
    craft.craftLeadOutPower = 200;
    CHECK(point(TRM_ACTION_WIRE_FEED_START_SDK, nullptr, &length) == PARAM_TYPE_ERROR_LASER_SDK);
    CHECK(cl_craft_inst_list_script_get(handle, data, true, nullptr, &length) == PARAM_TYPE_ERROR_LASER_SDK);
    CHECK(cl_trm_program_script_get(handle, &data, &values, 0, true, nullptr, &length) == PARAMETER_ERROR_LASER_SDK);

    const auto program = [&]() {
        return generate([&](char *buffer, int *len) {
            return cl_trm_program_script_get(handle, &data, &values, 1, true, buffer, len);
        });
    };
    const std::string weld = program();
    CHECK(weld.find("set_configurable_digital_out") == std::string::npos && weld.find("sys_event_register(2") != std::string::npos);
    CHECK(weld.find("{process=1, p0=400, p1=800, p2=200, w0=123, w1=345, w2=256") != std::string::npos);
    CHECK(weld.find("scan=150, width=23") != std::string::npos);
    values.scanFrequencyHz = 250;
    CHECK(program().find("scan=250, width=23") != std::string::npos);
    values.scanFrequencyHz = 150;
    CHECK(program() == weld);
    save("trm_program.lua", weld);
    save("trm_apply.lua", generate([&](char *buffer, int *len) {
        return cl_trm_craft_script_get(handle, &craft, &values, buffer, len);
    }));
    save("trm_manual.lua", generate([&](char *buffer, int *len) {
        return point(TRM_ACTION_MANUAL_SESSION_SDK, buffer, len);
    }));

    machine.robotHandle = 123;
    CHECK(cl_machine_resource_update(handle, &machine) == SUCCESS_LASER_SDK);
    CHECK(cl_trm_profile_get(handle, &bad) == SUCCESS_LASER_SDK && bad.capabilities == 0);
    CHECK(cl_machine_resource_release(handle) == SUCCESS_LASER_SDK);
    CHECK(cl_trm_profile_get(handle, &bad) == HANDLE_ERROR_LASER_SDK);
    std::cout << "TRM SDK API regressions passed\n";
}
