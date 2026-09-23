#include "cl_api.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
int main() {
 MachineResource r = {}; r.commuType = RS485_2_LASER_SDK; r.laserHeadBrand=CQLASERPOWER_SDK;
 r.weldingProcessType=WIRE_WELDING_LASERSDK;
 LaserMachineHandle h=-1; assert(cl_machine_resource_init(&r,&h)==SUCCESS_LASER_SDK);
 CraftInfoLaserSdk craft={};
 craft.laserHeadPeakPower=3000; craft.craftLaserPower=2000;craft.craftLeadInPower=1000;craft.craftLeadOutPower=1500;
 craft.craftScanWidth=2;craft.craftScanHeight=4;craft.craftScanSpeed=200;craft.graphicsSwitching=CIR_LASERSDK;
 craft.craftYAxisZoom=0.1;craft.craftXAxisZoom=0.1;craft.craftWeldingSpeed=10;craft.craftLeadOutRampDistance=10;
 craft.craftLeadInRampTime=200;craft.craftWeldingWireFeed=60;craft.laserHeadDutyCycle=100;craft.laserHeadLowPulseFrequency=2100;
 std::vector<char> b(1000000); int n=b.size();
 for(int iteration=0;iteration<4;++iteration) {
  r.laserHeadBrand=iteration%2 ? CQLASERPOWER_SDK : CQ_SUP26AD_LASERPOWER_SDK;
  assert(cl_machine_resource_update(h,&r)==SUCCESS_LASER_SDK);
  MachineResource actual={};assert(cl_machine_resource_get(h,&actual)==SUCCESS_LASER_SDK);assert(actual.laserHeadBrand==r.laserHeadBrand);
  n=b.size(); auto result=cl_craft_inst_devices_params_script_get(h,craft,b.data(),&n);
  std::cout<<"iteration "<<iteration<<" brand "<<r.laserHeadBrand<<" result "<<result<<std::endl;
  if(iteration%2) continue;
  assert(result==SUCCESS_LASER_SDK);std::string txt(b.data());
  assert(txt.find("write_value_port")==std::string::npos);
  assert(txt.find("write_int16_register(60, 10)")!=std::string::npos);
  assert(txt.find("write_int16_register(84, 200)")!=std::string::npos);
  assert(txt.find("write_int16_register(86, 1000)")!=std::string::npos);
  std::ofstream("sup26-craft.lua")<<txt;
  n=b.size();result=cl_get_arc_start_slow_script(h,b.data(),&n);assert(result==SUCCESS_LASER_SDK);assert(std::string(b.data()).find("write_value_port")==std::string::npos);
  std::ofstream("sup26-ramp-start.lua")<<b.data();
  n=b.size();result=cl_get_arc_stop_slow_script(h,b.data(),&n);assert(result==SUCCESS_LASER_SDK);assert(std::string(b.data()).find("write_value_port")==std::string::npos);
  std::ofstream("sup26-ramp-stop.lua")<<b.data();
 }
 assert(cl_machine_resource_release(h)==SUCCESS_LASER_SDK);
 std::cout<<"PASS brand switching, craft registers and ramp scripts"<<std::endl;
}
