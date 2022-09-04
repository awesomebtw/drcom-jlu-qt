//
// Created by btw on 2022/9/3.
//

#include <cryptopp/modes.h>
#include <cryptopp/sm4.h>
#include <cryptopp/filters.h>
#include "utils.h"

Qt::CheckState Utils::BooleanToCheckState(bool val)
{
    if (val) {
        return Qt::CheckState::Checked;
    } else {
        return Qt::CheckState::Unchecked;
    }
}

bool Utils::CheckStateToBoolean(Qt::CheckState val)
{
    return val != Qt::CheckState::Unchecked;
}

// key for encryption algorithm
constexpr static const char encKey[] = "@h\\Q\x1AME\fN4kz}%\\K";
constexpr static const char iv[] = "~pG&6\x19<V@3=e\x14,\t{";

QByteArray Utils::Encrypt(const QByteArray &arr, const QByteArray &salt)
{
    using namespace CryptoPP;

    CBC_CTS_Mode<SM4>::Encryption e;
    e.SetKeyWithIV(reinterpret_cast<const byte *>(encKey), SM4::DEFAULT_KEYLENGTH, reinterpret_cast<const byte *>(iv));

    StreamTransformationFilter encryptor(e, nullptr);
    encryptor.Put(reinterpret_cast<const byte *>(salt.data()), salt.length());
    encryptor.Put(reinterpret_cast<const byte *>(arr.data()), arr.length());
    for (auto i = arr.length(); i <= SM4::DEFAULT_KEYLENGTH; i++) {
        encryptor.Put(byte('\0')); // add padding if arr is too short
    }
    encryptor.MessageEnd();

    QByteArray res;
    if (encryptor.AnyRetrievable()) {
        res.resize(static_cast<qsizetype>(encryptor.MaxRetrievable()));
        encryptor.Get(reinterpret_cast<byte *>(&res[0]), res.length());
    }

    return res;
}

QByteArray Utils::Decrypt(const QByteArray &arr, const QByteArray &salt)
{
    using namespace CryptoPP;

    CBC_CTS_Mode<SM4>::Decryption d;
    d.SetKeyWithIV(reinterpret_cast<const byte *>(encKey), SM4::DEFAULT_KEYLENGTH, reinterpret_cast<const byte *>(iv));

    StreamTransformationFilter decryptor(d, nullptr);
    decryptor.Put(reinterpret_cast<const byte *>(arr.data()), arr.length());
    decryptor.MessageEnd();

    QByteArray res;
    if (decryptor.AnyRetrievable()) {
        res.resize(static_cast<qsizetype>(decryptor.MaxRetrievable()));
        decryptor.Get(reinterpret_cast<byte *>(&res[0]), res.length());
        res.resize(static_cast<qsizetype>(qstrlen(res.data()))); // remove padding NULs
    }

    return res.right(res.length() - salt.length());
}
