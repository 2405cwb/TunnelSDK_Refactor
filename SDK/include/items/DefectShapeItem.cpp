#include "./items/DefectShapeItem.h"
#include <QPen>
#include <QUuid>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include<QGraphicsScene>
// 构造函数保持不变
DefectShapeItem::DefectShapeItem(const QPainterPath& path, const QString& name, const DrawShape shape, QString uuid)
    : QGraphicsPathItem(path), m_name(name), m_shape(shape) {

     
    m_isClosed = (shape == Shape_Polygon);
     
    //setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);
    m_uuid = uuid.isEmpty() ? QUuid::createUuid().toString() : uuid;

    // 动画定时器 (保持之前的闪烁逻辑)
    m_animTimer = new QTimer(this);
    connect(m_animTimer, &QTimer::timeout, this, [this]() {
        m_dashOffset++;
        if (m_dashOffset > 10) m_dashOffset = 0;
        update();
        });
}

void DefectShapeItem::finalizeMove()
{
    if (pos() != QPointF(0, 0)) {
        QTransform t;
        t.translate(pos().x(), pos().y());
        setPath(t.map(path()));
        setPos(0, 0);
        emit sigDefectMoved(this); // 通知数据库更新
    }
}

//void DefectShapeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
//{
//    QGraphicsItem::mouseReleaseEvent(event); // 先执行默认的松开逻辑
//
//   //TODO 新增功能 20260310 移动多个病害 触发病害位置移动信号
//    if (scene()) {
//        QList<QGraphicsItem*> selectedItems = scene()->selectedItems();
//
//        for (QGraphicsItem* item : selectedItems) {
//            if (DefectShapeItem* defect = dynamic_cast<DefectShapeItem*>(item)) {
//                 
//                if (defect->pos() != QPointF(0, 0)) {
//                     
//                    QTransform t;
//                    t.translate(defect->pos().x(), defect->pos().y());
//                    defect->setPath(t.map(defect->path()));
//                     
//                    defect->setPos(0, 0); 
//                    // 挨个发送信号给上端更新数据库
//                    emit defect->sigDefectMoved(defect);
//                }
//            }
//        }
//    }
//}

//QPainterPath DefectShapeItem::shape() const
//{
//    // 获取原始的几何路径 (比如那一根极细的线)
//    QPainterPath p = path();
//
//    // 只有线状和点状病害才需要扩大点击判定区，面状（多边形）内部本来就是一整块，极好点中
//    if (m_shape == Shape_Line || m_shape == Shape_Point) {
//        QPainterPathStroker stroker;
//        // 💥 核心魔法：给线条加上 40 像素宽的“隐形判定区”
//        // 也就是说，线条左右各 20 像素的范围内，鼠标点下去都能瞬间命中它！
//        stroker.setWidth(40);
//        stroker.setCapStyle(Qt::RoundCap);
//        stroker.setJoinStyle(Qt::RoundJoin);
//
//        return stroker.createStroke(p);
//    }
//
//    // 面状病害直接返回原路径即可
//    return p;
//}
//
//QRectF DefectShapeItem::boundingRect() const
//{
//    // 边界矩形必须能够完全包裹住我们的“隐形胖衣服”
//     // 否则 Qt 会在屏幕边缘发生裁剪错误，导致病害残留在屏幕上
//    return shape().boundingRect();
//}

// 选中动画保持不变
QVariant DefectShapeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemSelectedHasChanged) {
        if (value.toBool()) m_animTimer->start(50);
        else m_animTimer->stop();
    }
    // ==========================================
    // 🟢 2. 核心魔法：拦截拖动物体的瞬间！
    // ==========================================
    else if (change == ItemPositionChange && scene()) {
        QPointF newPos = value.toPointF(); // 这是 Qt "计划"把物体移动到的新偏移量

        //  限制一：轴向锁定 (轨道锁)
        // 因为我们在 mouseReleaseEvent 里把偏移量合并到了 path 中并把 pos 归零了
        // 所以这里的 newPos 就是相对于原始 path 的【纯净偏移量】
        if (m_moveAxis == Axis_Horizontal) {
            newPos.setY(0); // 锁死 Y 轴偏移
        }
        else if (m_moveAxis == Axis_Vertical) {
            newPos.setX(0); // 锁死 X 轴偏移
        }

        // 🛡️ 限制二：活动范围锁定 (物理结界)
        if (m_moveBounds.isValid()) {
            // 获取图形本身的物理边界
            QRectF rect = path().boundingRect();
            // 预测移动后的物理边界
            QRectF nextRect = rect.translated(newPos);

            // X 轴碰壁检测：如果预测边界超出了给定的结界，就把 newPos 硬拽回来
            if (nextRect.left() < m_moveBounds.left()) {
                newPos.setX(m_moveBounds.left() - rect.left());
            }
            else if (nextRect.right() > m_moveBounds.right()) {
                newPos.setX(m_moveBounds.right() - rect.right());
            }

            // Y 轴碰壁检测
            if (nextRect.top() < m_moveBounds.top()) {
                newPos.setY(m_moveBounds.top() - rect.top());
            }
            else if (nextRect.bottom() > m_moveBounds.bottom()) {
                newPos.setY(m_moveBounds.bottom() - rect.bottom());
            }
        }

        // 把被我们阉割/修正过的新位置，强行还给 Qt
        return newPos;
    }

    return QGraphicsPathItem::itemChange(change, value);
}

// 完美的防遮挡 paint 保持不变
void DefectShapeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
    // 1. 绘制病害线条/图形本身
    QPen drawPen = pen();
    if (isSelected()) {
        drawPen.setColor(Qt::cyan);
        drawPen.setStyle(Qt::CustomDashLine);
        drawPen.setDashPattern({ 4, 4 });
        drawPen.setDashOffset(m_dashOffset);
    }
    drawPen.setCosmetic(true);
    painter->setPen(drawPen);
    painter->setBrush(brush());
    painter->drawPath(path());

    // 2. 绘制文字标签 (防遮挡 + 防出界版)
    if (m_name.isEmpty()) return;

    double lod = option->levelOfDetailFromTransform(painter->worldTransform());
    if (lod < 0.01) return;

    painter->save();

    QPointF scenePos;
    if (path().elementCount() > 0) {
        scenePos = QPointF(path().elementAt(0).x, path().elementAt(0).y);
    }
    else {
        scenePos = boundingRect().topLeft();
    }

    QPointF pixelPos = painter->worldTransform().map(scenePos);
    painter->setWorldTransform(QTransform());
    //TODO 新增功能 20260310  文字控制,用来适配导出图片功能,防止代码导出目标区域图片的时候文字过小 
    // ==========================================
    //  动态感知环境并计算放大倍数
    // ==========================================
    double exportScale = 1.0;

    // 如果 widget 为 nullptr，说明现在没有屏幕，正在向后台的 QImage 里导出数据！
    if (widget == nullptr) {
        // painter->device()->width() 能获取当前生成图片的总像素宽度
        // 假设正常屏幕宽 1500，如果导出图是 6000 像素宽，exportScale 就是 4.0 倍！
        exportScale = qMax(1.0, (double)painter->device()->width() / 1500.0);
    }

    // 🟢 性能优化：全局保存基础字体属性 (只存基准数据)
    static QFont baseFont = painter->font();
    static bool isFontInit = false;
    if (!isFontInit) {
        baseFont.setPointSize(12);
        baseFont.setBold(true);
        isFontInit = true;
    }

    // 🟢 拷贝一份字体用于当前作画，并应用环境放大倍数
    QFont currentFont = baseFont;
    currentFont.setPointSize(12 * exportScale);

    QFontMetrics fm(currentFont);

    // 文本的边距也要随之等比变胖，否则字会撑破黑底框
    int tw = fm.width(m_name) + 10 * exportScale;
    int th = fm.height() + 4 * exportScale;

    qreal targetX = pixelPos.x();
    qreal targetY = pixelPos.y() - 20 * exportScale; // 向上偏移的距离也要放大

    // 🟢 防出界逻辑：只在 UI 屏幕上有意义。导出高清图时画布无限大，不设空气墙。
    if (widget) {
        QRect viewportRect = widget->rect();
        if (targetX + tw > viewportRect.right())  targetX = viewportRect.right() - tw - 5;
        if (targetX < viewportRect.left())        targetX = viewportRect.left() + 5;
        if (targetY < viewportRect.top())         targetY = pixelPos.y() + 20;
        if (targetY + th > viewportRect.bottom()) targetY = viewportRect.bottom() - th - 5;
    }

    QRectF bgRect(targetX, targetY, tw, th);

    // 🟢 高对比度药丸标签绘制
    // 边框线也可以跟着 exportScale 稍微变粗一点
    painter->setPen(QPen(QColor(255, 255, 255, 60), 1 * exportScale));
    painter->setBrush(QColor(0, 0, 0, 230));
    painter->drawRoundedRect(bgRect, th / 2.0, th / 2.0);

    painter->setPen(Qt::white);
    painter->setFont(currentFont); // 必须用放大后的字体作画
    painter->drawText(bgRect, Qt::AlignCenter, m_name);

    painter->restore();

//#ifdef QT_DEBUG 
//    // 🟢 开启“显微镜”：把不可见的 shape() 物理碰撞区画出来
//    if (m_shape == Shape_Line || m_shape == Shape_Point) {
//        painter->save();
//        painter->setPen(Qt::NoPen); // 不要边框
//        painter->setBrush(QColor(0, 255, 0, 80)); // 极其骚气的荧光绿，半透明 80
//
//        // 💥 关键：画出它的灵魂形状（碰撞区），而不是视觉形状（path）
//        painter->drawPath(shape());
//        painter->restore();
//    }
//#endif
}
 
// =========================================================
// 🟢 核心改造：新增 toData()，把自己变成纯数据抛出去
// =========================================================
DefectData DefectShapeItem::toData() const {
    DefectData data;
    data.uuid = m_uuid;
    data.name = m_name;
    data.type = m_shape; // 将 DrawShape 枚举转为 int
    data.shape = path(); // 丢出真正的几何形状
    data.attributes = m_attributes; // 把上端塞进来的万能属性统统带走
    return data;
}

// =========================================================
// 🟢 核心改造：新增 fromData()，从纯数据恢复自己
// =========================================================
void DefectShapeItem::fromData(const DefectData& data) {
    m_uuid = data.uuid;
    m_name = data.name;
    m_shape = static_cast<DrawShape>(data.type);
    m_isClosed = (m_shape == Shape_Polygon);

    // 恢复万能属性
    m_attributes = data.attributes;

    // 恢复几何形状
    setPath(data.shape); 
}