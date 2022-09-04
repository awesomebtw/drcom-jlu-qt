#ifndef DRCOMJLUQT_DOGCOM_H
#define DRCOMJLUQT_DOGCOM_H

#include <QThread>
#include <QUdpSocket>
#include <random>
#include "constants.h"
#include "interruptiblesleeper.h"
#include "DogcomSocket.h"

class DogCom : public QThread {
Q_OBJECT
public:
    explicit DogCom(InterruptibleSleeper *);

    void Stop();

    void FillConfig(QString a, QString p, const QString &m);

protected:
    void run() override;

private:
    InterruptibleSleeper *sleeper;
    QString account;
    QString password;
    unsigned char macBinary[6]{};
    bool log;

    bool DhcpChallenge(DogcomSocket &socket, unsigned char seed[]);

    void PrintPacket(const char msg[], const unsigned char *packet, size_t length) const;

    LoginResult DhcpLogin(DogcomSocket &socket, unsigned char seed[], unsigned char auth_information[]);

    int Keepalive1(DogcomSocket &socket, unsigned char auth_information[]);

    int Keepalive2(DogcomSocket &socket, int &keepalive_counter, int &first);

    void GenCrc(unsigned char seed[], int encrypt_type, unsigned char crc[]);

    void Keepalive2PacketBuilder(unsigned char keepalive_2_packet[], int keepalive_counter, int filepacket, int type);

signals:

    void ReportOffline(LoginResult reason);

    void ReportOnline();

    void ReportIpAddress(unsigned char x1, unsigned char x2, unsigned char x3, unsigned char x4);
};

#endif //DRCOMJLUQT_DOGCOM_H
