#ifndef DRCOMJLUQT_MAINWINDOW_H
#define DRCOMJLUQT_MAINWINDOW_H

#include <QDialog>
#include <QMenuBar>
#include <QSettings>
#include <QRegularExpressionValidator>
#include <QSystemTrayIcon>
#include <QTimer>
#include <memory>

#include "dogcomcontroller.h"
#include "ui_mainwindow.h"

enum class State {
    OFFLINE,
    LOGGING,
    ONLINE,

    // 用于确保调用 socket 的析构函数，释放资源
    ABOUT_TO_QUIT,
    ABOUT_TO_RESTART,
};

class MainWindow : public QDialog {
Q_OBJECT

public:
    explicit MainWindow(QApplication *parentApp = nullptr, QWidget *parent = nullptr);

    void closeEvent(QCloseEvent *) override;

    void showEvent(QShowEvent *e) override;

private slots:

    void LoginButtonClicked();

    void IconActivated(QSystemTrayIcon::ActivationReason reason);

    static void BrowserButtonClicked();

    void UserLogOut();

    void on_checkBoxAutoLogin_toggled(bool checked);

    void on_checkBoxRemember_toggled(bool checked);

    void on_checkBoxNotShowWelcome_toggled(bool checked);

    void on_checkBoxHideLoginWindow_toggled(bool checked);

private:
    void HandleOfflineUserLogout() const;

    void HandleOfflineChallengeFailed(const QString &title);

    void HandleOfflineTimeout();

public slots:

    void HandleOffline(LoginResult reason);

    void HandleLoggedIn();

    void HandleIpAddress(const QString &ip);

    void ShowLoginWindow();

    void RestartDrcom();

    void QuitDrcom();

    void RestartDrcomByUserWithConfirm();

    void RestartDrcomByUser();

private:
    std::unique_ptr<Ui::MainWindow> ui;
    QApplication *app = nullptr;

    // 用于在未登录时关闭窗口就退出
    State currState;

    QRegularExpressionValidator macValidator;
    DogcomController dogcomController{};

    // uptime
    QTimer upTimer;
    size_t uptimeCounter{};

    void UpdateTimer();

    static void AboutDrcom();

    // 托盘图标
    std::unique_ptr<QAction> restartAction;
    std::unique_ptr<QAction> restoreAction;
    std::unique_ptr<QAction> logOutAction;
    std::unique_ptr<QAction> quitAction;
    std::unique_ptr<QSystemTrayIcon> trayIcon;
    std::unique_ptr<QMenu> trayIconMenu;

    QIcon offlineIcon, onlineIcon;

    void SetIcon(bool online);

    void WriteInputs();

    void LoadSettings();

    void SetMAC(const QString &m);

    void SetDisableInput(bool yes);
};

#endif // DRCOMJLUQT_MAINWINDOW_H
