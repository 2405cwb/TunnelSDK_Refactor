#ifndef SQLITEIMAGESLICER_H
#define SQLITEIMAGESLICER_H

#include <QString>
#include <QImage>
#include <QSharedPointer>
#include <QRect>
#include <QByteArray>
#include <QMutex>
#include <QJsonObject>
#include <atomic>

// 任务结构体：用于在多线程中传递单张切片的数据
struct SliceTask {
    QSharedPointer<QImage> sourceImage;
    int col;
    int row;
    QRect rect;
    QByteArray jpgData; // 用于接收内存压缩后的 JPG 二进制流
    int jpgQuality;     // 压缩质量
};

class SqliteImageSlicer
{
public:
    SqliteImageSlicer();
    ~SqliteImageSlicer();

    // 核心接口：传入源文件夹和输出文件夹，自动处理目录下的所有图片
    void processDirectory(const QString& srcFolder, const QString& dstFolder);

    // 参数配置接口（提供默认值，同事可以根据需求自行修改）
    void setConfig(int tileSize = 2048, int jpgQuality = 80);

private:
    // 切图参数
    int m_tileSize;
    int m_jpgQuality;
    int m_thumbWidth;

    // 运行模式：未知、极速模式（全内存）、安全模式（分块加载）
    enum BatchMode { MODE_UNKNOWN, MODE_FAST, MODE_SAFE };
    BatchMode m_batchMode;

    // 线程安全相关的锁和原子变量
    QMutex m_dbWriteMutex;
    std::atomic<int> m_processedCount;
    int m_totalTiles;

    // 核心处理逻辑
    void sliceOneImage(const QString& filePath, const QString& outputRoot, int currentIdx, int totalImages);
    void runFastMode(QSharedPointer<QImage> imgPtr, const QString& dbPath);
    void runSafeMode(const QString& filePath, const QString& dbPath, QSize imgSize);

    // 数据库交互逻辑
    bool isImageAlreadyCompleted(const QString& dbPath);
    void markDatabaseCompleted(const QString& dbPath);
    bool initDatabase(const QString& dbPath, const QJsonObject& info, const QImage& thumbImg);
    void writeTilesToDatabase(const QString& dbPath, const QList<SliceTask>& tasks);

    // 辅助工具
    QImage generateThumbnailFromFile(const QString& filePath);

    // 静态的线程任务函数，供 QtConcurrent 调用
    static void processTask(SliceTask& task);
};

#endif // SQLITEIMAGESLICER_H