//
// Created by btw on 2022/9/5.
//

#include <QObject>
#include "loginresult.h"

QString LoginResultUtil::ToQString(LoginResult r)
{
    switch (r) {
        case LoginResult::NOT_AN_ERROR:
        case LoginResult::USER_LOGOUT:
            return {};
        case LoginResult::BIND_FAILED:
            return QObject::tr("Binding port failed. Please check if there are other clients occupying the port.");
        case LoginResult::CHALLENGE_FAILED:
            return QObject::tr("Challenge failed.");
        case LoginResult::CHECK_MAC:
            return QObject::tr("Someone else is using this account, which cannot be used simultaneously.");
        case LoginResult::SERVER_BUSY:
            return QObject::tr("The server is busy. Please try again later.");
        case LoginResult::WRONG_PASS:
            return QObject::tr("Account and password does not match.");
        case LoginResult::NOT_ENOUGH:
            return QObject::tr("The cumulative time or traffic for this account has exceeded the limit.");
        case LoginResult::FREEZE_UP:
            return QObject::tr("This account is suspended.");
        case LoginResult::NOT_ON_THIS_IP:
            return QObject::tr("IP address does not match.") + " " +
                   QObject::tr("This account can only be logged in on computer(s) with specified IP and MAC addresses.");
        case LoginResult::NOT_ON_THIS_MAC:
            return QObject::tr("MAC address does not match.") + " " +
                   QObject::tr("This account can only be logged in on computer(s) with specified IP and MAC addresses.");
        case LoginResult::TOO_MUCH_IP:
            return QObject::tr("This account has too many IP addresses.");
        case LoginResult::UPDATE_CLIENT:
            return QObject::tr("The client version is too old and needs an update.");
        case LoginResult::NOT_ON_THIS_IP_MAC:
            return QObject::tr("This account can only be logged in on computer(s) with specified IP and MAC addresses.");
        case LoginResult::MUST_USE_DHCP:
            return QObject::tr("Your computer has a static IP address. Use DHCP instead and try again.");
        case LoginResult::TIMEOUT:
            return QObject::tr("Connection timed out.");
        case LoginResult::WSA_STARTUP:
            return QObject::tr("Initialize Windows socket failed.");
        case LoginResult::CREATE_SOCKET:
        case LoginResult::SET_SOCKET_TIMEOUT:
        case LoginResult::SET_SOCKET_REUSE:
            return QObject::tr("Set up socket failed.");
        case LoginResult::UNKNOWN:
        default:
            return QObject::tr("Unknown error.");
    }
}
