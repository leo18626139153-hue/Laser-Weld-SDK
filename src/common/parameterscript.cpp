#include "common/parameterscript.h"

#include <locale>
#include <new>
#include <set>
#include <sstream>

namespace laser_parameters
{

/**
 * @brief 校验完整写入计划并在临时输出流中生成映射脚本。
 * @param[in] plan 已编码的参数计划；字序和缩放已由参数编译器确定。
 * @param[in,out] script 仅在全部操作渲染成功后替换，失败保持原内容。
 * @return 成功、无效计划、地址越界或内存分配失败；空计划返回空脚本。
 */
LaserSdkResult EmitMappedLua(const WritePlan& plan, std::string& script)
{
    try
    {
        // 按操作逐项校验并只写入临时流；禁止重复语义、重复地址或非法多字范围。
        std::set<unsigned> parameterIds;
        std::set<unsigned> occupiedAddresses;
        std::ostringstream pendingScript;
        // 固定数字格式，避免系统区域设置把千位分隔符带入 Lua 数值。
        pendingScript.imbue(std::locale::classic());
        for (size_t index = 0; index < plan.operations.size(); ++index)
        {
            const WriteOperation& operation = plan.operations[index];
            const size_t width = operation.words.size();
            if (operation.parameterId == 0 || !parameterIds.insert(operation.parameterId).second
                    || (width != 1 && width != 2)
                    || (operation.signedSingleWord && width != 1))
                return PARAMETER_ERROR_LASER_SDK;
            if (operation.address > 0xFFFFu || width - 1 > 0xFFFFu - operation.address)
                return PARAMETER_OUT_OF_RANGE_LASER_SDK;

            for (size_t word = 0; word < width; ++word)
            {
                const unsigned address = operation.address + static_cast<unsigned>(word);
                if (!occupiedAddresses.insert(address).second)
                    return PARAMETER_ERROR_LASER_SDK;
                const uint16_t encoded = operation.words[word];
                // 保持原始寄存器位，单字有符号参数以负数表示兼容旧控制器接口。
                const int value = operation.signedSingleWord && encoded >= 0x8000u
                        ? static_cast<int>(encoded) - 65536 : static_cast<int>(encoded);
                pendingScript << "    write_int16_register(" << address << ", " << value << ")\n";
            }
        }
        // 流失败可能只设置状态位而不抛异常，确认完整输出后才交付字符串。
        if (!pendingScript.good())
            return MEMORY_ERROR_LASER_SDK;
        std::string completedScript = pendingScript.str();
        script.swap(completedScript);
        return SUCCESS_LASER_SDK;
    }
    catch (const std::bad_alloc&)
    {
        return MEMORY_ERROR_LASER_SDK;
    }
}

} // namespace laser_parameters
