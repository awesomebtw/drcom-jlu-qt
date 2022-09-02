#include <QtNetwork/QNetworkInterface>
#include <QApplication>
#include <QElapsedTimer>
#include <QMenuBar>
#include <QMessageBox>
#include <QValidator>
#include <QSettings>
#include <QWindow>
#include <QDesktopServices>
#include <QUrl>
#include <QCloseEvent>
#include <QProcess>
#include <chrono>
#include <cryptopp/modes.h>
#include <cryptopp/sm4.h>
#include <cryptopp/filters.h>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "constants.h"

// key for encryption algorithm
constexpr const char encKey[] = "Although never is often better than *right* now.\n"
                                "If the implementation is hard to explain, it's a bad idea.\n"
                                "If the implementation is easy to explain, it may be a good idea.\n"
                                "Namespaces are one honking great idea -- let's do more of those!";

MainWindow::MainWindow(QApplication *parentApp, QWidget *parent) :
        QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint),
        ui(new Ui::MainWindow),
        app(parentApp),
        s(SETTINGS_FILE_NAME, QSettings::IniFormat),
        macValidator(QRegularExpression("^[0-9a-fA-F]{2}([:-][0-9a-fA-F]{2}){5}$")),
        trayIconMenu(new QMenu(this)),
        onlineIcon(":/images/online.png"),
        offlineIcon(":/images/offline.png"),
        currState(State::OFFLINE),
        upTimer(this)
{
    // 关机时接收退出信号，释放socket
    QObject::connect(app, &QApplication::aboutToQuit, this, &MainWindow::QuitDrcom);

    qDebug() << "MainWindow constructor";
    ui->setupUi(this);

    // 记住窗口大小功能
    restoreGeometry(s.value(ID_MAIN_WINDOW_GEOMETRY).toByteArray());

    // index 0 is not-login panel
    ui->centralStackedWidget->setCurrentIndex(0);

    // 获取mac地址
    for (const QNetworkInterface &i: QNetworkInterface::allInterfaces()) {
        if (!i.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            ui->comboBoxMAC->addItem(i.hardwareAddress() + " (" + i.name() + ')', i.hardwareAddress());
        }
    }
    ui->comboBoxMAC->addItem(CUSTOM_MAC);

    // 重启功能
    connect(ui->restartPushButton, &QPushButton::clicked, this, &MainWindow::RestartDrcomByUser);

    // Login, logout and browser functionality
    connect(ui->loginButton, &QCommandLinkButton::clicked, this, &MainWindow::LoginButtonClicked);
    connect(ui->logoutButton, &QCommandLinkButton::clicked, this, &MainWindow::UserLogOut);
    connect(ui->browserButton, &QCommandLinkButton::clicked, this, &MainWindow::BrowserButtonClicked);

    // 创建托盘菜单和图标
    // 托盘菜单选项
    restoreAction = std::make_unique<QAction>(tr("&Restore"), this);
    connect(restoreAction.get(), &QAction::triggered, this, &MainWindow::ShowLoginWindow);
    logOutAction = std::make_unique<QAction>(tr("&Logout"), this);
    connect(logOutAction.get(), &QAction::triggered, this, &MainWindow::UserLogOut);
    restartAction = std::make_unique<QAction>(tr("Re&start"), this);
    connect(restartAction.get(), &QAction::triggered, this, &MainWindow::RestartDrcomByUser);
    quitAction = std::make_unique<QAction>(tr("&Quit"), this);
    connect(quitAction.get(), &QAction::triggered, this, &MainWindow::QuitDrcom);

    // 新建菜单
    trayIconMenu->addAction(restoreAction.get());
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(logOutAction.get());
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(restartAction.get());
    trayIconMenu->addAction(quitAction.get());

    // 新建托盘图标
    trayIcon = std::make_unique<QSystemTrayIcon>(this);
    trayIcon->setContextMenu(trayIconMenu.get());

    // 设置托盘菜单响应函数
    connect(trayIcon.get(), &QSystemTrayIcon::activated, this, &MainWindow::IconActivated);
    // 设置托盘图标和窗口图标
    SetIcon(false);
    // 显示出来托盘图标
    trayIcon->show();

    // 创建窗口菜单
    connect(ui->aboutPushButton, &QPushButton::clicked, this, &MainWindow::AboutDrcom);

    // 读取配置文件
    LoadSettings();

    // 设置回调函数
    connect(&dogcomController, &DogcomController::HaveBeenOffline, this, &MainWindow::HandleOffline);
    connect(&dogcomController, &DogcomController::HaveLoggedIn, this, &MainWindow::HandleLoggedIn);
    connect(&dogcomController, &DogcomController::HaveObtainedIp, this, &MainWindow::HandleIpAddress);

    // timer
    upTimer.setTimerType(Qt::TimerType::VeryCoarseTimer);
    connect(&upTimer, &QTimer::timeout, this, &MainWindow::UpdateTimer);

    // 验证手动输入的mac地址
    ui->lineEditMAC->setValidator(&macValidator);

    // 尚未登录 不可注销
    DisableLogOutTrayContextMenu(true);

    // 自动登录功能
    int restartTimes = s.value(ID_RESTART_TIMES, 0).toInt();
    qDebug() << "MainWindow constructor: restartTimes = " << restartTimes;
    if (restartTimes > 0 || s.value(ID_AUTO_LOGIN, false).toBool()) { // 尝试自动重启中
        emit ui->loginButton->click();
    }
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
    b.setWindowIcon(onlineIcon);
    b.setIcon(QMessageBox::Information);
    b.setWindowTitle(tr("About"));
    b.setInformativeText(QApplication::applicationVersion());
    b.setText(qAppName());
    b.setDetailedText("Forked by xxx, repo: xxx\n\nOriginal repo: https://github.com/code4lala/drcom-jlu-qt");
    b.exec();
}

void MainWindow::closeEvent(QCloseEvent *)
{
    s.setValue(ID_MAIN_WINDOW_GEOMETRY, saveGeometry());
    // 未登录时直接关闭窗口就退出
    if (currState == State::OFFLINE) {
        QuitDrcom();
    }
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
    s.setValue(ID_RESTART_TIMES, 0);
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

void MainWindow::RestartDrcomByUser()
{
    // 重启之前恢复重试计数
    s.setValue(ID_RESTART_TIMES, 0);
    qDebug() << "reset restartTimes";
    RestartDrcom();
}

void MainWindow::IconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        restoreAction->activate(restoreAction->Trigger);
    }
}

Qt::CheckState MainWindow::BooleanToCheckState(bool val)
{
    if (val) {
        return Qt::CheckState::Checked;
    } else {
        return Qt::CheckState::Unchecked;
    }
}

QByteArray MainWindow::Encrypt(QByteArray arr)
{
    using namespace CryptoPP;

    SecByteBlock iv(SM4::DEFAULT_KEYLENGTH);
    memset(iv, 0, iv.size());
    CBC_CTS_Mode<SM4>::Encryption e;
    e.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte *>(encKey), SM4::DEFAULT_KEYLENGTH, iv);

    StreamTransformationFilter encryptor(e, nullptr);
    encryptor.Put(reinterpret_cast<const CryptoPP::byte *>(arr.data()), arr.length());
    for (auto i = arr.length(); i <= SM4::DEFAULT_KEYLENGTH; i++) {
        encryptor.Put(CryptoPP::byte('\0')); // padding
    }
    encryptor.MessageEnd();

    QByteArray res;
    if (encryptor.AnyRetrievable()) {
        res.resize(static_cast<qsizetype>(encryptor.MaxRetrievable()));
        encryptor.Get(reinterpret_cast<CryptoPP::byte *>(&res[0]), res.length());
    }

    return res;
}

QByteArray MainWindow::Decrypt(QByteArray arr)
{
    using namespace CryptoPP;

    SecByteBlock iv(SM4::DEFAULT_KEYLENGTH);
    memset(iv, 0, iv.size());

    CBC_CTS_Mode<SM4>::Decryption d;
    d.SetKeyWithIV(reinterpret_cast<const byte *>(encKey), SM4::DEFAULT_KEYLENGTH, iv);

    StreamTransformationFilter decryptor(d, nullptr);
    decryptor.Put(reinterpret_cast<const byte *>(arr.data()), arr.length());
    decryptor.MessageEnd();

    QByteArray res;
    if (decryptor.AnyRetrievable()) {
        res.resize(static_cast<qsizetype>(decryptor.MaxRetrievable()));
        decryptor.Get(reinterpret_cast<byte *>(&res[0]), res.length());
        res.resize(static_cast<qsizetype>(qstrlen(res.data()))); // remove padding NULs
    }

    return res;
}

void MainWindow::LoadSettings()
{
    auto account = s.value(ID_ACCOUNT, "").toString();
    auto password = Decrypt(s.value(ID_PASSWORD, "").toByteArray());
    auto macAddr = s.value(ID_MAC, "").toString();
    auto remember = s.value(ID_REMEMBER, false).toBool();
    auto hideWindow = s.value(ID_HIDE_WINDOW, false).toBool();
    auto notShowWelcome = s.value(ID_NOT_SHOW_WELCOME, false).toBool();
    auto autoLogin = s.value(ID_AUTO_LOGIN, false).toBool();

    ui->lineEditAccount->setText(account);
    ui->lineEditPass->setText(password);
    SetMAC(macAddr);
    ui->loginButton->setText(tr("Login"));

    ui->checkBoxRemember->setCheckState(BooleanToCheckState(remember));
    ui->checkBoxAutoLogin->setCheckState(BooleanToCheckState(autoLogin));
    ui->checkBoxHideLoginWindow->setCheckState(BooleanToCheckState(hideWindow));
    ui->checkBoxNotShowWelcome->setCheckState(BooleanToCheckState(notShowWelcome));
}

void MainWindow::SaveSettings()
{
//    s.sync();
}

void MainWindow::SetMAC(const QString &m)
{
    for (int i = 0; i < ui->comboBoxMAC->count(); i++) {
        auto str = ui->comboBoxMAC->itemData(i);
        if (str.isNull())
            continue;
        if (str.toString().compare(m) == 0) {
            //当前列表中有该mac地址
            ui->comboBoxMAC->setCurrentIndex(i);
            return;
        }
    }
    // 当前列表中没有该mac地址，填充到输入框中
    ui->comboBoxMAC->setCurrentText(CUSTOM_MAC);
    ui->lineEditMAC->setText(m);
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

void MainWindow::on_comboBoxMAC_currentTextChanged(const QString &)
{
    ui->lineEditMAC->setDisabled(!ui->comboBoxMAC->currentData().isNull());
}

void MainWindow::LoginButtonClicked()
{
    WriteInputs();

    auto macAddr = s.value(ID_MAC, "").toString();
    auto account = s.value(ID_ACCOUNT, "").toString();
    auto password = Decrypt(s.value(ID_PASSWORD, "").toByteArray());

    if (account.isEmpty() || password.isEmpty() || macAddr.isEmpty()) {
        QMessageBox::warning(this, QApplication::applicationDisplayName(), tr("Fields cannot be empty!"));
        return;
    }
    if (macAddr.length() != 17) {
        QMessageBox::warning(this, QApplication::applicationDisplayName(), tr("Illegal MAC address!"));
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
    ui->lineEditMAC->setDisabled(yes);
    ui->checkBoxRemember->setDisabled(yes);
    ui->checkBoxAutoLogin->setDisabled(yes);
    ui->checkBoxNotShowWelcome->setDisabled(yes);
    ui->checkBoxHideLoginWindow->setDisabled(yes);
    ui->loginButton->setDisabled(yes);

    if (!yes) {
        // 要启用输入框
        if (!ui->comboBoxMAC->currentData().isNull()) {
            // 当前选的不是自定义mac地址
            ui->lineEditMAC->setDisabled(true);
        }
    }
}

void MainWindow::SetIcon(bool online)
{
    if (online) {
        // 设置彩色图标
        trayIcon->setToolTip(QApplication::applicationDisplayName().append(" - ").append(tr("online")));
        trayIcon->setIcon(onlineIcon);
        setWindowIcon(onlineIcon);
    } else {
        // 设置灰色图标
        trayIcon->setToolTip(QApplication::applicationDisplayName().append(" - ").append(tr("offline")));
        trayIcon->setIcon(offlineIcon);
        setWindowIcon(offlineIcon);
    }
}

void MainWindow::WriteInputs()
{
    s.setValue(ID_ACCOUNT, ui->lineEditAccount->text());
    s.setValue(ID_PASSWORD,
               Encrypt(ui->lineEditPass->text().toLatin1())); // since all characters in a password are ascii
    if (ui->lineEditMAC->isEnabled()) {
        s.setValue(ID_MAC, ui->lineEditMAC->text().toUpper());
    } else {
        s.setValue(ID_MAC, ui->comboBoxMAC->currentData().toString().toUpper());
    }
    s.setValue(ID_REMEMBER, ui->checkBoxRemember->checkState());
    s.setValue(ID_AUTO_LOGIN, ui->checkBoxAutoLogin->checkState());
}

void MainWindow::HandleOfflineUserLogout(const QString &string) const
{
    if (currState == State::ABOUT_TO_QUIT) {
        qApp->quit();
        return;
    }
    if (currState == State::ABOUT_TO_RESTART) {
        qDebug() << "Restarting Drcom...";
        QProcess::startDetached(qApp->applicationFilePath(), qApp->arguments());
        qApp->quit();
        qDebug() << "Restart done.";
        return;
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
    if (s.value(ID_REMEMBER, false).toBool()) {
        int restartTimes = s.value(ID_RESTART_TIMES, 0).toInt();
        qDebug() << "case OfflineReason::TIMEOUT: restartTimes = " << restartTimes;
        if (restartTimes <= RETRY_TIMES) {
            qDebug() << "case OfflineReason::TIMEOUT: restartTimes++";
            s.setValue(ID_RESTART_TIMES, restartTimes + 1);
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

    if (reason == LoginResult::WRONG_PASS) {
        // 清除已保存的密码
        s.setValue(ID_PASSWORD, QByteArray());
        s.setValue(ID_REMEMBER, false);
        s.setValue(ID_AUTO_LOGIN, false);
        SaveSettings();
    }
    // 重新启用输入
    SetDisableInput(false);
    SetIcon(false);
    // 禁用注销按钮
    DisableLogOutTrayContextMenu(true);
    // 显示出窗口
    ShowLoginWindow();
}

void MainWindow::HandleLoggedIn()
{
    // setup timer
    upTimer.start(10 * 1000); // update every 10 sec
    QTimer::singleShot(0, this, &MainWindow::UpdateTimer);

    int restartTimes = s.value(ID_RESTART_TIMES, 0).toInt();
    qDebug() << "HandleLoggedIn: restartTimes = " << restartTimes;

    currState = State::ONLINE;
    // 显示欢迎页
    if (restartTimes == 0 && !s.value(ID_NOT_SHOW_WELCOME, false).toBool()) {
        qDebug() << "open welcome page";
        QDesktopServices::openUrl(QUrl("http://login.jlu.edu.cn/notice.php"));
    }
    // 登录成功，保存密码
    if (!s.value(ID_REMEMBER, false).toBool()) {
        s.setValue(ID_PASSWORD, QByteArray());
    }
    SaveSettings();
    SetIcon(true);
    // 启用注销按钮
    DisableLogOutTrayContextMenu(false);

    // 1 is logged-in info panel
    ui->centralStackedWidget->setCurrentIndex(1);

    // set username
    ui->loggedInUsernameLabel->setText(ui->lineEditAccount->text());

    s.setValue(ID_RESTART_TIMES, 0);
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
    if (QMessageBox::question(this, tr("Logout"), tr("Are you sure you want to log out?")) !=
        QMessageBox::StandardButton::Yes) {
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
    DisableLogOutTrayContextMenu(true);

    // switch to logged-out panel
    ui->centralStackedWidget->setCurrentIndex(0);
}

void MainWindow::DisableLogOutTrayContextMenu(bool yes)
{
    if (yes) {
        logOutAction->setDisabled(true);
    } else {
        logOutAction->setEnabled(true);
    }
}

void MainWindow::on_checkBoxNotShowWelcome_toggled(bool checked)
{
    s.setValue(ID_NOT_SHOW_WELCOME, checked);
}

void MainWindow::on_checkBoxHideLoginWindow_toggled(bool checked)
{
    s.setValue(ID_HIDE_WINDOW, checked);
}
