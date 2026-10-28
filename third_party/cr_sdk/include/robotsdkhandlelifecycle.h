#ifndef ROBOTSDKHANDLELIFECYCLE_H
#define ROBOTSDKHANDLELIFECYCLE_H

/**
 * @file robotsdkhandlelifecycle.h
 * @brief 机器人 SDK 句柄创建和销毁的统一安全入口。
 */

#include "robotapi.h"
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>

namespace RobotSdkHandleLifecycle
{
/** @return SDK 返回码表示成功时返回 true。 */
inline bool IsSuccess(CRresult result)
{
    return result == CRresult::success || result == CRresult::sucess;
}

/** @return 进程内创建和销毁句柄共用的互斥锁。 */
inline QMutex& Mutex()
{
    static QMutex mutex;
    return mutex;
}

/**
 * @brief 调用 SDK 前检查句柄是否有效。
 * @return 句柄有效时返回 true，否则返回 false。
 */
inline bool ValidateForSdkCall(RobotHandle& robotHandle,
                               const char*,
                               const char*,
                               const char*)
{
    return robotHandle != -1;
}

/**
 * @brief 创建机器人 SDK 句柄。
 * @param[out] robotHandle 成功时接收新句柄，失败时重置为 -1。
 * @param[in] ipAddress 机器人 IP 地址。
 * @param[in] port 机器人 SDK 端口。
 * @param[in] password 机器人连接密码。
 * @param[in] owner 保留的句柄所有者参数。
 * @param[in] caller 保留的调用函数参数。
 * @return cr_create_robot() 的原始返回码。
 */
inline CRresult Create(RobotHandle& robotHandle,
                       const char* ipAddress,
                       int port,
                       const char* password,
                       const char* owner,
                       const char* caller)
{
    Q_UNUSED(owner);
    Q_UNUSED(caller);

    QMutexLocker locker(&Mutex());
    if(robotHandle != -1)
    {
        return CRresult::handle_repeat;
    }

    RobotHandle createdHandle = -1;
    const CRresult result = cr_create_robot(&createdHandle,
                                            ipAddress,
                                            port,
                                            password);
    robotHandle = IsSuccess(result) ? createdHandle : -1;
    return result;
}

/**
 * @brief 先使共享句柄失效，再销毁局部副本。
 * @param[in,out] robotHandle 待销毁句柄；调用开始后立即重置为 -1。
 * @param[in] owner 保留的句柄所有者参数。
 * @param[in] caller 保留的调用函数参数。
 * @return 有效句柄返回 cr_destroy_robot() 的结果；无效句柄返回 success。
 */
inline CRresult Destroy(RobotHandle& robotHandle,
                        const char* owner,
                        const char* caller)
{
    Q_UNUSED(owner);
    Q_UNUSED(caller);

    QMutexLocker locker(&Mutex());
    const RobotHandle handleToDestroy = robotHandle;
    robotHandle = -1;
    if(handleToDestroy == -1)
    {
        return CRresult::success;
    }

    return cr_destroy_robot(handleToDestroy);
}
}

#endif // ROBOTSDKHANDLELIFECYCLE_H
