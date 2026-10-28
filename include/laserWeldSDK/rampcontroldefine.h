#ifndef LASER_RAMP_CONTROL_DEFINE_H
#define LASER_RAMP_CONTROL_DEFINE_H

/** @brief 外控阶段配置版本；以后新增版本不得改变现有资源及工艺结构布局。 */
#define LASER_RAMP_CONTROL_CONFIG_VERSION_SDK 1u

/**
 * @brief SDK 外控功率和送丝阶段的独立配置，必须通过配置接口显式启用。
 * @details 启用后工艺三档送丝速度统一使用 mm/s，在品牌出口转换和编码；
 *          旧调用方默认不启用，继续使用原有脚本和原生单位。
 *          时间来自控制器 get_systime() 的秒、微秒，不使用主机时间或 CPU 时间。
 * @note 本配置只控制脚本生成，不执行通讯；配置改变后必须重新生成并加载程序。
 */
typedef struct LaserRampControlConfigSdk {
    unsigned structSize;     ///< 至少填写 sizeof(LaserRampControlConfigSdk)
    unsigned version;        ///< 固定为 LASER_RAMP_CONTROL_CONFIG_VERSION_SDK
    unsigned enabled;        ///< 0 保留旧行为，1 由 SDK 外控功率及可选送丝渐变
    unsigned updatePeriodMs; ///< 目标更新周期，1~1000ms，按控制器实际经过时间插值
    unsigned settleTimeMs;   ///< 写入后让出及稳定时间，0~1000ms；旧自由通讯至少50ms
    unsigned timeoutMs;      ///< 额外超时上界，1~60000ms，不能小于更新和稳定时间
} LaserRampControlConfigSdk;

#endif
