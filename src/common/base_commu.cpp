#include "base_commu.h"
#include "rs485_commu.h"
#include "clmath.h"
#include "utils.h"
#include <sstream>
#include <mutex>
#include <random>
#include <memory>
#include <unordered_map>
#include "brandadapter.h"

LaserSdkResult CommuBase::craft_script_judge_signal_setp_run_script_get(int instLoc, std::string &script) {
    std::string signalSetpStatusLoc = std::to_string(WeldControlAddressLaserSdk::INT32_REG_SINGLE_STEP_RUN_616);
    std::string scriptText = R"(
    if(read_integer_register(%1) == 1 and signalSetpPointLocListLookUp[%2] == 1)
    then
        pause()
    end
    )";
    script += strReplace(scriptText, std::vector<std::string>{signalSetpStatusLoc, std::to_string(instLoc)});
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

LaserSdkResult CommuBase::craft_script_judge_break_point_script_get(int instLoc, std::string &script) {
    unsigned int runBreakPointStatusLoc = WeldControlAddressLaserSdk::INT32_REG_RUN_BREAK_POINT_617;

    std::string scriptText = R"(
    if(breakPointLocListLookUp[%1] == 1)
    then
        write_integer_register(%2, 1)
        pause()
    end
    )";
    script += strReplace(scriptText, std::vector<std::string>{std::to_string(instLoc), std::to_string(runBreakPointStatusLoc)});
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

void CommuBase::resetPendingScriptFragments() {
    startLuaScripts.clear();
    startDataScripts.clear();
}

std::string CommuBase::loadLuaFunctionScriptOut(LaserHeadBrandSdk brand) {
    // 保留原单参数符号，避免内部C++接口的既有调用方链接失效。
    return loadLuaFunctionScriptOut(brand, false);
}

std::string CommuBase::loadLuaFunctionScriptOut(LaserHeadBrandSdk brand, bool mappedParameters) {
    std::string tmpLuaScript;
    for (auto it = startLuaScripts.begin(); it != startLuaScripts.end();) {
        tmpLuaScript += it->second + "\n";
        it = startLuaScripts.erase(it);
    }
    std::string tmpLuaStartDataScript;
    for (auto it = startDataScripts.begin(); it != startDataScripts.end();) {
        tmpLuaScript = it->second + tmpLuaScript + "\n";
        it = startDataScripts.erase(it);
    }
    // 本地映射输出由控制器主站负责通讯，禁止附带旧串口与CRC函数。
    std::string luaFunctionScript = mappedParameters ? std::string() : GetBrandProtocolAdapter(brand).buildInitLuaFunc();

    return tmpLuaStartDataScript + luaFunctionScript + tmpLuaScript;
}

/**
 * @brief 按通讯类型获取通讯脚本生成器实例
 *
 * 返回 RS485 单例的借用指针，不转移所有权，调用方不得释放。
 * 未支持的通讯类型必须返回错误，避免使用不匹配的协议生成脚本。
 *
 */
LaserSdkResult CommFactory::GetCommu(CommuTypeLaserSdk commuType, CommuBase *&commu) {
    commu = nullptr;
    if (commuType == CommuTypeLaserSdk::RS485_2_LASER_SDK) {
        commu = static_cast<CommuRS485 *>(CommuRS485::GetInstance());
        if (commu == nullptr) {
            return LaserSdkResult::COMMU_ERROR_LASER_SDK;
        }
    } else {
        return LaserSdkResult::COMMU_ERROR_LASER_SDK;
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}
