#include "TiledGraphicsView.h"
#include <QOpenGLWidget>
#include <QScrollBar>
#include <QMouseEvent>
#include <QDebug>
#include <QApplication>
#include <QtMath>
#include <QMenu>
#include <QMessageBox>

#include "./tools/DefectDrawTool.h"
#include "./items/DefectShapeItem.h"

TiledGraphicsView::TiledGraphicsView(QWidget* parent) : QGraphicsView(parent),m_scrollSpeed(50), m_orientation(LayoutOrientation::Horizontal)
{ 
	m_curDrawShape = Shape_Line;

    // 🐶 看门狗： 
    if (this->devicePixelRatio() > 1.0) {
        if (!QCoreApplication::testAttribute(Qt::AA_EnableHighDpiScaling)) {
            qCritical() << "❌【严重警告】检测到高分屏但未开启缩放！";
            qCritical() << "   请在 main.cpp 的 QApplication 创建前添加:";
            qCritical() << "   QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);";
            // 这里仅仅打印日志，不弹窗也不崩溃，起到提示作用即可
        }
    }
    // 配置 GraphicsView (硬件加速、事件拦截)
    setupGraphicsView();
    // 绑定信号槽、快捷键、定时器
    setupConnections();
	setViewMode(Mode_Browse);
	// 🟢 挂载万能绘制工具，并告诉它画什么形状
	m_currentTool = new DefectDrawTool(m_curDrawShape);
	m_currentTool->setView(this);
}

TiledGraphicsView::~TiledGraphicsView()
{ 
    clear();
	delete m_currentTool;
	m_currentTool = nullptr;
}

// 设置视图名称
void TiledGraphicsView::setViewName(QString strName)
{
	m_strViewName = strName;
}

// 获取视图名称
QString TiledGraphicsView::getViewName()
{
	return m_strViewName;
}

QPointF TiledGraphicsView::mapToGlobalScene(const QString & imageName, qreal localX, qreal localY)
{
	for (TunnelSectionItem* item : m_items)
	{
		if (item->getImageName().contains(imageName))
		{
			return item->mapToScene(QPointF(localX, localY));

		}
	}
	//if (m_items.size()==0)
	//{
	//	return QPointF(0,0);
	//}
	double curImageMile = imageName.split("-").at(0).mid(4).toDouble();
	double BegImageMile = m_items[0]->getImageName().split("-").at(0).mid(4).toDouble();
	if (curImageMile <= BegImageMile)
	{
		return  m_items[0]->mapToScene(QPointF(0, localY));
	}
	else
	{
		return  m_items[m_items.size() - 1]->mapToScene(QPointF(m_items[m_items.size() - 1]->boundingRect().width(), localY));
	}

	qWarning() << QString::fromLocal8Bit("坐标映射失败:未找到名称为") << imageName << "的图层";
	return QPointF(0, 0);
}

void TiledGraphicsView::focusOnPosition(const QPointF& scenePos, double targetScale)
{
    // 1. 如果指定了缩放级别，先应用缩放
    if (targetScale > 0) {
        // [限制上下限逻辑] 保持和你 wheelEvent 里一样的逻辑
        double minScale = getFitScale();
        double maxScale = 5.0;
        if (targetScale < minScale) targetScale = minScale;
        if (targetScale > maxScale) targetScale = maxScale;

        resetTransform();
        scale(targetScale, targetScale);
        m_currentScale = targetScale;
    }

    // 自动计算滚动条并居中目标点！
    centerOn(scenePos);


	// 🟢直接问 Scene 谁在这个点上，不用自己遍历 list
	// items() 返回的是 Z 值从上到下的列表，第一个通常就是最上面的
	QList<QGraphicsItem*> items = m_scene->items(scenePos);
	TunnelSectionItem* hoverItem = nullptr;
	for (auto item : items) {
		// 使用 dynamic_cast 确认是不是我们要找的切片层
		hoverItem = dynamic_cast<TunnelSectionItem*>(item);
		if (hoverItem) break;
	}

	// 3. 发送坐标信号给外部 HUD
	if (hoverItem) {
		QPointF localPos = hoverItem->mapFromScene(scenePos);
		emit sigCursorInfoChanged(
			hoverItem->getImageName(),
			(int)localPos.x(),
			(int)localPos.y()
		);
	}
	else {
		emit sigCursorInfoChanged("", 0, 0);
	}

    // 3. 强制触发一次高清切片加载和 LOD 刷新
    updateVisibleTiles();
}

void TiledGraphicsView::addLayer(AbstractTileSource* source)
{
    TunnelSectionItem* item = new TunnelSectionItem(source);
	connect(item, &TunnelSectionItem::sigTilesUpdated, this, &TiledGraphicsView::updateHUD);
    m_scene->addItem(item);

    // 🟢 自动拼接逻辑
    // 获取当前场景的总宽度，作为新 Item 的起始 X 坐标
    double offsetX = 0;
    if (!m_items.isEmpty()) {
        TunnelSectionItem* last = m_items.last();
        offsetX = last->pos().x() + last->boundingRect().width();
		
		if (m_items.size() <= 1)
		{
			last->m_senceRect = QRectF(0, 0, last->boundingRect().width(), last->boundingRect().height());
		}
		else
		{
			last->m_senceRect = QRectF(last->pos().x(), 0, last->boundingRect().width(), last->boundingRect().height());
		}
	
    }

    item->setPos(offsetX, 0);
    m_items.append(item);

    // 更新场景边界
    QRectF allRect = m_scene->itemsBoundingRect();
    m_scene->setSceneRect(allRect);

    // 如果是第一张图，自动适应高度
    if (m_items.size() == 1) {

         resetTransform();
        double startScale = getFitScale();
        scale(startScale, startScale);
        m_currentScale = startScale;
        
    }
}

void TiledGraphicsView::clear()
{
	if (m_vecCp3Manager) m_vecCp3Manager->clearDefects();
	if (m_vecPlatformManager) m_vecPlatformManager->clearDefects();
	if (m_vecChainManager) m_vecChainManager->clearDefects();
	if (m_defectManager) m_defectManager->clearDefects();
	if (m_vecTunnelLocManager) m_vecTunnelLocManager->clearDefects();
	if (m_vecRingInfoManager) m_vecRingInfoManager->clearDefects();
	if (m_vecSectionManager) m_vecSectionManager->clearDefects();
	if (m_vecReAutoRingManager) m_vecReAutoRingManager->clearDefects();
	if (m_currentTool) m_currentTool->deactivate();

    m_scene->clear(); // 这会 delete 掉所有的 item
    m_items.clear();
	m_exportBoxItem = nullptr;
    m_scene->setSceneRect(0, 0, 0, 0);
}


// 🟢 1. 实现公开的重置接口
void TiledGraphicsView::resetToFit()
{ 
    double s = getFitScale();
    resetTransform();
    scale(s, s);
    m_currentScale = s;
    updateVisibleTiles();
}


void TiledGraphicsView::updateHUD()
{
	int loadedTiles = 0;
	for (auto item : m_items) {
	 
		loadedTiles += item->getLoadedTileCount();
	}
 bool	isHighResMode = false;
	if (loadedTiles > 0)
	{
		isHighResMode = true;
	}

	
	QString quality = isHighResMode ? QString::fromLocal8Bit("高清") : QString::fromLocal8Bit("预览");

	QString stats = QString::fromLocal8Bit(
		"绘制模式: %1 | 切片: %2 | 缩放: %3% | 画质: %4 |LOD系数: %5"
	).arg(DrawModelStr).arg(loadedTiles)
		.arg(m_currentScale * 100, 0, 'f', 0).arg(quality)
		.arg(m_lodThresholdMultiplier, 0, 'f', 2);

	emit sigStatsUpdated(stats);
}

void TiledGraphicsView::setViewMode(ViewMode mode)
{
    m_currentMode = mode; 
 	setDragMode(QGraphicsView::NoDrag);
	if (m_exportBoxItem)
	{
		m_exportBoxItem->hide();
	}
    if (mode == Mode_Browse) {
        setDragMode(QGraphicsView::NoDrag);
        setCursor(Qt::ArrowCursor);
		DrawModelStr = QString::fromLocal8Bit("浏览模式");
    }
    else if (mode == Mode_Draw)
    {
		setDragMode(QGraphicsView::NoDrag);
		setCursor(Qt::CrossCursor); // 绘图模式用十字光标
		DrawModelStr = QString::fromLocal8Bit("绘图模式");

		startDrawingDefect(m_curDrawShape);
    }
	else if (mode == Mode_ExportBox)
     {

		setCursor(Qt::BlankCursor);
		DrawModelStr = QString::fromLocal8Bit("定焦截图模式");
		if (!m_exportBoxItem)
		{
			m_exportBoxItem = new QGraphicsRectItem;
			QPen pen(Qt::yellow, 3, Qt::DashLine);
			pen.setCosmetic(true);
			m_exportBoxItem->setPen(pen);
			m_exportBoxItem->setZValue(9999);
			m_scene->addItem(m_exportBoxItem);
		}
		m_exportBoxItem->setRect(0, 0, exportBoxSize, exportBoxSize);
		m_exportBoxItem->show();
     
    }

    // 可以在这里触发 updateVisibleTiles 以刷新可能的 UI 状态
	updateVisibleTiles();
}

// =========================================================
// 🟢 核心交互逻辑
// =========================================================

void TiledGraphicsView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
	if (horizontalScrollBar()->isSliderDown() || verticalScrollBar()->isSliderDown())
	{
		m_debounceTimer->start();
		return;
	} 
	
    // 触发防抖更新 (LOD 计算)
    m_debounceTimer->start();
}

void TiledGraphicsView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);

     double minScale = getFitScale();
    if (m_currentScale < minScale || qAbs(m_currentScale - minScale) < 0.001) {
        resetTransform();
        scale(minScale, minScale);
        m_currentScale = minScale;
    } 
    updateVisibleTiles();
}

void TiledGraphicsView::wheelEvent(QWheelEvent* event) {
    // ---------------------------------------------------------
     // 🟢 1. 缩放逻辑 (Ctrl + 滚轮)
     // ---------------------------------------------------------
    if (event->modifiers() & Qt::ControlModifier) {

        int angle = event->angleDelta().y();
        if (angle == 0) return;

        // 1. 获取当前缩放系数 (m11 是 x 轴缩放，通常 xy 一致)
        double currentScale = transform().m11();

        // 2. 计算缩放因子 (滚轮向上放大 1.1 倍，向下缩小 0.9 倍)
        double scaleFactor = (angle > 0) ? 1.15 : (1.0 / 1.15);

        // 3. 预测下一次的缩放值
        double nextScale = currentScale * scaleFactor;

        // =====================================================
        // 🟢 核心修改：动态计算边界
        // =====================================================

        // [下限]：不允许缩得比“适应窗口”还小
        // 这样用户狂滚滚轮，最后一定会停在刚好铺满屏幕的状态，非常舒服
        double minScale = getFitScale();

        // [上限]：最大允许放大到 5.0 倍 (即 1 个像素变 5 个像素大)
        // 隧道病害一般看清裂缝即可，5.0 足够了，太大全是锯齿
        double maxScale = 20.0;

      
        // 如果下一次缩放会超出边界，就只缩放到边界值
        if (nextScale < minScale) {
            scaleFactor = minScale / currentScale;
            nextScale = minScale;
        }
        else if (nextScale > maxScale) {
            scaleFactor = maxScale / currentScale;
            nextScale = maxScale;
        }

        // 5. 应用缩放
        // 只有当实际上发生了变化，且变化不过分微小的时候才执行
        if (qAbs(scaleFactor - 1.0) > 0.001) {
            setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
            scale(scaleFactor, scaleFactor);
            m_currentScale = nextScale; // 更新成员变量
            double minScale = getFitScale();
            bool isFitState = (m_currentScale <= minScale + 0.001);
           
            updateVisibleTiles();       // 触发 LOD
        }

        event->accept();
        return;
    }

    // 2. 滚动逻辑 (修复 Shift 支持)
    int delta = event->angleDelta().y();
    if (delta == 0) return;

    // 获取 Shift 键状态
    bool isShiftPressed = (event->modifiers() & Qt::ShiftModifier);

    if (m_orientation == LayoutOrientation::Vertical) {
        // === 纵向模式 (地铁) ===
        if (isShiftPressed) {
            // Shift: 滚水平条 (左右看)
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta);
        }
        else {
            // 普通: 滚垂直条 (上下跑里程)
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta);
        }
    }
    else {
        // === 横向模式 (公路) ===
        if (isShiftPressed) {
            // 🟢 修复点：Shift -> 滚垂直条 (上下看墙壁)
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta);
        }
        else {
            // 普通: 滚水平条 (左右跑里程)
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta);
        }
    } 
    event->accept(); 
}

void TiledGraphicsView::keyPressEvent(QKeyEvent* event) {
    
    // 如果外部禁用了导航（比如正在输入文字，或处于编辑模式），直接透传给父类
    if (!m_enableKeyNav) {
        QGraphicsView::keyPressEvent(event);
        return;
    }
    // 小优化：处理 Shift 加速
    int speed = m_scrollSpeed;
    bool isShift = (event->modifiers() & Qt::ShiftModifier);
    if (isShift) {
        speed *= 4;
        m_isFastScrolling = true; // 🚨 进入飙车模式
    }
   

    switch (event->key()) {
    case Qt::Key_Space:
        resetToFit();   // 调用之前封装好的公有槽函数
        event->accept(); // 标记事件已处理，防止传递给父类导致冲突
        break;
    case Qt::Key_W:
    case Qt::Key_Up:
        // W 键：只减小垂直滚动条 (向上)
        verticalScrollBar()->setValue(verticalScrollBar()->value() - speed);
        event->accept();
        break;

    case Qt::Key_S:
    case Qt::Key_Down:
        // S 键：只增加垂直滚动条 (向下)
        verticalScrollBar()->setValue(verticalScrollBar()->value() + speed);
        event->accept();
        break;

    case Qt::Key_A:
    case Qt::Key_Left:
        // A 键：只减小水平滚动条 (向左)
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - speed);
        event->accept();
        break;

    case Qt::Key_D:
    case Qt::Key_Right:
        // D 键：只增加水平滚动条 (向右)
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() + speed);
        event->accept();
        break;

    default:
        QGraphicsView::keyPressEvent(event);
        break;
    }
     

    //  如果是快速滚动，不要立即触发重绘逻辑，或者只做轻量级更新
    if (!isShift) {
        // 普通移动，照常处理
        QGraphicsView::keyPressEvent(event);
    }
    else {
        // 飙车模式下，我们 accept 事件，让滚动条动，但暂时不让 View 疯狂加载
        event->accept();
    }
}

void TiledGraphicsView::keyReleaseEvent(QKeyEvent* event)
{ 
    if (event->key() == Qt::Key_Shift) {
        m_isFastScrolling = false; // 🚨 飙车结束

        // 立即触发一次全量加载，把高清图刷出来
        updateVisibleTiles();
    }

    QGraphicsView::keyReleaseEvent(event);
}

void TiledGraphicsView::mouseMoveEvent(QMouseEvent* event)
{
	QPointF scensPos = mapToScene(event->pos());

	if (m_currentMode == Mode_ExportBox && m_exportBoxItem)
	{
		QRectF boxRect(0,0, exportBoxSize, exportBoxSize);
		boxRect.moveCenter(scensPos);
		m_exportBoxItem->setRect(boxRect);
	}
	// 🟢 1. 处理中键拖拽
	if (m_isPanning) {
		// 计算鼠标移动的差值
		int dx = event->pos().x() - m_lastMousePos.x();
		int dy = event->pos().y() - m_lastMousePos.y();

		// 拨动滚动条 (方向相反，鼠标往右划，内容往右走，滚动条其实是往左减)
		horizontalScrollBar()->setValue(horizontalScrollBar()->value() - dx);
		verticalScrollBar()->setValue(verticalScrollBar()->value() - dy); 
		// 更新坐标
		m_lastMousePos = event->pos();
		event->accept();
		return;  
	}

	// 🟢 绘图模式拦截：让工具画出跟随的虚线
	if (m_currentMode == Mode_Draw && m_currentTool) {
		QPointF scenePos = mapToScene(event->pos());
		m_currentTool->handleMouseMove(scenePos);
	}
	// 💥 3. 核心引擎：自定义病害集群精准拖拽！
	// ==========================================
	// 如果左键按下并处于拖拽状态，接管坐标换算
	if (m_isDraggingDefects) {
		QPointF currentScenePos = mapToScene(event->pos());
		// 计算鼠标在真实的物理世界里移动了多少距离
		QPointF delta = currentScenePos - m_lastDragScenePos;

		// 让所有被选中的病害跟着走，指哪打哪，绝对不乱飘！
		for (QGraphicsItem* item : m_scene->selectedItems()) {
			if (DefectShapeItem* defect = dynamic_cast<DefectShapeItem*>(item)) {
				defect->setPos(defect->pos() + delta);
			}
		}

		m_lastDragScenePos = currentScenePos; // 更新坐标
		event->accept();
		return; //  
	}
	// ==========================================
	// 💥 4. 浏览模式下的光标“雷达反馈”
	// ==========================================
	// 只有在没按任何鼠标键（纯移动探测）时，才触发嗅探
	if (m_currentMode == Mode_Browse && event->buttons() == Qt::NoButton) {

		// 调用我们强大的动态物理雷达探测器
		QList<QGraphicsItem*> items = getVisualItems(event->pos());

		bool hoverOnDefect = false;
		for (auto item : items) {
			if (dynamic_cast<DefectShapeItem*>(item)) {
				hoverOnDefect = true;
				break; // 只要探测到范围里有病害，立刻标记
			}
		}

		// 🟢 动态切换光标：碰到病害变“点击小手👆”，离开变“普通箭头↖”
		if (hoverOnDefect) {
			setCursor(Qt::PointingHandCursor);
		}
		else {
			setCursor(Qt::ArrowCursor);
		}
	}




    QGraphicsView::mouseMoveEvent(event);

    // 1. 获取鼠标在 Scene 中的坐标
    QPointF scenePos = mapToScene(event->pos());

    // 🟢直接问 Scene 谁在这个点上，不用自己遍历 list
    // items() 返回的是 Z 值从上到下的列表，第一个通常就是最上面的
    QList<QGraphicsItem*> items = m_scene->items(scenePos);
    TunnelSectionItem* hoverItem = nullptr;
    for (auto item : items) {
        // 使用 dynamic_cast 确认是不是我们要找的切片层
        hoverItem = dynamic_cast<TunnelSectionItem*>(item);
        if (hoverItem) break;
    }

    // 3. 发送坐标信号给外部 HUD
    if (hoverItem) {
        QPointF localPos = hoverItem->mapFromScene(scenePos);
        QString info = QString::fromLocal8Bit("图名: %1 | 坐标: (%2, %3)")
            .arg(hoverItem->getImageName())
            .arg((int)localPos.x())
            .arg((int)localPos.y());
        emit sigCursorInfoChanged(
            hoverItem->getImageName(),
            (int)localPos.x(),
            (int)localPos.y()
        );
    }
    else {
        emit sigCursorInfoChanged("", 0, 0);
    }
}

void TiledGraphicsView::mousePressEvent(QMouseEvent* event)
{ 
	// 1. 中键拖拽背景
	if (event->button() == Qt::MiddleButton) {
		m_isPanning = true;
		m_lastMousePos = event->pos();
		setCursor(Qt::ClosedHandCursor);
		event->accept();
		return;
	}

	// 2. 绘图模式
	if (m_currentMode == Mode_Draw && m_currentTool) {
		m_currentTool->handleMousePress(mapToScene(event->pos()), event->button());
		event->accept();
		return;
	}
	 if (m_currentMode == Mode_ExportBox && event->button() == Qt::RightButton)
	 {
		 if (m_exportBoxItem)
		 {
			 QRectF exprotRect = m_exportBoxItem->rect();

			 QPixmap resultpix = exportRegionData(exprotRect, Export_HighRes, false);
			 if (!resultpix.isNull())
			 {
				 emit sigRegionExported(resultpix);
			 }
		 }
		 event->accept();
		 return;
	 }
  
	if (m_currentMode == Mode_Browse && event->button() == Qt::LeftButton)
	{
		if (event->modifiers() & Qt::ShiftModifier) {
			setDragMode(QGraphicsView::RubberBandDrag);
			QGraphicsView::mousePressEvent(event);
			return;
		}
		else {
			setDragMode(QGraphicsView::NoDrag);

			QList<QGraphicsItem*> items = getVisualItems(event->pos());
			DefectShapeItem* hitDefect = nullptr;
			for (auto item : items) {
				if (DefectShapeItem* defect = dynamic_cast<DefectShapeItem*>(item)) {
					hitDefect = defect;
					break;
				}
			}

			if (hitDefect) {
				if (!hitDefect->isSelected()) {
					m_scene->clearSelection();
					hitDefect->setSelected(true);
				}
				// 💥 核心开启：激活我们自己的上帝拖拽引擎！
				m_isDraggingDefects = true;
				m_lastDragScenePos = mapToScene(event->pos());
				event->accept();
				return;
			}
			else {
				m_scene->clearSelection();
			}
		}
	}

	if (event->button() == Qt::RightButton) {
		QList<QGraphicsItem*> items = getVisualItems(event->pos());
		emit sigContextMenuRequested(event->globalPos(), items);
		event->accept();
		return;
	}

    QGraphicsView::mousePressEvent(event);
}

void TiledGraphicsView::mouseReleaseEvent(QMouseEvent* event)
{

	if (m_isDraggingDefects && event->button() == Qt::LeftButton) {
		m_isDraggingDefects = false;

		// 遍历所有选中的兄弟，挨个结算最终坐标写入数据库
		for (QGraphicsItem* item : m_scene->selectedItems()) {
			if (DefectShapeItem* defect = dynamic_cast<DefectShapeItem*>(item)) {
				defect->finalizeMove();
			}
		}
		event->accept();
		return;
	} 

	//  必须在最前面调用父类，让 Qt 引擎优先结算框选逻辑
	QGraphicsView::mouseReleaseEvent(event);

	//  松开中键
	if (event->button() == Qt::MiddleButton) {
		m_isPanning = false;


		if (m_currentMode == Mode_Browse) {
			setCursor(Qt::ArrowCursor);
		}
		else {
			setCursor(Qt::CrossCursor);
		}
		event->accept();
		return;
	}

	// ==========================================
	//   浏览模式松开左键：结算并抛出框选结果
	// ==========================================
	if (m_currentMode == Mode_Browse && event->button() == Qt::LeftButton)
	{
		if (dragMode() == QGraphicsView::RubberBandDrag) {

			QList<QGraphicsItem*> allSelected = m_scene->selectedItems();
			QList<DefectShapeItem*> selectedDefects;

			for (auto item : allSelected) {
				if (DefectShapeItem* defect = dynamic_cast<DefectShapeItem*>(item)) {

					if (defect->m_elementType != Type_Disease)
					{
						item->setSelected(false);
						continue;
					}
					selectedDefects.append(defect);
				}
				else {
					item->setSelected(false);
				}
			}

			// 发送信号给 MainWindow
			if (!selectedDefects.isEmpty()) {

				QTimer::singleShot(0, this, [this, selectedDefects]() {
					emit sigDefectsSelected(selectedDefects);
				});
			}
		}
		setDragMode(QGraphicsView::NoDrag);
	}
}

// =========================================================
// 🟢 核心逻辑与辅助函数
// =========================================================

void TiledGraphicsView::setupGraphicsView()
{
    setFrameShape(QFrame::NoFrame);
    // 1. 初始化场景
    m_scene = new QGraphicsScene(this);
    m_scene->setBackgroundBrush(QColor(40, 40, 40)); // 深灰色背景
    setScene(m_scene);

    // =========================================================
    // 🟢 性能与渲染配置 (原 MainWindow::setupGraphicsView)
    // =========================================================

    // 开启 OpenGL 硬件加速 (极其重要)
    setViewport(new QOpenGLWidget());

    // 强制全屏重绘，避免局部刷新留下的伪影
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    // 开启鼠标追踪 (即使不按键也能收到 MouseMove，用于显示坐标)
    setMouseTracking(true);
    viewport()->setMouseTracking(true);

    // 变换锚点设为鼠标中心 (缩放时以鼠标为中心)
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);

    // 抗锯齿
    setAlignment(Qt::AlignLeft | Qt::AlignTop); 
    setRenderHint(QPainter::Antialiasing);

    // 滚动条策略
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // 默认模式
    setDragMode(QGraphicsView::NoDrag);
    //setDragMode(QGraphicsView::ScrollHandDrag);
    setFocusPolicy(Qt::StrongFocus);
    setFocus(); // 启动时获取焦点

	m_vecCp3Manager = new DefectManager(m_scene, this);
	m_vecPlatformManager = new DefectManager(m_scene, this);
	m_vecChainManager = new DefectManager(m_scene, this);
	m_defectManager = new DefectManager(m_scene, this);
	m_vecTunnelLocManager = new DefectManager(m_scene, this);
	m_vecRingInfoManager = new DefectManager(m_scene, this);
	m_vecSectionManager = new DefectManager(m_scene, this);
	m_vecReAutoRingManager = new DefectManager(m_scene, this);

}

void TiledGraphicsView::setupConnections()
{
    // 2. 初始化防抖定时器
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(15); // 15ms 延迟

    auto triggerScroll = [this]() {
		m_debounceTimer->start();
        };

    // 3. 监听滚动条变化
    connect( horizontalScrollBar(), &QScrollBar::valueChanged, this, triggerScroll);
    connect( verticalScrollBar(), &QScrollBar::valueChanged, this, triggerScroll); 
    connect(m_debounceTimer, &QTimer::timeout, this, &TiledGraphicsView::updateVisibleTiles);

	auto onSilderReleased = [this]()
	{ 
		m_debounceTimer->start();
	};
	connect(horizontalScrollBar(), &QScrollBar::sliderReleased, this, onSilderReleased);
	connect(verticalScrollBar(), &QScrollBar::sliderReleased, this, onSilderReleased);

	 

	// 获取视图窗口区间
	connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [=]() {
		QRect viewPortRect = viewport()->rect();

		// 1. 获取鼠标在 Scene 中的坐标
		QPointF sceneTopLPos = mapToScene(viewPortRect.topLeft());
		QPointF sceneBottomRPos = mapToScene(viewPortRect.bottomRight());
		QPointF centerPos = QPointF((sceneTopLPos.x() + sceneBottomRPos.x()) / 2.0, sceneTopLPos.y());

		QString strNameL, strNameR;
		int xL, yL, xR, yR;
		bool bL = GlobalSceneToMap(sceneTopLPos, strNameL, xL, yL);
		bool bR = GlobalSceneToMap(sceneBottomRPos, strNameR, xR, yR);

		if (bL && bR)
		{
			emit sigViewInfoChanged(strNameL, xL, yL,
				strNameR, xR, yR);
		}

	});

}

void TiledGraphicsView::updateVisibleTiles()
{
	
	QElapsedTimer timer;
	timer.start();

	// 1. 获取可视区域
	QRect viewportRect = viewport()->rect();
	QRectF visibleSceneRect = mapToScene(viewportRect).boundingRect();

	// =========================================================
	// 💥 终极丝滑魔法：双层空间结界！
	// =========================================================

	// 【内层结界】：高清图的 Buffer (稍微外扩一点点，防边缘穿帮)
	double highResBuffer = 50.0;
	QRectF highResRect = visibleSceneRect.adjusted(-highResBuffer, -highResBuffer, highResBuffer, highResBuffer);

	// 【外层结界】：缩略图的“雷达预警区” (向外狂扩 1.5 个屏幕的宽度！)
	// 这意味着用户还没滚到那里，提前 1.5 个屏幕的缩略图就已经在后台悄悄解压了
	double prefetchX = visibleSceneRect.width() * 1.5;
	double prefetchY = visibleSceneRect.height() * 1.5;
	QRectF prefetchRect = visibleSceneRect.adjusted(-prefetchX, -prefetchY, prefetchX, prefetchY);

	m_currentScale = transform().m11();

	// 2. 遍历大管家：统一调度所有 Item 的生死与预加载
	for (auto item : m_items) {

		// --- 🟢 A. 缩略图雷达预加载逻辑 ---
		QRectF itemRect = item->sceneBoundingRect();

		if (prefetchRect.intersects(itemRect)) {
			// 只要进入雷达区，立刻发起异步请求！
			item->ensureThumbnailRequested();
		}
		else {
			// 如果连雷达区都跌出去了，立刻销毁缩略图，严控内存！
			item->releaseThumbnail();
		}
		if (m_isFastScrolling) {
			item->unloadAll();
		}
		else
		{
			// --- 🟢 B. 高清图 LOD 降级逻辑 ---
			// 高清图绝不能用 prefetchRect，必须用极其克制的 highResRect
			item->updateVisibleTiles(highResRect, m_currentScale, m_lodThresholdMultiplier);
		} 
	}
	viewport()->update();
}

void TiledGraphicsView::undoLastDrawPoint()
{
    // 既然画图由 Tool 接管，撤销当然也直接甩锅给 Tool 去做！
    if (m_currentMode == Mode_Draw && m_currentTool) {
        m_currentTool->undoLastPoint();
    }
}

void TiledGraphicsView::cancelCurrentDrawing()
{
    // 只有在绘图模式下，且工具存在时，才通知工具清空画面
    if (m_currentMode == Mode_Draw && m_currentTool) {
        m_currentTool->cancelDrawing();
    }
}

 

double TiledGraphicsView::getFitScale() const
{
    if (m_items.isEmpty()) return 1.0;

    // 获取图片真实的物理边界
    QRectF itemsRect = m_scene->itemsBoundingRect();
    if (itemsRect.isEmpty()) return 1.0;

    QSize viewSize = viewport()->size();

    if (m_orientation == LayoutOrientation::Horizontal) {
        // 地铁模式：适应高度
        return (double)viewSize.height() / itemsRect.height();
    }
    else {
        // 竖井模式：适应宽度
        return (double)viewSize.width() / itemsRect.width();
    }
}

QList<QGraphicsItem*> TiledGraphicsView::getVisualItems(QPoint viewPos)
{
	// 🟢 1. 获取设备像素比 (关键修改)
	// 如果宿主程序没开缩放，高分屏下这里会返回 1.25, 1.5, 2.0 等
	// 如果开了缩放，或者普通屏，这里通常是 1.0
	qreal ratio = viewport()->devicePixelRatio();

	// 2. 设定基础容差 (逻辑像素)
	int baseTolerance = 5;

	// 缩放很小时（上帝视角），线条很细，很难点中，所以要大幅扩大容差
	if (m_currentScale < 0.2) {
		baseTolerance = 10;
	}

	// 🟢 3. 计算最终容差
	// 这样无论在什么屏幕上，物理点击面积都是差不多大的，手感一致
	int finalTolerance = static_cast<int>(baseTolerance * ratio);

	// 4. 构造点击矩形
	QRect viewRect(
		viewPos.x() - finalTolerance,
		viewPos.y() - finalTolerance,
		finalTolerance * 2,
		finalTolerance * 2
	);

	return m_scene->items(mapToScene(viewRect));
}

 

void TiledGraphicsView::setDrawingGeometry(DrawShape shapeType)
{
	if (m_curDrawShape != shapeType)
	{
		m_curDrawShape = shapeType;
		startDrawingDefect(shapeType);
	}

}

void TiledGraphicsView::startDrawingDefect(DrawShape shapeType)
{
	// 切换到绘图模式
	m_currentMode = Mode_Draw;

	if (m_currentTool) {
		m_currentTool->deactivate();
	//delete m_currentTool;
	}

	m_currentTool->ChangeShapeType(shapeType);
	setDragMode(QGraphicsView::NoDrag);
	setCursor(Qt::CrossCursor);

	

	// ==========================================
	// 🟢 修复点：动态更新左上角提示文字
	// ==========================================
	if (shapeType == Shape_Point) {
		DrawModelStr = QString::fromLocal8Bit("绘图模式 [点]");
	}
	else if (shapeType == Shape_Line) {
		DrawModelStr = QString::fromLocal8Bit("绘图模式 [线]");
	}
	else if (shapeType == Shape_Polygon) {
		DrawModelStr = QString::fromLocal8Bit("绘图模式 [面]");
	}

	// 🟢 强制触发一次可见性更新，立刻刷新左上角的 HUD 文字
	updateVisibleTiles();
}

void TiledGraphicsView::setLodLevel(int level) {
    // 1. 限制范围 1~10
    if (level < 1) level = 1;
    if (level > 10) level = 10;

    // 2. 将 1~10 映射到 1.5 ~ 0.5
    // Level 1  -> 1.5 (最晚显示)
    // Level 10 -> 0.5 (最早显示)

    // 步长 = (最大系数 - 最小系数) / (最大等级 - 1)
    // 步长 = (1.5 - 0.5) / 9.0 ≈ 0.1111

    double step = 1.25 / 9.0;

    m_lodThresholdMultiplier = 1.5 - (level - 1) * step;

    // 3. 立即刷新
    updateVisibleTiles();
}



bool TiledGraphicsView::GlobalSceneToMap(QPointF pt, QString & imageName, int& localX, int& localY)
{
	// 🟢直接问 Scene 谁在这个点上，不用自己遍历 list
	// items() 返回的是 Z 值从上到下的列表，第一个通常就是最上面的
	QList<QGraphicsItem*> items = m_scene->items(pt);

	TunnelSectionItem* hoverItem = nullptr;
	for (auto item : items) {
		// 使用 dynamic_cast 确认是不是我们要找的切片层
		hoverItem = dynamic_cast<TunnelSectionItem*>(item);
		if (hoverItem) break;
	}

	if (hoverItem)
	{
		// 3. 发送坐标信号给外部 HUD
		QPointF localPos = hoverItem->mapFromScene(pt);

		//imageName = QFileInfo(hoverItem->getImageName()).completeBaseName();
	//	QString nameTemp =     QFileInfo (hoverItem->getImageName()).baseName();

	//	int index = nameTemp.lastIndexOf(".");
		//imageName = nameTemp.mid(0, index);
		imageName = hoverItem->getImageName();
		localX = localPos.x();
		localY = localPos.y();

		return true;
	}

	return false;
}

void TiledGraphicsView::setHighLightElement(int uuid, ElementType ele)
{

	switch (ele)
	{
	case Type_Cp3:
	{

		break;
	}
	case Type_Chain:
	{

		break;
	}
	case Type_Disease:
	{
		DefectShapeItem * item = m_defectManager->getItemUseId(uuid);

		if (item == NULL)
		{
			return;
		}

		QRectF sceneRect = item->sceneBoundingRect();
		qreal margin = 1.85;

		QRectF targetRect(0, 0, sceneRect.width()* margin, sceneRect.height()* margin);
		targetRect.moveCenter(sceneRect.center());

		// 确保图像不失心变形
		fitInView(targetRect, Qt::KeepAspectRatio);

		item->setSelected(true);

		break;
	}
	case Type_Ring:
	{

		break;
	}
	case Type_Section:
	{

		break;
	}
	case Type_Platform:
	{

		break;
	}
	default:
		break;
	}

	updateVisibleTiles();      
}

void TiledGraphicsView::mouseDoubleClickEvent(QMouseEvent * event)
{
	if (event->button() == Qt::LeftButton)
	{
		if (event->button() == Qt::MouseEventCreatedDoubleClick)
		{
			// 获取鼠标下的物体
			QList<QGraphicsItem*> items = getVisualItems(event->pos());

			emit sigDoubleClickedLeft(mapToScene(event->pos()), items);
		}

	}
}

//TODO 新增功能 20260310 增加图像导出功能，
QPixmap TiledGraphicsView::exportRegionData(const QRectF& sceneRect, ExportQuality quality, bool drawDefects)
{
	if (sceneRect.isEmpty() || m_items.isEmpty()) return QPixmap();

	QSize targetSize(qCeil(sceneRect.width()), qCeil(sceneRect.height()));
	/*if (targetSize.width() > 16384 || targetSize.height() > 16384) {
		qWarning() << QString::fromLocal8Bit("⚠️ 截取区域过大，已自动等比缩小！"); 
		targetSize.scale(16384, 16384, Qt::KeepAspectRatio);
	}*/ 
	//QImage resultImage(targetSize, QImage::Format_ARGB32_Premultiplied);
	QImage resultImage(targetSize, QImage::Format_RGB888); 
	resultImage.fill(Qt::white);

	QPainter painter(&resultImage);
	painter.setRenderHint(QPainter::Antialiasing,false);
	painter.setRenderHint(QPainter::SmoothPixmapTransform,false);
	 
	painter.setWindow(sceneRect.toRect());
	painter.setViewport(resultImage.rect());

	for (TunnelSectionItem* item : m_items) {
		QRectF itemRect = item->sceneBoundingRect();
		if (!sceneRect.intersects(itemRect)) continue; 
		AbstractTileSource* source = item->getSource();

		if (quality == Export_Thumbnail) {
			QImage thumb(source->getThumbnailImage());
			if (!thumb.isNull()) { 
				painter.drawImage(itemRect, thumb);
			}
		}
		else {
			int tSize = item->getTileSize();
			QRectF localIntersect = item->mapRectFromScene(sceneRect.intersected(itemRect)); 
			int startCol = qMax(0, qFloor(localIntersect.left() / tSize));
			int endCol = qFloor(localIntersect.right() / tSize);
			int startRow = qMax(0, qFloor(localIntersect.top() / tSize));
			int endRow = qFloor(localIntersect.bottom() / tSize);


			const QList<TileImageData> tiles = source->tileDataRange(startCol, endCol, startRow, endRow);
			for (const TileImageData& tileData : tiles) {
				QImage tile = QImage::fromData(tileData.data, "JPG");
				if (tile.isNull()) continue;

				QRectF tileSceneRect(
					itemRect.x() + tileData.col * tSize,
					itemRect.y() + tileData.row * tSize,
					tile.width(),
					tile.height()
				);
				painter.drawImage(tileSceneRect, tile);
			}
		}
	}

	// =========================================================
	// 绘制矢量病害层
	// =========================================================
	if (drawDefects) {
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
		// 1. 隐藏隧道底图
		for (TunnelSectionItem* item : m_items) {
			item->hide();
		}

		// 💥 2. 核心修复：临时抽走 Scene 的“黑背景”，防止它覆盖我们拼好的图片！
		QBrush oldBgBrush = m_scene->backgroundBrush();
		m_scene->setBackgroundBrush(Qt::NoBrush);

		// 3. 充当翻译官：统一两边的坐标系
		painter.setWindow(0, 0, targetSize.width(), targetSize.height());
		painter.setViewport(resultImage.rect());

		// 4. 画病害！此时因为背景是 NoBrush，病害会直接以透明底盖在我们的图片上
		m_scene->render(&painter, resultImage.rect(), sceneRect);

		// 💥 5. 打扫战场：把背景色和底图全还给界面，做到神不知鬼不觉
		m_scene->setBackgroundBrush(oldBgBrush);
		for (TunnelSectionItem* item : m_items) {
			item->show();
		}
	}

	painter.end();
	return QPixmap::fromImage(resultImage);
}





//TODO 新增功能 增加图像导出功能，返回 cv::Mat 灰度图格式 (极速单通道零拷贝版)
cv::Mat TiledGraphicsView::exportRegionGrayMat(const QRectF& sceneRect, ExportQuality quality, bool drawDefects)
{
	if (sceneRect.isEmpty() || m_items.isEmpty()) return cv::Mat();

	QSize targetSize(qCeil(sceneRect.width()), qCeil(sceneRect.height()));
	//if (targetSize.width() > 16384 || targetSize.height() > 16384) {
	//	qWarning() << QString::fromLocal8Bit("⚠️ 截取区域过大，已自动等比缩小！");
	//	targetSize.scale(16384, 16384, Qt::KeepAspectRatio);
	//}

	QImage resultImage(targetSize, QImage::Format_Grayscale8);
	resultImage.fill(Qt::white);

	QPainter painter(&resultImage);
	painter.setRenderHint(QPainter::Antialiasing, false);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

	painter.setWindow(sceneRect.toRect());
	painter.setViewport(resultImage.rect());

	for (TunnelSectionItem* item : m_items) {
		QRectF itemRect = item->sceneBoundingRect();
		if (!sceneRect.intersects(itemRect)) continue;

		AbstractTileSource* source = item->getSource();

		if (quality == Export_Thumbnail) {
			// 缩略图逻辑（按需放开）
		}
		else {
			int tSize = item->getTileSize();
			QRectF localIntersect = item->mapRectFromScene(sceneRect.intersected(itemRect));

			int startCol = qMax(0, qFloor(localIntersect.left() / tSize));
			int endCol = qFloor(localIntersect.right() / tSize);
			int startRow = qMax(0, qFloor(localIntersect.top() / tSize));
			int endRow = qFloor(localIntersect.bottom() / tSize);

			const QList<TileImageData> tiles = source->tileDataRange(startCol, endCol, startRow, endRow);
			for (const TileImageData& tileData : tiles) {
				QImage tile = QImage::fromData(tileData.data, "JPG");
				if (tile.isNull()) continue;

				QRectF tileSceneRect(
					itemRect.x() + tileData.col * tSize,
					itemRect.y() + tileData.row * tSize,
					tile.width(),
					tile.height()
				);
				painter.drawImage(tileSceneRect, tile);
			}
		}
	}

	// 绘制矢量病害层
	if (drawDefects) {
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

		for (TunnelSectionItem* item : m_items) {
			item->hide();
		}

		QBrush oldBgBrush = m_scene->backgroundBrush();
		m_scene->setBackgroundBrush(Qt::NoBrush);

		painter.setWindow(0, 0, targetSize.width(), targetSize.height());
		painter.setViewport(resultImage.rect());

		// 💥 这里哪怕你界面上画的是大红大黄的线段，最终砸进 finalMat 也会自动变成灰阶线段
		m_scene->render(&painter, resultImage.rect(), sceneRect);

		m_scene->setBackgroundBrush(oldBgBrush);
		for (TunnelSectionItem* item : m_items) {
			item->show();
		}
	}

	painter.end();

	cv::Mat finalMat(resultImage.height(),
		resultImage.width(),
		CV_8UC1,
		resultImage.bits(),
		resultImage.bytesPerLine());
	return finalMat.clone();
}


cv::Mat TiledGraphicsView::exportRegionMat(const QRectF& sceneRect,
	ExportQuality quality,
	bool drawDefects,
	bool returnBgr)
{
	if (sceneRect.isEmpty() || m_items.isEmpty()) {
		return cv::Mat();
	}

	QSize targetSize(qCeil(sceneRect.width()), qCeil(sceneRect.height()));
	if (targetSize.width() <= 0 || targetSize.height() <= 0) {
		return cv::Mat();
	}

	//if (targetSize.width() > 16384 || targetSize.height() > 16384) {
	//	qWarning() << QString::fromLocal8Bit("⚠️ 截取区域过大，已自动等比缩小！");
	//	targetSize.scale(16384, 16384, Qt::KeepAspectRatio);
	//}

	QImage resultImage(targetSize, QImage::Format_RGB888);
	resultImage.fill(Qt::white);

	QPainter painter(&resultImage);
	painter.setRenderHint(QPainter::Antialiasing, false);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

	painter.setWindow(sceneRect.toRect());
	painter.setViewport(resultImage.rect());

	for (TunnelSectionItem* item : m_items) {
		QRectF itemRect = item->sceneBoundingRect();
		if (!sceneRect.intersects(itemRect)) {
			continue;
		}

		AbstractTileSource* source = item->getSource();
		if (!source) {
			continue;
		}

		if (quality == Export_Thumbnail) {
			// 你当前原函数里缩略图逻辑也是注释掉的，这里先保持一致
			continue;
		}

		int tSize = item->getTileSize();
		QRectF localIntersect = item->mapRectFromScene(sceneRect.intersected(itemRect));

		int startCol = qMax(0, qFloor(localIntersect.left() / tSize));
		int endCol = qFloor(localIntersect.right() / tSize);
		int startRow = qMax(0, qFloor(localIntersect.top() / tSize));
		int endRow = qFloor(localIntersect.bottom() / tSize);

		const QList<TileImageData> tiles = source->tileDataRange(startCol, endCol, startRow, endRow);
		for (const TileImageData& tileData : tiles) {
			QImage tile = QImage::fromData(tileData.data, "JPG");
			if (tile.isNull()) {
				continue;
			}

			if (tile.format() != QImage::Format_RGB888) {
				tile = tile.convertToFormat(QImage::Format_RGB888);
			}

			QRectF tileSceneRect(
				itemRect.x() + tileData.col * tSize,
				itemRect.y() + tileData.row * tSize,
				tile.width(),
				tile.height()
			);

			painter.drawImage(tileSceneRect, tile);
		}
	}

	// =========================================================
	// 绘制矢量病害层
	// 这里仍然是画到 resultImage，但 resultImage 背后就是 matRgb.data
	// =========================================================
	if (drawDefects) {
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

		for (TunnelSectionItem* item : m_items) {
			item->hide();
		}

		QBrush oldBgBrush = m_scene->backgroundBrush();
		m_scene->setBackgroundBrush(Qt::NoBrush);

		painter.setWindow(0, 0, targetSize.width(), targetSize.height());
		painter.setViewport(resultImage.rect());

		m_scene->render(&painter, resultImage.rect(), sceneRect);

		m_scene->setBackgroundBrush(oldBgBrush);

		for (TunnelSectionItem* item : m_items) {
			item->show();
		}
	}

	painter.end();

	cv::Mat matRgb(resultImage.height(),
		resultImage.width(),
		CV_8UC3,
		resultImage.bits(),
		resultImage.bytesPerLine());

	// 如果后续还要 Qt 显示、或者你只是转灰度识别，可以直接 return matRgb，最快
	if (!returnBgr) {
		return matRgb.clone();
	}

	// OpenCV 的 imwrite / 大多数算法默认按 BGR 解释彩色图
	// 需要保存正常颜色时，再转 BGR
	cv::Mat matBgr;
	cv::cvtColor(matRgb, matBgr, cv::COLOR_RGB2BGR);
	return matBgr;
}


QPixmap TiledGraphicsView::exportRegionDataToWord(const QRectF& sceneRect, ExportQuality quality /*= Export_HighRes*/, bool drawDefects /*= true*/)
{
	if (sceneRect.isEmpty() || m_items.isEmpty()) return QPixmap();

	QSize targetSize(qCeil(sceneRect.width()), qCeil(sceneRect.height()));
	/*if (targetSize.width() > 16384 || targetSize.height() > 16384) {
	qWarning() << QString::fromLocal8Bit("⚠️ 截取区域过大，已自动等比缩小！");
	targetSize.scale(16384, 16384, Qt::KeepAspectRatio);
	}*/

	//QImage resultImage(targetSize, QImage::Format_ARGB32_Premultiplied);
	QImage resultImage(targetSize, QImage::Format_RGB888);

	// 💥 修复点 1：JPG不支持透明底，一律填成干净的白色 (或你需要的底图颜色)
	resultImage.fill(Qt::white);

	QPainter painter(&resultImage);
	painter.setRenderHint(QPainter::Antialiasing, false);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

	// 💥 修复点 2：上帝级坐标系映射！
	// 告诉画家：“你现在的画板代表的是真实的物理 SceneRect”
	painter.setWindow(sceneRect.toRect());
	painter.setViewport(resultImage.rect());

	for (TunnelSectionItem* item : m_items) {
		QRectF itemRect = item->sceneBoundingRect();
		if (!sceneRect.intersects(itemRect)) continue;

		AbstractTileSource* source = item->getSource();

		if (quality == Export_Thumbnail) {
			/*	QImage thumb = source->getThumbnailImage();
				if (!thumb.isNull()) {
					painter.drawImage(itemRect, thumb);
				}*/
		}
		else {
			int tSize = item->getTileSize();
			QRectF localIntersect = item->mapRectFromScene(sceneRect.intersected(itemRect));

			int startCol = qMax(0, qFloor(localIntersect.left() / tSize));
			int endCol = qFloor(localIntersect.right() / tSize);
			int startRow = qMax(0, qFloor(localIntersect.top() / tSize));
			int endRow = qFloor(localIntersect.bottom() / tSize);

			const QList<TileImageData> tiles = source->tileDataRange(startCol, endCol, startRow, endRow);
			for (const TileImageData& tileData : tiles) {
				QImage tile = QImage::fromData(tileData.data, "JPG");
				if (tile.isNull()) continue;

				QRectF tileSceneRect(
					itemRect.x() + tileData.col * tSize,
					itemRect.y() + tileData.row * tSize,
					tile.width(),
					tile.height()
				);
				painter.drawImage(tileSceneRect, tile);
			}
		}
	}

	// =========================================================
	// 绘制矢量病害层
	// =========================================================
	if (drawDefects) {
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
		// 1. 隐藏隧道底图
		for (TunnelSectionItem* item : m_items) {
			item->hide();
		}

		// 💥 2. 核心修复：临时抽走 Scene 的“黑背景”，防止它覆盖我们拼好的图片！
		QBrush oldBgBrush = m_scene->backgroundBrush();
		m_scene->setBackgroundBrush(Qt::NoBrush);

		// 3. 充当翻译官：统一两边的坐标系
		painter.setWindow(0, 0, targetSize.width(), targetSize.height());
		painter.setViewport(resultImage.rect());

		// 4. 画病害！此时因为背景是 NoBrush，病害会直接以透明底盖在我们的图片上
		m_scene->render(&painter, resultImage.rect(), sceneRect);

		// 💥 5. 打扫战场：把背景色和底图全还给界面，做到神不知鬼不觉
		m_scene->setBackgroundBrush(oldBgBrush);
		for (TunnelSectionItem* item : m_items) {
			item->show();
		}
	}

	painter.end();

	if (resultImage.width()>1500)
	{
		resultImage = resultImage.scaledToWidth(1500, Qt::SmoothTransformation);
	}
	return QPixmap::fromImage(resultImage);
}

// 指定某图片名获取该Mat指针
cv::Mat TiledGraphicsView::getMatByImageName(QString qstrImageName)
{
	QString qstrFloderPath;
	QString qstrFindName = qstrImageName.remove(".jpg");
	for (TunnelSectionItem* item : m_items)
	{
		if (item->getSource()->oriImageName() == qstrFindName)
		{
			qstrFloderPath = item->getSource()->getDbPath();
			break;
		}
	}


	// 获取所有X_X.jpg文件
	QDir dir(qstrFloderPath);
	if (!dir.exists())
	{
		return cv::Mat();
	}

	QFileInfoList fileList = dir.entryInfoList(QStringList() << "*.jpg", QDir::Files | QDir::NoSymLinks, QDir::NoSort);

	// 每列一个数组
	vector<vector<pair<QString,QString>>> vecVecColsImages;
	// 21680宽度是固定的
	vecVecColsImages.resize(22);
	for (const QFileInfo& fileInfo : fileList)
	{
		QString qstrFileName = fileInfo.fileName();
		qstrFileName = qstrFileName.remove(".jpg");
		QString absoluPath = fileInfo.absoluteFilePath();

		if (qstrFileName.contains("_"))
		{
			QStringList qstrList = qstrFileName.split("_");
			if (qstrList.size()==0)
			{
				continue;
			}
			bool isOk = false;
			int nIndex = qstrList[0].toInt(&isOk);
			if (isOk && nIndex < vecVecColsImages.size())
			{
				vecVecColsImages[nIndex].push_back({ qstrFileName, absoluPath });
			}
		}
	}

	// 排序
	for (size_t i = 0; i < vecVecColsImages.size(); i++)
	{
		sort(vecVecColsImages[i].begin(), vecVecColsImages[i].end(), [](pair<QString, QString>& a, pair<QString, QString>&b)
		{
			return  a.first.split("_")[1].toInt() < b.first.split("_")[1].toInt();
		});
	}


	vector<cv::Mat> vecVconcatMat;
	// 先按列读取
	for (size_t i = 0; i < vecVecColsImages.size(); i++)
	{
		vector<cv::Mat> vecMats;
		for (size_t j = 0; j < vecVecColsImages[i].size(); j++)
		{
			string strPath = vecVecColsImages[i][j].second.toLocal8Bit();
			cv::Mat srcImg = cv::imread(strPath, cv::IMREAD_UNCHANGED);
			vecVconcatMat.push_back(srcImg);
		}
		cv::Mat vconcatMat;
		cv::vconcat(vecMats, vconcatMat);
		vecVconcatMat.push_back(vconcatMat);
	}

	// 检查高度是否都一致
	bool isSame = true;
	for (int i = 1; i < vecVconcatMat.size(); i++)
	{
		if (vecVconcatMat[i].rows != vecVconcatMat[0].rows || vecVconcatMat[i].cols != vecVconcatMat[0].cols)
		{
			isSame = false;
		}
	}
	if (!isSame)
	{
		return cv::Mat();
	}

	cv::Mat result;
	cv::hconcat(vecVconcatMat, result);
	return result;

}

double TiledGraphicsView::getImageHeight()
{
	if (m_items.size() > 0)
	{
		return (double)m_items[0]->boundingRect().height() - 10;
	}

	return 0.0;
}

double TiledGraphicsView::getImageWidth()
{ 
	if (m_items.size() > 0)
	{ 
		return (double)m_items[0]->boundingRect().width();
	}

	return 0.0;
}
