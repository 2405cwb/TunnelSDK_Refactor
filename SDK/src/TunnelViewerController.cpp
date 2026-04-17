#include "TunnelViewerController.h"

// 这里才需要包含具体的实现类
#include "TiledGraphicsView.h" 
#include "AbstractSourceFactory.h"
#include "./items/DefectShapeItem.h"   
#include <QDebug>
#include<QSqlDatabase>
#include<QSqlQuery>
#include<QUuid>

TunnelViewerController::TunnelViewerController(TiledGraphicsView* view, QObject* parent)
    : QObject(parent), m_view(view), m_factory(nullptr)
{
}

TunnelViewerController::~TunnelViewerController()
{
    if (m_factory) delete m_factory;
    // 如果工厂是你接管的，可以在这里 delete m_factory;
    // 但通常建议谁 new 谁 delete，或者用智能指针
}

void TunnelViewerController::setSourceFactory(AbstractSourceFactory* factory)
{
    m_factory = factory;
}

bool TunnelViewerController::loadRoute(const QString& rootPath)
{
    if (!m_view || !m_factory) {
        qWarning() << QString::fromLocal8Bit( "Controller未初始化 View 或 Factory");
        return false;
    }
    m_rootPath = rootPath;
	m_view->setLayoutOrientation(m_factory->layoutOrientation());

    m_view->set_scrollSpeed(m_factory->scrollSpeed());



    clear(); // 先清空旧的

    QDir dir(rootPath);
    if (!dir.exists()) return false;

    // 1. 获取所有子文件夹
    QList<DbImageInfo> subFolders = scanDatabaseFolder(rootPath);

    // 2.  排序  
    if (m_factory) {
        m_factory->sortDatabaseFolder(subFolders);
    }
     
    //  获取拼接方向
    auto orientation = m_factory->layoutOrientation();

    // =========================================================
    // 🟢 性能优化 1: 暂时禁止 View 的视口更新
    // 否则每添加一个 Item，View 可能会尝试重绘或计算，拖慢 10倍 速度
    // =========================================================
    m_view->setUpdatesEnabled(false);
    if (m_view->scene()) m_view->scene()->blockSignals(true); // 暂时屏蔽场景信号
    // =========================================================
    // 🟢 UI 优化: 创建进度条
    // =========================================================
    int totalCount = subFolders.size();
    QProgressDialog progress(QString::fromLocal8Bit("正在加载项目数据..."),
        QString::fromLocal8Bit("取消"),
        0, totalCount, m_view);
    progress.setWindowModality(Qt::WindowModal); // 模态窗口，阻塞用户操作其他地方
    progress.setMinimumDuration(500); // 只有超过0.5秒的任务才显示进度条，避免闪烁
    progress.setValue(0);

    // 5. 循环加载
    int i = 0;
     
    // 3. 循环加载
    for (const DbImageInfo& dbInfo : subFolders) {
       
        // 🟢 更新进度条
        progress.setValue(i++);
        // 如果用户点了取消
        if (progress.wasCanceled()) {
            qDebug() << "Loading canceled by user.";
            // 恢复更新
            m_view->setUpdatesEnabled(true);
            if (m_view->scene()) m_view->scene()->blockSignals(false);
            return false;
        }
        QString dbPath = dbInfo.dbFilePath;
        // 使用工厂创建数据源 (多态调用)
        AbstractTileSource* source = m_factory->create(dbPath);

        qreal fixed = 0;

        // 检查数据源是否有效
        if (source && source->isValid()) {
            // 创建图元 
            m_view->addLayer(source);
             
        }
        else {
            // 如果无效，记得删除防止内存泄漏
            if (source) delete source;
            qWarning() << "Skipped invalid section:" << dbPath;
        }
    }
    // 进度条完成
    progress.setValue(totalCount); 
    // 恢复 View 更新 
    if (m_view->scene()) m_view->scene()->blockSignals(false);
    m_view->setUpdatesEnabled(true); 
    m_view->resetToFit();
    return true;
}

QList<DbImageInfo> TunnelViewerController::scanDatabaseFolder(const QString& rootFolder)
{
    QList<DbImageInfo> dbList;
    QDir dir(rootFolder);
    if (!dir.exists()) return dbList;

    // 只过滤出 .db 文件
    QStringList filters;
    filters << "*.db";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo& fileInfo : fileList) {
        DbImageInfo info;
        info.dbFilePath = fileInfo.absoluteFilePath();
        info.width = 0;
        info.height = 0;
        info.tileSize = 2048; // 默认防错

        // 为每个文件生成临时连接名，防止冲突
        QString connName = QUuid::createUuid().toString();
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
            db.setDatabaseName(info.dbFilePath);
            db.setConnectOptions("QSQLITE_OPEN_READONLY"); // 只读极速模式

            if (db.open()) {
                QSqlQuery query(db);
                // 1. 读尺寸信息
                if (query.exec("SELECT key, value FROM info")) {
                    while (query.next()) {
                        QString key = query.value(0).toString();
                        QString val = query.value(1).toString();
                        if (key == "width") info.width = val.toInt();
                        else if (key == "height") info.height = val.toInt();
                        else if (key == "tileSize") info.tileSize = val.toInt();
                        else if (key == "originalName")
                        {
                            QFileInfo file(val);
                            info.originalName = file.completeBaseName();
                        }
                    }
                }
                
            }
        }
        QSqlDatabase::removeDatabase(connName);

        // 只有成功读到了宽高的数据库，才认为是有效工程
        if (info.width > 0 && info.height > 0) {
            dbList.append(info);
        }
    }
    return dbList;
}

 
void TunnelViewerController::clear()
{
    if (m_view && m_view->scene()) {
        m_view->scene()->clear();
    }
    //m_currentTotalLength = 0;
}