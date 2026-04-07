#ifndef BRANDADAPTER_H
#define BRANDADAPTER_H

#include "internaldatadefine.h"
#include "utils.h"
/*** 品牌规则 ***/
/**
 * @brief 针对无法做兼容的品牌规则
 */
class brandAdapter
{
public:
    brandAdapter();
};

/**
 * @brief   “  超强伟业激光头 ” 图案 参数检查
 * @param   craftInfoCheck    工艺参数
 * @return  LaserSdkResult
 */
LaserSdkResult GetCraftCheck(CraftInfoLaserSdk craftInfoCheck);

#endif // BRANDADAPTER_H
