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
constexpr static const char encKey[] = "Although never is often better than *right* now.\n"
                                       "If the implementation is hard to explain, it's a bad idea.\n"
                                       "If the implementation is easy to explain, it may be a good idea.\n"
                                       "Namespaces are one honking great idea -- let's do more of those!";

QByteArray Utils::Encrypt(QByteArray arr)
{
    using namespace CryptoPP;

    SecByteBlock iv(SM4::DEFAULT_KEYLENGTH);
    memset(iv, 0, iv.size());
    CBC_CTS_Mode<SM4>::Encryption e;
    e.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte *>(encKey), SM4::DEFAULT_KEYLENGTH, iv);

    StreamTransformationFilter encryptor(e, nullptr);
    encryptor.Put(reinterpret_cast<const CryptoPP::byte *>(arr.data()), arr.length());
    for (auto i = arr.length(); i <= SM4::DEFAULT_KEYLENGTH; i++) {
        encryptor.Put(CryptoPP::byte('\0')); // add padding if arr is too short
    }
    encryptor.MessageEnd();

    QByteArray res;
    if (encryptor.AnyRetrievable()) {
        res.resize(static_cast<qsizetype>(encryptor.MaxRetrievable()));
        encryptor.Get(reinterpret_cast<CryptoPP::byte *>(&res[0]), res.length());
    }

    return res;
}

QByteArray Utils::Decrypt(QByteArray arr)
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
