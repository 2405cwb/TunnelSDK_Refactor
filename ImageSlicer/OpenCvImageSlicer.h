#ifndef OPENCVIMAGESLICER_H
#define OPENCVIMAGESLICER_H

#include <QString>
#include <QSharedPointer>
#include <QRect>
#include <QByteArray>
#include <QMutex>
#include <QJsonObject>
#include <atomic>

// 引入 OpenCV
#include <opencv2/opencv.hpp>

struct CvSliceTask {
    QSharedPointer<cv::Mat> sourceMat;
    int col;
    int row;
    cv::Rect rect;       // 使用 cv::Rect
    QByteArray jpgData;  // 依然转成 QByteArray 方便存 SQLite
    int jpgQuality;
};

class OpenCvImageSlicer
{
public:
    OpenCvImageSlicer();
    ~OpenCvImageSlicer();

    void processDirectory(const QString& srcFolder, const QString& dstFolder);
    void setConfig(int tileSize = 2048, int jpgQuality = 95);

private:
    int m_tileSize;
    int m_jpgQuality;
    int m_thumbWidth;

    enum BatchMode { MODE_UNKNOWN, MODE_FAST, MODE_SAFE };
    BatchMode m_batchMode;

    QMutex m_dbWriteMutex;
    std::atomic<int> m_processedCount;

    void sliceOneImage(const QString& filePath, const QString& outputRoot, int currentIdx, int totalImages);

    // 核心处理函数，改用 cv::Mat
    void runFastMode(QSharedPointer<cv::Mat> matPtr, const QString& dbPath);
    void runSafeMode(const QString& filePath, const QString& dbPath, cv::Size imgSize);

    bool isImageAlreadyCompleted(const QString& dbPath);
    void markDatabaseCompleted(const QString& dbPath);

    // 初始化数据库，接收 cv::Mat 的缩略图
    bool initDatabase(const QString& dbPath, const QJsonObject& info, const cv::Mat& thumbMat);
    void writeTilesToDatabase(const QString& dbPath, const QList<CvSliceTask>& tasks);

    cv::Mat generateThumbnailFromFile(const QString& filePath);

    // 静态的线程任务函数
    static void processTask(CvSliceTask& task);
};

#endif // OPENCVIMAGESLICER_H