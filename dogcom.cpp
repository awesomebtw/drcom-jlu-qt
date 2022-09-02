#include "dogcom.h"

#include <QDebug>
#include <utility>
#include "constants.h"
#include <cstring>

#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1

#include <cryptopp/md4.h>
#include <cryptopp/md5.h>
#include <cryptopp/sha.h>

#ifdef MSC_VER
#define VLA(type, name, len) type *name = _Alloca(sizeof(type) * len)
#else
#define VLA(type, name, len) type name[len]
#endif

static thread_local std::mt19937 gen{std::random_device().operator()()};
static thread_local std::uniform_int_distribution dist{0, 127};

DogCom::DogCom(InterruptibleSleeper *s)
{
    sleeper = s;
    log = true;
}

void DogCom::Stop()
{
    sleeper->Interrupt();
}

void DogCom::FillConfig(QString a, QString p, QString m)
{
    account = std::move(a);
    password = std::move(p);
    macAddr = std::move(m);
}

void DogCom::print_packet(const char msg[], const unsigned char *packet, size_t length) const
{
    if (!log)
        return;
    QString x;
    for (int i = 0; i < length; i++) {
        x.append(QString::asprintf("%02x ", packet[i]));
    }
    qDebug("%s %s", msg, x.toStdString().c_str());
}

void DogCom::run()
{
    // 每一个ReportOffline后边都跟着一个return语句，防止程序逻辑混乱
    qDebug() << Qt::endl;
    qDebug() << "Starting dogcom...";
    // 后台登录维持连接的线程
    gen.seed(std::random_device().operator()());

    DogcomSocket skt;
    try {
        skt.init();
    }
    catch (DogcomSocketException &e) {
        qCritical() << "dogcom socket init error"
                    << " msg: " << e.what();
        switch (e.errCode) {
            case DogcomError::WSA_START_UP:
                emit ReportOffline(LoginResult::WSA_STARTUP);
                return;
            case DogcomError::SOCKET:
                emit ReportOffline(LoginResult::CREATE_SOCKET);
                return;
            case DogcomError::BIND:
                emit ReportOffline(LoginResult::BIND_FAILED);
                return;
            case DogcomError::SET_SOCK_OPT_TIMEOUT:
                emit ReportOffline(LoginResult::SET_SOCKET_TIMEOUT);
                return;
            case DogcomError::SET_SOCK_OPT_REUSE:
                emit ReportOffline(LoginResult::SET_SOCKET_REUSE);
                return;
        }
    }

    qInfo() << "Bind Success!";
    unsigned char seed[4];
    unsigned char auth_information[16];
    if (!dhcp_challenge(skt, seed)) {
        qCritical() << "dhcp challenge failed";
        emit ReportOffline(LoginResult::CHALLENGE_FAILED);
        return;
    } else {
        //challenge成功 开始登录
        qDebug() << "trying to login...";
        sleeper->Sleep(200); // 0.2 sec
        qDebug() << "Wait for 0.2 second done.";
        LoginResult offLineReason;
        if ((offLineReason = dhcp_login(skt, seed, auth_information)) == LoginResult::NOT_AN_ERROR) {
            // 登录成功
            emit ReportOnline();
            int keepalive_counter = 0;
            int first = 1;
            while (true) {
                if (!keepalive_1(skt, auth_information)) {
                    sleeper->Sleep(200); // 0.2 second
                    if (keepalive_2(skt, keepalive_counter, first)) {
                        continue;
                    }
                    qDebug() << "Keepalive in loop.";
                    if (!sleeper->Sleep(20000)) {
                        // 先注销再退出，这样DogcomSocket的析构函数一定会执行
                        // 窗口函数部分处理是用户注销还是用户退出
                        qDebug() << "Interruptted by user";
                        emit ReportOffline(LoginResult::USER_LOGOUT);
                        return;
                    }
                } else {
                    qDebug() << "ReportOffline OfflineReason::TIMEOUT caused by keepalive_1";
                    emit ReportOffline(LoginResult::TIMEOUT);
                    return;
                }
            }
        } else {
            // 登录失败，提示用户失败原因
            emit ReportOffline(offLineReason);
            return;
        }
    }
}

bool DogCom::dhcp_challenge(DogcomSocket &socket, unsigned char seed[])
{
    constexpr size_t CHALLENGE_PACKET_LEN = 20, RECV_PACKET_LEN = 1024;

    qDebug() << "Begin dhcp challenge...";
    unsigned char challenge_packet[CHALLENGE_PACKET_LEN]{}, recv_packet[RECV_PACKET_LEN]{};
    challenge_packet[0] = 0x01;
    challenge_packet[1] = 0x02;
    challenge_packet[2] = dist(gen) & 0xff;
    challenge_packet[3] = dist(gen) & 0xff;
    challenge_packet[4] = 0x68;

    qDebug() << "writing to dest...";
    if (socket.write(reinterpret_cast<const char *>(challenge_packet), sizeof(challenge_packet)) <= 0) {
        qCritical() << "Failed to send challenge";
        return false;
    }
    print_packet("[Challenge sent]", challenge_packet, sizeof(challenge_packet));

    qDebug() << "reading from dest...";
    if (socket.read(reinterpret_cast<char *>(recv_packet)) <= 0) {
        qCritical() << "Failed to recv";
        return false;
    }
    qDebug() << "read done";
    print_packet("[Challenge recv]", recv_packet, 76);

    if (recv_packet[0] != 0x02) {
        return false;
    }
    // recv_packet[20]到[24]是ip地址
    emit ReportIpAddress(recv_packet[20], recv_packet[21], recv_packet[22], recv_packet[23]);

    memcpy(seed, &recv_packet[4], 4);
    qDebug() << "End dhcp challenge";
    return true;
}

enum LoginErrorCode {
    LOGIN_CHECK_MAC = 0x01,
    LOGIN_SERVER_BUSY = 0x02,
    LOGIN_WRONG_PASS = 0x03,
    LOGIN_NOT_ENOUGH = 0x04,
    LOGIN_FREEZE_UP = 0x05,
    LOGIN_NOT_ON_THIS_IP = 0x07,
    LOGIN_NOT_ON_THIS_MAC = 0x0B,
    LOGIN_TOO_MUCH_IP = 0x14,
    LOGIN_UPDATE_CLIENT = 0x15,
    LOGIN_NOT_ON_THIS_IP_MAC = 0x16,
    LOGIN_MUST_USE_DHCP = 0x17
};

LoginResult DogCom::dhcp_login(DogcomSocket &socket, unsigned char seed[], unsigned char auth_information[])
{
    size_t login_packet_size;
    size_t length_padding = 0;
    size_t JLU_padding = 0;

    if (password.length() > 8) {
//        length_padding = password.length() - 8 + (length_padding % 2);
        if (password.length() != 16) {
            JLU_padding = password.length() / 4;
        }
        length_padding = 28 + password.length() - 8 + JLU_padding;
    }
    login_packet_size = 338 + length_padding;

    // checksum1[8]改为checksum1[16]
    VLA(uint8_t, login_packet, login_packet_size);
    memset(login_packet, 0, login_packet_size);
    unsigned char recv_packet[1024]{}, MD5A[16]{}, MACxorMD5A[6]{}, MD5B[16]{}, checksum1[16]{}, checksum2[4]{};

    // build login-packet
    login_packet[0] = 0x03;
    login_packet[1] = 0x01;
    login_packet[2] = 0x00;
    login_packet[3] = account.length() + 20;

    size_t MD5A_len = 6 + password.length();
    CryptoPP::Weak::MD5 md5aDigest;
    VLA(uint8_t, MD5A_str, MD5A_len);
    MD5A_str[0] = 0x03;
    MD5A_str[1] = 0x01;
    memcpy(MD5A_str + 2, seed, 4);
    memcpy(MD5A_str + 6, password.toStdString().c_str(), password.length());
    md5aDigest.CalculateDigest(MD5A, MD5A_str, MD5A_len);
    memcpy(login_packet + 4, MD5A, 16);
    memcpy(login_packet + 20, account.toStdString().c_str(), account.length());
    login_packet[56] = 0x20;
    login_packet[57] = 0x03;
    uint64_t sum = 0;
    // unpack
    for (int i = 0; i < 6; i++) {
        sum = (int) MD5A[i] + sum * 256;
    }

    // unpack
    // 将QString转换为unsigned char
    unsigned char mac_binary[6];

    std::sscanf(macAddr.toLocal8Bit().data(), "%hhX%*c%hhX%*c%hhX%*c%hhX%*c%hhX%*c%hhX",
                &mac_binary[0], &mac_binary[1], &mac_binary[2],
                &mac_binary[3], &mac_binary[4], &mac_binary[5]);
    // 将QString转换为unsigned char结束
    uint64_t mac = 0;
    for (unsigned char i: mac_binary) {
        mac = i + mac * 256;
    }
    sum ^= mac;
    // pack
    for (int i = 6; i > 0; i--) {
        MACxorMD5A[i - 1] = (unsigned char) (sum % 256);
        sum /= 256;
    }
    memcpy(login_packet + 58, MACxorMD5A, sizeof(MACxorMD5A));

    CryptoPP::Weak::MD5 md5bDigest;
    size_t MD5B_len = 9 + password.length();
    VLA(uint8_t, MD5B_str, MD5B_len);
    memset(MD5B_str, 0, MD5B_len);
    MD5B_str[0] = 0x01;
    memcpy(&MD5B_str[1], password.toStdString().c_str(), password.length());
    memcpy(&MD5B_str[password.length() + 1], seed, 4);
    md5aDigest.CalculateDigest(MD5B, MD5B_str, MD5B_len);
    memcpy(login_packet + 64, MD5B, 16);
    login_packet[80] = 0x01;
    unsigned char host_ip[4] = {0};
    memcpy(login_packet + 81, host_ip, 4);

    CryptoPP::Weak::MD5 checksum1Digest;
    unsigned char checksum1_str[101]{}, checksum1_tmp[4] = {0x14, 0x00, 0x07, 0x0b};
    memcpy(checksum1_str, login_packet, 97);
    memcpy(checksum1_str + 97, checksum1_tmp, 4);
    checksum1Digest.CalculateDigest(checksum1, checksum1_str, 101);
    memcpy(login_packet + 97, checksum1, 8);
    login_packet[105] = 0x01;
    memcpy(login_packet + 110, "LIYUANYUAN", 10);

    unsigned char PRIMARY_DNS[4]{10, 10, 10, 10};
    memcpy(login_packet + 142, PRIMARY_DNS, 4);
    unsigned char dhcp_server[4] = {0};
    memcpy(login_packet + 146, dhcp_server, 4);
    unsigned char OSVersionInfoSize[4] = {0x94};
    unsigned char OSMajor[4] = {0x06};
    unsigned char OSMinor[4] = {0x02};
    unsigned char OSBuild[4] = {0xf0, 0x23};
    unsigned char PlatformID[4] = {0x02};
    OSVersionInfoSize[0] = 0x94;
    unsigned char ServicePack[40] = {0x33, 0x64, 0x63, 0x37, 0x39, 0x66, 0x35, 0x32, 0x31, 0x32, 0x65, 0x38, 0x31, 0x37,
                                     0x30, 0x61, 0x63, 0x66, 0x61, 0x39, 0x65, 0x63, 0x39, 0x35, 0x66, 0x31, 0x64, 0x37,
                                     0x34, 0x39, 0x31, 0x36, 0x35, 0x34, 0x32, 0x62, 0x65, 0x37, 0x62, 0x31};
    unsigned char hostname[9] = {0x44, 0x72, 0x43, 0x4f, 0x4d, 0x00, 0xcf, 0x07, 0x68};
    memcpy(login_packet + 182, hostname, 9);
    memcpy(login_packet + 246, ServicePack, 40);
    memcpy(login_packet + 162, OSVersionInfoSize, 4);
    memcpy(login_packet + 166, OSMajor, 4);
    memcpy(login_packet + 170, OSMinor, 4);
    memcpy(login_packet + 174, OSBuild, 4);
    memcpy(login_packet + 178, PlatformID, 4);
    login_packet[310] = 0x68;
    login_packet[311] = 0x00;
    size_t counter = 312;
    unsigned int ror_padding;
    if (password.length() <= 8) {
        ror_padding = 8 - password.length();
    } else {
        ror_padding = JLU_padding;
    }

    login_packet[counter + 1] = password.length();
    counter += 2;
    for (int i = 0, x; i < password.length(); i++) {
        x = (int) MD5A[i] ^ (int) password.at(i).toLatin1();
        login_packet[counter + i] = (unsigned char) (((x << 3) & 0xff) + (x >> 5));
    }
    counter += password.length();
    login_packet[counter] = 0x02;
    login_packet[counter + 1] = 0x0c;

    VLA(unsigned char, checksum2_str, counter + 18); // [counter + 14 + 4]
    memset(checksum2_str, 0, sizeof(checksum2_str));
    unsigned char checksum2_tmp[6] = {0x01, 0x26, 0x07, 0x11};
    memcpy(checksum2_str, login_packet, counter + 2);
    memcpy(checksum2_str + counter + 2, checksum2_tmp, 6);
    memcpy(checksum2_str + counter + 8, mac_binary, 6);
    sum = 1234;

    for (int i = 0; i < counter + 14; i += 4) {
        uint64_t ret = 0;
        for (int j = 4; j > 0; j--) {
            ret = ret * 256 + (int) checksum2_str[i + j - 1];
        }
        sum ^= ret;
    }
    sum = (1968 * sum) & 0xffffffff;
    for (int j = 0; j < 4; j++) {
        checksum2[j] = (unsigned char) (sum >> (j * 8) & 0xff);
    }
    memcpy(login_packet + counter + 2, checksum2, 4);
    memcpy(login_packet + counter + 8, mac_binary, 6);
    login_packet[counter + ror_padding + 14] = 0xe9;
    login_packet[counter + ror_padding + 15] = 0x13;
    login_packet[counter + ror_padding + 14] = 0x60;
    login_packet[counter + ror_padding + 15] = 0xa2;

    qDebug() << "login_packet_size:" << login_packet_size;
    socket.write(reinterpret_cast<const char *>(login_packet), static_cast<int>(login_packet_size));
//    print_packet("[Login sent]", login_packet, login_packet_size);

    if (socket.read(reinterpret_cast<char *>(recv_packet)) <= 0) {
        qCritical() << "Failed to recv data";
        return LoginResult::TIMEOUT;
    }

//    print_packet("[Login recv]", recv_packet, 100);
    if (recv_packet[0] != 0x04) {
        qDebug() << "<<< Login failed >>>";
        if (recv_packet[0] == 0x05) {
            switch (recv_packet[4]) {
                case LOGIN_CHECK_MAC:
                    return LoginResult::CHECK_MAC;
                case LOGIN_SERVER_BUSY:
                    return LoginResult::SERVER_BUSY;
                case LOGIN_WRONG_PASS:
                    return LoginResult::WRONG_PASS;
                case LOGIN_NOT_ENOUGH:
                    return LoginResult::NOT_ENOUGH;
                case LOGIN_FREEZE_UP:
                    return LoginResult::FREEZE_UP;
                case LOGIN_NOT_ON_THIS_IP:
                    return LoginResult::NOT_ON_THIS_IP;
                case LOGIN_NOT_ON_THIS_MAC:
                    return LoginResult::NOT_ON_THIS_MAC;
                case LOGIN_TOO_MUCH_IP:
                    return LoginResult::TOO_MUCH_IP;
                    // 升级客户端这个密码错了就会弹出俩
                case LOGIN_UPDATE_CLIENT:
                    return LoginResult::WRONG_PASS;
                case LOGIN_NOT_ON_THIS_IP_MAC:
                    return LoginResult::NOT_ON_THIS_IP_MAC;
                case LOGIN_MUST_USE_DHCP:
                    return LoginResult::MUST_USE_DHCP;
                default:
                    return LoginResult::UNKNOWN;
            }
        }
        return LoginResult::UNKNOWN;
    } else {
        qDebug() << "<<< Logged in >>>";
    }

    memcpy(auth_information, &recv_packet[23], 16);

    return LoginResult::NOT_AN_ERROR;
}

int DogCom::keepalive_1(DogcomSocket &socket, unsigned char auth_information[])
{
    unsigned char keepalive_1_packet1[8] = {0x07, 0x01, 0x08, 0x00, 0x01, 0x00, 0x00, 0x00};
    unsigned char recv_packet1[1024]{}, keepalive_1_packet2[38]{}, recv_packet2[1024]{};

    if (socket.write((const char *) keepalive_1_packet1, sizeof(keepalive_1_packet1)) <= 0) {
        qCritical() << "Failed to send data";
        return 1;
    }
    qDebug() << "[Keepalive1 sent]";
    //    print_packet("[Keepalive1 sent]",keepalive_1_packet1,42);
    while (true) {
        if (socket.read((char *) recv_packet1) <= 0) {
            qCritical() << "Failed to recv data";
            return 1;
        } else {
            qDebug() << "[Keepalive1 challenge_recv]";
            //            print_packet("[Keepalive1 challenge_recv]",recv_packet1,100);
            if (recv_packet1[0] == 0x07) {
                break;
            } else if (recv_packet1[0] == 0x4d) {
                qDebug() << "Get notice packet.";
                continue;
            } else {
                qDebug() << "Bad keepalive1 challenge response received.";
                return 1;
            }
        }
    }

    unsigned char keepalive1_seed[4] = {0};
    unsigned char crc[8] = {0};
    memcpy(keepalive1_seed, &recv_packet1[8], 4);
    int encrypt_type = keepalive1_seed[0] & 3;
    gen_crc(keepalive1_seed, encrypt_type, crc);
    keepalive_1_packet2[0] = 0xff;
    memcpy(keepalive_1_packet2 + 8, keepalive1_seed, 4);
    memcpy(keepalive_1_packet2 + 12, crc, 8);
    memcpy(keepalive_1_packet2 + 20, auth_information, 16);
    keepalive_1_packet2[36] = dist(gen) & 0xff;
    keepalive_1_packet2[37] = dist(gen) & 0xff;

    if (socket.write((const char *) keepalive_1_packet2, 42) <= 0) {
        qCritical() << "Failed to send data";
        return 1;
    }

    if (socket.read((char *) recv_packet2) <= 0) {
        qCritical() << "Failed to recv data";
        return 1;
    } else {
        qDebug() << "[Keepalive1 recv]";
        //        print_packet("[Keepalive1 recv]",recv_packet2,100);

        if (recv_packet2[0] != 0x07) {
            qDebug() << "Bad keepalive1 response received.";
            return 1;
        }
    }
    return 0;
}

int DogCom::keepalive_2(DogcomSocket &socket, int &keepalive_counter, int &first)
{
    unsigned char keepalive_2_packet[40]{}, recv_packet[1024]{}, tail[4]{};

    if (first) {
        // send the file packet
        keepalive_2_packet_builder(keepalive_2_packet, keepalive_counter % 0xFF, first, 1);
        keepalive_counter++;

        if (socket.write((const char *) keepalive_2_packet, sizeof(keepalive_2_packet)) <= 0) {
            qCritical() << "Failed to send data";
            return 1;
        }

        qDebug() << "[Keepalive2_file sent]";
        //        print_packet("[Keepalive2_file sent]",keepalive_2_packet,40);
        if (socket.read((char *) recv_packet) <= 0) {
            qCritical() << "Failed to recv data";
            return 1;
        }
        qDebug() << "[Keepalive2_file recv]";
        //        print_packet("[Keepalive2_file recv]",recv_packet,40);

        if (recv_packet[0] == 0x07) {
            if (recv_packet[2] == 0x10) {
                qDebug() << "Filepacket received.";
            } else if (recv_packet[2] != 0x28) {
                qDebug() << "Bad keepalive2 response received.";
                return 1;
            }
        } else {
            qDebug() << "Bad keepalive2 response received.";
            return 1;
        }
    }

    // send the first packet
    first = 0;
    keepalive_2_packet_builder(keepalive_2_packet, keepalive_counter % 0xFF, first, 1);
    keepalive_counter++;
    socket.write((const char *) keepalive_2_packet, sizeof(keepalive_2_packet));

    qDebug() << "[Keepalive2_A sent]";
    //    print_packet("[Keepalive2_A sent]",keepalive_2_packet,40);

    if (socket.read((char *) recv_packet) <= 0) {
        qCritical() << "Failed to recv data";
        return 1;
    }

    if (recv_packet[0] == 0x07) {
        if (recv_packet[2] != 0x28) {
            printf("Bad keepalive2 response received.\n");
            return 1;
        }
    } else {
        printf("Bad keepalive2 response received.\n");
        return 1;
    }
    memcpy(tail, &recv_packet[16], 4);

    memset(keepalive_2_packet, 0, sizeof(keepalive_2_packet));
    keepalive_2_packet_builder(keepalive_2_packet, keepalive_counter % 0xFF, first, 3);
    memcpy(keepalive_2_packet + 16, tail, 4);
    keepalive_counter++;
    socket.write((const char *) keepalive_2_packet, sizeof(keepalive_2_packet));

    qDebug() << "[Keepalive2_C sent]";
    //    print_packet("[Keepalive2_C sent]",keepalive_2_packet,40);

    if (socket.read((char *) recv_packet) <= 0) {
        qCritical() << "Failed to recv data";
        return 1;
    }
    qDebug() << "[Keepalive2_D recv]";
    //    print_packet("[Keepalive2_D recv]",recv_packet,40);

    if (recv_packet[0] == 0x07) {
        if (recv_packet[2] != 0x28) {
            qDebug() << "Bad keepalive2 response received.";
            return 1;
        }
    } else {
        qDebug() << "Bad keepalive2 response received.";
        return 1;
    }

    return 0;
}

void DogCom::gen_crc(unsigned char seed[], int encrypt_type, unsigned char crc[])
{
    if (encrypt_type == 0) {
        char DRCOM_DIAL_EXT_PROTO_CRC_INIT[4] = {(char) 0xc7, (char) 0x2f, (char) 0x31, (char) 0x01};
        char gencrc_tmp[4] = {0x7e};
        memcpy(crc, DRCOM_DIAL_EXT_PROTO_CRC_INIT, 4);
        memcpy(crc + 4, gencrc_tmp, 4);
    } else if (encrypt_type == 1) {
        unsigned char hash[32] = {0};
        CryptoPP::Weak::MD5 md5;
        md5.CalculateDigest(hash, seed, 4);
        crc[0] = hash[2];
        crc[1] = hash[3];
        crc[2] = hash[8];
        crc[3] = hash[9];
        crc[4] = hash[5];
        crc[5] = hash[6];
        crc[6] = hash[13];
        crc[7] = hash[14];
    } else if (encrypt_type == 2) {
        unsigned char hash[32] = {0};
        CryptoPP::Weak::MD4 md4;
        md4.CalculateDigest(hash, seed, 4);
        crc[0] = hash[1];
        crc[1] = hash[2];
        crc[2] = hash[8];
        crc[3] = hash[9];
        crc[4] = hash[4];
        crc[5] = hash[5];
        crc[6] = hash[11];
        crc[7] = hash[12];
    } else if (encrypt_type == 3) {
        unsigned char hash[32] = {0};
        CryptoPP::SHA1 sha1;
        sha1.CalculateDigest(hash, seed, 32);
        crc[0] = hash[2];
        crc[1] = hash[3];
        crc[2] = hash[9];
        crc[3] = hash[10];
        crc[4] = hash[5];
        crc[5] = hash[6];
        crc[6] = hash[15];
        crc[7] = hash[16];
    }
}

void
DogCom::keepalive_2_packet_builder(unsigned char keepalive_2_packet[], int keepalive_counter, int filepacket, int type)
{
    keepalive_2_packet[0] = 0x07;
    keepalive_2_packet[1] = keepalive_counter;
    keepalive_2_packet[2] = 0x28;
    keepalive_2_packet[4] = 0x0b;
    keepalive_2_packet[5] = type;
    if (filepacket) {
        keepalive_2_packet[6] = 0x0f;
        keepalive_2_packet[7] = 0x27;
    } else {
        unsigned char KEEP_ALIVE_VERSION[2]{0xDC, 0x02};
        memcpy(keepalive_2_packet + 6, KEEP_ALIVE_VERSION, 2);
    }
    keepalive_2_packet[8] = 0x2f;
    keepalive_2_packet[9] = 0x12;
    if (type == 3) {
        unsigned char host_ip[4] = {0};
        memcpy(keepalive_2_packet + 28, host_ip, 4);
    }
}
