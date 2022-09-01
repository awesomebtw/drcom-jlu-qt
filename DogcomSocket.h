#ifndef MYUDPSOCKET_DOGCOMSOCKET_H
#define MYUDPSOCKET_DOGCOMSOCKET_H

#include <exception>

#ifdef WIN32

#include <winsock2.h>

#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#endif

enum class DogcomError {
    WSA_START_UP,
    SOCKET,
    BIND,
    SET_SOCK_OPT_TIMEOUT,
    SET_SOCK_OPT_REUSE
};

class DogcomSocketException : public std::exception {
public:
    DogcomError errCode;
    int realErrCode;

    const char *what() const noexcept override; // NOLINT(modernize-use-nodiscard)
    DogcomSocketException(DogcomError errCode, int realErrCode);
};

class DogcomSocket {
private:
    int sockfd = -1;
    struct sockaddr_in bind_addr{};
    struct sockaddr_in dest_addr{};

public:
    DogcomSocket() = default;

    void init();

    int write(const char *buf, int len);

    int read(char *buf);

    virtual ~DogcomSocket();
};

#endif //MYUDPSOCKET_DOGCOMSOCKET_H
