#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QProcess>
#include <QSystemSemaphore>
#include <QSharedMemory>
#include <QTranslator>

#include "mainwindow.h"

static QString timePoint;

//日志生成
void LogMsgOutput(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    static QMutex mutex; //日志代码互斥锁

    // 持有锁
    mutex.lock();

    // Critical Resource of Code
    QByteArray localMsg = msg.toLocal8Bit();
    QString log;

    switch (type) {
        case QtDebugMsg:
            log.append(QString("Debug %1").arg(localMsg.constData()));
            break;
        case QtInfoMsg:
            log.append(QString("Info: %1").arg(localMsg.constData()));
            break;
        case QtWarningMsg:
            log.append(QString("Warning: %1").arg(localMsg.constData()));
            break;
        case QtCriticalMsg:
            log.append(QString("Critical: %1").arg(localMsg.constData()));
            break;
        case QtFatalMsg:
            log.append(QString("Fatal: %1").arg(localMsg.constData()));
            break;
    }

    QDir dir(QApplication::applicationDirPath());
    dir.mkdir("logs");
    QFile file(dir.path() + QString("/logs/log%1.lgt").arg(timePoint));
    file.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream out(&file);
    out << log << Qt::endl;
    file.close();

    // 释放锁
    mutex.unlock();
}

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(DrCOM_JLU_Qt);

    QApplication a(argc, argv);

    QSystemSemaphore semaphore("DrcomQtSem", 1);  // create semaphore
    semaphore.acquire(); // Raise the semaphore, barring other instances to work with shared memory

#ifndef Q_OS_WIN32
    // in linux / unix shared memory is not freed when the application terminates abnormally,
    // so you need to get rid of the garbage
    QSharedMemory nix_fix_shared_memory("DrcomQtSharedMemory");
    if(nix_fix_shared_memory.attach()){
        nix_fix_shared_memory.detach();
    }
#endif
    QSharedMemory sharedMemory("DrcomQtSharedMemory");  // Create a copy of the shared memory

    bool is_running = true;
    for (int i = 0; is_running && i < 100; i++) {
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
            is_running = false;
        }
    }
    semaphore.release();

    // If you already run one instance of the application, then we inform the user about it
    // and complete the current instance of the application
    if (is_running) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("The application is already running. Allowed to run only one instance of the application.");
        msgBox.exec();
        return 1;
    }

    //release模式输出日志到文件
    // 因为调用了QApplication::applicationDirPath()
    // 要在QApplication实例化之后调用
#ifndef QT_DEBUG
    timePoint = QDateTime::currentDateTime().toString("yyyyMMddHHmmss");
    qInstallMessageHandler(LogMsgOutput);
#endif
    QApplication::setApplicationName("DrcomQt");
    QApplication::setApplicationVersion("1.0.0.6");
    QApplication::setQuitOnLastWindowClosed(false);

    qDebug() << "...main...";

    QTranslator translator;
    (void) translator.load(":/ts/DrCOM_zh_CN.qm");
    QApplication::installTranslator(&translator);

    MainWindow w(&a);

    QSettings s(SETTINGS_FILE_NAME);
    bool hideWindow = s.value(ID_HIDE_WINDOW, false).toBool();
    // 如果是软件自行重启的就不显示窗口
    int restartTimes = s.value(ID_RESTART_TIMES, 0).toInt();
    qDebug() << "main: restartTimes=" << restartTimes;
    if (hideWindow) {
        qDebug() << "not show window caused by user settings";
    } else if (restartTimes > 0) {
        // 是自行重启不显示窗口
        qDebug() << "not show window caused by self restart";
    } else {
        w.ShowLoginWindow();
    }

    return QApplication::exec();
}
