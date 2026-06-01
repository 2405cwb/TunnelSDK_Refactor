#ifndef ABSTRACTTOOL_H
#define ABSTRACTTOOL_H


#include <QGraphicsSceneMouseEvent>

class TiledGraphicsView;

class AbstractTool {
public:
    virtual ~AbstractTool() {}
    virtual void setView(TiledGraphicsView* view) { m_view = view; }

    // 🟢 统一的鼠标交互接口 (简化版：直接接收 Scene 坐标和按键类型)
    virtual void handleMousePress(QPointF scenePos, Qt::MouseButton button) {}
    virtual void handleMouseMove(QPointF scenePos) {}
    virtual void handleMouseRelease(QPointF scenePos, Qt::MouseButton button) {}

    // 🟢 增加 deactivate 虚函数声明，允许子类在取消工具时清理垃圾（比如没画完的虚线）
    virtual void deactivate() {}

	virtual void cancelDrawing() {}; // 🟢 清空当前正在画的所有点

	virtual void undoLastPoint() {}; // 🟢 撤销上一个点（仅对线和面有效）

protected:
    TiledGraphicsView* m_view = nullptr;
};



#endif