#include <qpoint.h>
#ifndef TILEDGRAPHICSVIEW_H
#define TILEDGRAPHICSVIEW_H
#include<QCoreApplication> 
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QElapsedTimer>
#include <QTimer>
#include "TunnelSectionItem.h"
#include "TunnelGlobal.h" 
#include "DefectManager.h"
#include "TunnelGlobal.h"
class DefectDrawTool; // 前置声明
 
 
 
/**
 *  核心瓦片地图控件 (SDK 的门面)
 *
 * 功能列表：
 * 1. 自动管理无限长图拼接 (addLayer)
 * 2. 内置 LOD 调度与视锥剔除 (onScroll)
 * 3. 完整交互支持：WASD移动、Ctrl+滚轮缩放、右键菜单
 * 4. 性能监控与坐标反馈 (通过信号发出)
 */
class TiledGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    // 🟢 静态初始化函数，要求在 main 的第一行调用
    static void init() {
                #if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
                // 这一行必须在 QApplication 创建前调用才有效
                QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
                #endif
    } 


    explicit TiledGraphicsView(QWidget* parent = nullptr);
    ~TiledGraphicsView();

    // ==========================================
    // 🟢 SDK 核心 API  
    // ==========================================

    /**
     *  局部坐标转全局坐标
     * 根据图片名称和图片内的像素坐标，计算出在整个长图(Scene)中的真实坐标
     * @param imageName 图片/图层名称
     * @param localX 图片内的局部 X 坐标
     * @param localY 图片内的局部 Y 坐标
     * @return 映射后的全局 Scene 坐标。如果找不到图片，返回 QPointF(0,0)
     */
    QPointF mapToGlobalScene(const QString& imageName, qreal localX, qreal localY);
 
    //  暴露给上端的接口：跳转并聚焦到全局坐标
    // 参数 targetScale: 如果传大于0的值，跳转后会自动缩放到该比例（比如放大到 1.0 看清裂缝）
    void focusOnPosition(const QPointF& scenePos, double targetScale = -1.0);

    /**
     *  添加一段隧道数据
     * @param source 数据源接口 (由同事实现)
     * 自动将其拼接到当前场景的最右侧
     */
    void addLayer(AbstractTileSource* source);

    /**
     *   切换交互模式
     * @param mode Mode_Browse(浏览) 或 Mode_Draw(绘图)
     */
    void setViewMode(ViewMode mode);

    /**
     *   清空所有数据
     */
    void clear();

    /**
     *  获取当前场景指针 (方便外部添加自定义 Item)
     */
    QGraphicsScene* scene() const { return m_scene; }


    // 允许外部禁用自带的键盘导航
    void setKeyboardNavigationEnabled(bool enable) { m_enableKeyNav = enable; }

    void setLayoutOrientation(LayoutOrientation orientation) {
        m_orientation = orientation;
    }

    //设置滚动条速度
    void set_scrollSpeed(int speed)
    {
		m_scrollSpeed = speed;
    }
     
    // 上端调用此接口，告诉 SDK 要画什么
    void startDrawingDefect(DrawShape shapeType);

    // 给 Tool 回调用的内部接口
    void emitGeometryDrawn(DrawShape shapeType, const QPainterPath& path) {
        emit sigGeometryDrawn(shapeType, path);
    } 

    // 🟢  设置 LOD 等级 (1-5)
    // 1: 极度节省 (放很大才加载)
    // 5: 极度激进 (稍微放大就加载)
    void setLodLevel(int level);

    
    void updateVisibleTiles();          

   
    void undoLastDrawPoint();

    void cancelCurrentDrawing(); 

    DefectManager* defectManager() const { return m_defectManager; }

    /**
     *  绕过界面状态，直接从硬盘硬拼接导出指定区域的图像 (包含病害)
     *  sceneRect 想要导出的真实物理坐标区域
     *  quality 导出画质 (高清或缩略图)
     */
    QPixmap exportRegionData(const QRectF& sceneRect, ExportQuality quality = Export_HighRes, bool drawDefects = true);
public slots:
    // 🟢 1. 暴露功能接口：让外部随时可以调用代码来重置视图
    void resetToFit();
    void updateHUD(); // 专门负责刷新左上角的文字
signals:
    // ==========================================
    // 🟢 信号系统 (通知外部 UI 更新)
    // ==========================================
     
    //  抛给外部的信号：用户画完了一个图形，给你几何形状
    void sigGeometryDrawn(DrawShape shapeType, QPainterPath path);



    /**
     * @brief 鼠标位置更新信号 (用于更新左下角坐标 Label)
     * @param info 格式化好的字符串，如 "图名: Tunnel_01 | 坐标: (100, 200)"
     */
     // 🟢 修改：发送原始数据，而不是拼好的字符串
    void sigCursorInfoChanged(QString imgName, int x, int y);

    /**
     * @brief 性能统计更新信号 (用于更新 HUD)
     * @param stats 包含 FPS、显存占用、切片数量等信息的字符串
     */
    void sigStatsUpdated(QString stats);

    // 🟢 暴露事件接口：告诉外部“有人在这里点了右键”，并把点到了什么传出去
    void sigContextMenuRequested(QPoint globalPos, QList<QGraphicsItem*> itemsUnderMouse);

    void sigDefectsSelected(QList<DefectShapeItem*> selectedDefects);

    void sigRegionExported(QPixmap pixmap);
protected:
    // ==========================================
    // 🟢 事件处理 
    // ==========================================
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

    // 鼠标交互：拖拽、点击、右键
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;


    // 键盘交互：WASD 移动
    void keyPressEvent(QKeyEvent* event) override;

	void keyReleaseEvent(QKeyEvent* event) override;

private:

  void  setupGraphicsView();

  void setupConnections();
    // --- 内部辅助函数 ---

    double getFitScale() const;   // 计算“适应高度”的缩放比

    // 模糊查找 (用于点选细线)
    QList<QGraphicsItem*> getVisualItems(QPoint viewPos);

   

private:
    QGraphicsScene* m_scene;
    QList<TunnelSectionItem*> m_items; // 管理所有图层段
    QString DrawModelStr;
    // 状态变量
    ViewMode m_currentMode = Mode_Browse;

	DrawShape m_curDrawShape = Shape_Point; // 当前绘图形状 
    double m_currentScale = 1.0;

    // 性能监控
    QElapsedTimer m_perfTimer;

    // 防抖动定时器  
    QTimer* m_debounceTimer;

    LayoutOrientation m_orientation;


    // 漫游速度 (像素/次)
    int m_scrollSpeed  ;
    bool m_enableKeyNav = true; // 默认开启
    bool m_isFastScrolling = false; // 🟢  标记是否正在快速移动

    // 默认为 1.0 (对应等级 3)
    // 这个值越小，阈值越低，越早加载
    double m_lodThresholdMultiplier = 1.0;
    // 用于中键拖拽的状态变量
    bool m_isPanning = false;
    QPoint m_lastMousePos; 
    DefectManager* m_defectManager = nullptr;
    DefectDrawTool* m_currentTool = nullptr; // 暂用具体工具代替，后续可拓展多工具


    bool m_isDraggingDefects = false; // 标记当前是否正在拖动病害
    QPointF m_lastDragScenePos;       // 记录上一次鼠标在物理世界的坐标

    QGraphicsRectItem* m_exportBoxItem = nullptr;
    int exprotBoxSize = 1680;
   
};

#endif // TILEDGRAPHICSVIEW_H