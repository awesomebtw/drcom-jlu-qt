#include <QDir>
#include <QMessageBox>
#include <QSystemSemaphore>
#include <QSharedMemory>
#include <QTranslator>
#include <iostream>

#include "mainwindow.h"

//日志生成
void LogMsgOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    static QMutex mutex; //日志代码互斥锁
    mutex.lock();

    // Critical Resource of Code
    if (context.file != nullptr && context.function != nullptr) {
        std::clog << context.file << ": " << context.function << ": " << context.line << ": ";
    }

    switch (type) {
        case QtDebugMsg:
            std::clog << "[Debug] ";
            break;
        case QtInfoMsg:
            std::clog << "[Info] ";
            break;
        case QtWarningMsg:
            std::clog << "[Warning] ";
            break;
        case QtCriticalMsg:
            std::clog << "[Critical] ";
            break;
        case QtFatalMsg:
            std::clog << "[Fatal] ";
            break;
    }
    std::clog << msg.toLocal8Bit().data() << '\n';

    mutex.unlock();
}

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(DrcomJluQt);

    QApplication a(argc, argv);

    QTranslator translator;
    (void) translator.load(":/ts/DrComJluQt_zh_CN.qm");
    QApplication::installTranslator(&translator);

    QSystemSemaphore semaphore("DrcomQtSem", 1);  // create semaphore
    semaphore.acquire(); // Raise the semaphore, barring other instances to work with shared memory

#ifndef Q_OS_WIN32
    // in linux / unix shared memory is not freed when the application terminates abnormally,
    // so you need to get rid of the garbage
    QSharedMemory nix_fix_shared_memory("DrcomQtSharedMemory");
    if (nix_fix_shared_memory.attach()){
        nix_fix_shared_memory.detach();
    }
#endif
    QSharedMemory sharedMemory("DrcomQtSharedMemory");  // Create a copy of the shared memory

    bool isRunning = true;
    for (int i = 0; isRunning && i < 5; i++) {
        // We are trying to attach a copy of the shared memory to an existing segment
        // If successful, it determines that there is already a running instance
        // To support restarting functionality, waiting for some time to see if the last instance closed
        // Otherwise allocate 1 byte of memory
        // And determines that another instance is not running
        if (sharedMemory.attach()) {
            sharedMemory.detach();
            QThread::msleep(60);
        } else {
            sharedMemory.create(1);
            isRunning = false;
        }
    }
    semaphore.release();

    QApplication::setApplicationName("DrcomJluQt");
    QApplication::setApplicationDisplayName(QApplication::tr("DrCOM JLU Qt"));
    QApplication::setApplicationVersion("1.0.1");
    QApplication::setQuitOnLastWindowClosed(false);

    if (isRunning) {
        QMessageBox::warning(nullptr, "", QApplication::tr("The application is already running."));
        return 1;
    }

    std::ios::sync_with_stdio(false);
    qInstallMessageHandler(LogMsgOutput);

    qDebug() << "...main...";

    MainWindow w(&a);

    QSettings s(SETTINGS_FILE_NAME);
    bool hideWindow = s.value(ID_HIDE_WINDOW, false).toBool();
    // 如果是软件自行重启的就不显示窗口
    int restartTimes = s.value(ID_RESTART_TIMES, 0).toInt();
    qDebug() << "restartTimes = " << restartTimes;
    if (hideWindow) {
        qDebug() << "not show window caused by user settings";
    } else if (restartTimes > 0) {
        qDebug() << "not show window caused by self restart";
    } else {
        w.ShowLoginWindow();
    }

    return QApplication::exec();
}
