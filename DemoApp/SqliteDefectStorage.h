#pragma once
#include "IDefectStorage.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSqlError> // 确保包含了这个
class SqliteDefectStorage : public IDefectStorage {
public:
    SqliteDefectStorage() {
        // 给这个存储策略分配一个固定的数据库连接名
        m_connectionName = "TunnelDefectDB_Conn";
    }

    ~SqliteDefectStorage() {
        // 析构时关闭数据库
        if (QSqlDatabase::contains(m_connectionName)) {
            QSqlDatabase::database(m_connectionName).close();
        }
    }

    // =========================================================
    // 🟢 1. 全量读取 
    // =========================================================
    QList<DefectData> loadAll(const QString& uri) override {
        QList<DefectData> resultList;
        if (!initDatabase(uri)) return resultList;

        QSqlQuery query(QSqlDatabase::database(m_connectionName));
        query.exec("SELECT * FROM defects");

        while (query.next()) {
            resultList.append(parseRowToDefect(query));
        }
        return resultList;
    }

    // =========================================================
    // 🟢 2. 真正的数据库区间查询 (极速过滤！)
    // =========================================================
    QList<DefectData> loadByRange(const QString& uri, double startMile, double endMile) override {
        QList<DefectData> resultList;
        if (!initDatabase(uri)) return resultList;

        QSqlQuery query(QSqlDatabase::database(m_connectionName));

        // 💥 这里就是 SQLite 降维打击的地方：直接让数据库引擎去过滤！
        query.prepare("SELECT * FROM defects WHERE start_mileage <= ? AND end_mileage >= ?");
        query.addBindValue(endMile);
        query.addBindValue(startMile);

        if (!query.exec()) {
            qWarning() << "❌ 区间查询失败:" << query.lastError().text();
            return resultList;
        }

        while (query.next()) {
            resultList.append(parseRowToDefect(query));
        }

        qDebug() << QString::fromLocal8Bit("🔍 数据库区间查询 [%1 - %2], 命中 %3 条")
            .arg(startMile).arg(endMile).arg(resultList.size());
        return resultList;
    }

    // =========================================================
    // 🟢 3. 增量新增 (极其轻量，毫秒级)
    // =========================================================
    bool addOne(const DefectData& defect, const QString& uri) override {
        if (!initDatabase(uri)) return false;

        // 提取里程信息（如果没有，默认给 0）
        double startMile = defect.attributes.value("start_mileage", 0.0).toDouble();
        double endMile = defect.attributes.value("end_mileage", 0.0).toDouble();

        QSqlQuery query(QSqlDatabase::database(m_connectionName));
        // 使用 REPLACE 语法：如果 uuid 已经存在就覆盖，不存在就插入
        query.prepare(
            "REPLACE INTO defects (uuid, name, type, start_mileage, end_mileage, shape_json, attributes_json) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)"
        );
        query.addBindValue(defect.uuid);
        query.addBindValue(defect.name);
        query.addBindValue(static_cast<int>(defect.type));
        query.addBindValue(startMile);
        query.addBindValue(endMile);
        query.addBindValue(pathToJSONString(defect.shape));
        query.addBindValue(attributesToJSONString(defect.attributes));

        if (!query.exec()) {
            qWarning() << "❌ 插入数据失败:" << query.lastError().text();
            return false;
        }
        return true;
    }

    // =========================================================
    // 🟢 4. 增量删除 
    // =========================================================
    bool deleteOne(const QString& uuid, const QString& uri) override {
        if (!initDatabase(uri)) return false;

        QSqlQuery query(QSqlDatabase::database(m_connectionName));
        query.prepare("DELETE FROM defects WHERE uuid = ?");
        query.addBindValue(uuid);

        return query.exec();
    }

    // =========================================================
    // 🟢 5. 全量保存 (利用事务机制，一万条数据不到半秒钟写完)
    // =========================================================
    bool saveAll(const QList<DefectData>& defects, const QString& uri) override {
        if (!initDatabase(uri)) return false;

        QSqlDatabase db = QSqlDatabase::database(m_connectionName);
        db.transaction(); // 💥 开启事务：极大提升批量写入性能

        // 先清空旧数据
        QSqlQuery clearQuery(db);
        clearQuery.exec("DELETE FROM defects");

        // 循环插入
        for (const auto& data : defects) {
            addOne(data, uri); // 内部已经是预编译指令，速度很快
        }

        db.commit(); // 💥 提交事务：一次性写入磁盘
        qDebug() << "✅ 成功将" << defects.size() << "个病害保存至 SQLite 数据库";
        return true;
    }

private:
    QString m_connectionName;

    // 内部核心函数：初始化数据库并自动建表
    bool initDatabase(const QString& uri) {
        QSqlDatabase db;
        if (QSqlDatabase::contains(m_connectionName)) {
            db = QSqlDatabase::database(m_connectionName);
        }
        else {

            // 🟢 排查点 1：检查有没有 SQLite 驱动！
            if (!QSqlDatabase::drivers().contains("QSQLITE")) {
                qCritical() << QString::fromLocal8Bit("❌ 严重错误：你的 Qt 环境缺少 QSQLITE 驱动！");
                return false;
            }
            db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
        }
       
        QFileInfo fileInfo(uri);
        QDir dir = fileInfo.absoluteDir();
        if (!dir.exists()) {
            qDebug() << QString::fromLocal8Bit( "⚠️ 数据库目录不存在，正在自动创建:" )<< dir.absolutePath();
            if (!dir.mkpath(".")) {
                qCritical() << QString::fromLocal8Bit("❌ 创建目录失败，请检查 C 盘权限！");
                return false;
            }
        }
        db.setDatabaseName(uri);

        if (!db.isOpen() && !db.open()) {
            // 🟢 排查点 3：打印真正的死因！
            qCritical() << QString::fromLocal8Bit("❌ 无法打开 SQLite 数据库:") << uri;
            qCritical() << QString::fromLocal8Bit("❌ 具体报错原因:") << db.lastError().text();
            return false;
        }

        // 💥 巧妙设计：为了让里程查询极快，把起止里程单独抽出来作为物理列 (REAL)
        // 图形和万能属性依然打包成 JSON 字符串存入 TEXT 字段
        QSqlQuery query(db);
        QString createTableSQL =
            "CREATE TABLE IF NOT EXISTS defects ("
            "uuid TEXT PRIMARY KEY, "
            "name TEXT, "
            "type INTEGER, "
            "start_mileage REAL, "
            "end_mileage REAL, "
            "shape_json TEXT, "
            "attributes_json TEXT)";
        query.exec(createTableSQL);

        // 如果想让里程查询快到飞起，加个索引
        query.exec("CREATE INDEX IF NOT EXISTS idx_mileage ON defects(start_mileage, end_mileage)");

        return true;
    }

    // 解析单行 SQL 数据组装成 DefectData
    DefectData parseRowToDefect(QSqlQuery& query) const {
        DefectData data;
        data.uuid = query.value("uuid").toString();
        data.name = query.value("name").toString();
        data.type = static_cast<DrawShape>(query.value("type").toInt());

        // 恢复万能属性
        QString attrJsonStr = query.value("attributes_json").toString();
        data.attributes = QJsonDocument::fromJson(attrJsonStr.toUtf8()).object().toVariantMap();

        // 恢复几何图形
        QString shapeJsonStr = query.value("shape_json").toString();
        QJsonArray pointsArr = QJsonDocument::fromJson(shapeJsonStr.toUtf8()).array();
        QPainterPath path;
        for (int i = 0; i < pointsArr.size(); ++i) {
            QJsonObject ptObj = pointsArr[i].toObject();
            if (i == 0) path.moveTo(ptObj["x"].toDouble(), ptObj["y"].toDouble());
            else path.lineTo(ptObj["x"].toDouble(), ptObj["y"].toDouble());
        }
        if (data.type == Shape_Polygon && pointsArr.size() > 2) {
            path.closeSubpath();
        }
        data.shape = path;

        // 【关键】：数据库里存了一份里程作为索引，万能属性里也还有一份
        // 这里需要确保内存里的 Item 在业务逻辑读取时，也能拿到里程（兜底操作）
        if (!data.attributes.contains("start_mileage")) {
            data.attributes["start_mileage"] = query.value("start_mileage").toDouble();
            data.attributes["end_mileage"] = query.value("end_mileage").toDouble();
        }

        return data;
    }

    // 将 QPainterPath 转换成 JSON 字符串
    QString pathToJSONString(const QPainterPath& path) const {
        QJsonArray pointsArr;
        for (int i = 0; i < path.elementCount(); ++i) {
            const QPainterPath::Element& e = path.elementAt(i);
            if (e.type == QPainterPath::MoveToElement || e.type == QPainterPath::LineToElement) {
                QJsonObject ptObj; ptObj["x"] = e.x; ptObj["y"] = e.y;
                pointsArr.append(ptObj);
            }
        }
        return QString(QJsonDocument(pointsArr).toJson(QJsonDocument::Compact));
    }

    // 将 万能属性包 转换成 JSON 字符串
    QString attributesToJSONString(const QVariantMap& attrs) const {
        QJsonObject obj = QJsonObject::fromVariantMap(attrs);
        return QString(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }
};