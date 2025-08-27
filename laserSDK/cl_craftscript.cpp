#include "cl_craftscript.h"
#include "cl_api.h"
#include "cl_utils.h"

#include <mutex>
#include <random>
#include <memory>
#include <unordered_map>

//用于生成随机数，分配焊机句柄，保证唯一性和安全性。
std::mt19937 engine;
//均匀分布的整数随机数生成器
std::uniform_int_distribution<int> distribution;
//焊机资源的全局管理表，key 是句柄，value 是焊机资源对象。
std::unordered_map<int, MachineResource> machineResourcesMap;

//RS485单例
CommuRS485 *CommuRS485::s_commuRS485 = nullptr;
std::once_flag CommuRS485::initFlag;

LaserSdkResult CommuBase::craft_script_control_stability_voltage_current_script_get(bool operate, std::string &script)
{
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

CommuRS485 *CommuRS485::GetInstance()
{
    std::call_once(initFlag, [](){s_commuRS485 = new CommuRS485();});
    return s_commuRS485;
}

LaserSdkResult CommuRS485::craft_script_laser_on_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk laserSwitch, std::string &script)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];
    CRresult result;
    //获取脚本    
    if (laserSwitch == SWITCH_ON_LASER_SDK)
    {
        script = strSetStandardDigitalReplace(0,TRUE); // 开光
        cr_set_stdDigitalOut(machineResource.robotHandle, 0, true); // 开光        
        return crsdk_return_value_transfer(result);
    }
    else if (laserSwitch == SWITCH_OFF_LASER_SDK)
    {
        script = strSetStandardDigitalReplace(0,FALSE); // 不出光
        cr_set_stdDigitalOut(machineResource.robotHandle, 0, false); // 关光        
        return crsdk_return_value_transfer(result);
    }

}

LaserSdkResult CommuRS485::craft_script_start_weld_script_get(LaserMachineHandle laserMachineHandle, SwitchTypeLaserSdk startWelding, std::string &script)
{
    //获取句柄对应的焊机资源对象
    if (machineResourcesMap.find(laserMachineHandle) == machineResourcesMap.end())
    {
        return HANDLE_ERROR_LASER_SDK;
    }
    MachineResource &machineResource = machineResourcesMap[laserMachineHandle];

    int laserSwitchIsOpen = 0 ;
    CRresult result = cr_get_stdDigitalOut(machineResource.robotHandle, 1, &laserSwitchIsOpen);
    if(result != CRresult::sucess)
    {
        return crsdk_return_value_transfer(result);
    }
    //CO1置1使能
    if(laserSwitchIsOpen == 1)
    {
        //获取脚本
        if (startWelding == SWITCH_ON_LASER_SDK)
        {
            result = cr_set_stdDigitalOut(machineResource.robotHandle, 1, startWelding); // 开焊
            script = strSetStandardDigitalReplace(1,TRUE); // 开焊
            return crsdk_return_value_transfer(result);
        }
        else if (startWelding == SWITCH_OFF_LASER_SDK)
        {
            result = cr_set_stdDigitalOut(machineResource.robotHandle, 1, startWelding); // 关焊
            script = strSetStandardDigitalReplace(1,FALSE); // 不出焊
            return crsdk_return_value_transfer(result);
        }
    }
    else if (laserSwitchIsOpen == 0)
    {
        return LaserSdkResult::Arc_Ignition_Failure_LASER_SDK​ ;
    }

}

///激光器设置脚本
LaserSdkResult CommuRS485::craft_script_param_set_script_get(LaserMachineHandle laserMachineHandle, CgxiLaserPowerLaserSdk laserParams, std::string &script)
{
    return LaserSdkResult::SUCCESS_LASER_SDK;
}


