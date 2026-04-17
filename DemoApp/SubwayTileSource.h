#ifndef SUBWAYTILESOURCE_H
#define SUBWAYTILESOURCE_H

#include "AbstractTileSource.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include<QSize>
#include<QUuid>
#include<QSqlQuery>
// 这是一个具体的实现类，专门用于读取“标准切片格式”的文件夹
class SubwayTileSource : public AbstractTileSource {
public:
    SubwayTileSource(QString dbFilePath) : m_dbPath(dbFilePath) {
        // 生成唯一连接名，防止 Qt 在多线程或同时打开多个大图时报连接冲突警告
        QString connName = QUuid::createUuid().toString();

        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
            db.setDatabaseName(m_dbPath);
            db.setConnectOptions("QSQLITE_OPEN_READONLY"); // 开启只读模式，提升打开速度

            if (db.open()) {
                QSqlQuery query(db);

                // 1. 读取 info 表中的基础尺寸信息
                if (query.exec("SELECT key, value FROM info")) {
                    while (query.next()) {
                        QString key = query.value(0).toString();
                        QString val = query.value(1).toString();

                        if (key == "width") m_width = val.toInt();
                        else if (key == "height") m_height = val.toInt();
                        else if (key == "tileSize") m_tileSize = val.toInt();
                        else if (key == "originalName")
                        {
                            QFileInfo file(val);
                            m_imageName = file.completeBaseName();
                        }

                    }
                }

                // 容错处理，与切图端保持一致
                if (m_tileSize <= 0) m_tileSize = 2048; 
                
            }
            else {
                qWarning() << "无法打开切片数据库:" << m_dbPath;
            }
        }
        // 离开作用域后必须移除连接，释放底层 SQLite 文件句柄
        QSqlDatabase::removeDatabase(connName);
    }


	// 验证数据源是否有效
    bool isValid() const override {
        return (m_width > 0 && m_height > 0);
	}

    // 实现接口
    QSize totalSize() const override { return QSize(m_width, m_height); }
    int tileSize() const override { return m_tileSize; }
    QString oriImageName() const override { return m_imageName; }

  
    QString getDbPath() const { return m_dbPath; }

    QString getThumbnailImage() const override { return m_dbPath; }
     

private:
    QString m_dbPath;
    QString m_imageName; 
    int m_width = 0;
    int m_height = 0;
    int m_tileSize = 1024;
};

#endif // SUBWAYTILESOURCE_H