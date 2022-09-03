//
// Created by btw on 2022/9/3.
//

#ifndef DRCOMJLUQT_UTILS_H
#define DRCOMJLUQT_UTILS_H

#include <QApplication>

namespace Utils
{
    Qt::CheckState BooleanToCheckState(bool val);

    bool CheckStateToBoolean(Qt::CheckState val);

    QByteArray Encrypt(QByteArray arr);

    QByteArray Decrypt(QByteArray arr);
}

#endif //DRCOMJLUQT_UTILS_H
