#include <qstring.h>
#ifndef TUNNELSECTIONITEM_H
#define TUNNELSECTIONITEM_H

#include <QGraphicsObject>
#include <QPixmap>
#include <QHash>
#include <QDir>
#include <QJsonObject>
#include <QJsonDocument> 
#include "..\include\AbstractTileSource.h"


// 自定义哈希键结构，用于标识唯一的切片 (x, y)
struct TileKey {
    int x;
    int y;
    bool operator==(const TileKey& other) const {
        return x == other.x && y == other.y;
    }
};

// 全局哈希函数，配合 QHash 使用
inline uint qHash(const TileKey& key, uint seed = 0) {
    return qHash(key.x, seed) ^ qHash(key.y, seed);
}

/**
 * @brief 隧道图层段 Item
 * 负责管理单个大图（Section）的显示。
 * 核心功能：LOD（细节层次）、视锥剔除（Frustum Culling）、异步分块加载。
 */
class TunnelSectionItem :public QObject, public QGraphicsItem
{
        Q_OBJECT
        Q_INTERFACES(QGraphicsItem)

public:
    /**
     * @brief 构造函数
     * @param id 图层ID，用于排序或标识
     * @param folderPath 切片文件夹路径 (包含 info.json 和 thumbnail.jpg)
     */
    TunnelSectionItem(AbstractTileSource * source , QGraphicsItem * parent = nullptr);
    ~TunnelSectionItem();
     
    QRectF boundingRect() const override;
     
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    /**
     * @brief 核心更新函数：计算并加载当前视野内的切片
     * @param sceneRect 当前 View 在 Scene 中的可视区域 (带 buffer)
     * @param currentScale 当前 View 的缩放倍率
     */
    void updateVisibleTiles(const QRectF& sceneRect, double currentScale, double thresholdMultiplier);

    // 卸载所有高清切片，释放显存
    void unloadAll();

    // 获取当前已加载的切片数量 (用于调试监控)
    int getLoadedTileCount() const { return m_loadedTiles.size(); }
    int id() const { return m_id; }

 
    QString getImageName() const;


    // TODO 新增功能 20260310 为了让顶层导出引擎能直接去硬盘拿图，暴露出数据源和切片尺寸
    AbstractTileSource* getSource() const { return m_source; }
    int getTileSize() const { return m_tileSize; }

    void  ensureThumbnailRequested();
    void releaseThumbnail();
    
signals:
    void sigTilesUpdated();
private:

  
    // --- 内部辅助函数 --- 
   // void loadThumbnail(); // 读取缩放图

    // 计算需要加载的行列范围 (返回 true 表示需要加载，false 表示完全不可见)
    bool calcVisibleRange(const QRectF& sceneRect, int& startCol, int& endCol, int& startRow, int& endRow);

    // 执行切片加载任务
    void requestMissingTiles(int startCol, int endCol, int startRow, int endRow, QSet<TileKey>& neededKeys);

   

    // 执行内存清理 (移除不可见的切片)
    void processGarbageCollection(int startCol, int endCol, int startRow, int endRow);


   
     
private:
    //单个隧道段允许在显存中驻留的最大高清切片数 (约645mb)
    const  int MAX_LOADED_TILES = 500;

    //瞬间向网盘发起的最大并发请求数
    const  int MAX_CONCURRENT_REQUESTS = 50;

    //垃圾回首时的视口保留圈数(0表示离开屏幕立即销毁)
    const int GC_KEEP_MARGIN = 1;

    AbstractTileSource* m_source; // 存储“合同”指针
    // --- 基础属性 ---
    int m_id;
    qreal m_width = 100;
    qreal m_height = 100;
    int m_tileSize = 1024;
    // --- 资源管理 ---
    QPixmap m_thumbnail;                  // 低清缩略图 (仅在可见区附近保留)
    QHash<TileKey, QPixmap> m_loadedTiles;// 高清切片缓存 (动态加载/卸载)


    
    bool m_shouldKeepThumbnail = false;
    
    // 用于调试显示计算出的范围
    QRectF m_debugVisibleRect;
    // 🟢 新增：挂号账本
    // Key: 图片路径, Value: 切片坐标 (TileKey)
    // 作用：收到图片时，查这个表就知道它该放在哪
    QHash<QString, TileKey> m_pendingPaths;
    bool m_isThumbLoading = false;
};

#endif // TUNNELSECTIONITEM_H