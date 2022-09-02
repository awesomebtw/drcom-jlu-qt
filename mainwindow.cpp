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
    connect(ui->loginButton, &QCommandLinkButton::clicked, this, &MainWindow::on_pushButtonLogin_clicked);
    connect(ui->logoutPushButton, &QPushButton::clicked, this, &MainWindow::UserLogOut);

    // 创建托盘菜单和图标
    // 托盘菜单选项
    restoreAction = std::make_unique<QAction>(tr("&Restore"), this);
    connect(restoreAction.get(), &QAction::triggered, this, &MainWindow::ShowLoginWindow);
    logOutAction = std::make_unique<QAction>(tr("&Logout"), this);
    connect(logOutAction.get(), &QAction::triggered, this, &MainWindow::UserLogOut);
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

    milliseconds d(upElapsedTimer.elapsed());
    auto hr = duration_cast<hours>(d).count();
    auto min = duration_cast<minutes>(d).count() % 60;
    auto sec = duration_cast<seconds>(d).count() % 60;
    ui->uptimeLabel->setText(QString("%1:%2:%3").arg(hr).arg(min, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0')));
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
    restart = true;
    if (currState == State::ONLINE)
        dogcomController.LogOut();
    else if (currState == State::OFFLINE) {
        qDebug() << "Restarting Drcom...";
        QProcess::startDetached(qApp->applicationFilePath(), qApp->arguments());
        qApp->quit();
        qDebug() << "Restart done.";
    } // 正在登录时候退出，假装没看到，不理
}

void MainWindow::QuitDrcom()
{
    // 退出之前恢复重试计数
    s.setValue(ID_RESTART_TIMES, 0);
    qDebug() << "reset restartTimes QuitDrcom";

    quit = true;
    if (currState == State::ONLINE) {
        dogcomController.LogOut();
    } else if (currState == State::OFFLINE) {
        qApp->quit();
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
    switch (reason) {
        case QSystemTrayIcon::DoubleClick: {
            restoreAction->activate(restoreAction->Trigger);
            break;
        }
        default:
            break;
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
    ui->loginButton->setDescription(tr("Click here to login"));

    ui->checkBoxRemember->setCheckState(BooleanToCheckState(remember));
    ui->checkBoxAutoLogin->setCheckState(BooleanToCheckState(autoLogin));
    ui->checkBoxHideLoginWindow->setCheckState(BooleanToCheckState(hideWindow));
    ui->checkBoxNotShowWelcome->setCheckState(BooleanToCheckState(notShowWelcome));
}

void MainWindow::SaveSettings()
{
    s.sync();
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

void MainWindow::on_pushButtonLogin_clicked()
{
    WriteInputs();

    auto macAddr = s.value(ID_MAC, "").toString();
    auto account = s.value(ID_ACCOUNT, "").toString();
    auto password = Decrypt(s.value(ID_PASSWORD, "").toByteArray());

    if (!account.compare("") || !password.compare("") || !macAddr.compare("")) {
        QMessageBox::warning(this, APP_NAME, tr("Input can not be empty!"));
        return;
    }
    if (macAddr.length() != 17) {
        QMessageBox::warning(this, APP_NAME, tr("Illegal MAC address!"));
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
    QString toolTip;
    if (online) {
        // 设置彩色图标
        toolTip = tr("DrCOM JLU Qt -- online");
        trayIcon->setIcon(onlineIcon);
        setWindowIcon(onlineIcon);
    } else {
        // 设置灰色图标
        toolTip = tr("DrCOM JLU Qt -- offline");
        trayIcon->setIcon(offlineIcon);
        setWindowIcon(offlineIcon);
    }
    trayIcon->setToolTip(toolTip);
}

void MainWindow::WriteInputs()
{
    s.setValue(ID_ACCOUNT, ui->lineEditAccount->text());
    s.setValue(ID_PASSWORD, Encrypt(ui->lineEditPass->text().toLatin1())); // since all characters in a password are ascii
    if (ui->lineEditMAC->isEnabled()) {
        s.setValue(ID_MAC, ui->lineEditMAC->text().toUpper());
    } else {
        s.setValue(ID_MAC, ui->comboBoxMAC->currentData().toString().toUpper());
    }
    s.setValue(ID_REMEMBER, ui->checkBoxRemember->checkState());
    s.setValue(ID_AUTO_LOGIN, ui->checkBoxAutoLogin->checkState());
}

void MainWindow::HandleOffline(LoginResult reason)
{
    currState = State::OFFLINE;

    switch (reason) {
        case LoginResult::USER_LOGOUT: {
            if (quit) {
                qApp->quit();
                return;
            }
            if (restart) {
                qDebug() << "Restarting Drcom...";
                QProcess::startDetached(qApp->applicationFilePath(), qApp->arguments());
                qApp->quit();
                qDebug() << "Restart done.";
                return;
            }
            break;
        }
        case LoginResult::BIND_FAILED: {
            QMessageBox::critical(this, tr("Login failed"),
                                  tr("Binding port failed. Please check if there are other clients occupying the port"));
            break;
        }
        case LoginResult::CHALLENGE_FAILED: {
            // 弹出一个提示框，带一个直接重启客户端的按钮
            QMessageBox msgBox;
            msgBox.setText(tr("Login failed") + " " + tr("Challenge failed. Please check your connection:)") + " " +
                           tr("Attention that you should connect to wifi or wired firstly and then start the drcom "
                              "client. If you have connected, you may restart drcom to solve the problem.")
                           + " " + tr("Restart DrCOM?"));
            QAbstractButton *buttonYes = msgBox.addButton(tr("Yes"), QMessageBox::YesRole);
            msgBox.addButton(tr("Nope"), QMessageBox::NoRole);
            msgBox.exec();
            if (msgBox.clickedButton() == buttonYes) {
                qDebug() << "Restart DrCOM confirmed in case OfflineReason::CHALLENGE_FAILED";
                RestartDrcomByUser();
            }
            break;
        }
        case LoginResult::CHECK_MAC: {
            QMessageBox::critical(this, tr("Login failed"), tr("Someone is using this account with wired"));
            break;
        }
        case LoginResult::SERVER_BUSY: {
            QMessageBox::critical(this, tr("Login failed"), tr("The server is busy, please log back in again"));
            break;
        }
        case LoginResult::WRONG_PASS: {
            QMessageBox::critical(this, tr("Login failed"), tr("Account and password not match"));
            break;
        }
        case LoginResult::NOT_ENOUGH: {
            QMessageBox::critical(this, tr("Login failed"),
                                  tr("The cumulative time or traffic for this account has exceeded the limit"));
            break;
        }
        case LoginResult::FREEZE_UP: {
            QMessageBox::critical(this, tr("Login failed"), tr("This account is suspended"));
            break;
        }
        case LoginResult::NOT_ON_THIS_IP: {
            QMessageBox::critical(this, tr("Login failed"),
                                  tr("IP address does not match, this account can only be used in the specified IP address"));
            break;
        }
        case LoginResult::NOT_ON_THIS_MAC: {
            QMessageBox::critical(this, tr("Login failed"),
                                  tr("MAC address does not match, this account can only be used in the specified IP and MAC address"));
            break;
        }
        case LoginResult::TOO_MUCH_IP: {
            QMessageBox::critical(this, tr("Login failed"), tr("This account has too many IP addresses"));
            break;
        }
        case LoginResult::UPDATE_CLIENT: {
            QMessageBox::critical(this, tr("Login failed"), tr("The client version is incorrect"));
            break;
        }
        case LoginResult::NOT_ON_THIS_IP_MAC: {
            QMessageBox::critical(this, tr("Login failed"),
                                  tr("This account can only be used on specified MAC and IP address"));
            break;
        }
        case LoginResult::MUST_USE_DHCP: {
            QMessageBox::critical(this, tr("Login failed"),
                                  tr("Your PC set up a static IP, please change to DHCP, and then re-login"));
            break;
        }
        case LoginResult::TIMEOUT: {
            // 先尝试自己重启若干次，自个重启还不行的话再提示用户
            // 自己重启的话需要用户提前勾选记住密码
            if (s.value(ID_REMEMBER, false).toBool()) {
                int restartTimes = s.value(ID_RESTART_TIMES, 0).toInt();
                qDebug() << "case OfflineReason::TIMEOUT: restartTimes=" << restartTimes;
                if (restartTimes <= RETRY_TIMES) {
                    qDebug() << "case OfflineReason::TIMEOUT: restartTimes++";
                    s.setValue(ID_RESTART_TIMES, restartTimes + 1);
                    //尝试自行重启
                    qDebug() << "trying to restart to reconnect...";
                    qDebug() << "restarting for the" << restartTimes << "times";
                    RestartDrcom();
                    return;
                }
            }

            // 弹出一个提示框，带一个直接重启客户端的按钮
            auto boxText = tr("You have been offline") + " " + tr("Time out, please check your connection") + " " +
                           tr("The DrCOM client will try to restart to solve some unstable problems but the function "
                           "relies on \"remember me\"") + " " +
                           tr("Due to some reasons, you should connect to wifi or wired firstly and then start the "
                              "drcom client. So you may not login until you restart DrCOM :D") + " " +
                              tr("Restart DrCOM?");

            if (QMessageBox::question(this, tr("You have been offline"), boxText) == QMessageBox::StandardButton::Yes) {
                qDebug() << "Restart DrCOM confirmed in case OfflineReason::TIMEOUT";
                RestartDrcomByUser();
            }
            break;
        }
        case LoginResult::UNKNOWN:
        default:
            QMessageBox::critical(this, tr("You have been offline"), tr("Unknow reason"));
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
    upElapsedTimer.restart();
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

void MainWindow::UserLogOut()
{
    if (QMessageBox::question(this, tr("Logout"), tr("Are you sure you want to logout?")) !=
        QMessageBox::StandardButton::Yes) {
        return;
    }

    // reset timer
    upTimer.stop();
    upElapsedTimer.restart();
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

MainWindow::~MainWindow()
{
    delete ui;
}
