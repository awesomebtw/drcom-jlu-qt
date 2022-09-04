//
// Created by btw on 2022/9/4.
//

#ifndef DRCOMJLUQT_SETTINGS_H
#define DRCOMJLUQT_SETTINGS_H

#include <QSettings>
#include <QString>

class DrcomUserSettings {
private:
    QSettings s;
    DrcomUserSettings();

public:
    static DrcomUserSettings &Instance();

    QString Account() const;

    void SetAccount(const QString &account);

    QByteArray RawPassword() const;

    void SetRawPassword(const QByteArray &password);

    QString Mac() const;

    void SetMac(const QString &mac);

    bool RememberCredential() const;

    void SetRememberCredential(bool r);

    bool AutoLogin() const;

    void SetAutoLogin(bool a);

    bool HideWindow() const;

    void SetHideWindow(bool h);

    bool DisableWelcomePage() const;

    void SetDisableWelcomePage(bool d);

    uint SelfRestartCount() const;

    void IncreaseSelfRestartCount();

    void ResetSelfRestartCount();

    QByteArray MainWindowGeometry() const;

    void SetMainWindowGeometry(const QByteArray &);
};

#endif //DRCOMJLUQT_SETTINGS_H
