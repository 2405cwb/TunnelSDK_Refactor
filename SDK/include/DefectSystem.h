#pragma once
#include <QString>
#include <QPainterPath>
#include <QVariantMap>
#include "TunnelGlobal.h"
struct DefectData {
    QString uuid;
    QString name;
    DrawShape type;
    QPainterPath shape; // 如果包含 path，记得 include <QPainterPath> 
    int defectCode = 0;     // 💥 新增：业务病害代码

    QVariantMap attributes; // 💥 核心扩展点：万能属性包
};