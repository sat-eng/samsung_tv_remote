#pragma once

#include <QString>

struct TvDevice {
    QString name;
    QString ip;
    QString model;
    QString id;

    [[nodiscard]] QString displayName() const {
        QString label = name.isEmpty() ? QStringLiteral("Samsung TV") : name;
        if (!model.isEmpty()) label += QStringLiteral(" (%1)").arg(model);
        return QStringLiteral("%1 — %2").arg(label, ip);
    }
};
