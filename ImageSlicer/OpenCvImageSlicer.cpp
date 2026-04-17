#include "OpenCvImageSlicer.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QDebug>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QtConcurrent>
#include <QSemaphore>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMutexLocker>
#include <QUuid>

OpenCvImageSlicer::OpenCvImageSlicer()
    : m_tileSize(2048), m_jpgQuality(80), m_thumbWidth(2048), m_batchMode(MODE_UNKNOWN), m_processedCount(0) {
}

OpenCvImageSlicer::~OpenCvImageSlicer() {}

void OpenCvImageSlicer::setConfig(int tileSize, int jpgQuality)
{
    m_tileSize = tileSize;
    m_jpgQuality = jpgQuality;
    m_thumbWidth = tileSize;
}

void OpenCvImageSlicer::processDirectory(const QString& srcFolder, const QString& dstFolder)
{
    QDir sourceDir(srcFolder);
    if (!sourceDir.exists()) return;

    QStringList filters;
    filters << "*.jpg" << "*.jpeg" << "*.bmp" << "*.png";
    QFileInfoList fileList = sourceDir.entryInfoList(filters, QDir::Files);

    if (fileList.isEmpty()) return;

    qDebug() << "【OpenCV 引擎】开始批量处理任务，总计:" << fileList.size() << "张图片。";
    m_batchMode = MODE_UNKNOWN;

    // 保持顺次读取单张大图，防止内存 OOM
    for (int i = 0; i < fileList.size(); ++i) {
        sliceOneImage(fileList[i].absoluteFilePath(), dstFolder, i + 1, fileList.size());
    }
}

// =========================================================
// 💥 性能核武器：OpenCV 零拷贝与极速编码
// =========================================================
void OpenCvImageSlicer::processTask(CvSliceTask& task)
{
    // 1. 零拷贝提取 ROI 区域，耗时 0 毫秒！
    cv::Mat tileMat = (*task.sourceMat)(task.rect);

    // 2. 内存直压 JPG，跳过 Qt IO 缓冲
    std::vector<uchar> buf;
    std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, task.jpgQuality };

    // OpenCV 默认是 BGR，压入 JPG 不会改变色彩
    cv::imencode(".jpg", tileMat, buf, params);

    // 3. 转接给 QByteArray 以便入库
    task.jpgData = QByteArray(reinterpret_cast<const char*>(buf.data()), buf.size());
}

void OpenCvImageSlicer::writeTilesToDatabase(const QString& dbPath, const QList<CvSliceTask>& tasks)
{
    if (tasks.isEmpty()) return;
    QMutexLocker locker(&m_dbWriteMutex);

    QString connName = QUuid::createUuid().toString();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(dbPath);
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
    }
    QSqlDatabase::removeDatabase(connName);
}

bool OpenCvImageSlicer::isImageAlreadyCompleted(const QString& dbPath)
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
            if (q.exec("SELECT value FROM info WHERE key = 'status'") && q.next()) {
                if (q.value(0).toString() == "completed") isCompleted = true;
            }
        }
    }
    QSqlDatabase::removeDatabase(connName);
    if (!isCompleted) QFile::remove(dbPath);
    return isCompleted;
}

void OpenCvImageSlicer::markDatabaseCompleted(const QString& dbPath)
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

bool OpenCvImageSlicer::initDatabase(const QString& dbPath, const QJsonObject& info, const cv::Mat& thumbMat)
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
        q.exec("INSERT INTO info (key, value) VALUES ('status', 'processing')");

        q.prepare("INSERT INTO info (key, value) VALUES (?, ?)");
        for (auto it = info.begin(); it != info.end(); ++it) {
            q.addBindValue(it.key());
            q.addBindValue(it.value().toVariant().toString());
            q.exec();
        }

        if (!thumbMat.empty()) {
            std::vector<uchar> buf;
            cv::imencode(".jpg", thumbMat, buf, { cv::IMWRITE_JPEG_QUALITY, 80 });
            QByteArray thumbData(reinterpret_cast<const char*>(buf.data()), buf.size());

            q.prepare("INSERT INTO thumbnail (data) VALUES (?)");
            q.addBindValue(thumbData);
            q.exec();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return true;
}

cv::Mat OpenCvImageSlicer::generateThumbnailFromFile(const QString& filePath)
{
    // 为防止 Windows 下中文路径读取失败，采用 Qt 读取字节再 imdecode
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        cv::Mat img = cv::imdecode(cv::Mat(1, data.size(), CV_8UC1, (void*)data.data()), cv::IMREAD_COLOR);
        if (!img.empty()) {
            cv::Mat thumb;
            int targetHeight = m_thumbWidth * img.rows / img.cols;
            cv::resize(img, thumb, cv::Size(m_thumbWidth, targetHeight), 0, 0, cv::INTER_AREA);
            return thumb;
        }
    }
    return cv::Mat();
}

void OpenCvImageSlicer::runFastMode(QSharedPointer<cv::Mat> matPtr, const QString& dbPath)
{
    int w = matPtr->cols;
    int h = matPtr->rows;
    QList<CvSliceTask> tasks;
    tasks.reserve((w / m_tileSize + 1) * (h / m_tileSize + 1));

    for (int y = 0; y < h; y += m_tileSize) {
        for (int x = 0; x < w; x += m_tileSize) {
            CvSliceTask task;
            task.sourceMat = matPtr;
            task.col = x / m_tileSize;
            task.row = y / m_tileSize;
            task.rect = cv::Rect(x, y, std::min(m_tileSize, w - x), std::min(m_tileSize, h - y));
            task.jpgQuality = m_jpgQuality;
            tasks.append(task);
        }
    }

    // 并发执行 OpenCV 切片与压缩
    QtConcurrent::blockingMap(tasks, processTask);
    writeTilesToDatabase(dbPath, tasks);
}

void OpenCvImageSlicer::runSafeMode(const QString& filePath, const QString& dbPath, cv::Size imgSize)
{
    const int STRIP_HEIGHT = 2048;
    const int MAX_CONCURRENT_STRIPS = 2;
    QSemaphore semaphore(MAX_CONCURRENT_STRIPS);
    QList<QFuture<void>> runningFutures;

    QImageReader reader(filePath);

    for (int startY = 0; startY < imgSize.height; startY += STRIP_HEIGHT) {
        semaphore.acquire(1);

        for (auto it = runningFutures.begin(); it != runningFutures.end();) {
            if (it->isFinished()) it = runningFutures.erase(it);
            else ++it;
        }

        int currentStripH = std::min(STRIP_HEIGHT, imgSize.height - startY);
        QRect stripRect(0, startY, imgSize.width, currentStripH);

        reader.setFileName(filePath);
        reader.setAutoTransform(true);
        reader.setClipRect(stripRect);
        QImage stripImg = reader.read();

        if (stripImg.isNull()) {
            semaphore.release(1);
            continue;
        }

        // Qt 读出来的通常是 RGB32 或 RGB888，OpenCV 需要 BGR888
        if (stripImg.format() != QImage::Format_RGB888) {
            stripImg = stripImg.convertToFormat(QImage::Format_RGB888);
        }
        // RGB 转 BGR 以适应 OpenCV
        stripImg = stripImg.rgbSwapped();

        // 深拷贝一份 cv::Mat 放到堆区，交由智能指针管理
        cv::Mat stripMat(currentStripH, imgSize.width, CV_8UC3, (void*)stripImg.constBits(), stripImg.bytesPerLine());
        cv::Mat* heapMat = new cv::Mat(stripMat.clone());
        QSharedPointer<cv::Mat> sharedStrip(heapMat);

        QList<CvSliceTask> stripTasks;
        for (int localY = 0; localY < currentStripH; localY += m_tileSize) {
            int globalY = startY + localY;
            for (int globalX = 0; globalX < imgSize.width; globalX += m_tileSize) {
                CvSliceTask task;
                task.sourceMat = sharedStrip;
                task.col = globalX / m_tileSize;
                task.row = globalY / m_tileSize;
                task.rect = cv::Rect(globalX, localY, std::min(m_tileSize, imgSize.width - globalX), std::min(m_tileSize, currentStripH - localY));
                task.jpgQuality = m_jpgQuality;
                stripTasks.append(task);
            }
        }

        QFuture<void> future = QtConcurrent::run([this, stripTasks, sharedStrip, dbPath, &semaphore]() mutable {
            QtConcurrent::blockingMap(stripTasks, processTask);
            this->writeTilesToDatabase(dbPath, stripTasks);
            semaphore.release(1);
            });
        runningFutures.append(future);
    }

    for (auto& f : runningFutures) f.waitForFinished();
}

void OpenCvImageSlicer::sliceOneImage(const QString& filePath, const QString& outputRoot, int currentIdx, int totalImages)
{
    QFileInfo fileInfo(filePath);
    QString baseName = fileInfo.completeBaseName();
    QString dbPath = outputRoot + "/" + baseName + ".db";

    qDebug() << QString("\n[%1/%2] 【OpenCV】开始处理文件: %3").arg(currentIdx).arg(totalImages).arg(fileInfo.fileName());

    if (isImageAlreadyCompleted(dbPath)) {
        qDebug() << "检测到完整的数据库缓存，跳过处理。";
        return;
    }

    QElapsedTimer timer;
    timer.start();

    // OpenCV 读取文件
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;
    QByteArray fileData = file.readAll();
    file.close();

    // 使用 imdecode 防止中文路径导致的死锁和失败
    cv::Mat fullImg = cv::imdecode(cv::Mat(1, fileData.size(), CV_8UC1, (void*)fileData.data()), cv::IMREAD_COLOR);

    QJsonObject jsonObj;
    jsonObj.insert("originalName", fileInfo.fileName());
    jsonObj.insert("tileSize", m_tileSize);

    if (!fullImg.empty()) {
        if (m_batchMode == MODE_UNKNOWN) m_batchMode = MODE_FAST;
        jsonObj.insert("width", fullImg.cols);
        jsonObj.insert("height", fullImg.rows);

        // 生成缩略图
        cv::Mat thumb;
        int targetHeight = m_thumbWidth * fullImg.rows / fullImg.cols;
        cv::resize(fullImg, thumb, cv::Size(m_thumbWidth, targetHeight), 0, 0, cv::INTER_AREA);
        initDatabase(dbPath, jsonObj, thumb);

        QSharedPointer<cv::Mat> sharedImg = QSharedPointer<cv::Mat>::create(fullImg);
        runFastMode(sharedImg, dbPath);
    }
    else {
        qDebug() << "图片过大或读取失败，降级为 Qt+OpenCV 混合安全分块模式。";
        m_batchMode = MODE_SAFE;

        QImageReader reader(filePath);
        cv::Size imgSize(reader.size().width(), reader.size().height());
        jsonObj.insert("width", imgSize.width);
        jsonObj.insert("height", imgSize.height);

        cv::Mat thumb = generateThumbnailFromFile(filePath);
        initDatabase(dbPath, jsonObj, thumb);
        runSafeMode(filePath, dbPath, imgSize);
    }

    markDatabaseCompleted(dbPath);
    qDebug() << "【OpenCV】完成输出:" << baseName + ".db" << " | 耗时:" << timer.elapsed() / 1000.0 << "秒";
}