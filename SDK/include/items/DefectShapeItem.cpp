#include "./items/DefectShapeItem.h"
#include <QPen>
#include <QUuid>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
// 构造函数保持不变
DefectShapeItem::DefectShapeItem(const QPainterPath& path, const QString& name, const DrawShape shape, int uuid)
    : QGraphicsPathItem(path), m_name(name), m_shape(shape) {
  
    m_isClosed = (shape == Shape_Polygon);

	ringEndPt = QPointF(0, 0);
     
	setFlags(ItemIsSelectable  | ItemSendsGeometryChanges);
	//setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
    m_uuid = uuid;

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
	if (pos() != QPointF(0,0))
	{
		QTransform t;
		t.translate(pos().x(), pos().y());
		setPath(t.map(path()));
		setPos(0, 0);
		emit sigDefectMoved(this);
	}
}

 

// 选中动画保持不变
QVariant DefectShapeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemSelectedHasChanged) {
        if (value.toBool()) m_animTimer->start(50);
        else m_animTimer->stop();
    }
    
    else if (change == ItemPositionChange && scene()) {
		if (m_isLocked)
		{
			return pos();
		}

        QPointF newPos = value.toPointF(); 

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

       
        return newPos;
    }

    return QGraphicsPathItem::itemChange(change, value);
}

// 完美的防遮挡 paint 保持不变
void DefectShapeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
    // 1. 绘制病害线条/图形本身
	double exportScale = 1.0;
	if (widget == nullptr)
	{
		exportScale = qMax(1.0, (double)painter->device()->width() / 1500.0);
	}
    QPen drawPen = pen();
    if (isSelected()) {
        drawPen.setColor(QColor(153,51,255));
        drawPen.setStyle(Qt::CustomDashLine);
        drawPen.setDashPattern({ 4, 4 });
        drawPen.setDashOffset(m_dashOffset);
    }
	if (widget == nullptr)
	{
		drawPen.setCosmetic(false);
		double baseWidth = qMax(3.0, drawPen.widthF());
		drawPen.setWidthF(baseWidth * exportScale * 3.0);
		drawPen.setCapStyle(Qt::RoundCap);
		drawPen.setJoinStyle(Qt::RoundJoin);
	}
	else
	{
		drawPen.setCosmetic(true);

	}
    painter->setPen(drawPen);
    painter->setBrush(brush());

	if (path().elementCount() == 2)
	{
		// 设置感应区间
		QPainterPathStroker stroker;
		stroker.setWidth(50);
		path() = stroker.createStroke(path());
	}

    painter->drawPath(path());

    // 2. 绘制文字标签 (防遮挡 + 防出界版)
    if (m_name.isEmpty()) return;

    double lod = option->levelOfDetailFromTransform(painter->worldTransform());
    if (lod < 0.01) return;

    painter->save();

	int fontSize = 12;
	//TODO 新增优化 20260227 全局保存字体防止重复创建，提升性能 
	static  QFont f = painter->font();
	static bool isFontInit = false;
	if (!isFontInit) {
		f.setPointSize(12);
		f.setBold(true);
		isFontInit = true;
	}

	QFontMetrics fm(f);
	int tw = fm.width(m_name) + 10;
	int th = fm.height() + 4;

    QPointF scenePos;
    if (path().elementCount() > 0) {

		// 取中心
		scenePos.rx() = (path().boundingRect().topLeft().x() + path().boundingRect().bottomRight().x()) / 2;
		scenePos.ry() = (path().boundingRect().topLeft().y() + path().boundingRect().bottomRight().y()) / 2;

		if (m_elementType == Type_Ring && ringEndPt.x() != 0)
		{
			scenePos.rx() = (scenePos.x() + ringEndPt.x()) / 2 - tw;
		}

        //scenePos = QPointF(path().elementAt(0).x, path().elementAt(0).y);
    }
    else {
        scenePos = boundingRect().topLeft();
    }

    QPointF pixelPos = painter->worldTransform().map(scenePos);
    painter->setWorldTransform(QTransform());



	// ==========================================
	//  动态感知环境并计算放大倍数
	// ==========================================
//	double exportScale = 1.0;

	// 如果 widget 为 nullptr，说明现在没有屏幕，正在向后台的 QImage 里导出数据！
	/*if (widget == nullptr) {
		 painter->device()->width() 能获取当前生成图片的总像素宽度
		 假设正常屏幕宽 1500，如果导出图是 6000 像素宽，exportScale 就是 4.0 倍！
		exportScale = qMax(1.0, (double)painter->device()->width() / 1500.0);
	}*/

    qreal targetX = pixelPos.x();
    qreal targetY = pixelPos.y() - 20;

    if (widget) {
        QRect viewportRect = widget->rect();
        if (targetX + tw > viewportRect.right())  targetX = viewportRect.right() - tw - 5;
        if (targetX < viewportRect.left())        targetX = viewportRect.left() + 5;
        if (targetY < viewportRect.top())         targetY = pixelPos.y() + 20;
        if (targetY + th > viewportRect.bottom()) targetY = viewportRect.bottom() - th - 5;
    }

    QRectF bgRect(targetX, targetY, tw, th); 
    //TODO 新增功能 20260227 修改病害文字绘制效果 
    QColor defectColor = pen().color();                 // 获取病害自身的颜色 (红/黄/蓝)
    painter->setPen(QPen(defectColor, 1));              // 用病害颜色做 1 像素精细描边
    //painter->setBrush(QColor(15, 15, 15, 210));         // 加深黑底的浓度 (210)，让白色文字对比度极高
    //painter->drawRoundedRect(bgRect, 4, 4);
    painter->setFont(f);
    painter->drawText(bgRect, Qt::AlignCenter, m_name);

    painter->restore();
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