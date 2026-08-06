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


//判断单步
LaserSdkResult CommuBase::craft_script_judge_signal_setp_run_script_get(int instLoc, std::string &script)
{
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

//断点判断
LaserSdkResult CommuBase::craft_script_judge_break_point_script_get(int instLoc, std::string &script)
{
    unsigned int runBreakPointStatusLoc = WeldControlAddressLaserSdk::INT32_REG_RUN_BREAK_POINT_617;

    std::string scriptText = R"(
    if(breakPointLocListLookUp[%1] == 1)
    then
        write_bool_register(%2, 1)
        pause()
    end
    )";
    script += strReplace(scriptText, std::vector<std::string> {std::to_string(instLoc), std::to_string(runBreakPointStatusLoc)});
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

void CommuBase::resetPendingScriptFragments()
{
    startLuaScripts.clear();
    startDataScripts.clear();
}

std::string CommuBase::loadLuaFunctionScriptOut(LaserHeadBrandSdk brand)
{
    std::string tmpLuaScript;   //接口函数
    for (auto it = startLuaScripts.begin(); it != startLuaScripts.end(); )
    {
        tmpLuaScript += it->second + "\n";
        it = startLuaScripts.erase(it); // erase 自动返回下一个有效迭代器
    }
    std::string tmpLuaStartDataScript;
    for (auto it = startDataScripts.begin(); it != startDataScripts.end(); )
    {
        tmpLuaScript = it->second + tmpLuaScript + "\n";
        it = startDataScripts.erase(it); //加载后删除
    }
    //获取品牌脚本中的函数定义
    std::string luaFunctionScript = GetBrandProtocolAdapter(brand).buildInitLuaFunc();

    return tmpLuaStartDataScript + luaFunctionScript + tmpLuaScript ;
}

/**
 * @brief 按通讯类型获取通讯脚本生成器实例
 *
 * 修改理由：cl_api.cpp 中 8 处重复判断 commuType 并手动 static_cast 获取 CommuRS485 单例，
 *   职责分散且不利于未来扩展 TCP/IP 等通讯方式。
 * 逻辑：RS485 类型返回现有 CommuRS485 单例（不转移所有权），
 *   未支持的通讯类型返回 COMMU_ERROR_LASER_SDK。
 *   未来新增 TCP/IP 只需在此处增加分支，不需要修改上层调用方。
 *
 * @legacy_source src/craftscript.cpp
 * @legacy_function craft_script_commu_class_get
 * @legacy_baseline 6fefa14
 * @legacy_lines 22-46
 * @compatibility 原有RS485单例获取行为和错误码保持不变
 */
LaserSdkResult CommFactory::GetCommu(CommuTypeLaserSdk commuType, CommuBase*& commu)
{
    commu = nullptr;
    if (commuType == CommuTypeLaserSdk::RS485_2_LASER_SDK)
    {
        commu = static_cast<CommuRS485*>(CommuRS485::GetInstance());
        if (commu == nullptr)
        {
            return LaserSdkResult::COMMU_ERROR_LASER_SDK;
        }
    }
    else
    {
        // 未支持的通讯类型（如 TCP/IP），本次不实现，返回通讯错误码
        return LaserSdkResult::COMMU_ERROR_LASER_SDK;
    }

    return LaserSdkResult::SUCCESS_LASER_SDK;
}
