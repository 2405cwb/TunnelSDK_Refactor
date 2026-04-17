#include ".\TunnelSectionItem.h"
#include <QPainter>
#include <QFile>
#include <QDebug>
#include <QtMath> 
#include <QSet>
#include "AsyncImageLoader.h" 
#include<QStyleOptionGraphicsItem>
// ============================================================================
// 构造与初始化
// ============================================================================

TunnelSectionItem::TunnelSectionItem(AbstractTileSource* source, QGraphicsItem* parent)
    : m_source(source), QGraphicsItem(parent)
{
    QSize size = m_source->totalSize();
    m_width = size.width();
    m_height = size.height();
    m_tileSize = m_source->tileSize();

    // 开启设备坐标缓存，对静态图元有一定性能提升
   // setCacheMode(DeviceCoordinateCache);
   // 
    // 瞬间获取缩略图 (彻底告别异步加载延迟！)
    // 尝试将抽象源强转为我们的数据库源，直接提取驻留在内存中的缩略图 
    m_isThumbLoading = false;
    m_shouldKeepThumbnail = false;
    // 💥 恢复：监听缩略图加载完毕的信号
    connect(AsyncImageLoader::instance(), &AsyncImageLoader::sigThumbnailLoaded,
        this, [this](const QString& path, QPixmap pix) {
            // 核对：是当前这个数据库的缩略图吗？
            if (path == m_source->getThumbnailImage()) {
                m_isThumbLoading = false;
                if (!m_shouldKeepThumbnail) return; // 如果还没加载完就移出屏幕了，直接丢弃

                m_thumbnail = pix;
                update(); // 刷新界面，画上缩略图
            }
        });


    //   监听高清切片加载信号
    connect(AsyncImageLoader::instance(), &AsyncImageLoader::sigImageLoaded,
        this, [this](const QString& path, QPixmap pix) {

            //  
            if (m_pendingPaths.contains(path)) {

                // B. 取出对应的物理坐标 (同时从账本中删除，防止内存堆积)
                TileKey key = m_pendingPaths.take(path);

                if (!pix.isNull()) {
                    // 防爆显存锁
                    if (m_loadedTiles.size() > MAX_LOADED_TILES) {
                        return;
                    }
                    m_loadedTiles.insert(key, pix);
                    update(); // 刷新界面
                    emit sigTilesUpdated();
                }
            }
        });
}

TunnelSectionItem::~TunnelSectionItem()
{ 
    if (m_source)
    {
        delete m_source;
    }
}

// ============================================================================
// 绘图逻辑
// ============================================================================

QRectF TunnelSectionItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}
 

void TunnelSectionItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(widget);

    // 获取当前在屏幕上“真正暴露（可见）”的物理区域
    QRectF exposedRect = option->exposedRect;
    if (m_thumbnail.isNull()) {
        m_thumbnail = AsyncImageLoader::instance()->getSyncThumbnail(m_source->getThumbnailImage());
    }
    // 1. 底层：绘制缩略图 
    if (!m_thumbnail.isNull()) {
        painter->drawPixmap(boundingRect(), m_thumbnail, m_thumbnail.rect());
    }
    else {
        // 如果没图，画个灰底占位 (只填充可见区域，极其省性能)
        painter->fillRect(exposedRect, QColor(50, 50, 50));
        ensureThumbnailRequested();
       
    }

    // 2. 表层：绘制高清切片
    if (!m_loadedTiles.isEmpty()) {

       // bool oldAa = painter->testRenderHint(QPainter::Antialiasing);
        //painter->setRenderHint(QPainter::Antialiasing, false);
       // painter->setRenderHint(QPainter::SmoothPixmapTransform, true);


        QHashIterator<TileKey, QPixmap> i(m_loadedTiles);
        while (i.hasNext()) {
            i.next();
            const TileKey& key = i.key();

            // 计算当前这张切片所在的物理位置和大小
            QRectF tileRect(key.x, key.y, m_tileSize, m_tileSize); 

            if (exposedRect.intersects(tileRect)) {

                if (i.value().isNull() || i.value().width() == 0) {
                    // 跳过损坏的空图元
                    continue;
                } 
                //painter->drawPixmap(key.x, key.y, i.value());

                // 1. 获取当前这块切片在内存中的真实像素尺寸
                qreal tileW = i.value().width();
                qreal tileH = i.value().height();

                // 2. 在物理世界中划定一个绝对精准的“着陆区”
                QRectF targetRect(key.x, key.y, tileW, tileH);
                //painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
                //QRectF expandedRect = targetRect.adjusted(-0.5, -0.5, 0.5, 0.5);

               // painter->drawPixmap(expandedRect, i.value(), i.value().rect());
                // 3. 强行 1:1 像素映射绘制！彻底无视任何 DPI 干扰！
                painter->drawPixmap(targetRect, i.value(), i.value().rect());
            }
        }
    }

        #ifndef QT_NO_DEBUG
        // 只有在 Debug 模式下才编译这部分代码
        painter->save();
    
        QPen debugPen(Qt::gray, 2); debugPen.setCosmetic(true);
        painter->setPen(debugPen);
        painter->setBrush(Qt::NoBrush);
    
        // 绘制高清切片网格
        auto it = m_loadedTiles.constBegin();
        while (it != m_loadedTiles.constEnd()) {
            const TileKey& key = it.key();
    
            // 画切片格子
            painter->drawRect(key.x, key.y, m_tileSize, m_tileSize);
    
            if (painter->transform().m11() > 0.1) {
                painter->setPen(Qt::yellow);
                painter->drawText(key.x + 10, key.y + 30, QString("%1,%2").arg(key.x).arg(key.y));
                painter->setPen(debugPen);
            }
            ++it;
        }
    
        if (m_loadedTiles.isEmpty()) {
            QPen overPen(Qt::cyan, 4); overPen.setCosmetic(true);
            painter->setPen(overPen);
            painter->drawRect(boundingRect());
            painter->drawText(boundingRect().center(), "Thumbnail Mode");
        }
    
        painter->save();
        painter->setPen(QPen(Qt::red, 5));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(m_debugVisibleRect);
    
        painter->restore();
    #endif // DEBUG
}

// ============================================================================
// 核心切片管理逻辑 (LOD + 视锥剔除 + 异步加载)
// ============================================================================

void TunnelSectionItem::updateVisibleTiles(const QRectF& sceneRect, double currentScale, double thresholdMultiplier)
{
    // 💥 纯净版：已经彻底删除了所有干扰缩略图状态的代码！
    int startCol, endCol, startRow, endRow;

    // 如果完全不可见 (Item 跑出屏幕外了)
    if (!calcVisibleRange(sceneRect, startCol, endCol, startRow, endRow)) {
        unloadAll(); // 卸载高清图
        return;
    }

    double baseThreshold = 0.15;
    double finalThreshold = baseThreshold * thresholdMultiplier;

    // 判断：缩放不够大，就降级
    if (currentScale < finalThreshold) {
        unloadAll(); // 卸载所有高清切片
        update();    // 刷新显示缩略图
        return;
    }

    // =============================================================
    // 3. 计算本帧需要的切片 Key
    // =============================================================
    QSet<TileKey> neededKeys;
    for (int r = startRow; r <= endRow; ++r) {
        for (int c = startCol; c <= endCol; ++c) {
            neededKeys.insert({ c * m_tileSize, r * m_tileSize });
        }
    }

    // =============================================================
    // 4. GC 清理 (先腾地方)
    // =============================================================
    processGarbageCollection(startCol, endCol, startRow, endRow);

    // 5. 请求缺失的高清图
    requestMissingTiles(startCol, endCol, startRow, endRow, neededKeys);

    update();
}

bool TunnelSectionItem::calcVisibleRange(const QRectF& sceneRect, int& startCol, int& endCol, int& startRow, int& endRow) {

    QRectF localRect = mapFromScene(sceneRect).boundingRect();
    QRectF myRect = boundingRect();

    // 求交集
    QRectF intersect = localRect.intersected(myRect);
     
    if (intersect.isEmpty()) {
        return false;
    }

    startCol = qFloor(intersect.left() / m_tileSize);
    endCol = qFloor(intersect.right() / m_tileSize);
    startRow = qFloor(intersect.top() / m_tileSize);
    endRow = qFloor(intersect.bottom() / m_tileSize);

    return true;
}

void TunnelSectionItem::requestMissingTiles(int startCol, int endCol, int startRow, int endRow, QSet<TileKey>& neededKeys)
{   
    Q_UNUSED(neededKeys);

    if (m_loadedTiles.size() >= MAX_LOADED_TILES || m_pendingPaths.size() >= MAX_CONCURRENT_REQUESTS)
    {
        return;
    }

    int maxLoadCount = MAX_LOADED_TILES;
    int currentLoadCount = 0;

    for (int r = startRow; r <= endRow; ++r) {
        for (int c = startCol; c <= endCol; ++c) {
            if (currentLoadCount >= maxLoadCount)
            {
                return;
            }
            TileKey key = { c * m_tileSize, r * m_tileSize };
            if (m_loadedTiles.contains(key)) continue;

            QString baseDbPath = m_source->getDbPath();
            QString requestUri = QString("%1|%2|%3").arg(baseDbPath).arg(c).arg(r);

            if (m_pendingPaths.contains(requestUri)) continue;

            // 3. 记账：记录 路径 -> 坐标
            m_pendingPaths.insert(requestUri, key);

            // 4. 发起请求
            AsyncImageLoader::instance()->requestImage(requestUri);
            currentLoadCount++;
        }
    }
}

void TunnelSectionItem::processGarbageCollection(int startCol, int endCol, int startRow, int endRow)
{
    QSet<TileKey> keepKeys;

    // 1. 直接算出带有 Margin 的扩张边界
    int keepStartCol = qMax(0, startCol - GC_KEEP_MARGIN);
    int keepEndCol = endCol + GC_KEEP_MARGIN;
    int keepStartRow = qMax(0, startRow - GC_KEEP_MARGIN);
    int keepEndRow = endRow + GC_KEEP_MARGIN;

    // 2. 只需要一次极其干净的二维循环，没有任何重复计算！
    for (int r = keepStartRow; r <= keepEndRow; ++r) {
        for (int c = keepStartCol; c <= keepEndCol; ++c) {
            TileKey key = { c * m_tileSize, r * m_tileSize };

            // 越界检查
            if (key.x < m_width && key.y < m_height) {
                keepKeys.insert(key);
            }
        }
    }
    bool isChanged = false;
    // 3. 执行删除：不在 keepKeys 里的旧切片，统统杀掉
    auto it = m_loadedTiles.begin();
    while (it != m_loadedTiles.end()) {
        if (!keepKeys.contains(it.key())) {
            it = m_loadedTiles.erase(it);
            isChanged = true;
        }
        else {
            ++it;
        }
    }
    if (isChanged) {
        emit sigTilesUpdated();
    }
}

void TunnelSectionItem::ensureThumbnailRequested()
{
    m_shouldKeepThumbnail = true;
    if (!m_thumbnail.isNull() || m_isThumbLoading) return;

    m_isThumbLoading = true;
    // 发起异步请求，传入数据库路径 (Loader 会自己判断并去库里查)
    AsyncImageLoader::instance()->requestThumbnail(m_source->getThumbnailImage(), QSize(2048, 2048));
}

void TunnelSectionItem::releaseThumbnail()
{
    m_shouldKeepThumbnail = false;
    if (!m_thumbnail.isNull()) {
        m_thumbnail = QPixmap(); // 💥 释放图元内部的显存占用
    }
}

void TunnelSectionItem::unloadAll()
{
    bool isChanged = false; // 加个状态锁，防止无效的重复发信号
    if (!m_loadedTiles.isEmpty()) {
        m_loadedTiles.clear(); 
        isChanged = true;
    }

    
    if (!m_pendingPaths.isEmpty()) {
        m_pendingPaths.clear();
    }
    releaseThumbnail();
    if (isChanged) {
        emit sigTilesUpdated();
    }
}
QString TunnelSectionItem::getImageName() const {
    return m_source->oriImageName();
}