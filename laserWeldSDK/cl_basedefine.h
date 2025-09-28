#ifndef CL_BASEDEFINE_H
#define CL_BASEDEFINE_H

#include"basestruct.h"

//#pragma execution_character_set("utf-8")   //防止中文乱码
typedef int LaserMachineHandle;

// 接口调用结果
enum LaserSdkResult
{
    SUCCESS_LASER_SDK = 0,                             ///< 成功
    ERROR_LASER_SDK = 1,                               ///< 一般错误
    MEMORY_ERROR_LASER_SDK = 2,                        ///< 内存错误
    HANDLE_ERROR_LASER_SDK = 3,                        ///< 焊机句柄错误
    SPEED_LIMIT_LASER_SDK = 4,                         ///< 速度超限
    ACC_LIMIT_LASER_SDK = 5,                           ///< 加速度超限
    SPEED_CAL_ERROR_LASER_SDK = 6,                     ///< 速度计算错误
    ARC_FAILURE_LASER_SDK = 7,                         ///< 起弧失败,(前提：CO0未置1)
    COMMU_ERROR_LASER_SDK = 8,                         ///< 通讯错误
    PARAMETER_ERROR_LASER_SDK = 9,                     ///< 参数错误，没有传值

    //  参考 weldSDK //
    CALCULATE_CIRCLE_ERROR_LASER_SDK = 10,               // 计算圆错误
    SHORT_AXIS_LIMIT_LASER_SDK = 11,                     // 短轴限制
    BACK_DISTANCE_LIMIT_LASER_SDK = 12,                  // 回退距离超出范围
    LOCATE_FAIL_LASER_SDK = 13,                          // 焊丝寻位失败
    ARC_TEADCHING_ABNORMAL_LASER_SDK = 14,               // 圆弧示教异常(建议检查示教点)
    ARC_CALCULATE_FAILED_LASER_SDK = 15,                 // 圆弧计算失败(建议检查示教点)
    WELD_TYPE_ERROR_LASER_SDK = 16,                      // 焊接类型错误
    MOVEJ_TRAJECTORY_SPLIT_FAILED_LASER_SDK = 17,        // movej轨迹拆分失败
    INTERMITTENT_ERROR_LASER_SDK = 18,                   // 间断焊接错误（焊接总长度小于一个焊接周期）
    INSTLIST_ERROR_LASER_SDK = 19,                     ///< 指令列表错误
    PARAMETER_OUT_OF_RANGE_LASER_SDK =20,              ///< 激光焊参数设置越界

    WELD_LOCATE_THREAD_IS_RUN_LASER_SDK = 21,          // 焊丝寻位状态异常，寻位线程仍在运行
    WELD_LOCATE_HAVE_CIRCLE_POINT_LASER_SDK = 22,      // 焊丝寻位存在圆弧定点
    WELD_LOCATE_REFPOINT_LESS_LASER_SDK = 23,          // 焊丝寻位参考点数不足
    INTERVAL_WELD_INST_LIST_ERROR_LASER_SDK = 24,      // 间断焊接指令列表错误
    NEED_SET_PITCHANGLE_LASER_SDK = 25,                // 多层多道未设置俯仰角
    WELD_PARA_ERROR_LASER_SDK = 26,                    // 多层多道自动计算输入参数不合理（输入尺寸过小或者角度不合理）
    MLMC_ONLY_ONE_LAYER_LASER_SDK = 27,                // 多层多道自动计算，计算结果仅有一层一道（焊脚尺寸过小或层高太大）
    MUTLILAYER_CHANNEL_LASER_LINE_TYPE_ERROR = 28,     // 多层多道焊缝类型错误
    MUTLILAYER_CHANNEL_ADVANCE_ANGLE_TYPE_ERROR = 29,  // 多层多道前进角类型错误

    CALCULATION_RESULT_ILLOGICAL_SDK = 98,             // 计算过程中出现未考虑到的场景（计算失败，结算结果不可用）
    OTHER_ERROR_LASER_SDK = 99,                        // 未知错误
    // cr_sdk
    UNDEFINDED_ERROR_CRSDK_LASER_SDK = 100,            ///< cr_sdk中未知错误
    ERROR_CRSDK_LASER_SDK = 101,                       ///< 一般错误
    THREAD_RUNNING_CRSDK_LASER_SDK = 102,              ///< 通讯连接重复建立
    OPERATE_TIMEOUT_CRSDK_LASER_SDK = 103,             ///< 操作超时，无返回值；注：处理超时导致无结果输出
    RESULT_INVALID_CRSDK_LASER_SDK = 104,              ///< 返回结果无效；原因：rpc获取结果为空
    OUT_OF_RANGE_CRSDK_LASER_SDK = 105,                ///< 输入数值越界；需输入在范围内的值
    MUTEX_INVALID_CRSDK_LASER_SDK = 106,               ///< 内部错误
    PARA_ERROR_CRSDK_LASER_SDK = 107,                  ///< 参数无效；需检查输入参数是否为空
    NO_RESULT_CRSDK_LASER_SDK = 108,                   ///< 无对应数值
    NO_ASSIGNTCPINDEX_CRSDK_LASER_SDK = 109,           ///< 未找到指定索引号TCP偏移量
    NO_HANDLE_CRSDK_LASER_SDK = 110,                   ///< 未创建相关句柄
    HANDLE_REPEAT_CRSDK_LASER_SDK = 111,               ///< 重复建立相同的句柄
    REPEAT_NAME_CRSDK_LASER_SDK = 112,                 ///< 重命名
    DELETE_INVALID_CRSDK_LASER_SDK = 113,              ///< 删除唯一的TCP或者负载
    SET_BIT_REG_INVALID_CRSDK_LASER_SDK = 114,         ///< 同时设置位寄存器输入与输出
    REPEAT_ID_CRSDK_LASER_SDK = 115,                   ///< id重复
    FILE_ENCRYPTION_CRSDK_LASER_SDK = 116,             ///< 文件加密
    ROBOT_MODE_ERROR_CRSDK_LASER_SDK = 117,            ///< 当前状态不能执行该操作
    MOVE_ERROR_CRSDK_LASER_SDK = 118,                  ///< 移动阻塞运动时,返回的移动状态错误
    FORCECTRL_WRONGPARA_CRSDK_LASER_SDK = 119,         /// 力控SDK参数设置错误：包括参数长度错误、payload为负数
    FORCECTRL_INVAILDCMD_CRSDK_LASER_SDK = 120,        /// 力控SDK无效的指令：包括forceEna等于1,0以外的值forcebasetype除0以外的值 forcectrltype除1,0以外  auto/mannual 1,0以外的值
    FORCECTRL_PARABLOCKED_CRSDK_LASER_SDK = 121,       /// 力控SDK freedrive模式下部分参数失效被屏蔽
    FORCECTRL_CMDBLOCKED_CRSDK_LASER_SDK = 122,        /// 力控SDK 已在力控模式下时禁止再打开力控
    FORCECTRL_CONFIGBLOCKED_CRSDK_LASER_SDK = 123,     /// 力控SDK 机器人运动时或已在力控模式下时禁止下发配置
    FORCECTRL_DISABLE_CRSDK_LASER_SDK = 124,           /// 力控SDK 机器人使能之前不能打开传感器
    FORCECTRL_WRONGSENSOR_CRSDK_LASER_SDK = 125,       /// 力控SDK 传感器型号选择错误
    OUTPUT_RULE_ERROR_CRSDK_LASER_SDK = 126,           ///< 未设置系统输出，禁止启用自复位
    ANALOG_INPUT_TYPE_ERROR_CRSDK_LASER_SDK = 127      ///< 模拟量输入类型错误：注：工具端模拟量输入类型只有电压

};

//通讯形式
enum CommuTypeLaserSdk
{
    RS485_2_LASER_SDK = 0,    //RS485
    //SIMULATE_LASER_SDK = 1    //模拟量通讯
    //ETHERCAT_LASER_SDK = 2,   //EtherCAT通讯
};

//焊接模式 点焊- 连续焊
enum WeldModeLaserSdk
{
    SPOT_LASER_SDK = 0,        //点焊模式
    CONTINUOUS_LASER_SDK = 1   //连续焊接
};

//送丝机模式设置
enum WeldFuncLaserSdk
{
    CONTINUE_LASER_SDK = 0,      //连续模式
    PULSE_LASER_SDK = 1          //脉冲模式
};

//点焊类型
enum WeldLineTypeLaserSdk
{
    TIG_WELD_LASER_SDK = 0,           //鱼鳞焊 Tungsten Inert Gas Welding || TIG焊
    INTERVAL_WELD_LASER_SDK = 1       //间断焊
};

///高低电平
enum PulseTypeLaserSdk
{
    HIGH_LASER_SDK = 0,      //高电平
    LOW_LASER_SDK = 1        //低电平
};

//开关类型
enum SwitchTypeLaserSdk
{
    SWITCH_ON_LASER_SDK = 0,   //开
    SWITCH_OFF_LASER_SDK = 1   //关
};

//激光器
typedef struct CgxiLaserPowerLaserSdk
{
    BOOL isLaserOn;             ///< 是否 出光
    BOOL isLaserAlarm;          ///< 激光器有无报警
    WeldModeLaserSdk weldMode;  ///< 焊接模式(0 点焊 - 1 连续)

    int laserPower;             ///< 激光器功率 0-5000W
    int laserPowerOpen;         ///< 开光功率 0-5000W
    int laserPowerClose;        ///< 关光功率 0-5000W
    int laserRampUpTime;        ///< 开光渐进时间 0~1000ms
    int laserRampDownTime;      ///< 关光渐进时间{关光渐进距离 0~300mm, 根据机器人移动速度换算}
    int wireFeedAdvanceTime;    ///< 送丝延时补偿 ：提前送丝。 例：设1000则按扳机后，先送丝1S之后再出光。
    int gasShutoffDelayTime;       ///< 关气渐进 滞后送气时间	0~3000ms
    int gasStartupAdvanceTime;     ///< 开气渐进 提前送气时间	0~3000ms
    int spotWeldDurationTime;      ///<点焊持续时间 0~60000ms	整型
    int spotWeldIntervalTime;      ///<点焊间隔时间 0~60000ms	整型
    int driverTemp;             ///<  驱动器温度报警阈值 0~85°
    int protectiveWindowTemp;   ///<  保护镜温度报警阈值 0~85°
    int collimatorTemp;         ///<  准直镜温度报警阈值 0~85°
    double laserHeadXOffset;    ///< X偏移 -3~+3mm	浮点型
    double laserHeadYOffset;    ///< Y偏移 -3~+3mm	浮点型
    double laserHeadXEnlarge;   ///< X放大 0.01~4	浮点型
    double laserHeadYEnlarge;   ///< Y放大 0.01~4	浮点型
    WeldLineTypeLaserSdk spotWeldType;       ///<  点焊类型 (0:间断焊 1:鱼鳞焊)
    PulseTypeLaserSdk laserAlarmType;        ///<  激光器报警类型 (0:高电平 1:低电平)
    PulseTypeLaserSdk waterCoolerAlarm;      ///<  水冷机报警 (0:高电平 1:低电平)
    PulseTypeLaserSdk gasPressureAlarm;      ///<  气压报警 (0:高电平 1:低电平)
} CgxiLaserPowerLaserSdk;

enum SpotPatternLaserSdk
{
    CIR_LASERSDK = 0,       ///< 圆形（参数：直径mm）
    HORIZON_8_LASERSDK = 1, ///< 8 字形（参数：长轴mm）
    LINE_LASERSDK = 2,      ///< 水平线（参数：长度mm）
    STAR_LASERSDK = 3,      ///< 正方形（参数：边长mm）
    TRIANGLE_LASERSDK = 4,  ///< 等边三角形（参数：边长mm）
    CROSS_LASERSDK = 5      ///< 十字形（参数：边长mm）
};

///< 激光头工艺参数
typedef struct CgxiLaserHeadLaserSdk
{
    SpotPatternLaserSdk laserSpotPattern;   ///< 光斑图案
    int laserHeadSpinAngle;         ///< 旋转角度 0~360° 整型
    int laserHeadPeakPower;         ///< 峰值功率
    int laserHeadDutyCycle;         ///< 占空比
    int laserHeadLowPulseFrequency;      ///< 脉冲频率 低位
    int laserHeadHighPulseFrequency;     ///< 脉冲频率 高位
    int laserHeadScanSpeed;      ///< 扫描速度 0~3000mm/s	整型
    double laserHeadScanWidth;   ///< 扫描宽度 0.1~5mm 	浮点型
    double laserHeadScanHeight;  ///< 扫描高度 0.1~5mm	浮点型
    SwitchTypeLaserSdk WireFeederGasSignalSwitch;      ///< 送气信号开关 激光头厂家内控地址
    SwitchTypeLaserSdk WireFeederSignalSwitch;         ///< 送丝信号开关 激光头厂家内控地址

} CgxiLaserHeadLaserSdk;

// 水冷机 尚未使用
typedef struct CgxiWaterCoolerLaserSdk
{
    BOOL isWaterCoolerStart;   ///< 是否 水冷机开机
    BOOL isWaterCoolerAlarm;   ///< 水冷机报警
} CgxiWaterCoolerLaserSdk;

///< 送丝机
typedef struct CgxiWireFeederLaserSdk
{
    BOOL isWireFeederStart;                   ///< 送丝机是否开机
    BOOL isWireFeederEnable;                  ///< 是否 使能 送丝机连接状态？
    BOOL isWireFeederDataSave;                ///< 是否保存送丝机数据
    BOOL isWireFeederManualFeedSwitch;        ///< 手动送丝 停止 | 运行
    BOOL isWireFeederManualRetractSwitch;     ///< 手动回抽 停止 | 运行
    BOOL isWireFeederDangerCurrentSave;       ///< 过流报警信号
    //WeldFuncLaserSdk wireFeedFunction;        ///< 送丝机模式设置(0	连续  1	脉冲) 0x010C R/W
    //WeldFuncLaserSdk wireCutFunction;    断丝模式 0：常规 1：高速（仅步进有效）0x010D R/W

    int wireFeederMotorSpeed;                  ///< 自动送丝速度 15~600cm/min 0x0100 R/W
    int wireFeederStartMotorSpeed;             ///< 开始自动送丝速度 15~600cm/min 0x0100 R/W
    int wireFeederStopMotorSpeed;             ///< 开始自动送丝速度 15~600cm/min 0x0100 R/W
    int wireFeederManualFeedSpeed;             ///< 手动送丝速度 15~600cm/min 0x0101 R/W
    int wireFeederManualBackSpeed;             ///< 手动回抽速度 15~600 cm/min 0x0102 R/W
    int wireFeederFeedLength;                  ///< 补丝长度
    int wireFeederBackLength;                  ///< 抽丝长度
    int wireFeederWaitTime;                    ///< 延时等待时间
    int wireFeederSmoothness;                  ///< 平滑度 25 %~80 % 0x0108 R/W
    int wireFeederPulsePeriod;                 ///< 脉冲周期

} CgxiWireFeederLaserSdk;

///< 机械臂参数
typedef struct CgxiRobotLaserSdk
{
    double emptyRunTCPSpeed;            //空走TCP速度
    double emptyRunTCPAcc;              //空走TCP加速度
    double weldTCPAcc;                  //焊接TCP加速度
    double emptyRunJointSpeed;          //空走关节速度
    double emptyRunJointAcc;            //空走关节加速度
    double emptyPointTransitionRadius;  //空走点位过渡半径
    double weldPointTransitionRadius;   //焊接点位过渡半径
} CgxiRobotLaserSdk;

/// 焊机资源 ///
typedef struct MachineResource
{
    CommuTypeLaserSdk commuType;    ///< 通讯形式
    RobotHandle robotHandle;        ///< 机器人句柄

    /* 读---------- 输入状态地址 ---------- */
    //送丝机参数
    unsigned int wireFeederOvercurrentLoc;   ///< 送丝机过流告警地址 (1:告警 2:正常) 0x0112 → ​​274​
    unsigned int wireFeederStatusLoc;        ///< 送丝机运行状态地址 (0:停止 1:运行) 0x0115 → ​​277
    //激光头参数
    unsigned int laserHeadStatusLoc;         ///< 激光头系统状态地址 (0:出光（未使能） 1:出光（未使能）)  0x2000 → ​​8192​
    unsigned int globalAlarmLoc;             ///< 总报警状态地址 0x2001 → ​​8193​
    unsigned int infAlarmLoc;                ///< 报警信息地址 0x2002 → ​​8194​
    //还没找到地址出处:
    unsigned int wireFeederConnectedLoc;     ///< 送丝机连接状态地址 (0:断开 1:已连接) 0x042e → ​​1070​
    unsigned int laserTriggerSignalLoc;      ///< 激光器扳机信号地址 (0:异常 1:正常)  0x0409 → ​​1033​
    unsigned int laserAlarmSignalLoc;        ///< 激光器报警信号地址 (0:异常 1:正常)  0x040a → ​​1034​
    unsigned int safetyLockInputLoc;         ///<  安全地锁信号地址 (0:异常 1:正常) 0x040b → ​​1035​
    unsigned int chillerAlarmInputLoc;       ///<  水冷机报警信号地址 (0:异常 1:正常) 0x040c → ​​1036​
    unsigned int airPressureAlarmLoc;        ///<  气压报警信号地址 (0:异常 1:正常) 0x040d → ​​1037​
    unsigned int wireFeedOutputLoc;          ///<  送丝输出信号地址 (0:关闭 1:送丝) 0x040e → ​​1038​
    unsigned int pwmOutputLoc;               ///<  PWM输出信号地址 (0:无输出 1:有输出) 0x040f → ​​1039​
    unsigned int laserEnableOutputLoc;       ///<  激光使能输出信号地址 (0:无输出 1:有输出) 0x0410 → ​​1040​
    unsigned int gasValveOutputLoc;          ///<  气阀输出信号地址 (0:无输出 1:有输出) 0x0411 → ​​1041​
    unsigned int dacVoltageLoc;              ///<  DAC模拟量输出电压地址 (0:无输出 1:有输出) 0x0412 → ​​1042

    /* 读|写------ 激光头 - 图案工艺参数 ------ */
    unsigned int laserHeadScanSpeedLoc;               ///<  扫描速度地址 0x0000 0
    unsigned int laserHeadScanWidthLoc;               ///<  扫描宽度地址 0x0001 1
    unsigned int laserHeadPeakPowerLoc;               ///<  峰值功率 0x0002 2
    unsigned int laserHeadDutyCycleLoc;               ///<  占空比地址 0x0003 3
    unsigned int laserHeadFrequencyHighLoc;           ///<  脉冲频率 : 31~16位地址 0x0004  4
    unsigned int laserHeadFrequencyLowLoc;            ///<  脉冲频率 : 15~0位地址  0x0005  5
    unsigned int laserHeadScanHeightLoc;              ///<  扫描高度地址 0x0006 6
    unsigned int laserHeadRotationAngleLoc;           ///<  旋转角度地址 0x0007 7
    unsigned int laserHeadPatternIdLoc;               ///<  图形切换地址 0x0030 48

    unsigned int gasValveSignalLoc;            ///<  送气信号开关地址 0x0031 → ​​49​ 激光头厂家内控地址
    unsigned int wireFeedSignalLoc;            ///<  送丝信号开关地址 0x0032 → ​​50 激光头厂家内控地址
    /* 写---------- 焊接工艺 ---------- */
    unsigned int weldingModeLoc;             ///< 焊接模式地址 0x0202  ​​514​

    /* 读|写 ------ 激光器 - 工艺参数 ------ */

    unsigned int laserPowerLoc;              ///<  激光器功率地址 0x0230 560
    unsigned int laserOnPowerLoc;            ///<  开光功率地址 0x0231 561
    unsigned int laserOnRampTimeLoc;         ///<  开光渐进时间地址 0x0232 562
    unsigned int laserOffPowerLoc;           ///<  关光功率地址 0x0233 563
    unsigned int laserOffRampTimeLoc;        ///<  关光渐进时间地址 0x0234 564
    unsigned int gasONDelayLoc;              ///<  开气延时地址 0x0235 565
    unsigned int gasOffDelayLoc;             ///<  关气延时地址 0x0236 566
    unsigned int wireOutDelayLoc;            ///<  延时送丝地址 0x0237 567

    unsigned int laserHeadYOffsetLoc;        ///<  Y轴偏移地址 0x0239 569
    unsigned int laserHeadXOffsetLoc;        ///<  X轴偏移地址 0x023b 571
    unsigned int laserHeadYScaleLoc;         ///<  Y轴放大地址 0x0238 568
    unsigned int laserHeadXScaleLoc;         ///<  X轴放大地址 0x023a 570

    unsigned int spotWeldTypeLoc;            ///<  点焊类型地址 0x0242 578
    unsigned int spotWeldDurationLoc;        ///<  焊持续时间地址 0x0243 579
    unsigned int spotWeldIntervalLoc;        ///<  点焊间隔时间地址 0x0244 580
    /* 读|写---------- 温度监控 ---------- */
    unsigned int driverTempLoc;              ///<  驱动器温度值地址 0x023f 575
    unsigned int protectiveWindowTempLoc;    ///<  保护镜温度值地址 0x0240 576
    unsigned int collimatorTempLoc;          ///<  准直镜温度值地址 0x0241 577
    /* 读|写--------- 系统报警 ---------- */
    unsigned int laserAlarmCodeLoc;            ///<  激光器报警代码地址 (0 低电平，正常 || 1 高电平，报警) 0x023c → ​​572​
    unsigned int waterCoolerAlarmCodeLoc;      ///<  水冷机报警代码地址 (0 低电平，正常 || 1 高电平，报警) 0x023d → ​​573​
    unsigned int airPressureAlarmCodeLoc;      ///<  气压报警代码地址 (0 低电平，正常 || 1 高电平，报警).0x023e → ​​574​

    /* 读|写---------- 送丝机参数 ---------- */
    unsigned int autoWireFeedSpeedLoc;       ///<  自动送丝速度地址 0x0100 256
    unsigned int manualWireFeedSpeedLoc;     ///<  手动送丝速度地址 0x0101 257
    unsigned int manualWireRetractSpeedLoc;  ///<  手动抽丝速度地址 0x0102 258
    unsigned int wireFeedLengthLoc;          ///<  补丝长度地址 0x0105 261
    unsigned int wireRetractLengthLoc;       ///<  抽丝长度地址 0x0104 260
    unsigned int manualWireFeedSwitchLoc;    ///<  手动送丝使能地址 (0	停    1	运行)  0x0140  320
    unsigned int manualWireRetractSwitchLoc; ///<  手动抽丝使能地址 (0 停    1 运行)  0x0141  321
    /*写 --- 送丝机参数--- */
    unsigned int saveCurrentWireFeederDataLoc; ///<  保存当前送丝机数据 地址 0x013F → ​​319​

    CgxiLaserHeadLaserSdk laserHeadParams;     ///< 激光头参数
    CgxiLaserPowerLaserSdk laserPowerParams;   ///< 激光器参数
    CgxiWireFeederLaserSdk wireFeederParams;   ///< 送丝机参数
    CgxiRobotLaserSdk robotParams;             ///< 机械臂参数
} MachineResource;

//工艺-材质
enum MaterialLaserSdk
{
    STAINLESS_STEEL = 0,    // 不锈钢
    ALUMINUM_ALLOY = 1,     // 铝合金
    TITANIUM_ALLOY = 2,     // 钛合金
    LOW_CARBON_STEEL = 3,   // 低碳钢
    OTHER_MATERIAL = 4      // 其他
};
//工艺参数
struct CraftInfoLaserSdk
{
    WeldModeLaserSdk craftweldMode; //焊接模式
    WeldLineTypeLaserSdk craftWeldType; //点焊类型
    int craftSpotWeldDuration;      //点焊持续时间
    int craftSpotWeldInterval;      //点焊间隔时间

    SpotPatternLaserSdk graphicsSwitching;  //图形切换
    double craftScanWidth;      //扫描宽度
    double craftScanHeight;     //扫描高度
    int craftScanSpeed;         //扫描速度
    int craftRotationAngle;     //旋转角度

    double craftYAxisZoom;     //Y轴放大
    double craftXAxisZoom;     //X轴放大
    double craftYAxisOffset;   //Y轴偏移
    double craftXAxisOffset;   //X轴偏移

    int craftPreGasTime;        //提前送气时间
    int craftPostGasTime;       //滞后送气时间
    int craftLeadInPower;       //开光功率
    int craftLeadInRampTime;    //开光渐进时间
    int craftLaserPower;        //激光器焊接功率
    double craftWeldingSpeed;   //焊接速度
    int craftLeadOutPower;      //关光功率
    int craftLeadOutRampDistance;   //关光渐进距离
    int craftRetractionDistance;    //提前回抽距离

    int craftLeadInWireFeed;            //开光送丝     送丝速度
    int craftWeldingWireFeed;           //焊接送丝     送丝速度
    int craftLeadOutWireFeed;           //关光送丝     送丝速度
    int craftEndingWireRetraction;      //收尾抽丝     回抽长度
    int craftEndLoopbackLength;         //收尾回送长度 补丝长度
};

/* ---指令列表--- */
//路点类型
enum PointTypeLaserSdk
{
    SAFE_POINT_WELD_SDK = 0,      //安全点
    LINE_POINT_WELD_SDK = 1,      //直线定点
    CIRCLE_POINT_WELD_SDK = 2,    //圆弧定点
    JOINT_POINT_WELD_SDK = 3      //关节运动
};
//路点数据
struct PointDataLaserSdk
{
    int pointID;
    PointTypeLaserSdk pointType;
    double pose[6];
    double joint[6];
};

//指令列表转换数据
struct LaserWeldStartData
{
    int *instList;                               ///< { 指令枚举，在对应数据列表中的地址 } 数对列表
    int instNum;                                 ///< 指令数对列表中数对对数
    CraftInfoLaserSdk *craftInfoList;            ///< 工艺信息列表
    int craftNum;                                ///< 工艺信息列表长度
    PointDataLaserSdk *pointList;                ///< 路点数据列表
    int pointsNum;                               ///< 路点数据列表长度
    int *signalStepPointLocList; ///< 单步运行路点位置列表
    int signalStepPointNum;      ///< 单步运行路点位置列表长度
    int *breakPointLocList; ///< 断点位置列表
    int breakPointNum;      ///< 断点位置列表长度
};

//指令类型
enum InstTypeLaserSdk
{
    CRAFT_SELECT_INST_LASER_WELD_SDK = 0,      // 工艺模板指令
    ARC_START_INST_LASER_WELD_SDK = 1,         // 起弧指令
    ARC_END_INST_LASER_WELD_SDK = 2,           // 收弧指令
    LINE_POINT_INST_LASER_WELD_SDK = 3,        // 直线定点指令
    CIRCLE_POINT_INST_LASER_WELD_SDK = 4,      // 圆弧定点指令
    JOINT_POINT_INST_LASER_WELD_SDK = 5,       // 关节运动指令
    SAFE_POINT_MOVEL_INST_LASER_WELD_SDK = 6,  // 安全点指令 MoveL
    SAFE_POINT_MOVEJ_INST_LASER_WELD_SDK = 7  // 安全点指令 MoveJ
};

/// 焊接控制寄存器位///
enum WeldControlAddressLaserSdk
{
    // BoolReg Int16Reg

    BOOL_REG_TERMINAL_BUTTON_610 = 610,                     ///< 末端按钮拖动状态控制位：1 有效 0 无效，需要重置为1
    BOOL_REG_EXTSTRUCT_611 = 611,                           ///< 焊接触发信号
    BOOL_REG_EXTSTRUCT_612 = 612,                           ///< 安全点触发信号
    BOOL_REG_WELD_STATUS_MONITOR_613 = 613,                 ///< 焊接中断检测开启状态位：1 有效 0 无效
    BOOL_REG_PROGRAM_STOP_ARC_END_614 = 614,                ///< 程序停止后自动收弧开启状态位：1 有效 0 无效

    BOOL_REG_SINGLE_STEP_RUN_616 = 616, ///< 单步运行状态位： 1 ： 有效 | 0 ： 无效
    BOOL_REG_RUN_BREAK_POINT_617 = 617, ///< 运行中断点状态位：1 触发 | 0 未触发

    INT_REG_ARC_START_FAIL_673 = 673,   ///< 起弧异常信号
    INT_REG_WELD_INTERRUPT_674 = 674,   ///< 焊接中断信号
    INT_REG_ARC_END_FAIL_675 = 675,     ///< 收弧异常信号

    INT_REG_WELD_MODE_660 = 660,        ///< 执行模式：0.空状态 1.轨迹模拟 2.焊接
    INT_REG_WELD_STATE_661 = 799,       ///< 非0表示上一个已经到达的路点在指令列表中的位置,为0表示不在起收弧中间

    INT_REG_WELD_PATH_662 = 662,         ///< 焊接路径分段信息
    INT_REG_ARRIVE_POINT_LOC_663 = 663,  ///< 轨迹模拟下已到达的定点指令位置 用于高亮显示

    INT_REG_PRESENT_CRAFT_LOC_664 = 664, ///< 当前工艺指令在指令列表中的位置

    // 激光通讯的交互
    INT_REG_WRITE_PARAMETER_62 = 62,
    INT_REG_WRITE_PARAMETER_63 = 63,
    INT_REG_WRITE_PARAMETER_64 = 64,
    INT_REG_WRITE_PARAMETER_65 = 65,

    INT_REG_READ_PARAMETER_66 = 66,
    INT_REG_READ_PARAMETER_67 = 67,
    INT_REG_READ_PARAMETER_68 = 68,
    INT_REG_READ_PARAMETER_69 = 69,

    INT_REG_RESULT_77 = 77
};

//// WeldSdk XXX ////
/*
 //计算俯仰角
struct PitchAngleData
{
    double waypoint1[3];                ///<焊缝路径上的第一个点
    double waypoint2[3];                ///<焊缝路径上的第二个点
    double planePoint1[3];              ///<焊缝其中一个面上的点
    double planePoint2[3];              ///<焊缝另一个面上的点

    double pitchAngleOut;               ///<俯仰角角度
    double angleBisectLineOut[3];       ///<角平分线单位向量
    double planeUnitVector1[3];         //面上垂直焊缝的单位向量1
    double planeUnitVector2[3];         //另一个面上垂直焊缝的单位向量2
};

enum AdvanceAngleType
{
    USE_TEACH_POSE = 0,           //起点和终点的前进角按照示教姿态确定
    USE_START_POINT_TEACH_POSE = 1, //起点和终点的前进角按照起点的示教姿态确定
    USE_INPUT_POSE = 2,            //起点和终点的前进角由输入参数指定

};
*/

//计算回退点 - 圆弧
struct RollbackCircleData
{
    double startPoint[6];           ///<起点 基于基坐标系下固定路点的位姿
    double transPoint[6];           ///<过度点 基于基坐标系下固定路点的位姿
    double endPoint[6];             ///<终点 基于基坐标系下固定路点的位姿
    double stopPoint[6];            ///<停止点
    double distance;            ///<延迟距离

    double stopMappingPoint[6];     ///<停止点的映射点 - 出参
    double startPointOut[6];        ///<起点 基于基坐标系下固定路点的位姿 - 出参
    double transPointOut[6];        ///<过度点 基于基坐标系下固定路点的位姿 - 出参
    double endPointOut[6];          ///<终点 基于基坐标系下固定路点的位姿 - 出参
};

//计算回退点 - 圆弧
struct RollbackStraightLineData
{
    double startPoint[6];           ///<起点 基于基坐标系下固定路点的位姿
    double endPoint[6];             ///<终点 基于基坐标系下固定路点的位姿
    double distance;                ///<延迟距离
    double pointOut[6];        ///<降功率点 - 出参
};


#endif // CL_BASEDEFINE_H
