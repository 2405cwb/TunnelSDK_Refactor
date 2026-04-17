#pragma once
#include <QGraphicsPathItem>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

#include "TunnelGlobal.h" // 必须包含 DrawShape 枚举
#include "DefectSystem.h" // 必须包含 DefectData 数据结构 (DTO)


enum MoveAxis {
    Axis_Free = 0,      // 自由移动 (默认)
    Axis_Horizontal,    // 只能水平拖动 (锁死 Y 轴)
    Axis_Vertical       // 只能纵向拖动 (锁死 X 轴)
};


class DefectShapeItem : public QObject, public QGraphicsPathItem {
    Q_OBJECT
public:
    // 构造函数
    DefectShapeItem(const QPainterPath& path, const QString& name, const DrawShape shape, QString uuid = "");

    //设置病害的纵向或者水平移动
    void setMoveAxis(MoveAxis axis) { m_moveAxis = axis; }

	//设置病害的移动范围结界 (如果 isValid() 为 false 则表示不限制)
    void setMoveBounds(const QRectF& bounds) { m_moveBounds = bounds; }

	//void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

    void finalizeMove();
	// 设置病害能否被移动 (锁定/解锁)，锁定后用户无法通过鼠标拖动它改变位置
    void setLocked(bool locked) {
        m_isLocked = locked;
        // 核心魔法：直接剥夺或赋予它底层可移动的权限！
        setFlag(QGraphicsItem::ItemIsMovable, !locked);
        update(); // 触发重绘，我们可以借机改变一下外观提示用户
    }

    // 查询当前是否被锁
    bool isLocked() const { return m_isLocked; }
    // ==========================================
    // 🟢 核心改造：数据模型转换接口 (替代原来的 toJson)
    // ==========================================

    // 1. 数据导出：将自身图形转化为纯数据包 (交给 Manager 去存)
    DefectData toData() const;

    // 2. 数据导入：从纯数据包还原自身图形 (从 Manager 读取时使用)
    void fromData(const DefectData& data);

    // ==========================================
    // 🟢 核心改造：万能属性包接口
    // ==========================================
    void setAttribute(const QString& key, const QVariant& value) { m_attributes[key] = value; }
    QVariant getAttribute(const QString& key) const { return m_attributes.value(key); }
    QVariantMap getAttributes() const { return m_attributes; }

    // ==========================================
    // 基础 Getter 接口
    // ==========================================
    QString getUuid() const { return m_uuid; }
    QString getName() const { return m_name; }
    DrawShape getShapeType() const { return m_shape; }
    bool isClosed() const { return m_isClosed; }

    //TODO 新增优化 20260309 重写物理碰撞区和边界，解决“线太细点不中”的问题
  //  QPainterPath shape() const override;
  //  QRectF boundingRect() const override;
signals:
    void sigDefectMoved(DefectShapeItem* item);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    QString m_uuid;
    QString m_name;
    DrawShape m_shape;
    bool m_isClosed;

    // 💥 新增：万能属性包。上端 UI 填写的任何乱七八糟的属性，都存在这里。
    QVariantMap m_attributes;

    // 动画相关
    QTimer* m_animTimer;
    int m_dashOffset = 0;

    MoveAxis m_moveAxis = Axis_Free;
    QRectF m_moveBounds; // 移动范围结界 (如果 isValid() 为 false 则表示不限制)


    bool m_isLocked = false; // 默认不锁
};