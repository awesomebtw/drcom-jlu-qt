//
// Created by btw on 2022/9/4.
//

#include <QString>
#include "settings.h"

const QString SETTINGS_FILE_NAME = "DrComJluQt.ini";
const QString ID_ACCOUNT = "account";
const QString ID_PASSWORD = "password";
const QString ID_MAC = "mac";
const QString ID_REMEMBER = "remember";
const QString ID_AUTO_LOGIN = "autoLogin";
const QString ID_HIDE_WINDOW = "hideWindow";
const QString ID_NOT_SHOW_WELCOME = "dontShowWelcome";
const QString ID_RESTART_TIMES = "restartTimes";
const QString ID_MAIN_WINDOW_GEOMETRY = "mainWindowGeometry";

const QByteArray emptyByteArray;

DrcomUserSettings::DrcomUserSettings() : s(SETTINGS_FILE_NAME, QSettings::IniFormat, this) {}

DrcomUserSettings &DrcomUserSettings::Instance()
{
    static DrcomUserSettings s;
    return s;
}

QString DrcomUserSettings::Account() const
{
    return s.value(ID_ACCOUNT, "").toString();
}

void DrcomUserSettings::SetAccount(const QString &account)
{
    s.setValue(ID_ACCOUNT, account);
}

QByteArray DrcomUserSettings::RawPassword() const
{
    return s.value(ID_PASSWORD, emptyByteArray).toByteArray();
}

void DrcomUserSettings::SetRawPassword(const QByteArray &password)
{
    s.setValue(ID_PASSWORD, password);
}

QString DrcomUserSettings::Mac() const
{
    return s.value(ID_MAC, "").toString();
}

void DrcomUserSettings::SetMac(const QString &mac)
{
    s.setValue(ID_MAC, mac);
}

bool DrcomUserSettings::RememberCredential() const
{
    return s.value(ID_REMEMBER, false).toBool();
}

void DrcomUserSettings::SetRememberCredential(bool r)
{
    s.setValue(ID_REMEMBER, r);
}

bool DrcomUserSettings::AutoLogin() const
{
    return s.value(ID_AUTO_LOGIN, false).toBool();
}

void DrcomUserSettings::SetAutoLogin(bool a)
{
    s.setValue(ID_AUTO_LOGIN, a);
}

bool DrcomUserSettings::HideWindow() const
{
    return s.value(ID_HIDE_WINDOW, false).toBool();
}

void DrcomUserSettings::SetHideWindow(bool h)
{
    s.setValue(ID_HIDE_WINDOW, h);
}

bool DrcomUserSettings::DisableWelcomePage() const
{
    return s.value(ID_NOT_SHOW_WELCOME, false).toBool();
}

void DrcomUserSettings::SetDisableWelcomePage(bool d)
{
    s.setValue(ID_NOT_SHOW_WELCOME, d);
}

uint DrcomUserSettings::SelfRestartCount() const
{
    return s.value(ID_RESTART_TIMES, 0).toUInt();
}

void DrcomUserSettings::IncreaseSelfRestartCount()
{
    s.setValue(ID_RESTART_TIMES, SelfRestartCount() + 1);
}

void DrcomUserSettings::ResetSelfRestartCount()
{
    s.setValue(ID_RESTART_TIMES, 0);
}

QByteArray DrcomUserSettings::MainWindowGeometry() const
{
    return s.value(ID_MAIN_WINDOW_GEOMETRY, emptyByteArray).toByteArray();
}

void DrcomUserSettings::SetMainWindowGeometry(const QByteArray &geometry)
{
    s.setValue(ID_MAIN_WINDOW_GEOMETRY, geometry);
}
