﻿#ifndef DRCOMJLUQT_LOGINRESULT_H
#define DRCOMJLUQT_LOGINRESULT_H

#include <QString>

enum class LoginResult {
    NOT_AN_ERROR,

    // 离线原因
    UNKNOWN,
    USER_LOGOUT,
    CHALLENGE_FAILED,
    CHECK_MAC,
    SERVER_BUSY,
    WRONG_PASS,
    NOT_ENOUGH,
    FREEZE_UP,
    NOT_ON_THIS_IP,
    NOT_ON_THIS_MAC,
    TOO_MUCH_IP,
    UPDATE_CLIENT,
    NOT_ON_THIS_IP_MAC,
    MUST_USE_DHCP,
    TIMEOUT,

    // 初始化失败的原因
    WSA_STARTUP,
    CREATE_SOCKET,
    BIND_FAILED,
    SET_SOCKET_TIMEOUT,
    SET_SOCKET_REUSE,
};

namespace LoginResultUtil
{
    QString ToQString(LoginResult r);
}

#endif //DRCOMJLUQT_LOGINRESULT_H
