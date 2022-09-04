#include <QtNetwork/QNetworkInterface>
#include <QApplication>
#include <QTimer>
#include <QMenuBar>
#include <QMessageBox>
#include <QValidator>
#include <QWindow>
#include <QDesktopServices>
#include <QUrl>
#include <QCloseEvent>
#include <QProcess>
#include <chrono>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "constants.h"
#include "settings.h"
#include "utils.h"

constexpr const int RETRY_TIMES = 3;

enum {
    NOT_LOGGED_IN_PAGE_INDEX = 0,
    LOGGED_IN_PAGE_INDEX = 1,
};

MainWindow::MainWindow(QApplication *parentApp, QWidget *parent) :
        QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint),
        ui(new Ui::MainWindow),
        app(parentApp),
        macValidator(QRegularExpression("^[0-9a-fA-F]{2}([:-][0-9a-fA-F]{2}){5}"), this),
        // 创建托盘菜单和图标
        trayIcon(new QSystemTrayIcon(this)),
        trayIconMenu(new QMenu(this)),
        restoreAction(new QAction(tr("&Restore"), this)),
        logOutAction(new QAction(tr("&Logout"), this)),
        restartAction(new QAction(tr("Re&start"), this)),
        quitAction(new QAction(tr("&Quit"), this)),
        onlineIcon(":/images/online.png"),
        offlineIcon(":/images/offline.png"),
        currState(State::OFFLINE),
        upTimer(this)
{
    // 关机时接收退出信号，释放 socket
    QObject::connect(app, &QApplication::aboutToQuit, this, &MainWindow::QuitDrcom);

    qDebug() << "MainWindow constructor";
    ui->setupUi(this);

    // 记住窗口大小功能
    restoreGeometry(DrcomUserSettings::Instance().MainWindowGeometry());

    // put mac addresses to combobox
    for (const QNetworkInterface &i: QNetworkInterface::allInterfaces()) {
        if (!i.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            ui->comboBoxMAC->addItem(i.hardwareAddress() + " [" + i.humanReadableName() + ']', i.hardwareAddress());
        }
    }

    // set validator for mac address
    ui->comboBoxMAC->lineEdit()->setValidator(&macValidator);

    // connect to controller's login logout signals
    connect(&dogcomController, &DogcomController::HaveBeenOffline, this, &MainWindow::HandleOffline);
    connect(&dogcomController, &DogcomController::HaveLoggedIn, this, &MainWindow::HandleLoggedIn);
    connect(&dogcomController, &DogcomController::HaveObtainedIp, this, &MainWindow::HandleIpAddress);

    // all buttons' functionality
    connect(ui->restartPushButton, &QPushButton::clicked, this, &MainWindow::RestartDrcomByUserWithConfirm);
    connect(ui->loginButton, &QCommandLinkButton::clicked, this, &MainWindow::LoginButtonClicked);
    connect(ui->logoutButton, &QCommandLinkButton::clicked, this, &MainWindow::UserLogOut);
    connect(ui->browserButton, &QCommandLinkButton::clicked, this, &MainWindow::BrowserButtonClicked);
    connect(ui->aboutPushButton, &QPushButton::clicked, this, &MainWindow::AboutDrcom);

    // 托盘菜单选项
    connect(restoreAction.get(), &QAction::triggered, this, &MainWindow::ShowLoginWindow);
    connect(logOutAction.get(), &QAction::triggered, this, &MainWindow::UserLogOut);
    connect(restartAction.get(), &QAction::triggered, this, &MainWindow::RestartDrcomByUserWithConfirm);
    connect(quitAction.get(), &QAction::triggered, this, &MainWindow::QuitDrcom);

    // 设置托盘菜单响应函数
    connect(trayIcon.get(), &QSystemTrayIcon::activated, this, &MainWindow::IconActivated);

    // 新建菜单
    trayIconMenu->addAction(restoreAction.get());
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(logOutAction.get());
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(restartAction.get());
    trayIconMenu->addAction(quitAction.get());

    // 新建托盘图标
    trayIcon->setContextMenu(trayIconMenu.get());

    // 设置托盘图标和窗口图标
    SetIcon(false);
    trayIcon->show();

    // hardcode proxy to focus to the right widget
    ui->notLoggedInPage->setFocusProxy(ui->lineEditAccount);
    ui->loggedInPage->setFocusProxy(ui->browserButton);
    ui->centralStackedWidget->setCurrentIndex(NOT_LOGGED_IN_PAGE_INDEX);

    // timer
    upTimer.setTimerType(Qt::TimerType::VeryCoarseTimer);
    connect(&upTimer, &QTimer::timeout, this, &MainWindow::UpdateTimer);

    LoadSettings(); // 读取配置文件
    logOutAction->setEnabled(false); // 尚未登录不可注销

    // 自动登录功能
    if (DrcomUserSettings::Instance().SelfRestartCount() > 0 || DrcomUserSettings::Instance().AutoLogin()) {
        emit ui->loginButton->click();
    }
}

void MainWindow::closeEvent(QCloseEvent *)
{
    DrcomUserSettings::Instance().SetMainWindowGeometry(saveGeometry());
    // 未登录时直接关闭窗口就退出
    if (currState == State::OFFLINE) {
        QuitDrcom();
    }
}

void MainWindow::showEvent(QShowEvent *e)
{
    if (focusWidget()) {
        focusWidget()->clearFocus();
    }
    // ensure current focus widget is not "about" button
    ui->centralStackedWidget->currentWidget()->setFocus();

    QDialog::showEvent(e);
}

void MainWindow::ShowLoginWindow()
{
    if (!isVisible()) {
        showNormal(); // 若登录窗口没显示则显示出来
    } else {
        activateWindow(); // 已显示则将窗口设为焦点
    }
}

void MainWindow::RestartDrcom()
{
    switch (currState) {
        case State::ONLINE:
            dogcomController.LogOut(); // restart after logout
        case State::OFFLINE:
            qDebug() << "Restarting Drcom...";
            QProcess::startDetached(qApp->applicationFilePath(), qApp->arguments());
            currState = State::ABOUT_TO_RESTART;
            qApp->quit();
            qDebug() << "Restart done.";
            break;
        default:
            break;
    } // 正在登录时候退出，假装没看到，不理
}

void MainWindow::QuitDrcom()
{
    // 退出之前恢复重试计数
    DrcomUserSettings::Instance().ResetSelfRestartCount();
    qDebug() << "reset restartTimes QuitDrcom";

    switch (currState) {
        case State::ONLINE:
            dogcomController.LogOut(); // quit after logout
        case State::OFFLINE:
            currState = State::ABOUT_TO_QUIT;
            qApp->quit();
            break;
        default:
            break;
    }
    // 正在登录时候退出，假装没看到，不理
    // qApp->quit() 调用放到了注销响应那块
}

void MainWindow::RestartDrcomByUserWithConfirm()
{
    if (QMessageBox::question(this, "", tr("Are you sure you want to restart?")) == QMessageBox::StandardButton::Yes) {
        RestartDrcomByUser();
    }
}

void MainWindow::RestartDrcomByUser()
{
    // 重启之前恢复重试计数
    DrcomUserSettings::Instance().ResetSelfRestartCount();
    qDebug() << "reset restartTimes";
    RestartDrcom();
}

void MainWindow::IconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        restoreAction->activate(restoreAction->Trigger);
    }
}

void MainWindow::LoadSettings()
{
    auto account = DrcomUserSettings::Instance().Account();
    auto password = Utils::Decrypt(DrcomUserSettings::Instance().RawPassword());
    auto macAddr = DrcomUserSettings::Instance().Mac();
    auto remember = DrcomUserSettings::Instance().RememberCredential();
    auto hideWindow = DrcomUserSettings::Instance().HideWindow();
    auto notShowWelcome = DrcomUserSettings::Instance().DisableWelcomePage();
    auto autoLogin = DrcomUserSettings::Instance().AutoLogin();

    ui->lineEditAccount->setText(account);
    ui->lineEditPass->setText(password);
    SetMAC(macAddr);
    ui->loginButton->setText(tr("Login"));

    ui->checkBoxRemember->setCheckState(Utils::BooleanToCheckState(remember));
    ui->checkBoxAutoLogin->setCheckState(Utils::BooleanToCheckState(autoLogin));
    ui->checkBoxHideLoginWindow->setCheckState(Utils::BooleanToCheckState(hideWindow));
    ui->checkBoxNotShowWelcome->setCheckState(Utils::BooleanToCheckState(notShowWelcome));
}

void MainWindow::SaveSettings()
{
//    s.sync();
}

void MainWindow::SetMAC(const QString &m)
{
    for (int i = 0; i < ui->comboBoxMAC->count(); i++) {
        auto str = ui->comboBoxMAC->itemData(i);
        if (str.toString().compare(m) == 0 || (str.isNull() && ui->comboBoxMAC->itemText(i).compare(m) == 0)) {
            //当前列表中有该mac地址
            ui->comboBoxMAC->setCurrentIndex(i);
            return;
        }
    }
    // 当前列表中没有该mac地址，填充到输入框中
    ui->comboBoxMAC->addItem(m, m);
    ui->comboBoxMAC->setCurrentText(m);
}

void MainWindow::on_checkBoxAutoLogin_toggled(bool checked)
{
    if (checked) {
        ui->checkBoxRemember->setChecked(true);
    }
}

void MainWindow::on_checkBoxRemember_toggled(bool checked)
{
    if (!checked) {
        ui->checkBoxAutoLogin->setChecked(false);
    }
}

void MainWindow::LoginButtonClicked()
{
    WriteInputs();

    auto macAddr = DrcomUserSettings::Instance().Mac();
    auto account = DrcomUserSettings::Instance().Account();
    auto password = Utils::Decrypt(DrcomUserSettings::Instance().RawPassword());

    if (account.isEmpty() || password.isEmpty() || macAddr.isEmpty()) {
        QMessageBox::warning(this, "", tr("Fields cannot be empty!"));
        return;
    }
    if (macAddr.length() != 17) {
        QMessageBox::warning(this, "", tr("Illegal MAC address!"));
        return;
    }

    // 输入无误，执行登录操作
    // 先禁用输入框和按钮
    SetDisableInput(true);
    currState = State::LOGGING;
    dogcomController.Login(account, QString(password), macAddr);
}

void MainWindow::SetDisableInput(bool yes)
{
    ui->lineEditAccount->setDisabled(yes);
    ui->lineEditPass->setDisabled(yes);
    ui->comboBoxMAC->setDisabled(yes);
    ui->checkBoxRemember->setDisabled(yes);
    ui->checkBoxAutoLogin->setDisabled(yes);
    ui->checkBoxNotShowWelcome->setDisabled(yes);
    ui->checkBoxHideLoginWindow->setDisabled(yes);
    ui->loginButton->setDisabled(yes);
}

void MainWindow::SetIcon(bool online)
{
    if (online) {
        // 设置彩色图标
        trayIcon->setToolTip(QApplication::applicationDisplayName().append(" - ").append(tr("online")));
        trayIcon->setIcon(onlineIcon);
        qApp->setWindowIcon(onlineIcon);
    } else {
        // 设置灰色图标
        trayIcon->setToolTip(QApplication::applicationDisplayName().append(" - ").append(tr("offline")));
        trayIcon->setIcon(offlineIcon);
        qApp->setWindowIcon(offlineIcon);
    }
}

void MainWindow::WriteInputs()
{
    DrcomUserSettings &s = DrcomUserSettings::Instance();
    s.SetAccount(ui->lineEditAccount->text());
    s.SetRawPassword(
            Utils::Encrypt(ui->lineEditPass->text().toLatin1())); // since all characters in a password are ascii
    if (ui->comboBoxMAC->currentData().isNull()) {
        s.SetMac(ui->comboBoxMAC->currentText().toUpper());
    } else {
        s.SetMac(ui->comboBoxMAC->currentData().toString().toUpper());
    }
    s.SetRememberCredential(Utils::CheckStateToBoolean(ui->checkBoxRemember->checkState()));
    s.SetAutoLogin(Utils::CheckStateToBoolean(ui->checkBoxAutoLogin->checkState()));
    s.SetDisableWelcomePage(Utils::CheckStateToBoolean(ui->checkBoxNotShowWelcome->checkState()));
    s.SetHideWindow(Utils::CheckStateToBoolean(ui->checkBoxHideLoginWindow->checkState()));
}

void MainWindow::HandleOfflineUserLogout(const QString &string) const
{
    if (currState == State::ABOUT_TO_QUIT) {
        qApp->quit();
    } else if (currState == State::ABOUT_TO_RESTART) {
        qDebug() << "Restarting Drcom...";
        QProcess::startDetached(qApp->applicationFilePath(), qApp->arguments());
        qApp->quit();
        qDebug() << "Restart done.";
    }
}

void MainWindow::HandleOfflineChallengeFailed(const QString &title)
{
    // 弹出一个提示框，带一个直接重启客户端的按钮
    auto text = tr("Challenge failed.") + " " + tr("Please check your connection.") + " " +
                tr("Note that you should connect to either wireless or wired network "
                   "before starting the DrCOM client.") + " " +
                tr("If you have connected, you may restart DrCOM to solve the problem.") + " " +
                tr("Restart DrCOM?");
    auto buttonPressed = QMessageBox::critical(
            this,
            title,
            text,
            QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No
    );

    if (buttonPressed == QMessageBox::StandardButton::Yes) {
        qDebug() << "Restart DrCOM confirmed in case OfflineReason::CHALLENGE_FAILED";
        RestartDrcomByUser();
    }
}

void MainWindow::HandleOfflineTimeout(const QString &string)
{
    // 先尝试自己重启若干次，自个重启还不行的话再提示用户
    // 自己重启的话需要用户提前勾选记住密码
    if (DrcomUserSettings::Instance().RememberCredential()) {
        uint restartTimes = DrcomUserSettings::Instance().SelfRestartCount();
        qDebug() << "case OfflineReason::TIMEOUT: restartTimes = " << restartTimes;
        if (restartTimes <= RETRY_TIMES) {
            qDebug() << "case OfflineReason::TIMEOUT: restartTimes++";
            DrcomUserSettings::Instance().IncreaseSelfRestartCount();
            //尝试自行重启
            qDebug() << "trying to restart and reconnect...";
            qDebug() << "restarting for the" << restartTimes << "time(s)";
            RestartDrcom();
            return;
        }
    }

    // 弹出一个提示框，带一个直接重启客户端的按钮
    auto boxText = tr("Connection timed out.") + " " + tr("Please check your connection.") + " " +
                   tr("The client will try to restart to solve some stability problems, "
                      "which needs \"remember me\" option to be enabled.") + " " +
                   tr("Note that you should connect to either wireless or wired network before "
                      "starting the DrCOM client.") + " " +
                   tr("You may be unable to login until you restart DrCOM.") + " " +
                   tr("Restart DrCOM?");

    if (QMessageBox::question(this, tr("Login failed"), boxText) == QMessageBox::StandardButton::Yes) {
        qDebug() << "Restart DrCOM confirmed in case OfflineReason::TIMEOUT";
        RestartDrcomByUser();
    } else {
        dogcomController.LogOut(); // if not restart, must log out to release socket
    }
}

void MainWindow::HandleOffline(LoginResult reason)
{
    currState = State::OFFLINE;

    auto loginFailedTitle = tr("Login failed");
    auto offlineTitle = tr("You have been offline");
    switch (reason) {
        case LoginResult::USER_LOGOUT:
            HandleOfflineUserLogout(loginFailedTitle);
            break;
        case LoginResult::BIND_FAILED:
            QMessageBox::critical(this, loginFailedTitle, tr(
                    "Binding port failed. Please check if there are other clients occupying the port."));
            break;
        case LoginResult::CHALLENGE_FAILED:
            HandleOfflineChallengeFailed(loginFailedTitle);
            break;
        case LoginResult::CHECK_MAC:
            QMessageBox::critical(this, loginFailedTitle, tr(
                    "Someone else is using this account, which cannot be used simultaneously."));
            break;
        case LoginResult::SERVER_BUSY:
            QMessageBox::critical(this, loginFailedTitle, tr("The server is busy. Please try again later."));
            break;
        case LoginResult::WRONG_PASS:
            QMessageBox::critical(this, loginFailedTitle, tr("Account and password does not match."));
            // 清除已保存的密码
            DrcomUserSettings::Instance().SetRawPassword(QByteArray());
            DrcomUserSettings::Instance().SetRememberCredential(false);
            DrcomUserSettings::Instance().SetAutoLogin(false);
            SaveSettings();
            break;
        case LoginResult::NOT_ENOUGH:
            QMessageBox::critical(this, loginFailedTitle, tr(
                    "The cumulative time or traffic for this account has exceeded the limit."));
            break;
        case LoginResult::FREEZE_UP:
            QMessageBox::critical(this, loginFailedTitle, tr("This account is suspended."));
            break;
        case LoginResult::NOT_ON_THIS_IP:
            QMessageBox::critical(
                    this,
                    loginFailedTitle,
                    tr("IP address does not match.") + " " +
                    tr("This account can only be logged in on computer(s) with specified IP and MAC addresses."));
            break;
        case LoginResult::NOT_ON_THIS_MAC:
            QMessageBox::critical(
                    this,
                    loginFailedTitle,
                    tr("MAC address does not match.") + " " +
                    tr("This account can only be logged in on computer(s) with specified IP and MAC addresses."));
            break;
        case LoginResult::TOO_MUCH_IP:
            QMessageBox::critical(this, loginFailedTitle, tr("This account has too many IP addresses."));
            break;
        case LoginResult::UPDATE_CLIENT:
            QMessageBox::critical(this, loginFailedTitle, tr("The client version is too old and needs an update."));
            break;
        case LoginResult::NOT_ON_THIS_IP_MAC:
            QMessageBox::critical(this, loginFailedTitle, tr(
                    "This account can only be logged in on computer(s) with specified IP and MAC addresses."));
            break;
        case LoginResult::MUST_USE_DHCP:
            QMessageBox::critical(this, loginFailedTitle, tr(
                    "Your computer has a static IP address. Use DHCP instead and try again."));
            break;
        case LoginResult::TIMEOUT:
            HandleOfflineTimeout(loginFailedTitle);
            break;
        case LoginResult::UNKNOWN:
        default:
            QMessageBox::critical(this, offlineTitle, tr("Unknow reason"));
            break;
    }

    // 重新启用输入
    SetDisableInput(false);
    SetIcon(false);
    // 禁用注销按钮
    logOutAction->setEnabled(false);
    ui->centralStackedWidget->setCurrentIndex(NOT_LOGGED_IN_PAGE_INDEX);
    // 显示出窗口
    ShowLoginWindow();
}

void MainWindow::HandleLoggedIn()
{
    // setup timer
    upTimer.start(10 * 1000); // update every 10 sec
    QTimer::singleShot(0, this, &MainWindow::UpdateTimer);

    uint restartTimes = DrcomUserSettings::Instance().SelfRestartCount();
    qDebug() << "HandleLoggedIn: restartTimes = " << restartTimes;

    currState = State::ONLINE;
    // 显示欢迎页
    if (restartTimes == 0 && !DrcomUserSettings::Instance().DisableWelcomePage()) {
        qDebug() << "open welcome page";
        QDesktopServices::openUrl(QUrl("http://login.jlu.edu.cn/notice.php"));
    }

    if (!DrcomUserSettings::Instance().RememberCredential()) {
        DrcomUserSettings::Instance().SetRawPassword(QByteArray());
    }
    SaveSettings();
    SetIcon(true);
    // 启用注销按钮
    logOutAction->setEnabled(true);

    ui->centralStackedWidget->setCurrentIndex(LOGGED_IN_PAGE_INDEX);

    // set username
    ui->loggedInUsernameLabel->setText(ui->lineEditAccount->text());

    DrcomUserSettings::Instance().ResetSelfRestartCount();
    qDebug() << "HandleLoggedIn: reset restartTimes";
}

void MainWindow::HandleIpAddress(const QString &ip)
{
    ui->ipLabel->setText(ip);
}

void MainWindow::BrowserButtonClicked()
{
    QDesktopServices::openUrl(QUrl("http://ip.jlu.edu.cn"));
}

void MainWindow::UserLogOut()
{
    if (QMessageBox::question(this, "", tr("Are you sure you want to log out?")) != QMessageBox::StandardButton::Yes) {
        return;
    }

    // reset timer
    upTimer.stop();
    uptimeCounter = 0;
    // 用户主动注销
    dogcomController.LogOut();
    // 注销后应该是想重新登录或者换个号，因此显示出用户界面
    restoreAction->activate(restoreAction->Trigger);
    // 禁用注销按钮
    logOutAction->setEnabled(false);

    // switch to logged-out panel
    ui->centralStackedWidget->setCurrentIndex(NOT_LOGGED_IN_PAGE_INDEX);
}

void MainWindow::UpdateTimer()
{
    using namespace std::chrono;

    milliseconds d(upTimer.intervalAsDuration() * uptimeCounter);
    auto hr = duration_cast<hours>(d).count();
    auto min = duration_cast<minutes>(d).count() % 60;
    auto sec = duration_cast<seconds>(d).count() % 60;
    ui->uptimeLabel->setText(QString("%1:%2:%3").arg(hr).arg(min, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0')));

    uptimeCounter++;
}

void MainWindow::AboutDrcom()
{
    QMessageBox b;
    b.setIcon(QMessageBox::Information);
    b.setWindowTitle(tr("About"));
    b.setText(QApplication::applicationName() + " " + QApplication::applicationDisplayName());
    b.setInformativeText(QApplication::applicationVersion());
    b.setDetailedText("Forked by btw, repo: https://github.com/awesometw/drcom-jlu-qt\n\n"
                      "Original repo: https://github.com/code4lala/drcom-jlu-qt");
    b.exec();
}

void MainWindow::on_checkBoxNotShowWelcome_toggled(bool checked)
{
    DrcomUserSettings::Instance().SetDisableWelcomePage(checked);
}

void MainWindow::on_checkBoxHideLoginWindow_toggled(bool checked)
{
    DrcomUserSettings::Instance().SetHideWindow(checked);
}
