#include "SqliteImageSlicer.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QDebug>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QtConcurrent>
#include <QSemaphore>
#include <QFuture>
#include <QBuffer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMutexLocker>
#include <QUuid>

SqliteImageSlicer::SqliteImageSlicer()
    : m_tileSize(2048),
    m_jpgQuality(80),
    m_thumbWidth(2048),
    m_batchMode(MODE_UNKNOWN),
    m_processedCount(0),
    m_totalTiles(0)
{
}

SqliteImageSlicer::~SqliteImageSlicer()
{
}

void SqliteImageSlicer::setConfig(int tileSize, int jpgQuality)
{
    m_tileSize = tileSize;
    m_jpgQuality = jpgQuality;
    m_thumbWidth = tileSize; // 缩略图宽度通常与切片宽度保持一致
}

void SqliteImageSlicer::processDirectory(const QString& srcFolder, const QString& dstFolder)
{
    QDir sourceDir(srcFolder);
    if (!sourceDir.exists()) {
        qWarning() << "源文件夹不存在:" << srcFolder;
        return;
    }

    QStringList filters;
    filters << "*.jpg" << "*.jpeg" << "*.bmp" << "*.png";
    sourceDir.setNameFilters(filters);

    QFileInfoList fileList = sourceDir.entryInfoList(filters, QDir::Files);
    int totalCount = fileList.size();

    if (fileList.isEmpty()) {
        qWarning() << "未在目录中找到支持的图片文件。";
        return;
    }

    qDebug() << "开始批量处理任务，总计:" << totalCount << "张图片。断点续传机制已启用。";

    // 每次处理新目录时重置模式探测器
    m_batchMode = MODE_UNKNOWN;

    for (int i = 0; i < totalCount; ++i) {
        sliceOneImage(fileList[i].absoluteFilePath(), dstFolder, i + 1, totalCount);
    }

    qDebug() << "当前目录所有图片处理完毕。";
}

void SqliteImageSlicer::processTask(SliceTask& task)
{
    // 从大图中拷贝出当前切片区域
    QImage tileImg = task.sourceImage->copy(task.rect);
    if (!tileImg.isNull()) {
        // 使用 QBuffer 充当内存文件管道，避免直接写硬盘带来的 IO 阻塞
        // 直接在内存中完成 JPG 压缩算法，并将二进制流存入 task.jpgData
        QBuffer buffer(&task.jpgData);
        buffer.open(QIODevice::WriteOnly);
        tileImg.save(&buffer, "JPG", task.jpgQuality);
    }
}

void SqliteImageSlicer::writeTilesToDatabase(const QString& dbPath, const QList<SliceTask>& tasks)
{
    if (tasks.isEmpty()) return;

    // 加锁防止多条带并发时发生数据库写入冲突
    QMutexLocker locker(&m_dbWriteMutex);

    // 生成唯一连接名，防止 Qt 报连接重名警告
    QString connName = QUuid::createUuid().toString();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(dbPath);

        // 核心优化：
        // 1. 关闭 synchronous 压榨写入速度，因为我们使用事务打包写入，容错由外部断点机制保证。
        // 2. 开启 WAL (预写日志) 模式，提升数据库并发读取性能。
        db.setConnectOptions("QSQLITE_OPEN_URI;PRAGMA synchronous=OFF;PRAGMA journal_mode=WAL;");

        if (db.open()) {
            db.transaction();
            QSqlQuery query(db);
            query.prepare("INSERT INTO tiles (col, row, data) VALUES (?, ?, ?)");

            for (const auto& task : tasks) {
                if (!task.jpgData.isEmpty()) {
                    query.addBindValue(task.col);
                    query.addBindValue(task.row);
                    query.addBindValue(task.jpgData);
                    query.exec();
                }
            }
            db.commit();
        }
        else {
            qWarning() << "数据库打开失败，无法写入切片:" << db.lastError().text();
        }
    }
    QSqlDatabase::removeDatabase(connName);
}

bool SqliteImageSlicer::isImageAlreadyCompleted(const QString& dbPath)
{
    if (!QFile::exists(dbPath)) return false;

    bool isCompleted = false;
    QString connName = QUuid::createUuid().toString();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(dbPath);
        db.setConnectOptions("QSQLITE_OPEN_READONLY");

        if (db.open()) {
            QSqlQuery q(db);
            // 检查数据库中是否存在已完成的标记
            if (q.exec("SELECT value FROM info WHERE key = 'status'") && q.next()) {
                if (q.value(0).toString() == "completed") {
                    isCompleted = true;
                }
            }
        }
    }
    QSqlDatabase::removeDatabase(connName);

    // 如果文件存在但状态不是 completed，说明是上次崩溃残留的损坏文件，必须清理重建
    if (!isCompleted) {
        qWarning() << "发现中断或损坏的数据库缓存，正在清理重建:" << QFileInfo(dbPath).fileName();
        QFile::remove(dbPath);
    }

    return isCompleted;
}

void SqliteImageSlicer::markDatabaseCompleted(const QString& dbPath)
{
    QString connName = QUuid::createUuid().toString();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(dbPath);
        if (db.open()) {
            QSqlQuery q(db);
            q.exec("UPDATE info SET value = 'completed' WHERE key = 'status'");
        }
    }
    QSqlDatabase::removeDatabase(connName);
}

bool SqliteImageSlicer::initDatabase(const QString& dbPath, const QJsonObject& info, const QImage& thumbImg)
{
    if (QFile::exists(dbPath)) QFile::remove(dbPath);

    QString connName = QUuid::createUuid().toString();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(dbPath);
        if (!db.open()) return false;

        QSqlQuery q(db);
        q.exec("CREATE TABLE IF NOT EXISTS info (key TEXT PRIMARY KEY, value TEXT)");
        q.exec("CREATE TABLE IF NOT EXISTS thumbnail (data BLOB)");
        q.exec("CREATE TABLE IF NOT EXISTS tiles (col INTEGER, row INTEGER, data BLOB, PRIMARY KEY(col, row))");

        // 写入初始状态防崩溃
        q.exec("INSERT INTO info (key, value) VALUES ('status', 'processing')");

        q.prepare("INSERT INTO info (key, value) VALUES (?, ?)");
        for (auto it = info.begin(); it != info.end(); ++it) {
            q.addBindValue(it.key());
            q.addBindValue(it.value().toVariant().toString());
            q.exec();
        }

        // 保存缩略图数据
        if (!thumbImg.isNull()) {
            QByteArray thumbData;
            QBuffer buffer(&thumbData);
            buffer.open(QIODevice::WriteOnly);
            thumbImg.save(&buffer, "JPG", 80);

            q.prepare("INSERT INTO thumbnail (data) VALUES (?)");
            q.addBindValue(thumbData);
            q.exec();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return true;
}

QImage SqliteImageSlicer::generateThumbnailFromFile(const QString& filePath)
{
    QImageReader thumbReader(filePath);
    thumbReader.setAutoTransform(true);
    QSize oriSize = thumbReader.size();
    if (oriSize.isValid()) {
        QSize targetSize(m_thumbWidth, m_thumbWidth * oriSize.height() / oriSize.width());
        thumbReader.setScaledSize(targetSize);
        return thumbReader.read();
    }
    return QImage();
}

void SqliteImageSlicer::runFastMode(QSharedPointer<QImage> imgPtr, const QString& dbPath)
{
    int w = imgPtr->width();
    int h = imgPtr->height();
    QList<SliceTask> tasks;
    tasks.reserve((w / m_tileSize + 1) * (h / m_tileSize + 1));

    // 构建切片任务队列
    for (int y = 0; y < h; y += m_tileSize) {
        for (int x = 0; x < w; x += m_tileSize) {
            SliceTask task;
            task.sourceImage = imgPtr;
            task.col = x / m_tileSize;
            task.row = y / m_tileSize;
            task.rect = QRect(x, y, qMin(m_tileSize, w - x), qMin(m_tileSize, h - y));
            task.jpgQuality = m_jpgQuality;
            tasks.append(task);
        }
    }

    // 多线程并发执行内存裁剪与压缩
    QtConcurrent::blockingMap(tasks, processTask);

    // 将压缩好的数据一次性刷入数据库
    writeTilesToDatabase(dbPath, tasks);
}

void SqliteImageSlicer::runSafeMode(const QString& filePath, const QString& dbPath, QSize imgSize)
{
    // 每次处理两条带，控制内存水位线
    const int STRIP_HEIGHT = 2048;
    const int MAX_CONCURRENT_STRIPS = 2;
    QSemaphore semaphore(MAX_CONCURRENT_STRIPS);
    QList<QFuture<void>> runningFutures;

    int totalStrips = (imgSize.height() + STRIP_HEIGHT - 1) / STRIP_HEIGHT;
    int currentStripIdx = 0;

    QImageReader reader(filePath);

    for (int startY = 0; startY < imgSize.height(); startY += STRIP_HEIGHT) {
        semaphore.acquire(1);
        currentStripIdx++;

        // 清理已完成的线程句柄
        for (auto it = runningFutures.begin(); it != runningFutures.end();) {
            if (it->isFinished()) it = runningFutures.erase(it);
            else ++it;
        }

        int currentStripH = qMin(STRIP_HEIGHT, imgSize.height() - startY);
        QRect stripRect(0, startY, imgSize.width(), currentStripH);

        reader.setFileName(filePath);
        reader.setAutoTransform(true);
        reader.setClipRect(stripRect);
        QImage stripImg = reader.read();

        if (stripImg.isNull()) {
            semaphore.release(1);
            continue;
        }
        if (stripImg.format() == QImage::Format_Indexed8) {
            stripImg = stripImg.convertToFormat(QImage::Format_RGB888);
        }

        QSharedPointer<QImage> sharedStrip = QSharedPointer<QImage>::create(stripImg);
        QList<SliceTask> stripTasks;

        for (int localY = 0; localY < currentStripH; localY += m_tileSize) {
            int globalY = startY + localY;
            for (int globalX = 0; globalX < imgSize.width(); globalX += m_tileSize) {
                SliceTask task;
                task.sourceImage = sharedStrip;
                task.col = globalX / m_tileSize;
                task.row = globalY / m_tileSize;
                task.rect = QRect(globalX, localY, qMin(m_tileSize, imgSize.width() - globalX), qMin(m_tileSize, currentStripH - localY));
                task.jpgQuality = m_jpgQuality;
                stripTasks.append(task);
            }
        }

        // 将当前条带的切图任务丢入后台异步处理
        QFuture<void> future = QtConcurrent::run([this, stripTasks, sharedStrip, dbPath, &semaphore]() mutable {
            QtConcurrent::blockingMap(stripTasks, processTask);
            this->writeTilesToDatabase(dbPath, stripTasks);
            semaphore.release(1);
            });
        runningFutures.append(future);
    }

    // 阻塞等待最后几个条带处理完毕
    for (auto& f : runningFutures) {
        f.waitForFinished();
    }
}

void SqliteImageSlicer::sliceOneImage(const QString& filePath, const QString& outputRoot, int currentIdx, int totalImages)
{
    QFileInfo fileInfo(filePath);
    QString baseName = fileInfo.completeBaseName();
    QString dbPath = outputRoot + "/" + baseName + ".db";

    qDebug() << QString("\n[%1/%2] 开始处理文件: %3").arg(currentIdx).arg(totalImages).arg(fileInfo.fileName());

    // 触发断点检查机制
    if (isImageAlreadyCompleted(dbPath)) {
        qDebug() << "检测到完整的数据库缓存，跳过处理。";
        return;
    }

    QElapsedTimer timer;
    timer.start();

    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    QSize imgSize = reader.size();
    if (!imgSize.isValid()) {
        qWarning() << "无法获取图片尺寸，跳过。";
        return;
    }

    QJsonObject jsonObj;
    jsonObj.insert("width", imgSize.width());
    jsonObj.insert("height", imgSize.height());
    jsonObj.insert("tileSize", m_tileSize);
    
    jsonObj.insert("originalName", fileInfo.fileName());

    // 内存安全回退机制
    if (m_batchMode == MODE_SAFE) {
        QImage thumb = generateThumbnailFromFile(filePath);
        initDatabase(dbPath, jsonObj, thumb);
        runSafeMode(filePath, dbPath, imgSize);
    }
    else {
        reader.setFileName(filePath);
        QImage fullImg = reader.read();

        if (!fullImg.isNull()) {
            if (m_batchMode == MODE_UNKNOWN) m_batchMode = MODE_FAST;
            QImage thumb = fullImg.scaledToWidth(m_thumbWidth, Qt::FastTransformation);
            initDatabase(dbPath, jsonObj, thumb);

            QSharedPointer<QImage> sharedImg = QSharedPointer<QImage>::create(fullImg);
            runFastMode(sharedImg, dbPath);
        }
        else {
            qDebug() << "首图内存申请超限，后续任务自动降级为安全分块模式。";
            m_batchMode = MODE_SAFE;
            QImage thumb = generateThumbnailFromFile(filePath);
            initDatabase(dbPath, jsonObj, thumb);
            runSafeMode(filePath, dbPath, imgSize);
        }
    }

    // 更新数据库状态标记为已完成
    markDatabaseCompleted(dbPath);

    qDebug() << "完成输出:" << baseName + ".db" << " | 耗时:" << timer.elapsed() / 1000.0 << "秒";
}