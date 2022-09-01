#ifndef CONSTANTS_H
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

enum {
    // challenge 成功 获取到服务器返回的ip地址
    OBTAIN_IP_ADDRESS,
};

enum LoginErrorCode {
    LOGIN_CHECK_MAC = 0x01,
    LOGIN_SERVER_BUSY = 0x02,
    LOGIN_WRONG_PASS = 0x03,
    LOGIN_NOT_ENOUGH = 0x04,
    LOGIN_FREEZE_UP = 0x05,
    LOGIN_NOT_ON_THIS_IP = 0x07,
    LOGIN_NOT_ON_THIS_MAC = 0x0B,
    LOGIN_TOO_MUCH_IP = 0x14,
    LOGIN_UPDATE_CLIENT = 0x15,
    LOGIN_NOT_ON_THIS_IP_MAC = 0x16,
    LOGIN_MUST_USE_DHCP = 0x17
};

constexpr const int PORT_BIND = 61440;
constexpr const int PORT_DEST = 61440;
constexpr const char SERVER_IP[] = "10.100.61.3";
const QString SETTINGS_FILE_NAME = "DrCOM_JLU_Qt.ini";
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
