#include "craftscript.h"
#include <mutex>


class CommuRS485 : public CommuBase
{
public:
    ~CommuRS485() override{}
    static CommuRS485* GetInstance();


private:
    CommuRS485();
    void initData();
    static CommuRS485* s_commuRS485;
    static std::once_flag initFlag; ///< initFlag >>> 保证单例对象只被初始化一次，且线程安全
    int funcCode = 0x0906;

public:
    //--报文组件
    static std::string get_laser_param_set_script(std::map<unsigned int, int> locVec);

public:
    // 起弧/收弧功能脚本
    LaserSdkResult craft_script_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch) override;
    // 起弧/收弧执行程序
    LaserSdkResult craft_script_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string& script) override;
    //激光器设置脚本
    LaserSdkResult craft_laser_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;

    //激光头图案设置脚本
    LaserSdkResult craft_laser_head_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;
    //送丝机设置脚本
    LaserSdkResult craft_wire_feeder_param_set_script_get(LaserMachineHandle laserMachineHandle, const std::vector<unsigned int> locVec, std::string& script) override;

    //缓升缓降 起收弧功能脚本
    LaserSdkResult craft_script_slow_arc_function_switch_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch) override;
    //缓升缓降 起收弧执行程序
    LaserSdkResult craft_script_slow_arc_program_run_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string& script) override;

    /**
     * @brief 10连续写地址相关接口，无法兼容超强以外的参数
     * @param multipleRegistersValue  传入的初始化脚本（通过loadLuaFunctionScript放入脚本中的初始变量），如果输入""，则调用超强板卡的专用接口
     */
    void init_multiple_registers_value(std::string multipleRegistersValue);

    /**
     * @brief 工艺参数修改配置生效脚本
     * @param laserMachineHandle 句柄
     * @param craftInfo 工艺参数
     * @param script    输出脚本
     * @return LaserSdkResult
     */
    LaserSdkResult craft_script_configure_machine_param_set_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo ,std::string& script) override;
    /**
     * @brief 修改工艺参数，多写（送丝机参数除外）
     * @param laserMachineHandle 句柄
     * @param craftInfo 焊接工艺参数
     * @param script    输出脚本
     * @return LaserSdkResult
     */
    LaserSdkResult craft_script_write_craft_resource_script_get(LaserMachineHandle laserMachineHandle, const CraftInfoLaserSdk& craftInfo, std::string& script) override;
};
