#ifndef DRCOMJLUQT_DOGCOMCONTROLLER_H
#define DRCOMJLUQT_DOGCOMCONTROLLER_H

#include <QApplication>
#include <QObject>
#include <memory>
#include "constants.h"
#include "dogcom.h"
#include "interruptiblesleeper.h"

class DogcomController : public QObject
{
	Q_OBJECT
public:
	DogcomController();
	~DogcomController() override = default;
	void Login(const QString &account, const QString &password, const QString &mac);
	void LogOut();
public slots:
	void HandleDogcomOffline(LoginResult reason);
	void HandleDogcomOnline();
	void HandleIpAddress(unsigned char x1, unsigned char x2, unsigned char x3, unsigned char x4);
signals:
	void HaveBeenOffline(LoginResult reason);
	void HaveLoggedIn();
	void HaveObtainedIp(const QString &ip);
private:
	std::unique_ptr<InterruptibleSleeper> sleeper;
	std::unique_ptr<DogCom> dogcom;
};

#endif //DRCOMJLUQT_DOGCOMCONTROLLER_H
