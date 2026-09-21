#include "brandadapter.h"
#include <cmath>

namespace {

const unsigned int INVALID_REGISTER_ADDR = 0xFFFFFFFF;
const double BCX_CENTER_OFFSET_MIN_MM = -15.0;
const double BCX_CENTER_OFFSET_MAX_MM = 15.0;
const double BCX_CENTER_OFFSET_SCALE = 100.0;
const int BCX_RAMP_TIME_MAX_MS = 9999;

enum Sup26AdLocalRegister
{
    SUP26AD_Y_SCALE = 60,
    SUP26AD_Y_OFFSET = 61,
    SUP26AD_X_SCALE = 62,
    SUP26AD_X_OFFSET = 63,
    SUP26AD_WELD_MODE = 64,
    SUP26AD_PRE_GAS_TIME = 65,
    SUP26AD_POST_GAS_TIME = 66,
    SUP26AD_SPOT_DURATION = 67,
    SUP26AD_SPOT_INTERVAL = 68,
    SUP26AD_EXPERT_MODE = 69,
    SUP26AD_PATTERN = 70,
    SUP26AD_SCAN_FREQUENCY = 71,
    SUP26AD_SCAN_WIDTH = 72,
    SUP26AD_SCAN_HEIGHT = 73,
    SUP26AD_ROTATION_ANGLE = 74,
    SUP26AD_PEAK_POWER = 75,
    SUP26AD_DUTY_CYCLE = 76,
    SUP26AD_PWM_HIGH = 77,
    SUP26AD_PWM_LOW = 78,
    SUP26AD_WIRE_FEED_SPEED = 79,
    SUP26AD_WIRE_FEED_SWITCH = 80,
    SUP26AD_WIRE_RETRACT_LENGTH = 81,
    SUP26AD_WIRE_STICK_LENGTH = 82,
    SUP26AD_LASER_ON_POWER = 83,
    SUP26AD_RAMP_UP_TIME = 84,
    SUP26AD_LASER_OFF_POWER = 85,
    SUP26AD_RAMP_DOWN_TIME = 86
};

int sup26AdPatternValue(SpotPatternLaserSdk pattern)
{
    if (pattern == SpotPatternLaserSdk::TRIANGLE_LASERSDK)
        return 4;
    if (pattern == SpotPatternLaserSdk::CROSS_LASERSDK)
        return 5;
    return static_cast<int>(pattern);
}

int sup26AdScanFrequency(const CraftInfoLaserSdk& craftInfo)
{
    const double width = craftInfo.craftScanWidth;
    const double height = craftInfo.craftScanHeight;
    const double speed = static_cast<double>(craftInfo.craftScanSpeed);
    double frequency = 0.0;
    switch (craftInfo.graphicsSwitching)
    {
    case SpotPatternLaserSdk::CIR_LASERSDK:
    case SpotPatternLaserSdk::DOUBLE_CIRCLE_LASERSDK:
        frequency = speed / (PI * width);
        break;
    case SpotPatternLaserSdk::LINE_LASERSDK:
        frequency = speed / (width * 2.0);
        break;
    case SpotPatternLaserSdk::STAR_LASERSDK:
        frequency = speed * 2.0 / (3.0 * (width + height));
        break;
    case SpotPatternLaserSdk::HORIZON_8_LASERSDK:
        frequency = speed * 2.0 / (PI * (width + height));
        break;
    case SpotPatternLaserSdk::CROSS_LASERSDK:
        frequency = speed / (width * 2.0 + height * 2.0);
        break;
    case SpotPatternLaserSdk::TRIANGLE_LASERSDK:
        frequency = speed / (3.0 * width);
        break;
    }
    return static_cast<int>(std::lround(frequency));
}

int sup26AdMaxScanFrequency(const CraftInfoLaserSdk& craftInfo)
{
    // SUP26AD(150聚焦) 协议表，当前UI开放宽度0.1~5.0mm和0~6七种图形。
    static const int limits[7][5] = {
        {500, 500, 500, 500, 400}, // 圆
        {500, 450, 350, 300, 200}, // 8字
        {500, 500, 500, 500, 500}, // 一字
        {400, 400, 400, 400, 350}, // 四尖瓣
        {500, 500, 500, 500, 400}, // 三尖瓣
        {400, 400, 400, 300, 250}, // 十字
        {500, 500, 500, 500, 450}  // 双圆
    };
    const double graphSize = craftInfo.craftScanWidth > craftInfo.craftScanHeight
            ? craftInfo.craftScanWidth : craftInfo.craftScanHeight;
    int sizeIndex = static_cast<int>(std::ceil(graphSize)) - 1;
    if (sizeIndex < 0) sizeIndex = 0;
    if (sizeIndex > 4) sizeIndex = 4;
    const int patternIndex = sup26AdPatternValue(craftInfo.graphicsSwitching);
    return limits[patternIndex][sizeIndex];
}

void appendSup26AdWrite(std::ostringstream& script, int address, int value)
{
    script << "    write_int16_register(" << address << ", " << value << ")\n";
}

/**
 * @brief 将宝辰鑫中心偏移转换为寄存器有符号整数。
 * @param offsetMm 中心偏移，单位mm。
 * @return 放大100倍并四舍五入后的寄存器值。
 */
int16_t scaleBcxCenterOffset(double offsetMm)
{
    return static_cast<int16_t>(std::lround(offsetMm * BCX_CENTER_OFFSET_SCALE));
}

/**
 * @brief 获取宝辰鑫脚本使用的寄存器参数值。
 * @param parameter 参数类型。
 * @param registerValue 寄存器原始16位数据。
 * @return 中心偏移按有符号数返回，其它参数保持无符号取值。
 */
int bcxScriptRegisterValue(LaserParam parameter, uint16_t registerValue)
{
    if (parameter == LaserParam::CenterOffsetX
            || parameter == LaserParam::CenterOffsetY)
    {
        return static_cast<int16_t>(registerValue);
    }
    return static_cast<int>(registerValue);
}

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
    // 2. 填丝焊追加速度、抽丝长度、补丝长度、补丝延迟和断丝模式。
    // 3. 自熔焊不追加普通送丝机参数.
    std::vector<BrandRegisterRole> roles;
    roles.push_back(BrandRegisterRole::WireFeederRunStatus);
    if (weldingType == WeldingProcessType::WIRE_WELDING_LASERSDK)
    {
        roles.push_back(BrandRegisterRole::AutoWireFeedSpeed);
        roles.push_back(BrandRegisterRole::WireRetractLength);
        roles.push_back(BrandRegisterRole::WireFeedLength);
        roles.push_back(BrandRegisterRole::WireFeedBackDelay);
        roles.push_back(BrandRegisterRole::BreakWireFeedingMode);
    }
    return roles;
}

std::string BrandProtocolAdapter::writeLuaScript(const std::string& functionCode,
                                                const std::string& portAddress,
                                                const std::string& paramValue,
                                                const std::string& portNum,
                                                const std::string& firstRegisterNum,
                                                const std::string& receiptNeed) const
{
    // 修改理由: 默认实现统一生成 write_value_port 调用, 各品牌无差异时复用, 避免逐字重复.
    // 逻辑: 参数依次为功能码、地址、值、端口、起始寄存器、回执标志; 子品牌可覆写定制.
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
    script << "    write_multi_value_port(laserRegisterMap, " << writeMultiFuncCode << ", 3)      --写入激光焊接参数\n"
           << "    write_multi_value_port(laserHeadRegisterMap, " << writeMultiFuncCode << ", 3)  --写入激光头\n"
           << "    write_multi_value_port(spotWeld, " << writeMultiFuncCode << ", 3)              --写入点焊参数\n";

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

std::string SupProtocolAdapter::buildInitLuaFunc() const
{
    // 修改理由: laserRegisterMap / wireRegisterMap / spotWeld / laserHeadRegisterMap
    //   数据表属于超强品牌地址规则, 不应硬编码在通讯层 initData() 中.
    // 逻辑: 返回超强品牌的 Lua 数据表定义, 通讯层拼接通用工具函数后输出完整初始化脚本.
    std::ostringstream script;

    std::string commandListStartData =
    R"(--激光器工艺
    local laserRegisterMap = {
        [1] = {0x0230, 2000},  -- 激光功率
        [2] = {0x0231, 100},  -- 开光功率
        [3] = {0x0232, 0},    -- 开光功率上升时间
        [4] = {0x0233, 100},  -- 关光功率
        [5] = {0x0234, 0},   -- 关光功率下降时间
        [6] = {0x0235, 2000},     -- 气体打开延时 --提前送气时间
        [7] = {0x0236, 1000},     -- 气体关闭延时 --滞后送气时间
        [8] = {0x0237, 0},    -- 送丝延时 默认0
        [9] = {0x0238, 1},    -- 激光头Y轴缩放
        [10] = {0x0239, 0},   -- 激光头Y轴偏移
        [11] = {0x023A, 1 },  -- 激光头X轴缩放
        [12] = {0x023B, 0 },  -- 激光头X轴偏移
    }
    --送丝机参数
    local wireRegisterMap = {
        [1] = {0x0103, 0},      --自动送丝延时
        [2] = {0x0104, 100},    --回抽长度
        [3] = {0x0105, 100},    --补丝长度
        [4] = {0x0106, 0},    --补丝延时

        [100] = {0x0100, 123},  --自动送丝速度
        [115] = {)" + std::to_string(registerInfo(BrandRegisterRole::WireFeederRunStatus).writeAddr) + R"(, 0},    --送丝机运行状态
    }
    --点焊工艺
    local spotWeld = {
        [1] = {0x0242, 0}, --点焊类型
        [2] = {0x0243, 0}, --点焊持续时间  0 ~ 60000 ms
        [3] = {0x0244, 0} --点焊间隔时间   0 ~ 60000 ms
    }
    -- 激光头控制参数
    local laserHeadRegisterMap = {
        [1] = { 0x0000, 2000},  -- 激光头扫描速度
        [2] = { 0x0001, 25 },    -- 激光头扫描宽度
        [3] = { 0x0002, 3000 },    -- 峰值功率，焊接功率
        [4] = { 0x0003, 100},   -- 占空比
        [5] = { 0x0004, 0 },
        [6] = { 0x0005, 20000 },    --脉冲频率/工艺无控制
        [7] = { 0x0006, 25 },    -- 激光头扫描高度
        [8] = { 0x0007, 0 },    -- 激光头旋转角度
    }
    )";
    std::string startData = R"(
    local SendDataType = 0 --bool位寄存器 通过bool寄存器发送字节数据
    local RecvDataType = 1 --16位寄存器 接收到的数据放到16位寄存器中
    local SendCommuRegAddr = 0		--发送指令从bool通讯变量的 0 地址开始存储
    local SendCommuReg_n = 16		--发送指令从bool通讯变量的 0 地址开始存储
    local RecvCommuRegAddr = 30		--接收的数据从16位通讯变量的 30 地址开始存储
    local RecvCommuReg_n = 60		--接收30个字节
    local flag = 0  --标识位
    )";

    std::string swap_bytes = R"(
    -- 1.高低字节交换函数
    function swap_bytes(data)
        -- 获取低字节 (通过与操作获取低8位)
        local low_byte = data & 0xFF
        -- 获取高字节 (右移8位)
        local high_byte = data >> 8
        -- 交换并组合
        return (low_byte << 8) | high_byte
    end
    )";

    std::string modbus_crc16 = R"(
    -- 2.CRC-16 (Modbus) 计算函数 无高低字节互转
    function modbus_crc16(data)
        local crc = 0xFFFF  		-- CRC寄存器初始化为0xFFFF
        local polynomial = 0xA001   -- CRC-16多项式（0x8005反转）
        -- 对每个字节进行处理
        for _, byte in ipairs(data) do
            crc = crc ~ byte  -- 将字节与CRC寄存器进行异或
            -- 对CRC寄存器进行16次循环移位
            for i = 1, 8 do
                if (crc & 0x0001) ~= 0 then
                    crc = (crc >> 1) ~ polynomial
                else
                    crc = crc >> 1
                end
            end
        end
        return crc
    end
    )";
    std::string lua_split = R"(
    --3.高低字节拆分 如0x1234 输出{0x12, 0x34}
    function Lua_split(dataSplit)
        local value = dataSplit < 0 and (65536 + dataSplit) or dataSplit
        local low_byte = value & 0xFF
        local high_byte = value >> 8
        return {high_byte, low_byte}
    end
    )";
    std::string register_clear = R"(
    --4.定义寄存器清空函数(起始地址，清空位数)
    function register_clear(firstAddr, cleanNums)
        for i=0,cleanNums-1,1 do
            write_int16_register(i + firstAddr, 0)
        end
    end
    function bool_register_clear(firstAddr, cleanNums)
        for i=0,cleanNums-1,1 do
            write_bool_register(i + firstAddr, 0)
        end
    end
    )";
    // 【修改理由】删除了 Lua_data_get 函数：该函数在任何生成的Lua脚本中均未被调用，属于僵尸代码
    // 【逻辑】保留其他功能函数，不影响 write_value_port / write_multi_value_port 的执行
    std::string concatenate_crc = R"(
    -- 6.将十进制数转换为十六进制，并以两位字符表示
    function to_hex_string(number)
        return string.format("%02X", number)
    end
    -- 7.遍历表中单字节数据拼接对应的十六进制字符串，输入 {0x09, 0x10, {0x1234}, {0x56}}，输出 0910123456
    function concatenate_crc(crc_mes)
        local result = ""
        for _, value in ipairs(crc_mes) do
            if type(value) == "number" then
                local hex_value = to_hex_string(value)
                result = result .. hex_value
            elseif type(value) == "table" then
                local byte_pair = Lua_split(value[1])
                local high_hex = to_hex_string(byte_pair[1])
                local low_hex = to_hex_string(byte_pair[2])
                result = result .. high_hex .. low_hex
            end
        end
        return result
    end
    )" ;
    std::string parse_little_endian_bytes = R"(
    -- 每2个字符为一组(1字节)转换为整数
    local function parse_little_endian_bytes(hex_str)
        local bytes = {}
        for i = 1, #hex_str, 2 do
            local byte = hex_str:sub(i, i + 1)
            if #byte == 2 then
                local value = tonumber(byte, 16)
                table.insert(bytes, value)
            end
        end
        return bytes
    end
    )";

    std::string writeHex2Commu_reg = R"(
    --将数据写入通讯变量
    local function WriteHex2Commu_reg(hex)
        local byte_values = parse_little_endian_bytes(hex)
        local cnt = 0;
        for i, byte in ipairs(byte_values) do
            write_char_register(SendCommuRegAddr+i-1,byte)
            cnt = i
        end
        return cnt
    end
    )";

    std::string task_write_port = R"(
    --单参数写入 (功能码(2 Byte), 寄存器地址(2 Bytes), 寄存器值(2 Bytes), 端口号， 起始寄存器, 接受回执)
    function write_value_port(function_code, port_address, parameter_value, comNo, SendCommuRegAddr, receiptNeed)
        --------将通讯变量清零--------
        bool_register_clear(SendCommuRegAddr, 8)
        register_clear(RecvCommuRegAddr, RecvCommuReg_n)
        ----------高低字节拆分计算校验码---------
        local data1=Lua_split(function_code)
        local data2=Lua_split(port_address)
        local data3=Lua_split(parameter_value)
        local mes_1={data1[1], data1[2], data2[1], data2[2], data3[1], data3[2]}
        local mes_2=Lua_split(modbus_crc16(mes_1))    --计算CRC16校验码无反转
        --构造报文，高低字节反转发送
        mes_3 = {data1[1], data1[2],  data2[1], data2[2], data3[1], data3[2], mes_2[2], mes_2[1]}
        local tmp_string = concatenate_crc(mes_3)  --将数据拼接成字符串
        print(tmp_string)
        local CommuReg_n =  WriteHex2Commu_reg(tmp_string)  --将数据写入通讯变量，返回字节数
        if receiptNeed == 0 then
            serial_send_receive(comNo, CommuReg_n, 0, SendCommuRegAddr,RecvDataType,RecvCommuRegAddr, 0)
            wait(50)
            return
        else
            receiptNeed = 1
            serial_send_receive(comNo, CommuReg_n, 0, SendCommuRegAddr,RecvDataType,RecvCommuRegAddr, 1)
        end
        local tryTimes = 3
        local timeout = 1000
        while(read_int16_register(RecvCommuRegAddr) == 0) do
            wait(20)
            timeout = timeout - 20
            if read_int16_register(RecvCommuRegAddr) ~= 0 then
                print("recieve successful")
                break
            end
            if timeout <= 0 then
                tryTimes = tryTimes - 1
                timeout = 300
            end
            if tryTimes <= 0 then
                print(string.format("超时首地址 : 0x%04X", port_address))
                write_integer_register(821, port_address)
                return 0
            elseif(tryTimes > 0 )
            then
                serial_send_receive(comNo, CommuReg_n, 0, SendCommuRegAddr,RecvDataType,RecvCommuRegAddr,receiptNeed)
            end
        end

    end
    function write_multi_value_port(addr_value_map, slave_addr, comNo)
        -- 报文格式 {功能码-寄存器首地址-寄存器数量-字节数量-DATAX.-.CRC16}
        bool_register_clear(0, #addr_value_map * 4 + 18) -- 清空通讯变量
        register_clear(RecvCommuRegAddr, RecvCommuReg_n)
        local crc_mes = {}
        -- 1 功能码由品牌层传入, 例如超强 0x0910, 宝辰鑫 0x5A09
        local split_function_code = Lua_split(slave_addr)
        -- 2 寄存器首地址
        local split_data_2 = Lua_split(addr_value_map[1][1])
        -- 3 寄存器数量
        local split_data_3 = Lua_split(#addr_value_map)
        -- 4 字节数量
        crc_mes = {split_function_code[1], split_function_code[2], split_data_2[1], split_data_2[2], split_data_3[1], split_data_3[2], #addr_value_map * 2}
        -- 5. 将对应的变量进行高低字节拆分 放入mes中
        for k, address  in ipairs(addr_value_map) do
            local split_data_x = Lua_split(address[2])
            table.insert(crc_mes, split_data_x[1])
            table.insert(crc_mes, split_data_x[2])
        end
        --6. 计算校验码
        local split_data_crc = Lua_split(swap_bytes(modbus_crc16(crc_mes)))
        table.insert(crc_mes, split_data_crc[1])
        table.insert(crc_mes, split_data_crc[2])
        --7. 写入报文
        local mes_string = concatenate_crc(crc_mes)
        local CommuReg_n = WriteHex2Commu_reg(mes_string)
        print("发送报文："..mes_string)
        --8. 发送报文
        local timeout = 500
        local tryTimes = 3
        --端口--写寄存器个数--寄存器类型--写起始地址--读取存放寄存器类型--读起始地址--是否需要回执
        serial_send_receive(comNo, CommuReg_n, 0, SendCommuRegAddr, RecvDataType, RecvCommuRegAddr, 1)
        while true do
            wait(20)
            timeout = timeout - 20
            if read_int16_register(RecvCommuRegAddr) ~= 0 then
                print("recieve success")
                break
            end
            if timeout <= 0 then
                tryTimes = tryTimes - 1
                timeout = 310
            end
            if tryTimes <= 0 then
                print(string.format("发送超时|起始地址-0x%04X", addr_value_map[1][1]))
                write_integer_register(821, addr_value_map[1][1])
                return 0
            elseif(tryTimes > 0 )
            then
                serial_send_receive(comNo, CommuReg_n, 0, SendCommuRegAddr, RecvDataType, RecvCommuRegAddr, 1)
            end
        end
    end
    )";

    std::string read_value_port =
        "--单参数写入 (功能码(2 Byte), 寄存器地址(2 Bytes), 寄存器值(2 Bytes), 端口号， 起始寄存器), 接受回执\n"
        "function read_value_port(function_code, port_address, parameter_value, comNo, SendCommuRegAddr,SendCommuReg_n)\n"
        "	--------将通讯变量清零--------\n"
        "	bool_register_clear(SendCommuRegAddr, SendCommuReg_n)\n"
        "	register_clear(RecvCommuRegAddr, RecvCommuReg_n)\n"
        "	----------高低字节拆分计算校验码---------    \n"
        "	local data1=Lua_split(function_code)     \n"
        "	local data2=Lua_split(port_address)      \n"
        "    local data3=Lua_split(parameter_value)  \n"
        "	local mes_1={data1[1], data1[2], data2[1], data2[2], data3[1], data3[2]}\n"
        "	local mes_2=Lua_split(modbus_crc16(mes_1))    --计算CRC16校验码无反转    \n"
        "    print(string.format(\"CRC校验值: 0x%02X\", mes_2[1], mes_2[2])) \n"
        "    --构造报文，高低字节反转发送\n"
        "	mes_3 = {data1[1], data1[2],  data2[1], data2[2], data3[1], data3[2], mes_2[2], mes_2[1]}	\n"
        "    local tmp_string = concatenate_crc(mes_3)  --将数据拼接成字符串\n"
        "	print(\"发送的报文:\",tmp_string)\n"
        "    local CommuReg_n =  WriteHex2Commu_reg(tmp_string)  --将数据写入通讯变量，返回字节数\n"
        "    local timeout = 1   --超时时间\n"
        "    --端口--写字节数--写类型--写起始地址--读取字节数--读类型--读起始地址--超时时间\n"
        "	serial_send_receive(comNo, CommuReg_n, 0, SendCommuRegAddr, RecvDataType,RecvCommuRegAddr)\n"
        "    --等待接收结果\n"
        "    local timeout = 500    \n"
        "	print(\"等待接收结果...\",read_int16_register(RecvCommuRegAddr))  \n"
        "    while(read_int16_register(RecvCommuRegAddr) == 0 and timeout > 0) do   \n"
        "		print(\"等待接收结果...\")    \n"
        "        wait(10)\n"
        "        timeout = timeout - 10\n"
        "    end\n"
        "    print(\"接收结果完成\")\n"
        "end\n";

    // 【修改理由】移除了 lua_data_get（零引用僵尸函数）和 alarm_bits_list（状态监控已注释）
    // 【逻辑】其余函数按依赖顺序拼接：基础工具 → 数据表 → 变量 → CRC → 拆分 → 清空 → 串口读写
    script << commandListStartData << startData << swap_bytes << modbus_crc16
           << lua_split << register_clear << concatenate_crc
           << parse_little_endian_bytes << writeHex2Commu_reg << task_write_port;

    return script.str();
}

//----------------------------------------------------------------
// 超强 SUP26AD 品牌适配器

LaserHeadBrandSdk Sup26AdProtocolAdapter::brand() const
{
    return LaserHeadBrandSdk::CQ_SUP26AD_LASERPOWER_SDK;
}

BrandProtocolProfile Sup26AdProtocolAdapter::profile() const
{
    BrandProtocolProfile result;
    result.slaveAddr = 0x09;
    result.readFuncCode = 0x03;
    result.writeSingleFuncCode = 0x06;
    result.writeMultiFuncCode = 0x10;
    result.laserWriteSingleScriptCode = 0x0906;
    result.wireFeederWriteSingleScriptCode = 0x0906;
    result.writeMultiScriptCode = 0x0910;
    result.baudRate = 115200;
    result.dataBits = 8;
    result.stopBits = 1;
    return result;
}

LaserSdkResult Sup26AdProtocolAdapter::checkCraft(const CraftInfoLaserSdk& craftInfo) const
{
    const int peakPower = craftInfo.laserHeadPeakPower;
    if (peakPower <= 0
            || craftInfo.craftLaserPower <= 0
            || craftInfo.craftLaserPower > peakPower
            || craftInfo.craftLeadInPower < 0
            || craftInfo.craftLeadInPower > craftInfo.craftLaserPower
            || craftInfo.craftLeadOutPower < 0
            || craftInfo.craftLeadOutPower > craftInfo.craftLaserPower)
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;

    if (!std::isfinite(craftInfo.craftScanWidth)
            || !std::isfinite(craftInfo.craftScanHeight)
            || craftInfo.craftScanWidth < 0.1 || craftInfo.craftScanWidth > 5.0
            || craftInfo.craftScanHeight < 0.1 || craftInfo.craftScanHeight > 5.0)
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;

    const int pattern = static_cast<int>(craftInfo.graphicsSwitching);
    if (pattern < static_cast<int>(SpotPatternLaserSdk::CIR_LASERSDK)
            || pattern > static_cast<int>(SpotPatternLaserSdk::DOUBLE_CIRCLE_LASERSDK))
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;

    const int frequency = sup26AdScanFrequency(craftInfo);
    if (frequency < 10 || frequency > sup26AdMaxScanFrequency(craftInfo))
        return LaserSdkResult::PARAM_SCAN_FREQUENCY_ERROR_LASER_SDK;

    if (craftInfo.craftYAxisZoom < 0.01 || craftInfo.craftYAxisZoom > 4.0
            || craftInfo.craftXAxisZoom < 0.01 || craftInfo.craftXAxisZoom > 4.0
            || craftInfo.craftYAxisOffset < -3.0 || craftInfo.craftYAxisOffset > 3.0
            || craftInfo.craftXAxisOffset < -3.0 || craftInfo.craftXAxisOffset > 3.0
            || craftInfo.craftRotationAngle < 0 || craftInfo.craftRotationAngle > 360
            || craftInfo.craftPreGasTime < 0 || craftInfo.craftPreGasTime > 3000
            || craftInfo.craftPostGasTime < 0 || craftInfo.craftPostGasTime > 3000
            || craftInfo.craftLeadInRampTime < 0 || craftInfo.craftLeadInRampTime > 2000
            || craftInfo.craftLeadOutRampDistance < 0
            || craftInfo.craftWeldingWireFeed < 0 || craftInfo.craftWeldingWireFeed > 600
            || craftInfo.craftRetractionLength < 0 || craftInfo.craftRetractionLength > 200
            || craftInfo.craftEndLoopbackLength < 0 || craftInfo.craftEndLoopbackLength > 200
            || !std::isfinite(craftInfo.craftWeldingSpeed)
            || craftInfo.craftWeldingSpeed <= 0.0)
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;

    const double rampDownTime = craftInfo.craftLeadOutRampDistance * 1000.0
            / craftInfo.craftWeldingSpeed;
    if (!std::isfinite(rampDownTime) || rampDownTime > 2000.0)
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    return LaserSdkResult::SUCCESS_LASER_SDK;
}

bool Sup26AdProtocolAdapter::hasRegister(BrandRegisterRole role) const
{
    return registerInfo(role).writeAddr != INVALID_REGISTER_ADDR;
}

BrandRegisterInfo Sup26AdProtocolAdapter::registerInfo(BrandRegisterRole role) const
{
    switch (role)
    {
    case BrandRegisterRole::GasValveSignal:
        return makeRegister(0x1061, 0x1061);
    case BrandRegisterRole::LaserOnOff:
        return makeRegister(0x1060, 0x1060);
    default:
        return makeRegister(INVALID_REGISTER_ADDR, INVALID_REGISTER_ADDR,
                            int16Reg, 1.0, false);
    }
}

bool Sup26AdProtocolAdapter::allowWireFeederWrite(
        WeldingProcessType weldingType, BrandRegisterRole role) const
{
    (void)weldingType;
    (void)role;
    return false;
}

int Sup26AdProtocolAdapter::wireFeederRunStatusValue(
        WeldingProcessType weldingType) const
{
    (void)weldingType;
    return 0;
}

std::vector<BrandRegisterRole> Sup26AdProtocolAdapter::craftWireFeederRoles(
        WeldingProcessType weldingType) const
{
    (void)weldingType;
    return std::vector<BrandRegisterRole>();
}

bool Sup26AdProtocolAdapter::buildConfigureMachineBodyScript(
        const MachineResource& machineResource,
        const CraftInfoLaserSdk& craftInfo,
        std::string& scriptBody) const
{
    const CgxiLaserHeadLaserSdk& head = machineResource.laserHeadParams;
    const uint32_t pwmFrequency =
            (static_cast<uint32_t>(head.laserHeadHighPulseFrequency) << 16)
            | (static_cast<uint32_t>(head.laserHeadLowPulseFrequency) & 0xFFFFu);
    const int rampDownTime = static_cast<int>(std::lround(
            craftInfo.craftLeadOutRampDistance * 1000.0
            / craftInfo.craftWeldingSpeed));
    const int weldingPower = craftInfo.craftLaserPower;
    if (weldingPower <= 0)
    {
        return false;
    }
    const int laserOnPowerPercent = static_cast<int>(std::lround(
            static_cast<double>(craftInfo.craftLeadInPower) * 100.0
            / static_cast<double>(weldingPower)));
    const int laserOffPowerPercent = static_cast<int>(std::lround(
            static_cast<double>(craftInfo.craftLeadOutPower) * 100.0
            / static_cast<double>(weldingPower)));

    std::ostringstream script;
    appendSup26AdWrite(script, SUP26AD_Y_SCALE,
                       static_cast<int>(std::lround(craftInfo.craftYAxisZoom * 100.0)));
    appendSup26AdWrite(script, SUP26AD_Y_OFFSET,
                       static_cast<int>(std::lround(craftInfo.craftYAxisOffset * 100.0)));
    appendSup26AdWrite(script, SUP26AD_X_SCALE,
                       static_cast<int>(std::lround(craftInfo.craftXAxisZoom * 100.0)));
    appendSup26AdWrite(script, SUP26AD_X_OFFSET,
                       static_cast<int>(std::lround(craftInfo.craftXAxisOffset * 100.0)));
    appendSup26AdWrite(script, SUP26AD_WELD_MODE,
                       craftInfo.craftweldMode == WeldModeLaserSdk::SPOT_LASER_SDK ? 1 : 0);
    appendSup26AdWrite(script, SUP26AD_PRE_GAS_TIME, craftInfo.craftPreGasTime);
    appendSup26AdWrite(script, SUP26AD_POST_GAS_TIME, craftInfo.craftPostGasTime);
    appendSup26AdWrite(script, SUP26AD_SPOT_DURATION, craftInfo.craftSpotWeldDuration);
    appendSup26AdWrite(script, SUP26AD_SPOT_INTERVAL, craftInfo.craftSpotWeldInterval);
    // 0x123C 固定专家模式，确保0x139E~0x13A5及渐进参数生效。
    appendSup26AdWrite(script, SUP26AD_EXPERT_MODE, 1);
    appendSup26AdWrite(script, SUP26AD_PATTERN,
                       sup26AdPatternValue(craftInfo.graphicsSwitching));
    appendSup26AdWrite(script, SUP26AD_SCAN_FREQUENCY, sup26AdScanFrequency(craftInfo));
    appendSup26AdWrite(script, SUP26AD_SCAN_WIDTH,
                       static_cast<int>(std::lround(craftInfo.craftScanWidth * 10.0)));
    appendSup26AdWrite(script, SUP26AD_SCAN_HEIGHT,
                       static_cast<int>(std::lround(craftInfo.craftScanHeight * 10.0)));
    appendSup26AdWrite(script, SUP26AD_ROTATION_ANGLE, craftInfo.craftRotationAngle);
    appendSup26AdWrite(script, SUP26AD_PEAK_POWER, craftInfo.craftLaserPower);
    appendSup26AdWrite(script, SUP26AD_DUTY_CYCLE, head.laserHeadDutyCycle);
    appendSup26AdWrite(script, SUP26AD_PWM_HIGH,
                       static_cast<int>((pwmFrequency >> 16) & 0xFFFFu));
    appendSup26AdWrite(script, SUP26AD_PWM_LOW,
                       static_cast<int>(pwmFrequency & 0xFFFFu));
    appendSup26AdWrite(script, SUP26AD_WIRE_FEED_SPEED,
                       craftInfo.craftWeldingWireFeed);
    appendSup26AdWrite(script, SUP26AD_WIRE_FEED_SWITCH,
                       machineResource.weldingProcessType
                       == WeldingProcessType::WIRE_WELDING_LASERSDK ? 1 : 0);
    appendSup26AdWrite(script, SUP26AD_WIRE_RETRACT_LENGTH,
                       craftInfo.craftRetractionLength);
    appendSup26AdWrite(script, SUP26AD_WIRE_STICK_LENGTH,
                       craftInfo.craftEndLoopbackLength);
    appendSup26AdWrite(script, SUP26AD_LASER_ON_POWER, laserOnPowerPercent);
    appendSup26AdWrite(script, SUP26AD_RAMP_UP_TIME, craftInfo.craftLeadInRampTime);
    appendSup26AdWrite(script, SUP26AD_LASER_OFF_POWER, laserOffPowerPercent);
    appendSup26AdWrite(script, SUP26AD_RAMP_DOWN_TIME, rampDownTime);
    scriptBody = script.str();
    return true;
}

bool Sup26AdProtocolAdapter::shouldGenerateWireFeederScript(
        WeldingProcessType weldingType) const
{
    (void)weldingType;
    return false;
}

void Sup26AdProtocolAdapter::BeginNewSession()
{
    // SUP26AD 仅写本地映射寄存器，不复用 SUP25A 的自由通讯会话状态。
}

std::string Sup26AdProtocolAdapter::buildInitLuaFunc() const
{
    return std::string();
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
    // 1. 宝辰鑫功率使用瓦特，焊接、开光和关光功率均必须小于激光器峰值功率。
    // 2. 缓升时间和由距离换算出的缓降时间均不得超过板卡9999ms上限。
    // 3. 中心偏移寄存器按0.01mm缩放，允许范围为-15.00～15.00mm。
    const int peakPower = craftInfo.laserHeadPeakPower;
    if (peakPower <= 0
            || craftInfo.craftLaserPower < 0
            || craftInfo.craftLaserPower >= peakPower
            || craftInfo.craftLeadInPower < 0
            || craftInfo.craftLeadInPower >= peakPower
            || craftInfo.craftLeadOutPower < 0
            || craftInfo.craftLeadOutPower >= peakPower
            || craftInfo.craftLeadInRampTime < 0
            || craftInfo.craftLeadInRampTime > BCX_RAMP_TIME_MAX_MS
            || craftInfo.craftLeadOutRampDistance < 0
            || !std::isfinite(craftInfo.craftWeldingSpeed)
            || craftInfo.craftWeldingSpeed <= 0.0)
    {
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }

    const double rampDownTime = static_cast<double>(craftInfo.craftLeadOutRampDistance) * 1000.0
            / craftInfo.craftWeldingSpeed;
    if (!std::isfinite(rampDownTime) || rampDownTime > BCX_RAMP_TIME_MAX_MS)
    {
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }

    if (!std::isfinite(craftInfo.craftXAxisOffset)
            || !std::isfinite(craftInfo.craftYAxisOffset)
            || craftInfo.craftXAxisOffset < BCX_CENTER_OFFSET_MIN_MM
            || craftInfo.craftXAxisOffset > BCX_CENTER_OFFSET_MAX_MM
            || craftInfo.craftYAxisOffset < BCX_CENTER_OFFSET_MIN_MM
            || craftInfo.craftYAxisOffset > BCX_CENTER_OFFSET_MAX_MM)
    {
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    }
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
    currentParameters[LaserParam::CenterOffsetX] =
            static_cast<uint16_t>(scaleBcxCenterOffset(pw.laserHeadXOffset));
    currentParameters[LaserParam::CenterOffsetY] =
            static_cast<uint16_t>(scaleBcxCenterOffset(pw.laserHeadYOffset));
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


    std::ostringstream script;
    // 生成脚本：遍历变化的帧
    // 替换为 C++11 兼容的迭代器遍历
    for (auto it = currentParameters.begin(); it != currentParameters.end(); ++it) {
        script << "    write_int16_register(0x"
               << std::hex << std::setw(4) << std::setfill('0')
               << static_cast<uint16_t>(it->first)  // 获取枚举地址
               << ", " << std::dec
               << bcxScriptRegisterValue(it->first, it->second) // 获取参数值
               << ")\n";
    }


    scriptBody = script.str();
    return true;
}

void BcxOmniProtocolAdapter::BeginNewSession()
{
    // 宝辰鑫采用全量寄存器写入, 不维护跨会话增量同步状态, 此处无需清理.
}

bool BcxOmniProtocolAdapter::shouldGenerateWireFeederScript(WeldingProcessType weldingType) const
{
    // 【修改理由】宝辰鑫品牌在 buildConfigureMachineBodyScript 中已通过 write_int16_register
    //   将所有送丝机参数一并写入，通讯层不应再重复生成送丝机配置脚本。
    // 【逻辑】始终返回 false，避免通讯层调用 craft_wire_feeder_param_set_script_get 重复写入。
    (void)weldingType;
    return false;
}

std::string BcxOmniProtocolAdapter::buildInitLuaFunc() const
{
    // 修改理由: 宝辰鑫 X-Weld Omni Controller 1 使用 0x1000 写地址段和 0x2000 读地址段,
    //   数据表定义属于品牌地址规则, 不应硬编码在通讯层 initData() 中.
    // 逻辑: 返回宝辰鑫品牌的 Lua 数据表定义, 通讯层拼接通用工具函数后输出完整初始化脚本.
    std::ostringstream script;

    // ---- 激光器/激光头寄存器数据表 (写地址 0x1000 段) ----
    script << "--BcxOmniProtocolAdapter::buildInitLuaFunc--\n";

    return script.str();
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
    static const Sup26AdProtocolAdapter sup26AdAdapter;
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
    case LaserHeadBrandSdk::CQ_SUP26AD_LASERPOWER_SDK:
        return sup26AdAdapter;
    default:
        return supAdapter;
    }
}
