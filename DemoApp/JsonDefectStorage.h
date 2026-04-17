#pragma once
#include "IDefectStorage.h" 
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

class JsonDefectStorage : public IDefectStorage {
public:
    // 🟢 1. 全量读取 (软件启动时用)
    QList<DefectData> loadAll(const QString& uri) override {
        QList<DefectData> resultList;
        QFile file(uri);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return resultList;

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (!doc.isArray()) return resultList;

        QJsonArray rootArray = doc.array();
        for (const QJsonValue& val : rootArray) {
            if (!val.isObject()) continue;
            QJsonObject defectObj = val.toObject();

            DefectData data;
            data.uuid = defectObj["uuid"].toString();
            data.name = defectObj["name"].toString();
            data.type = static_cast<DrawShape>(defectObj["type"].toInt());
            data.attributes = defectObj["attributes"].toObject().toVariantMap();

            QJsonArray pointsArr = defectObj["points"].toArray();
            QPainterPath path;
            for (int i = 0; i < pointsArr.size(); ++i) {
                QJsonObject ptObj = pointsArr[i].toObject();
                double x = ptObj["x"].toDouble();
                double y = ptObj["y"].toDouble();
                if (i == 0) path.moveTo(x, y);
                else path.lineTo(x, y);
            }

            if (data.type == Shape_Polygon && pointsArr.size() > 2) {
                path.closeSubpath();
            }
            data.shape = path;
            resultList.append(data);
        }
        return resultList;
    }

    // 🟢 5. 模拟数据库的区间查询
    QList<DefectData> loadByRange(const QString& uri, double startMile, double endMile) override {
        // 1. 先把本地 JSON 里所有的病害读出来
        QList<DefectData> allDefects = loadAll(uri);
        QList<DefectData> filteredDefects;

        // 2. 遍历过滤，模拟 SQL 的 WHERE 语句
        for (const auto& defect : allDefects) {
            //  把里程信息塞在 attributes 这个万能包里
            if (defect.attributes.contains("start_mileage") && defect.attributes.contains("end_mileage")) {
                double dStart = defect.attributes["start_mileage"].toDouble();
                double dEnd = defect.attributes["end_mileage"].toDouble();

                // 🟢 核心算法：判断两个区间是否重叠 (病害的区间 vs 屏幕请求的区间)
                // 只要病害的起点小于请求的终点，且病害的终点大于请求的起点，就算在视野内！
                if (dStart <= endMile && dEnd >= startMile) {
                    filteredDefects.append(defect);
                }
            }
        }
        qDebug() << QString::fromLocal8Bit("🔍 模拟区间查询 [%1 - %2], 命中 %3 条病害")
            .arg(startMile).arg(endMile).arg(filteredDefects.size());

        return filteredDefects;
    }

    // 🟢 2. 全量保存
    bool saveAll(const QList<DefectData>& defects, const QString& uri) override {
        QJsonArray rootArray;
        for (const auto& data : defects) {
            QJsonObject defectObj;
            defectObj["uuid"] = data.uuid;
            defectObj["name"] = data.name;
            defectObj["type"] = data.type;
            defectObj["attributes"] = QJsonObject::fromVariantMap(data.attributes);

            QJsonArray pointsArr;
            for (int i = 0; i < data.shape.elementCount(); ++i) {
                const QPainterPath::Element& e = data.shape.elementAt(i);
                if (e.type == QPainterPath::MoveToElement || e.type == QPainterPath::LineToElement) {
                    QJsonObject ptObj; ptObj["x"] = e.x; ptObj["y"] = e.y;
                    pointsArr.append(ptObj);
                }
            }
            defectObj["points"] = pointsArr;
            rootArray.append(defectObj);
        }

        QFile file(uri);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        file.write(QJsonDocument(rootArray).toJson(QJsonDocument::Indented));
        file.close();
        return true;
    }

    // 🟢 3. 增量新增 
    bool addOne(const DefectData& defect, const QString& uri) override {
        QList<DefectData> all = loadAll(uri);
        all.append(defect);
        return saveAll(all, uri);
    }

    // 🟢 4. 增量删除 
    bool deleteOne(const QString& uuid, const QString& uri) override {
        QList<DefectData> all = loadAll(uri);
        // 移除匹配 UUID 的那条数据
        all.erase(std::remove_if(all.begin(), all.end(),
            [&](const DefectData& d) { return d.uuid == uuid; }), all.end());
        return saveAll(all, uri);
    }
};