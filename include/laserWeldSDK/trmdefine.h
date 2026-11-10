#ifndef LASER_TRM_DEFINE_H
#define LASER_TRM_DEFINE_H

#define LASER_TRM_PROFILE_VERSION_SDK 1u
#define LASER_TRM_CRAFT_VERSION_SDK 1u

/** @brief V1只接受经过验收的控制器映射RTU。 */
enum LaserTrmTransportSdk {
    TRM_TRANSPORT_NONE_SDK = 0,
    TRM_TRANSPORT_MAPPED_RTU_SDK = 1
};

/** @brief 已在当前设备及控制器组合上验收的动作。 */
enum LaserTrmCapabilitySdk {
    TRM_CAP_AUTOMATIC_WELD_SDK = 1,
    TRM_CAP_WIRE_WELD_SDK = 2,
    TRM_CAP_GAS_CHECK_SDK = 4,
    TRM_CAP_POINT_WELD_SDK = 8,
    TRM_CAP_CRAFT_APPLY_SDK = 16
};

/** @brief 调用方依据厂家资料和台架验证确认的条件。SDK不探测这些能力。 */
enum LaserTrmCommissioningFlagSdk {
    TRM_CONF_ORDERED_FRESH_FEEDBACK_SDK = 1,
    TRM_CONF_SAFE_STOP_NO_REPLAY_SDK = 2,
    TRM_CONF_LIVE_POWER_SDK = 4,
    TRM_CONF_LIVE_WIRE_SDK = 8,
    TRM_CONF_GAS_TIMING_SDK = 16,
    TRM_CONF_ZERO_WIRE_SAFE_SDK = 32
};

/** @brief SDK脚本发布的逻辑状态。设备出光及故障以有效的原始状态采样为准。 */
enum LaserTrmRuntimeStateSdk {
    TRM_STATE_UNKNOWN_SDK = 0,
    TRM_STATE_IDLE_SDK = 1,
    TRM_STATE_PREPARING_SDK = 2,
    TRM_STATE_EMITTING_SDK = 3,
    TRM_STATE_PAUSED_SDK = 4,
    TRM_STATE_STOPPING_SDK = 5,
    TRM_STATE_FAULT_SDK = 6,
    TRM_STATE_GAS_SDK = 7,
    TRM_STATE_CRAFT_APPLIED_SDK = 8
};

/** @brief 特尔码独立动作或手动会话。抽送丝动作在协议补齐前不支持。 */
enum LaserTrmActionSdk {
    TRM_ACTION_GAS_START_SDK = 0,
    TRM_ACTION_GAS_STOP_SDK = 1,
    TRM_ACTION_POINT_START_SDK = 2,
    TRM_ACTION_POINT_RELEASE_SDK = 3,
    TRM_ACTION_ABORT_SDK = 4,
    TRM_ACTION_WIRE_FEED_START_SDK = 5,
    TRM_ACTION_WIRE_FEED_STOP_SDK = 6,
    TRM_ACTION_WIRE_RETRACT_START_SDK = 7,
    TRM_ACTION_WIRE_RETRACT_STOP_SDK = 8,
    TRM_ACTION_MANUAL_SESSION_SDK = 9
};

/**
 * @brief 特尔码映射RTU的已验收配置，按焊机句柄保存副本。
 * @details 未配置或capabilities为0时禁止实际设备动作。设备地址、串口和映射由调用方配置。
 *          控制器须在成功、完整的新远端采样后更新反馈序号；旧缓存和周期计数不能代替反馈。
 *          远端控制字的读回必须使用实际设备数据，不能回读本地写入槽作为确认。
 */
struct LaserTrmProfileSdk {
    unsigned structSize;              ///< 填sizeof(LaserTrmProfileSdk)
    unsigned version;                 ///< LASER_TRM_PROFILE_VERSION_SDK
    unsigned transport;               ///< LaserTrmTransportSdk
    unsigned capabilities;            ///< LaserTrmCapabilitySdk位掩码，0取消授权
    unsigned commissioningFlags;      ///< LaserTrmCommissioningFlagSdk位掩码
    unsigned realtimeWriteBase;       ///< 实时写块在控制器的16位起始槽，长度8
    unsigned realtimeReadBase;        ///< 实时远端回读块在控制器的16位起始槽，长度8
    unsigned statusReadBase;          ///< 16位状态回读槽，依次为出光、故障低字、故障高字
    unsigned feedbackSequenceAddress; ///< 控制器发布的新鲜采样序号，32位槽
    unsigned runtimeStateAddress;     ///< SDK发布的LaserTrmRuntimeStateSdk，32位槽
    unsigned ratedPowerW;             ///< 当前设备额定功率，W
    unsigned minimumPowerW;           ///< 验收的实时功率下限，W
    unsigned updatePeriodMs;          ///< 渐变目标更新周期，毫秒
    unsigned timeoutMs;               ///< 事务、状态新鲜度超时，毫秒
    unsigned maximumPointWeldMs;      ///< 点焊从开光提交起的最大时长，毫秒
    unsigned preGasControlWord;       ///< 经验证的提前送气字，激光位为0
    unsigned enableControlWord;       ///< 经验证的使能字，激光位为0
    unsigned laserOnControlWord;      ///< 经验证的开光字
    unsigned postGasControlWord;      ///< 经验证的关光后保留气体字
    unsigned offControlWord;          ///< 经验证的安全关光、关气字
    unsigned gasCheckControlWord;     ///< 经验证的独立检气字
};

/**
 * @brief 特尔码每条工艺的设备值，随原CraftInfoLaserSdk一并传入。
 * @details 原工艺传递三段功率W、渐变、轨迹和送气时序。此数组元素布局在V1中固定。
 */
struct LaserTrmCraftSdk {
    unsigned structSize;       ///< 填sizeof(LaserTrmCraftSdk)
    unsigned version;          ///< LASER_TRM_CRAFT_VERSION_SDK
    double wireLeadInMPerMin;  ///< 起始送丝速度，m/min
    double wireWeldMPerMin;    ///< 焊接送丝速度，m/min
    double wireLeadOutMPerMin; ///< 收尾送丝速度，m/min
    double scanWidthMm;        ///< 扫描宽度，mm
    unsigned laserFrequencyHz; ///< 激光频率，Hz
    unsigned dutyCyclePercent; ///< 占空比，%
    unsigned scanFrequencyHz;  ///< 扫描频率，Hz
};

#endif
