﻿#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <qstring.h>

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

const QString SETTINGS_FILE_NAME = "DrComJluQt.ini";
const QString ID_ACCOUNT = "account";
const QString ID_PASSWORD = "password";
const QString ID_MAC = "mac";
const QString ID_REMEMBER = "remember";
const QString ID_AUTO_LOGIN = "autoLogin";
const QString ID_HIDE_WINDOW = "showWindow";
const QString ID_NOT_SHOW_WELCOME = "dontShowWelcome";
const QString ID_RESTART_TIMES = "restartTimes";
const QString ID_MAIN_WINDOW_GEOMETRY = "mainWindowGeometry";
constexpr const int RETRY_TIMES = 3;

#endif // CONSTANTS_H
