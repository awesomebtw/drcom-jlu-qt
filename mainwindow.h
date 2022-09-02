#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDialog>
#include <QMenuBar>
#include <QSettings>
#include <QRegularExpressionValidator>
#include <QSystemTrayIcon>
#include <QTimer>
#include <memory>

#include "dogcomcontroller.h"
#include "ui_mainwindow.h"

namespace Ui
{
    class MainWindow;
}

enum class State {
    OFFLINE,
    LOGGING,
    ONLINE,
};

class MainWindow : public QDialog {
Q_OBJECT

public:
    explicit MainWindow(QApplication *parentApp = nullptr, QWidget *parent = nullptr);

    void closeEvent(QCloseEvent *) override;

private slots:

    void on_checkBoxAutoLogin_toggled(bool checked);

    void on_checkBoxRemember_toggled(bool checked);

    void on_comboBoxMAC_currentTextChanged(const QString &);

    void LoginButtonClicked();

    void IconActivated(QSystemTrayIcon::ActivationReason reason);

    void UserLogOut();

    void on_checkBoxNotShowWelcome_toggled(bool checked);

    void on_checkBoxHideLoginWindow_toggled(bool checked);

private:
    void HandleOfflineUserLogout(const QString &string) const;

    void HandleOfflineChallengeFailed(const QString &title);

    void HandleOfflineTimeout(const QString &string);

public slots:

    void HandleOffline(LoginResult reason);

    void HandleLoggedIn();

    void HandleIpAddress(const QString &ip);

    void ShowLoginWindow();

    void RestartDrcom();

    void QuitDrcom();

    void RestartDrcomByUser();

private:
    std::unique_ptr<Ui::MainWindow> ui;
    QApplication *app = nullptr;
    const QString CUSTOM_MAC = tr("custom (format: 1A:2B:3C:4D:5E:6F case insensitive)");
    const QString APP_NAME = tr("DrCOM JLU Qt version");
    QSettings s;

    // 用于确保调用socket的析构函数，释放资源
    bool quit = false;
    bool restart = false;

    // 用于在未登录时关闭窗口就退出
    State currState;

    QRegularExpressionValidator macValidator;
    DogcomController dogcomController{};

    // 设置托盘中的注销按钮的可用性
    void DisableLogOutTrayContextMenu(bool yes);

    // uptime
    QTimer upTimer;
    QElapsedTimer upElapsedTimer;

    void UpdateTimer();

    void AboutDrcom();

    // 托盘图标
    std::unique_ptr<QAction> restartAction;
    std::unique_ptr<QAction> restoreAction;
    std::unique_ptr<QAction> logOutAction;
    std::unique_ptr<QAction> quitAction;
    std::unique_ptr<QSystemTrayIcon> trayIcon;
    std::unique_ptr<QMenu> trayIconMenu;

    QIcon offlineIcon, onlineIcon;

    void CreateActions();

    void CreateTrayIcon();

    void SetIcon(bool online);

    void WriteInputs();

    static Qt::CheckState BooleanToCheckState(bool val);

    static QByteArray Encrypt(QByteArray arr);

    static QByteArray Decrypt(QByteArray arr);

    void LoadSettings();

    void SaveSettings();

    void SetMAC(const QString &m);

    void SetDisableInput(bool yes);
};

#endif // MAINWINDOW_H
