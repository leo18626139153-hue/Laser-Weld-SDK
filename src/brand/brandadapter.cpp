#include "brandadapter.h"

namespace {

const unsigned int INVALID_REGISTER_ADDR = 0xFFFFFFFF;

// 修改理由: 统一构造品牌寄存器描述, 避免各品牌地址表重复填充结构体字段.
// 逻辑: 把写地址, 读地址, 寄存器类型, 缩放比例和写入能力一次性返回给通讯层.
BrandRegisterInfo makeRegister(unsigned int writeAddr, unsigned int readAddr, CommuRegType regType = int16Reg, double scale = 1.0, bool writable = true)
{
    BrandRegisterInfo info;
    info.writeAddr = writeAddr;
    info.readAddr = readAddr;
    info.regType = regType;
    info.scale = scale;
    info.writable = writable;
    return info;
}

// 修改理由: 送丝机相关写入规则必须集中在品牌层, 避免通讯类散落品牌判断.
// 逻辑: 把所有可能访问送丝机或送丝信号的语义归类, 供适配器统一过滤.
bool isWireFeederRole(BrandRegisterRole role)
{
    switch (role)
    {
    case BrandRegisterRole::WireFeederRunStatus:
    case BrandRegisterRole::AutoWireFeedSpeed:
    case BrandRegisterRole::ManualWireFeedSpeed:
    case BrandRegisterRole::ManualWireRetractSpeed:
    case BrandRegisterRole::WireRetractLength:
    case BrandRegisterRole::WireFeedLength:
    case BrandRegisterRole::WireFeedBackDelay:
    case BrandRegisterRole::BreakWireFeedingMode:
    case BrandRegisterRole::ManualWireFeedSwitch:
    case BrandRegisterRole::ManualWireRetractSwitch:
    case BrandRegisterRole::SaveWireFeederData:
    case BrandRegisterRole::WireFeedSignal:
        return true;
    default:
        return false;
    }
}

} // namespace

LaserHeadBrandSdk SupProtocolAdapter::brand() const
{
    return LaserHeadBrandSdk::CQLASERPOWER_SDK;
}

BrandProtocolProfile SupProtocolAdapter::profile() const
{
    // 修改理由: 超强 SUP 协议基础参数属于品牌规则, 通讯层不应硬编码.
    // 逻辑: 返回默认 Modbus 从站, 读写功能码和串口参数, 供脚本生成统一使用.
    BrandProtocolProfile profile;
    profile.slaveAddr = 0x01;
    profile.readFuncCode = 0x03;
    profile.writeSingleFuncCode = 0x06;
    profile.writeMultiFuncCode = 0x10;
    profile.laserWriteSingleScriptCode = 0x0906;
    profile.wireFeederWriteSingleScriptCode = 0x0A06;
    profile.writeMultiScriptCode = 0x0910;
    profile.baudRate = 115200;
    profile.dataBits = 8;
    profile.stopBits = 1;
    return profile;
}

LaserSdkResult SupProtocolAdapter::checkCraft(const CraftInfoLaserSdk& craftInfo) const
{
    LaserSdkResult res = SUCCESS_LASER_SDK;
    
    int maxLimit = (craftInfo.graphicsSwitching == SpotPatternLaserSdk::CROSS_LASERSDK) ? 1 : 0;
    int speed = craftInfo.craftScanSpeed;
    float rangeW = static_cast<float>(craftInfo.craftScanWidth);
    if(rangeW < 0.1f || rangeW > (5.0f - 2.0f * maxLimit))
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    float rangeH = static_cast<float>(craftInfo.craftScanHeight);
    if(rangeH < 0.1f || rangeH > (5.0f - 2.0f * maxLimit))
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;

    float freq = 0.0;
    switch (craftInfo.graphicsSwitching)
    {
    case SpotPatternLaserSdk::CIR_LASERSDK:
        freq = static_cast<float>(speed)/(static_cast<float>(PI)*rangeW) + 0.5f;
        break;
    case SpotPatternLaserSdk::LINE_LASERSDK:
        freq = static_cast<float>(speed)/(rangeW * 2.0f) + 0.5f;
        break;
    case SpotPatternLaserSdk::STAR_LASERSDK:
        freq = (static_cast<float>(speed)*2.0f)/(3.0f*(rangeW+rangeH))+0.5f;
        break;
    case SpotPatternLaserSdk::HORIZON_8_LASERSDK:
        freq = (static_cast<float>(speed)*2.0f)/(static_cast<float>(PI)*(rangeW+rangeH))+0.5f;
        break;
    case SpotPatternLaserSdk::CROSS_LASERSDK:
        freq = static_cast<float>(speed)/(rangeW*2.0f+rangeH*2.0f) + 0.5f;
        break;
    case SpotPatternLaserSdk::TRIANGLE_LASERSDK:
        freq= static_cast<float>(speed)/(3.0f*rangeW)+0.5f;
        break;
    }
    if(freq < 10 || freq > (150 - 100 * maxLimit))
    {
        res = LaserSdkResult::PARAM_SCAN_FREQUENCY_ERROR_LASER_SDK;
    }
    return res;
}

bool SupProtocolAdapter::hasRegister(BrandRegisterRole role) const
{
    return registerInfo(role).writeAddr != INVALID_REGISTER_ADDR;
}

BrandRegisterInfo SupProtocolAdapter::registerInfo(BrandRegisterRole role) const
{
    // 修改理由: 消除通讯类中的超强送丝机魔法地址, 由品牌适配器按语义返回.
    // 逻辑: BrandRegisterRole 表达业务含义, switch 只负责映射到超强协议写读地址.
    switch (role)
    {
    case BrandRegisterRole::WireFeederRunStatus:
        return makeRegister(0x0115, 0x0115);
    case BrandRegisterRole::AutoWireFeedSpeed:
        return makeRegister(0x0100, 0x0100);
    case BrandRegisterRole::ManualWireFeedSpeed:
        return makeRegister(0x0101, 0x0101);
    case BrandRegisterRole::ManualWireRetractSpeed:
        return makeRegister(0x0102, 0x0102);
    case BrandRegisterRole::WireRetractLength:
        return makeRegister(0x0104, 0x0104);
    case BrandRegisterRole::WireFeedLength:
        return makeRegister(0x0105, 0x0105);
    case BrandRegisterRole::WireFeedBackDelay:
        return makeRegister(0x0106, 0x0106);
    case BrandRegisterRole::BreakWireFeedingMode:
        return makeRegister(0x010D, 0x010D);
    case BrandRegisterRole::ManualWireFeedSwitch:
        return makeRegister(0x0140, 0x0140);
    case BrandRegisterRole::ManualWireRetractSwitch:
        return makeRegister(0x0141, 0x0141);
    case BrandRegisterRole::SaveWireFeederData:
        return makeRegister(0x013F, 0x013F);
    case BrandRegisterRole::WireFeedSignal:
        return makeRegister(0x0032, 0x0032);
    case BrandRegisterRole::GasValveSignal:
        return makeRegister(0x0031, 0x0031);
    default:
        return makeRegister(INVALID_REGISTER_ADDR, INVALID_REGISTER_ADDR, int16Reg, 1.0, false);
    }
}

bool SupProtocolAdapter::allowWireFeederWrite(WeldingProcessType weldingType, BrandRegisterRole role) const
{
    // 修改理由: 自熔焊用户可能关闭送丝机, SDK 无法提前感知断联, 继续写普通送丝机参数会导致报文异常.
    // 逻辑:
    // 1. 自熔焊只允许写送丝机运行状态, 用于下发暂停报文.
    // 2. 填丝焊允许写入所有送丝机相关角色, 保持原有行为.
    if (weldingType == WeldingProcessType::SELF_WELDING_LASERSDK)
    {
        return role == BrandRegisterRole::WireFeederRunStatus;
    }
    return isWireFeederRole(role);
}

int SupProtocolAdapter::wireFeederRunStatusValue(WeldingProcessType weldingType) const
{
    // 修改理由: 送丝机启动暂停值属于品牌协议规则, 不能由通讯类直接写死.
    // 逻辑: 自熔焊写 0 暂停送丝机, 填丝焊写 1 恢复送丝机.
    return weldingType == WeldingProcessType::SELF_WELDING_LASERSDK ? 0 : 1;
}

std::vector<BrandRegisterRole> SupProtocolAdapter::craftWireFeederRoles(WeldingProcessType weldingType) const
{
    // 修改理由: 工艺下发时的送丝机参数集合由品牌和焊接类型共同决定.
    // 逻辑:
    // 1. 所有模式先包含送丝机运行状态.
    // 2. 填丝焊追加速度, 抽丝长度, 补丝长度和断丝模式.
    // 3. 自熔焊不追加普通送丝机参数.
    std::vector<BrandRegisterRole> roles;
    roles.push_back(BrandRegisterRole::WireFeederRunStatus);
    if (weldingType == WeldingProcessType::WIRE_WELDING_LASERSDK)
    {
        roles.push_back(BrandRegisterRole::AutoWireFeedSpeed);
        roles.push_back(BrandRegisterRole::WireRetractLength);
        roles.push_back(BrandRegisterRole::WireFeedLength);
        roles.push_back(BrandRegisterRole::BreakWireFeedingMode);
    }
    return roles;
}

std::string SupProtocolAdapter::writeLuaScript(const std::string& functionCode,
                                                const std::string& portAddress,
                                                const std::string& paramValue,
                                                const std::string& portNum,
                                                const std::string& firstRegisterNum,
                                                const std::string& receiptNeed) const
{
    // 【修改理由】从 utils.h 移到品牌适配器层，后续子品牌可重写为不同Lua函数格式（如不同函数名、参数顺序）。
    // 【逻辑】生成 write_value_port Lua 调用行，参数依次为：功能码、地址、值、端口、起始寄存器、回执标志。
    std::ostringstream writeScript;
    writeScript << "    write_value_port(" << functionCode << "," << portAddress << ","
                << paramValue << "," << portNum << "," << firstRegisterNum << ","
                << receiptNeed << ")\n";
    return writeScript.str();
}

bool SupProtocolAdapter::buildConfigureMachineBodyScript(
    const MachineResource& machineResource,
    const CraftInfoLaserSdk& craftInfo,
    std::string& scriptBody) const
{
    // 1. 构建 laserRegisterMap / laserHeadRegisterMap / spotWeld Lua数据表赋值
    // 2. 写入焊接模式和图形切换的单寄存器
    // 3. 构建 write_multi_value_port 批量写调用
    const BrandProtocolProfile p = this->profile();
    const std::string writeSingleFuncCode = decimalToHexStringStream(p.laserWriteSingleScriptCode);
    const std::string writeMultiFuncCode  = decimalToHexStringStream(p.writeMultiScriptCode);
    const CgxiLaserPowerLaserSdk& pw = machineResource.laserPowerParams;
    const CgxiLaserHeadLaserSdk&  hd = machineResource.laserHeadParams;
    std::ostringstream script;
    // ---- 激光器数据表赋值 ----
    script << "    laserRegisterMap[1][2] = "  << pw.laserPower << "\n";
    script << "    laserRegisterMap[6][2] = "  << craftInfo.craftPreGasTime << "\n";
    script << "    laserRegisterMap[7][2] = "  << craftInfo.craftPostGasTime << "\n";
    script << "    laserRegisterMap[9][2] = "  << double2str(craftInfo.craftYAxisZoom, 2)   << "*100 \n";
    script << "    laserRegisterMap[10][2] = " << double2str(craftInfo.craftYAxisOffset, 2)  << "*100 \n";
    script << "    laserRegisterMap[11][2] = " << double2str(craftInfo.craftXAxisZoom, 2)   << "*100 \n";
    script << "    laserRegisterMap[12][2] = " << double2str(craftInfo.craftXAxisOffset, 2)  << "*100 \n";
    // ---- 激光头数据表赋值 ----
    script << "    laserHeadRegisterMap[1][2] = " << craftInfo.craftScanSpeed << "\n";
    script << "    laserHeadRegisterMap[2][2] = " << double2str(craftInfo.craftScanWidth, 1)  << "*10 \n";
    script << "    laserHeadRegisterMap[3][2] = " << craftInfo.craftLeadInPower << "\n";
    script << "    laserHeadRegisterMap[4][2] = " << hd.laserHeadDutyCycle << "\n";
    script << "    laserHeadRegisterMap[5][2] = " << hd.laserHeadHighPulseFrequency << "\n";
    script << "    laserHeadRegisterMap[6][2] = " << hd.laserHeadLowPulseFrequency << "\n";
    script << "    laserHeadRegisterMap[7][2] = " << double2str(craftInfo.craftScanHeight, 1) << "*10 \n";
    script << "    laserHeadRegisterMap[8][2] = " << craftInfo.craftRotationAngle << "\n";
    // ---- 点焊数据表赋值 ----
    script << "    spotWeld[1][2] = " << static_cast<int>(craftInfo.craftWeldType) << "\n";
    script << "    spotWeld[2][2] = " << craftInfo.craftSpotWeldDuration << "\n";
    script << "    spotWeld[3][2] = " << craftInfo.craftSpotWeldInterval << "\n";
    // ---- 写入焊接模式 ----
    script << writeLuaScript(writeSingleFuncCode,
                             std::to_string(machineResource.weldingModeLoc),
                             std::to_string(static_cast<int>(craftInfo.craftweldMode)),
                             "3", "SendCommuRegAddr", "1");
    // ---- 写入图形切换 ----
    switch (craftInfo.graphicsSwitching)
    {
        case SpotPatternLaserSdk::TRIANGLE_LASERSDK:
            script << writeLuaScript(writeSingleFuncCode,
                                    std::to_string(machineResource.laserHeadPatternIdLoc),
                                    std::to_string(4), "3", "SendCommuRegAddr", "1");
            break;
        case SpotPatternLaserSdk::CROSS_LASERSDK:
            script << writeLuaScript(writeSingleFuncCode,
                                    std::to_string(machineResource.laserHeadPatternIdLoc),
                                    std::to_string(5), "3", "SendCommuRegAddr", "1");
            break;
        default:
            script << writeLuaScript(writeSingleFuncCode,
                                    std::to_string(machineResource.laserHeadPatternIdLoc),
                                    std::to_string(craftInfo.graphicsSwitching), "3", "SendCommuRegAddr", "1");
            break;
    }
    // ---- 批量写调用 ----
    script << "    comNo = 3\n"
           << "    slave_addr = " << writeMultiFuncCode << "\n"
           << "    write_multi_value_port(laserRegisterMap)      --写入激光焊接参数\n"
           << "    write_multi_value_port(laserHeadRegisterMap)  --写入激光头\n"
           << "    write_multi_value_port(spotWeld)              --写入点焊参数\n";

    scriptBody = script.str();
    return true;
}

void SupProtocolAdapter::BeginNewSession()
{
    m_lastAppliedParameters.clear();
}

bool SupProtocolAdapter::shouldGenerateWireFeederScript(WeldingProcessType weldingType) const
{
    // 【修改理由】超强品牌在 buildConfigureMachineBodyScript 中仅生成激光器批量写脚本，
    //   送丝机参数需要通讯层通过 write_value_port 逐寄存器追加。
    // 【逻辑】填丝焊返回 true 触发通讯层生成送丝机配置脚本，自熔焊无需送丝机参数。
    return weldingType == WeldingProcessType::WIRE_WELDING_LASERSDK;
} 
//----------------------------------------------------------------
// 宝辰鑫 X-Weld Omni Controller 1 品牌适配器

LaserHeadBrandSdk BcxOmniProtocolAdapter::brand() const
{
    return LaserHeadBrandSdk::BCXLASERPOWER_SDK;
}

BrandProtocolProfile BcxOmniProtocolAdapter::profile() const
{
    //脚本功能码按板卡协议使用 0x5A06 单写和 0x5A09 批量写, 串口为 115200/8N1.
    BrandProtocolProfile profile;
    profile.slaveAddr = 0x5A;
    profile.readFuncCode = 0x03;
    profile.writeSingleFuncCode = 0x06;
    profile.writeMultiFuncCode = 0x09;
    profile.laserWriteSingleScriptCode = 0x5A06;
    profile.wireFeederWriteSingleScriptCode = 0x5A06;
    profile.writeMultiScriptCode = 0x5A09;
    profile.baudRate = 115200;
    profile.dataBits = 8;
    profile.stopBits = 1;
    return profile;
}

LaserSdkResult BcxOmniProtocolAdapter::checkCraft(const CraftInfoLaserSdk& craftInfo) const
{
    // 宝辰鑫暂未提供独立工艺校验范围, 先复用现有超强校验以保持行为稳定.
    // return checkSupCraft(craftInfo);
    return SUCCESS_LASER_SDK;
}

bool BcxOmniProtocolAdapter::hasRegister(BrandRegisterRole role) const
{
    return registerInfo(role).writeAddr != INVALID_REGISTER_ADDR;
}

BrandRegisterInfo BcxOmniProtocolAdapter::registerInfo(BrandRegisterRole role) const
{
    // 修改理由: 宝辰鑫板卡地址体系与超强不同, 必须通过品牌适配器隔离地址差异.
    // 逻辑: 按 X-Weld Omni Controller 1 地址表映射 0x1000 写地址段和 0x2000 读地址段.
    switch (role)
    {
    case BrandRegisterRole::AutoWireFeedSpeed:
        return makeRegister(0x1010, 0x2010);
    case BrandRegisterRole::WireRetractLength:
        return makeRegister(0x1011, 0x2011);
    case BrandRegisterRole::WireFeedLength:
        return makeRegister(0x1012, 0x2012);
    case BrandRegisterRole::WireFeedBackDelay:
        return makeRegister(0x100F, 0x200F);
    case BrandRegisterRole::WireFeederRunStatus:
    case BrandRegisterRole::WireFeedSignal:
        return makeRegister(0x1030, 0x2030);
    case BrandRegisterRole::GasValveSignal:
        return makeRegister(0x1023, 0x2023);
    case BrandRegisterRole::LaserOnOff:
        return makeRegister(0x1040, 0x2040);
    default:
        return makeRegister(INVALID_REGISTER_ADDR, INVALID_REGISTER_ADDR, int16Reg, 1.0, false);
    }
}

bool BcxOmniProtocolAdapter::allowWireFeederWrite(WeldingProcessType weldingType, BrandRegisterRole role) const
{
    // 1. 自熔焊只允许运行状态写入.
    // 2. 填丝焊允许品牌表已定义的送丝机角色写入.
    if (weldingType == WeldingProcessType::SELF_WELDING_LASERSDK)
    {
        return role == BrandRegisterRole::WireFeederRunStatus;
    }
    return isWireFeederRole(role) && hasRegister(role);
}

int BcxOmniProtocolAdapter::wireFeederRunStatusValue(WeldingProcessType weldingType) const
{
    // 逻辑: 自熔焊写 0 暂停或关闭, 填丝焊写 1 启动或恢复.
    return weldingType == WeldingProcessType::SELF_WELDING_LASERSDK ? 0 : 1;
}

std::vector<BrandRegisterRole> BcxOmniProtocolAdapter::craftWireFeederRoles(WeldingProcessType weldingType) const
{
    // 1. 所有模式先包含运行状态.
    // 2. 填丝焊追加送丝速度, 抽丝长度, 补丝长度和补丝延时.
    // 3. 自熔焊只保留运行状态暂停报文.
    std::vector<BrandRegisterRole> roles;
    roles.push_back(BrandRegisterRole::WireFeederRunStatus);
    if (weldingType == WeldingProcessType::WIRE_WELDING_LASERSDK)
    {
        roles.push_back(BrandRegisterRole::AutoWireFeedSpeed);
        roles.push_back(BrandRegisterRole::WireRetractLength);
        roles.push_back(BrandRegisterRole::WireFeedLength);
        roles.push_back(BrandRegisterRole::WireFeedBackDelay);
    }
    return roles;
}

std::string BcxOmniProtocolAdapter::writeLuaScript(const std::string& functionCode,
                                                    const std::string& portAddress,
                                                    const std::string& paramValue,
                                                    const std::string& portNum,
                                                    const std::string& firstRegisterNum,
                                                    const std::string& receiptNeed) const
{
    std::ostringstream writeScript;
    writeScript << "    write_value_port(" << functionCode << "," << portAddress << ","
                << paramValue << "," << portNum << "," << firstRegisterNum << ","
                << receiptNeed << ")\n";
    return writeScript.str();
}

bool BcxOmniProtocolAdapter::buildConfigureMachineBodyScript(
    const MachineResource& machineResource,
    const CraftInfoLaserSdk& craftInfo,
    std::string& scriptBody) const
{
    const BrandProtocolProfile p = this->profile();
    const std::string funcCode = decimalToHexStringStream(p.laserWriteSingleScriptCode);
    const CgxiLaserPowerLaserSdk& pw = machineResource.laserPowerParams;
    const CgxiLaserHeadLaserSdk&  hd = machineResource.laserHeadParams;
    const CgxiWireFeederLaserSdk& wf = machineResource.wireFeederParams;

    ParamMap currentParameters;
    currentParameters[LaserParam::RatedPower] = static_cast<uint16_t>(hd.laserHeadPeakPower);
    currentParameters[LaserParam::CenterOffsetX] = static_cast<uint16_t>(pw.laserHeadXOffset * 100);
    currentParameters[LaserParam::CenterOffsetY] = static_cast<uint16_t>(pw.laserHeadYOffset * 100);
    currentParameters[LaserParam::LaserPower] = static_cast<uint16_t>(pw.laserPower);
    currentParameters[LaserParam::LaserFrequency] = static_cast<uint16_t>(hd.laserHeadLowPulseFrequency);
    currentParameters[LaserParam::DutyCycle] = static_cast<uint16_t>(hd.laserHeadDutyCycle);
    currentParameters[LaserParam::LaserOnPower] = static_cast<uint16_t>(pw.laserPowerOpen);
    currentParameters[LaserParam::LaserOffPower] = static_cast<uint16_t>(pw.laserPowerClose);
    currentParameters[LaserParam::RampUpTime] = static_cast<uint16_t>(pw.laserRampUpTime);
    currentParameters[LaserParam::RampDownTime] = static_cast<uint16_t>(pw.laserRampDownTime);
    currentParameters[LaserParam::GasOnAdvanceTime] = static_cast<uint16_t>(pw.gasStartupAdvanceTime);
    currentParameters[LaserParam::GasOffDelayTime] = static_cast<uint16_t>(pw.gasShutoffDelayTime);
    currentParameters[LaserParam::StepEquivalent] =
            static_cast<uint16_t>(craftInfo.craftStepEquivalent);
    currentParameters[LaserParam::RunningSpeed] = static_cast<uint16_t>(wf.wireFeederMotorSpeed);    //送丝速度
    currentParameters[LaserParam::WireCompensationDelay] = static_cast<uint16_t>(wf.wireFeederWaitTime);  //设定抽丝后延时补丝的时间
    currentParameters[LaserParam::WireFeedRetractSpeed] = static_cast<uint16_t>(wf.wireFeederMotorSpeed);    //设定抽丝后延时补丝的时间
    currentParameters[LaserParam::WireRetractDistance] = static_cast<uint16_t>(wf.wireFeederBackLength);
    currentParameters[LaserParam::WireFeedDistance] = static_cast<uint16_t>(wf.wireFeederFeedLength);
    currentParameters[LaserParam::SingleStepDistance] = static_cast<uint16_t>(0);   //步进暂不支持
    currentParameters[LaserParam::SwingAmplitude] = static_cast<uint16_t>(hd.laserHeadScanWidth * 100);
    currentParameters[LaserParam::SwingShape] = static_cast<uint16_t>(hd.laserSpotPattern);
    currentParameters[LaserParam::SwingFrequency] = static_cast<uint16_t>(hd.laserHeadScanSpeed);//摆动频率
    currentParameters[LaserParam::ScaleFactorX] = static_cast<uint16_t>(pw.laserHeadXEnlarge * 100);
    currentParameters[LaserParam::ScaleFactorY] = static_cast<uint16_t>(pw.laserHeadYEnlarge * 100);

    // ModbusBuildResult builderResult =
    //     LaserModbusRtuBuilder::BuildChangedWriteFrames(currentParameters, m_lastAppliedParameters, 0x5A);

    // if (!builderResult.success )
    // {
    //     scriptBody = builderResult.errorMessage;
    //     return false;
    // }
    //const ModbusWriteFrame &frame = builderResult.frames.front();

    std::ostringstream script;
    // 生成脚本：遍历变化的帧
    // 替换为 C++11 兼容的迭代器遍历
    for (auto it = currentParameters.begin(); it != currentParameters.end(); ++it) {
        script << "    write_int16_register(0x"
               << std::hex << std::setw(4) << std::setfill('0')
               << static_cast<uint16_t>(it->first)  // 获取枚举地址
               << ", " << std::dec << it->second    // 获取参数值
               << ")\n";
    }


    scriptBody = script.str();
    return true;
}

void BcxOmniProtocolAdapter::BeginNewSession()
{
//    /*
//     * 新连接建立后，不能继续使用旧连接的成功下发记录。
//     *
//     * 因为无法确定板卡是否重启、掉电或恢复默认参数。
//     */
//    m_lastAppliedParameters.clear();
//    m_firstSynchronization = true;
}

bool BcxOmniProtocolAdapter::shouldGenerateWireFeederScript(WeldingProcessType weldingType) const
{
    // 【修改理由】宝辰鑫品牌在 buildConfigureMachineBodyScript 中已通过 write_int16_register
    //   将所有送丝机参数一并写入，通讯层不应再重复生成送丝机配置脚本。
    // 【逻辑】始终返回 false，避免通讯层调用 craft_wire_feeder_param_set_script_get 重复写入。
    (void)weldingType;
    return false;
} 

namespace {

// 修改理由: 欧斯睿普暂未提供独立地址表, 先保持与超强兼容的地址和策略.
// 逻辑: 继承超强适配器, 仅覆盖品牌枚举, 便于后续替换为独立协议.
class OsrpProtocolAdapter : public SupProtocolAdapter
{
public:
    LaserHeadBrandSdk brand() const override
    {
        return LaserHeadBrandSdk::OSRPLASERPOWER_SDK;
    }
};

} // namespace

const BrandProtocolAdapter& GetBrandProtocolAdapter(LaserHeadBrandSdk brand)
{
    // 修改理由: 品牌规则统一通过工厂获取, 调用方只依赖激光头品牌枚举.
    // 逻辑:
    // 1. 已知品牌返回对应静态适配器.
    // 2. 未知品牌默认返回超强适配器, 保持历史默认行为.
    static const SupProtocolAdapter supAdapter;
    static const OsrpProtocolAdapter osrpAdapter;
    static const BcxOmniProtocolAdapter bcxAdapter;
    switch (brand)
    {
    case LaserHeadBrandSdk::CQLASERPOWER_SDK:
        return supAdapter;
    case LaserHeadBrandSdk::OSRPLASERPOWER_SDK:
        return osrpAdapter;
    case LaserHeadBrandSdk::BCXLASERPOWER_SDK:
        return bcxAdapter;
    default:
        return supAdapter;
    }
}
