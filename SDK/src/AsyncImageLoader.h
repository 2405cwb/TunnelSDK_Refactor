#pragma once

#include <QObject>
#include <QCache>
#include <QPixmap>
#include <QMutex>
#include <functional> 
#include <QThreadPool>
class AsyncImageLoader : public QObject
{
    Q_OBJECT
public:
    static AsyncImageLoader* instance();

    void setBrightness(int value);
    int brightness() const;
     
    QPixmap getSyncThumbnail(const QString& pathUri);
    // 发起缩略图请求
    void requestThumbnail(const QString& pathUri, const QSize& targetSize = QSize());
     
   
    void requestImage(const QString& path );


    

signals: 
    // 🟢 定义信号：当图片加载好后广播出去
    // path: 用来区分是哪张图
    // pix: 加载好的图片
    void sigThumbnailLoaded(const QString& path, QPixmap pix);
    void sigImageLoaded(const QString& path, QPixmap pix);
private:
    AsyncImageLoader();
    QCache<QString, QPixmap> m_cache; 
    QMutex m_mutex;
    int m_brightness = 0; 

    QCache<QString, QPixmap> m_thumbnailCache;
    QThreadPool m_thumbThreadPool;
};


