#ifndef LASER_PARAMETER_DEFINE_H
#define LASER_PARAMETER_DEFINE_H

#include "cl_basedefine.h"

/**
 * @file parameterdefine.h
 * @brief 参数语义、物理值、寄存器编码和地址绑定的公共数据定义。
 * @details 参数语义与板卡地址分开保存；同一语义可在不同配置中使用不同地址和编码。
 *          本文件不保存串口、主从站或设备运行状态，通讯配置由上位机管理。
 */

/** @brief 未配置的参数寄存器地址；地址零是有效地址。 */
#define LASER_PARAMETER_INVALID_ADDRESS_SDK 0xFFFFFFFFu

/**
 * @brief 稳定的参数语义编号，不表示设备寄存器地址。
 *
 * 新型号复用参数语义，通过描述和绑定提供地址及编码差异。
 * 描述结构的 parameterId 允许使用非零的自定义 unsigned 编号。
 * 已发布编号保持稳定；用户扩展编号须与同一配置中的其他参数编号不同。
 */
enum LaserParameterIdSdk
{
    LASER_PARAM_RATED_POWER_SDK = 1,             ///< 额定功率
    LASER_PARAM_WELD_POWER_SDK = 2,              ///< 焊接功率
    LASER_PARAM_LASER_ON_POWER_SDK = 3,          ///< 开光功率
    LASER_PARAM_LASER_OFF_POWER_SDK = 4,         ///< 关光功率
    LASER_PARAM_RAMP_UP_TIME_SDK = 5,            ///< 缓升时间
    LASER_PARAM_RAMP_DOWN_TIME_SDK = 6,          ///< 缓降时间
    LASER_PARAM_PRE_GAS_TIME_SDK = 7,            ///< 提前送气时间
    LASER_PARAM_POST_GAS_TIME_SDK = 8,           ///< 滞后送气时间
    LASER_PARAM_WIRE_OUTPUT_DELAY_SDK = 9,       ///< 延时送丝时间
    LASER_PARAM_WELD_MODE_SDK = 10,              ///< 焊接模式
    LASER_PARAM_SPOT_WELD_TYPE_SDK = 11,         ///< 点焊类型
    LASER_PARAM_SPOT_DURATION_SDK = 12,          ///< 点焊持续时间
    LASER_PARAM_SPOT_INTERVAL_SDK = 13,          ///< 点焊间隔时间
    LASER_PARAM_SPOT_PATTERN_SDK = 14,           ///< 光斑图案
    LASER_PARAM_SCAN_SPEED_SDK = 15,             ///< 扫描速度
    LASER_PARAM_SCAN_FREQUENCY_SDK = 16,         ///< 扫描频率
    LASER_PARAM_SCAN_WIDTH_SDK = 17,             ///< 扫描宽度
    LASER_PARAM_SCAN_HEIGHT_SDK = 18,            ///< 扫描高度
    LASER_PARAM_ROTATION_ANGLE_SDK = 19,         ///< 光斑旋转角度
    LASER_PARAM_DUTY_CYCLE_SDK = 20,             ///< 占空比
    LASER_PARAM_PWM_FREQUENCY_SDK = 21,          ///< 完整脉冲频率
    LASER_PARAM_PWM_HIGH_WORD_SDK = 22,          ///< 兼容旧工艺的脉冲频率高字
    LASER_PARAM_PWM_LOW_WORD_SDK = 23,           ///< 兼容旧工艺的脉冲频率低字
    LASER_PARAM_Y_SCALE_SDK = 24,                ///< 光斑纵向缩放
    LASER_PARAM_X_SCALE_SDK = 25,                ///< 光斑横向缩放
    LASER_PARAM_Y_OFFSET_SDK = 26,               ///< 光斑纵向偏移
    LASER_PARAM_X_OFFSET_SDK = 27,               ///< 光斑横向偏移
    LASER_PARAM_STEP_EQUIVALENT_SDK = 28,        ///< 步进当量
    LASER_PARAM_WIRE_FEED_SPEED_SDK = 29,        ///< 焊接送丝速度
    LASER_PARAM_START_WIRE_FEED_SPEED_SDK = 30,  ///< 开光送丝速度
    LASER_PARAM_STOP_WIRE_FEED_SPEED_SDK = 31,   ///< 关光送丝速度
    LASER_PARAM_MANUAL_FEED_SPEED_SDK = 32,      ///< 手动送丝速度
    LASER_PARAM_MANUAL_RETRACT_SPEED_SDK = 33,   ///< 手动抽丝速度
    LASER_PARAM_WIRE_RETRACT_LENGTH_SDK = 34,    ///< 回抽长度
    LASER_PARAM_WIRE_FEED_LENGTH_SDK = 35,       ///< 补丝长度
    LASER_PARAM_WIRE_FEED_DELAY_SDK = 36,        ///< 补丝延时
    LASER_PARAM_WIRE_CUT_MODE_SDK = 37,          ///< 断丝模式
    LASER_PARAM_WIRE_RUN_STATE_SDK = 38,         ///< 送丝机运行状态
    LASER_PARAM_MANUAL_FEED_SWITCH_SDK = 39,     ///< 手动送丝开关
    LASER_PARAM_MANUAL_RETRACT_SWITCH_SDK = 40,  ///< 手动抽丝开关
    LASER_PARAM_GAS_VALVE_SWITCH_SDK = 41,       ///< 气阀开关
    LASER_PARAM_LASER_ENABLE_SWITCH_SDK = 42,    ///< 激光使能开关
    LASER_PARAM_WIRE_FEED_SWITCH_SDK = 43,       ///< 送丝信号开关
    LASER_PARAM_WIRE_RETRACT_SPEED_SDK = 44,     ///< 收尾回抽速度
    LASER_PARAM_DRIVER_TEMPERATURE_SDK = 45,    ///< 驱动器温度
    LASER_PARAM_WINDOW_TEMPERATURE_SDK = 46,    ///< 保护镜温度
    LASER_PARAM_COLLIMATOR_TEMPERATURE_SDK = 47, ///< 准直镜温度
    LASER_PARAM_EXPERT_MODE_SDK = 48,           ///< 板卡专家模式
    LASER_PARAM_SINGLE_STEP_DISTANCE_SDK = 49,  ///< 送丝机单步距离
    LASER_PARAM_HEAD_PEAK_POWER_SDK = 50,       ///< 激光头初始峰值功率
    LASER_PARAM_FAST_PEAK_POWER_SDK = 51        ///< 激光头快速峰值功率
};

/**
 * @brief 参数编码类型；多字类型占用两个连续的十六位寄存器。
 * @note 有符号整数按补码保存寄存器位，浮点数按 IEEE 754 单精度位表示保存。
 */
enum LaserParameterDataTypeSdk
{
    LASER_PARAMETER_U16_SDK = 0, ///< 十六位无符号整数
    LASER_PARAMETER_I16_SDK = 1, ///< 十六位有符号整数
    LASER_PARAMETER_U32_SDK = 2, ///< 三十二位无符号整数
    LASER_PARAMETER_I32_SDK = 3, ///< 三十二位有符号整数
    LASER_PARAMETER_F32_SDK = 4  ///< 三十二位 IEEE 754 浮点数
};

/**
 * @brief 多字参数在连续寄存器中的字序；寄存器内部不交换字节。
 * @note 单寄存器类型仍须填写合法字序枚举，但编码时无需交换寄存器顺序。
 */
enum LaserParameterWordOrderSdk
{
    LASER_PARAMETER_HIGH_FIRST_SDK = 0, ///< 高十六位写在起始地址
    LASER_PARAMETER_LOW_FIRST_SDK = 1   ///< 低十六位写在起始地址
};

/** @brief 整数编码舍入策略；浮点编码不使用整数舍入。 */
enum LaserParameterRoundingSdk
{
    LASER_PARAMETER_NEAREST_SDK = 0,  ///< 四舍五入，半整数向远离零方向取整
    LASER_PARAMETER_TRUNCATE_SDK = 1, ///< 向零截断
    LASER_PARAMETER_EXACT_SDK = 2     ///< 必须能表示为整数，允许浮点计算精度误差
};

/**
 * @brief 参数访问能力，可通过按位或组合；是否本次写入由值列表决定。
 * @note 可读能力用于描述及地址校验，不表示生成写入计划时会主动读取设备。
 */
enum LaserParameterFlagsSdk
{
    LASER_PARAMETER_READABLE_SDK = 1, ///< 支持读取
    LASER_PARAMETER_WRITABLE_SDK = 2  ///< 支持写入
};

/**
 * @brief 参数类型、物理单位及编码规则；structSize 填 sizeof 本结构。
 * @details 编码顺序为：检查闭区间物理范围，再计算 value * scale + offset，
 *          最后按类型检查编码范围并生成寄存器原始值。整数使用 rounding，浮点数不使用整数舍入。
 * @note scale 必须为有限的非零值，offset 和范围端点必须为有限值，minimum 不得大于 maximum。
 *       structSize 用于检查当前结构是否完整；输入数组仍按当前结构布局排列，不采用自定义元素步长。
 */
typedef struct LaserParameterDescriptorSdk
{
    unsigned structSize;                     ///< 调用方结构大小，至少为当前结构大小
    unsigned parameterId;                    ///< 非零的稳定参数语义编号
    LaserParameterDataTypeSdk dataType;       ///< 寄存器数据编码类型
    LaserParameterWordOrderSdk wordOrder;     ///< 多字参数的寄存器字序
    LaserParameterRoundingSdk rounding;       ///< 整数编码的舍入策略
    double scale;                            ///< 有限非零的编码比例；原始值等于物理值乘 scale 再加 offset
    double offset;                           ///< 编码偏移，必须为有限数
    double minimum;                          ///< 允许的最小物理值，包含端点，必须为有限数
    double maximum;                          ///< 允许的最大物理值，包含端点，必须为有限数
    unsigned flags;                          ///< 可读、可写能力位
    char name[64];                           ///< 参数显示名，必须以零字符结束
    char unit[16];                           ///< 物理单位，必须以零字符结束
} LaserParameterDescriptorSdk;

/**
 * @brief 参数寄存器绑定，显式配置时使用控制器本地地址。
 * @note 未配置的旧超强兼容档案保留原物理寄存器地址；切换到映射模式前须由上位机转换绑定。
 *       地址范围为 0 至 65535；多字类型连续占用两个地址，不能从 65535 开始。
 *       读写方向分别检测地址重叠，同一参数的读地址与写地址可以相同。
 *       未配置方向使用 LASER_PARAMETER_INVALID_ADDRESS_SDK，不使用地址零表示缺失。
 */
typedef struct LaserParameterBindingSdk
{
    unsigned structSize;   ///< 调用方结构大小，至少为当前结构大小
    unsigned parameterId;  ///< 绑定到的参数语义编号
    unsigned writeAddress; ///< 写寄存器起始地址；未配置时用无效地址常量
    unsigned readAddress;  ///< 读寄存器起始地址；未配置时用无效地址常量
} LaserParameterBindingSdk;

/**
 * @brief 本次需要写入的参数物理值，单位必须与该参数描述一致。
 * @note 值列表只列出本次需要写入的参数，不要求包含配置中的全部可写参数；同一列表不得重复编号。
 */
typedef struct LaserParameterValueSdk
{
    unsigned structSize;  ///< 调用方结构大小，至少为当前结构大小
    unsigned parameterId; ///< 参数语义编号
    double value;         ///< 描述规定单位的物理值
} LaserParameterValueSdk;

/**
 * @brief 参数配置或编码诊断；structSize 填 sizeof 本结构。
 * @note 内核成功时清空 parameterId 和 message，并将 result 设为 SUCCESS_LASER_SDK。
 *       失败时记录首个检测到的错误；诊断结构大小不足时返回参数错误且不写诊断缓冲区。
 */
typedef struct LaserParameterDiagnosticSdk
{
    unsigned structSize;   ///< 调用方结构大小，至少为当前结构大小
    unsigned parameterId;  ///< 失败参数编号；成功或无法归属单一参数的错误为零
    LaserSdkResult result; ///< 与函数返回值相同的结果码
    char message[160];     ///< 以零字符结束的中文诊断
} LaserParameterDiagnosticSdk;

#endif // LASER_PARAMETER_DEFINE_H
