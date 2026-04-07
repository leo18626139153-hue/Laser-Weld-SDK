#include "brandadapter.h"

brandAdapter::brandAdapter()
{

}

LaserSdkResult GetCraftCheck(CraftInfoLaserSdk curCraftInfo)
{
    /*超强--相关参数检查*/
    LaserSdkResult res = SUCCESS_LASER_SDK;
    int maxLimit = (curCraftInfo.graphicsSwitching == SpotPatternLaserSdk::CROSS_LASERSDK) ? 1 : 0;
    int speed = curCraftInfo.craftScanSpeed;
    float rangeW = static_cast<float>(curCraftInfo.craftScanWidth);
    if(rangeW < 0.1f || rangeW > (5.0f - 2.0f * maxLimit))
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    float rangeH = static_cast<float>(curCraftInfo.craftScanHeight);
    if(rangeH < 0.1f || rangeH > (5.0f - 2.0f * maxLimit))
        return LaserSdkResult::PARAMETER_OUT_OF_RANGE_LASER_SDK;
    //    圆：频率=速度/(3.14*宽度)
    //    直线：频率=速度/[2*宽度]
    //    8 字：频率=速度*2/[3.14*(宽度+高度)]
    //    十字：频率=速度/[2*(宽度+高度)]
    //    三尖瓣：频率=速度/(3*宽度)
    //    四尖瓣：频率=速度*2/[3*(宽度+高度)]，
    //    “扫描频率”必须满足 10Hz< 频率<150Hz（十字型 10Hz<频率<50Hz）
    float freq = 0.0;
    switch (curCraftInfo.graphicsSwitching)
    {
        case SpotPatternLaserSdk::CIR_LASERSDK:
            freq = static_cast<float>(speed)/(static_cast<float>(PI)*rangeW) + 0.5f;
        break;
        case SpotPatternLaserSdk::LINE_LASERSDK:
            freq = static_cast<float>(speed)/(rangeW * 2.0f) + 0.5f;
        break;
        case SpotPatternLaserSdk::STAR_LASERSDK:
            freq = ( static_cast<float>(speed)*2.0f)/(3.0f*(rangeW+rangeH))+0.5f;
        break;
        case SpotPatternLaserSdk::HORIZON_8_LASERSDK:
            freq = ( static_cast<float>(speed)*2.0f)/(static_cast<float>(PI)*(rangeW+rangeH))+0.5f;
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
