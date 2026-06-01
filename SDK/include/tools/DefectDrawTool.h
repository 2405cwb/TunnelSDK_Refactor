#pragma once
#include "AbstractTool.h"
#include <QGraphicsPathItem>
#include "TunnelGlobal.h"
#include <QPen> 
class TiledGraphicsView;

class DefectDrawTool : public AbstractTool {
public:
    // 构造时传入要画什么形状
    DefectDrawTool(DrawShape shapeType) : m_shapeType(shapeType), m_previewItem(nullptr) {}

	void ChangeShapeType(DrawShape shapeType)
	{
		m_shapeType = shapeType;

		m_points.clear();
		if (m_previewItem)
		{
			m_previewItem->setPath(QPainterPath());
		}
	}

    void handleMousePress(QPointF scenePos, Qt::MouseButton button) override {
        if (button == Qt::LeftButton) {
            // 🟢 如果是画【点】，点一下就直接结束了
            if (m_shapeType == Shape_Point) {
                QPainterPath path;
                // 用一个小圆圈代表点 (半径这里默认给个 5，可调)
                path.addEllipse(scenePos, 5.0, 5.0);

                // 触发 View 发送信号给外部
                triggerSignalAndFinish(path);
                return;
            }

            // 如果是线或面，记录点并更新橡皮筋
            m_points.append(scenePos);
            updatePreview(scenePos);
        }
        else if (button == Qt::RightButton) {
            // 右键结束画线/画面
            if (m_shapeType != Shape_Point) {
                finishDrawing();
            }
        }
    }

    void handleMouseMove(QPointF scenePos) override {
        if (!m_points.isEmpty()) {
            updatePreview(scenePos); // 实时更新橡皮筋
        }
    }

    void deactivate() override {
        if (m_previewItem && m_view) {
            m_view->scene()->removeItem(m_previewItem);
            delete m_previewItem;
            m_previewItem = nullptr;
        }
        m_points.clear();
    }

	void undoLastPoint()
	{
		// 1. 如果还没点下任何点，直接无视
		if (m_points.isEmpty()) {
			return;
		}

		// 2. 踢掉最后一个确定点！
		m_points.removeLast();

		// 3. 边缘情况：如果点全删光了，彻底清空画面，回到刚按下 F3/F4 的初始状态
		if (m_points.isEmpty()) {
			if (m_previewItem) {
				m_previewItem->setPath(QPainterPath());
			}
			qDebug() << QString::fromLocal8Bit("🔙 撤销了起点，等待重新绘制...");
			return;
		}

		// 4. 重建剩下的确定路线
		QPainterPath newPath;
		newPath.moveTo(m_points.first());
		for (int i = 1; i < m_points.size(); ++i) {
			newPath.lineTo(m_points[i]);
		}

		// 5. 让橡皮筋线重新连上当前的鼠标！
		// 因为你在 Tool 里保存了 view 的指针 m_view，我们可以随时获取当前鼠标的真实位置
		QPointF currentMousePos = m_view->mapToScene(m_view->mapFromGlobal(QCursor::pos()));

		QPainterPath tempPath = newPath;
		tempPath.lineTo(currentMousePos);

		// 6. 刷新屏幕
		if (m_previewItem) {
			m_previewItem->setPath(tempPath);
		}

		qDebug() << "🔙 成功撤销上一个点，当前剩余点数:" << m_points.size();
	}

	void cancelDrawing() override {
		// 如果本来就没开始点，直接返回
		if (m_points.isEmpty()) {
			return;
		}

		// 1. 清空所有点位数组
		m_points.clear();

		// 2. 擦除屏幕上的临时预览虚线
		if (m_previewItem) {
			m_previewItem->setPath(QPainterPath());
		}
	}

private:
    void updatePreview(const QPointF& currentMousePos) {
        if (m_points.isEmpty() || !m_view) return;

        QPainterPath path;
        path.moveTo(m_points.first());
        for (int i = 1; i < m_points.size(); ++i) path.lineTo(m_points[i]);
        path.lineTo(currentMousePos);

        // 如果是多边形，预览时最好也连回起点，看起来更直观
        if (m_shapeType == Shape_Polygon && m_points.size() > 1) {
            path.lineTo(m_points.first());
        }

        if (!m_previewItem) {
            m_previewItem = new QGraphicsPathItem();
			QPen pen(Qt::red, 1, Qt::DashLine);
			pen.setCosmetic(true);
            m_previewItem->setPen(pen);
            m_view->scene()->addItem(m_previewItem);
        }
        m_previewItem->setPath(path);
    }

    void finishDrawing() {
        if (m_points.size() < 2) { deactivate(); return; }

        QPainterPath finalPath;
        finalPath.moveTo(m_points.first());
        for (int i = 1; i < m_points.size(); ++i) finalPath.lineTo(m_points[i]);

        // 🟢 如果是【面】，必须闭合路径；如果是【线】，就不闭合
        if (m_shapeType == Shape_Polygon) {
            finalPath.closeSubpath();
        }

        triggerSignalAndFinish(finalPath);
    }

    void triggerSignalAndFinish(const QPainterPath& path) {
        if (m_view) {
            // 工具不创建 Item！而是通知 View 把纯几何数据抛给上层！
            m_view->emitGeometryDrawn(m_shapeType, path);

			
        }
		if (m_previewItem)
		{
			deactivate();

		}
    }

private:
    DrawShape m_shapeType;
    QVector<QPointF> m_points;
    QGraphicsPathItem* m_previewItem;
};