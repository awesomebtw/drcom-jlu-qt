﻿#ifndef DOGCOM_H
#define DOGCOM_H

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

    void FillConfig(QString a, QString p, QString m);

protected:
    void run() override;

private:
    InterruptibleSleeper *sleeper;
    QString account;
    QString password;
    QString macAddr;
    bool log;

    bool dhcp_challenge(DogcomSocket &socket, unsigned char seed[]);

    void print_packet(const char msg[], const unsigned char *packet, size_t length) const;

    LoginResult dhcp_login(DogcomSocket &socket, unsigned char seed[], unsigned char auth_information[]);

    int keepalive_1(DogcomSocket &socket, unsigned char auth_information[]);

    int keepalive_2(DogcomSocket &socket, int &keepalive_counter, int &first);

    void gen_crc(unsigned char seed[], int encrypt_type, unsigned char crc[]);

    void
    keepalive_2_packet_builder(unsigned char keepalive_2_packet[], int keepalive_counter, int filepacket, int type);

signals:

    void ReportOffline(LoginResult reason);

    void ReportOnline();

    void ReportIpAddress(unsigned char x1, unsigned char x2, unsigned char x3, unsigned char x4);
};

#endif // DOGCOM_H
