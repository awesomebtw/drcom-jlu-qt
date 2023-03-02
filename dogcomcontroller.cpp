#include "dogcomcontroller.h"
#include <QDebug>

DogcomController::DogcomController() : sleeper(new InterruptibleSleeper()), dogcom() {
    dogcom = std::make_unique<DogCom>(sleeper.get());

    connect(dogcom.get(), &DogCom::ReportOnline, this, &DogcomController::HandleDogcomOnline);
    connect(dogcom.get(), &DogCom::ReportOffline, this, &DogcomController::HandleDogcomOffline);
    connect(dogcom.get(), &DogCom::ReportIpAddress, this, &DogcomController::HandleIpAddress);
}

void DogcomController::Login(const QString &account, const QString &password, const QString &mac_addr) {
    qDebug() << "Filling config...";
    dogcom->FillConfig(account, password, mac_addr);
    qDebug() << "Fill config done.";
    dogcom->start();
}

void DogcomController::LogOut() {
    dogcom->Stop();
}

void DogcomController::HandleDogcomOffline(LoginResult reason) {
    emit HaveBeenOffline(reason);
}

void DogcomController::HandleDogcomOnline() {
    emit HaveLoggedIn();
}

void DogcomController::HandleIpAddress(unsigned char x1, unsigned char x2, unsigned char x3, unsigned char x4) {
    QString ip = QString::asprintf("%d.%d.%d.%d", x1, x2, x3, x4);
    qDebug() << "IP ADDRESS:" << ip;
    emit HaveObtainedIp(ip);
}
