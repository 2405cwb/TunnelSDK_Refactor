#include "MainWindow.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QMessageBox>

#include<QKeyEvent>
#include<QInputDialog>
#include "JsonDefectStorage.h"
#include <QtGlobal>
#include <QTime> 
#include <QElapsedTimer>
#include "SqliteDefectStorage.h" // 记得在顶部 Include
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    
    setupUI();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    initWindowLayout();   // 1. 基础布局
    initTopControls();    // 2. 顶部工具栏
    initGraphicsView();   // 3. 核心视图
    initController();     // 4. 业务逻辑
    initHUD();            // 5. 悬浮信息层
    initConnections();    // 6. 信号连接 (让各部分动起来)

    this->showMaximized();
   
}

void MainWindow::initWindowLayout()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(10, 10, 10, 0);
    m_mainLayout->setSpacing(5);
}

void MainWindow::initTopControls()
{
    QHBoxLayout* topControlLayout = new QHBoxLayout();

    // --- A. 左侧：亮度调节 ---
    QLabel* lblBr = new QLabel(QString::fromLocal8Bit("亮度调节:"), m_centralWidget);
    m_sliderBrightness = new QSlider(Qt::Horizontal, m_centralWidget);
    m_sliderBrightness->setRange(-100, 100);
    m_sliderBrightness->setValue(0);
    m_sliderBrightness->setFixedWidth(200);

    m_valBrightness = new QLabel("0", m_centralWidget);
    m_valBrightness->setFixedWidth(30);

    topControlLayout->addWidget(lblBr);
    topControlLayout->addWidget(m_sliderBrightness);
    topControlLayout->addWidget(m_valBrightness);

    // --- 中间占位 ---
    topControlLayout->addStretch();

    // --- B. 右侧：LOD 等级 ---
    QLabel* lblLod = new QLabel(QString::fromLocal8Bit("LOD 性能等级 (1-10):"), m_centralWidget);
    m_sliderLod = new QSlider(Qt::Horizontal, m_centralWidget);
    m_sliderLod->setRange(1, 10);
    m_sliderLod->setValue(8);
    m_sliderLod->setFixedWidth(200);

    m_valLod = new QLabel("8", m_centralWidget);
    m_valLod->setFixedWidth(30);

    topControlLayout->addWidget(lblLod);
    topControlLayout->addWidget(m_sliderLod);
    topControlLayout->addWidget(m_valLod);

    // 加入主布局
    m_mainLayout->addLayout(topControlLayout);
}

void MainWindow::initGraphicsView()
{
    m_view = new TiledGraphicsView();
    // 设置初始 LOD
    m_view->setLodLevel(8);

    // 加入布局
    m_mainLayout->addWidget(m_view);
}

void MainWindow::initController()
{
    m_controller = new TunnelViewerController(m_view, m_centralWidget);
    m_controller->setSourceFactory(new SubwaySourceFactory());

    // 建议：路径最好不要硬编码，后续可以改为 QFileDialog 选择
   // QString rootPath = "C:\\Users\\cwb\\Desktop\\job\\codeTest\\0122\\temp";
   // QString rootPath = QString::fromLocal8Bit("Y:\\06城轨中心\\11北京19号线-2026年\\20260202\\2026020219号线隧道表观数据\\2026_02_02\\HN088_新宫站—太平桥站02_05_57\\已划分区间\\03草桥-景风门\\数据管理\\灰度图\\images");
   QString rootPath = QString::fromLocal8Bit("Y:\\06城轨中心\\15北京地铁6号线2026\\20260212\\隧道表观检测系统\\HN088_北京地铁6号线_02_09_47\\已划分区间\\已划分区间\\马蹄数据\\07海淀五路居-慈寿寺-上行\\数据管理\\灰度图\\images");
 
  
    //QString rootPath = QString::fromLocal8Bit( "C:\\Users\\cwb\\Desktop\\job\\codeTest\\0122\\temp");
    if (m_controller->loadRoute(rootPath)) {
        qDebug() << QString::fromLocal8Bit("项目加载成功！");  
    }
    else {
        qWarning() << QString::fromLocal8Bit("加载失败，请检查路径: %1").arg(rootPath);
    }
    initDefectSystem();
}

void MainWindow::initDefectSystem()
{ 
    DefectManager* defectMgr = m_view->defectManager();
     
    // ==========================================
    // 🟢  在这里对接他们数据库里的病害类型字典！
    // ==========================================
    QList<DefectTypeConfig> mySystemDefects;

    // 1. 纵向裂缝 (代码 101)：红色，2像素宽 (永远保持2像素！)
    QPen pen101(Qt::red, 2, Qt::SolidLine);
    pen101.setCosmetic(true); // 💥 核心魔法：开启像素绝对宽度！
    mySystemDefects.append({
        101, QString::fromLocal8Bit("纵向裂缝"), Shape_Line,
        pen101, Qt::NoBrush, QString("标记1")
        });

    // 2. 横向裂缝 (代码 102)：黄色，4像素，虚线 (DashLine)
    mySystemDefects.append({
        102, QString::fromLocal8Bit("横向裂缝"), Shape_Line,
        QPen(Qt::yellow, 4, Qt::DashLine), Qt::NoBrush,QString("标记2")
        });

    // 3. 严重渗水 (代码 201)：蓝色，实心点
    mySystemDefects.append({
        201, QString::fromLocal8Bit("严重渗水"), Shape_Point,
        QPen(Qt::blue, 2), QBrush(Qt::blue),QString("标记3")
        });

    // 4. 混凝土掉块 (代码 301)：橙色边框，半透明橙色填充
    mySystemDefects.append({
        301, QString::fromLocal8Bit("混凝土掉块"), Shape_Polygon,
        QPen(QColor(255, 165, 0), 3, Qt::SolidLine), QBrush(QColor(255, 165, 0, 100)),QString("标记4")
        });

    // 💥 一键把整个系统的病害字典注入到你的 SDK 中！
    defectMgr->registerDefectConfigs(mySystemDefects);



    
    // 🟢 2. 注入 JSON 存储策略
    // ==========================================
    //defectMgr->setStorageStrategy(new JsonDefectStorage());

    // 尝试加载本地病害 (假设存放在项目根目录)
    //QString defectFilePath = m_controller->getRootPath() + "\\defects.json";
    //defectMgr->setConnectionUri(defectFilePath);
    ////defectMgr->loadDefects();
    //defectMgr->loadDefectsByRange(100,120);


    defectMgr->setStorageStrategy(new SqliteDefectStorage());
    // 后缀改成 .db 或 .sqlite，数据库文件会被自动创建
    QString dbPath = m_controller->getRootPath() + "\\tunnel_data.db";
    defectMgr->setConnectionUri(dbPath);
    defectMgr->loadDefectsByRange(100, 120);

}

void MainWindow::initHUD()
{
    int hudStartY = 60; // 避开顶部工具栏

    // A. 性能统计
    m_statsLabel = new QLabel(m_centralWidget);
    m_statsLabel->setStyleSheet("background: rgba(0,0,0,150); color: lime; padding: 5px; font-weight: bold; font-size: 12px; border-radius: 4px;");
    m_statsLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_statsLabel->setText(QStringLiteral ("尚未加载任何高清图像"));
    m_statsLabel->move(20, hudStartY);
    m_statsLabel->show();

    // B. 坐标信息
    m_cursorLabel = new QLabel(m_centralWidget);
    m_cursorLabel->setStyleSheet("background: rgba(0,0,0,150); color: #FFD700; padding: 5px; font-weight: bold; font-size: 14px; border-radius: 4px;");
    m_cursorLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_cursorLabel->setText(QString::fromLocal8Bit("鼠标移动以获取坐标"));
    m_cursorLabel->move(20, hudStartY + 40);
    m_cursorLabel->show();
}

void MainWindow::initConnections()
{
    // === 1. UI 控件交互 ===

    // 亮度调节
    connect(m_sliderBrightness, &QSlider::valueChanged, this, [this](int value) {
        m_valBrightness->setText(QString::number(value));
        AsyncImageLoader::instance()->setBrightness(value);
        m_view->updateVisibleTiles(); // 强制刷新视图
        });

    // LOD 调节
    connect(m_sliderLod, &QSlider::valueChanged, this, [this](int value) {
        m_valLod->setText(QString::number(value));
        m_view->setLodLevel(value);
        });

    // === 2. HUD 数据反馈 ===

    // 性能数据更新
    connect(m_view, &TiledGraphicsView::sigStatsUpdated, m_statsLabel, [this](QString stats) {
        m_statsLabel->setText(stats);
        m_statsLabel->adjustSize();
        });

    connect(m_view, &TiledGraphicsView::sigDefectsSelected, this, [this](QList<DefectShapeItem*> defects) {

        // 收集被选中的病害名字
        QString names;
        for (auto d : defects) names += d->toData().name + " ";

        QMessageBox::information(this, QString::fromLocal8Bit("临时框选成功"),
            QString::fromLocal8Bit("你在浏览模式下框中了 %1 个病害！\n分别是: %2")
            .arg(defects.size()).arg(names));
        });

    // 鼠标坐标更新
    connect(m_view, &TiledGraphicsView::sigCursorInfoChanged, m_cursorLabel, [this](QString imgName, int x, int y) {
        if (imgName.isEmpty()) {
            m_cursorLabel->setText(QString::fromLocal8Bit("无数据区域"));
        }
        else { 
            QPointF globalPos = m_view->mapToGlobalScene(imgName, x, y);

            QString text = QString::fromLocal8Bit("图号: %1 | 坐标: X=%2 Y=%3\n"
                "大图位置: X=%4 Y=%5")
                .arg(imgName).arg(x).arg(y).arg(globalPos.x()).arg(globalPos.y());
            m_cursorLabel->setText(text);
        }
        m_cursorLabel->adjustSize();
        });

    // === 3. 业务交互 (右键菜单) ===
    connect(m_view, &TiledGraphicsView::sigContextMenuRequested, [this](QPoint globalPos, QList<QGraphicsItem*> items) {
        QMenu menu;
        bool hitSomething = false;


        // 过滤出我们点中的病害 Item
        QList<DefectShapeItem*> defectItems;
        for (auto item : items) {
            if (DefectShapeItem* defect = dynamic_cast<DefectShapeItem*>(item)) {
                defectItems.append(defect);
            }
        }


        if (!defectItems.isEmpty()) {
             
            menu.addAction(QString::fromLocal8Bit("删除此病害"), [this, defectItems]() {
                for (auto defect : defectItems) {
                    m_view->defectManager()->removeDefect(defect);
                }
                }); 
            hitSomething = true;
        }

        if (!hitSomething) {
            menu.addAction(QString::fromLocal8Bit("重置视图"), m_view, &TiledGraphicsView::resetToFit);
            menu.addSeparator();

            // 🟢 加入【保存所有病害】按钮
            menu.addAction(QString::fromLocal8Bit("保存所有病害"), [this]() {
                if (m_view->defectManager()->saveDefects()) { // 直接调用无参保存即可
                    QMessageBox::information(this, QString::fromLocal8Bit("成功"), QString::fromLocal8Bit("保存成功！"));
                }
                });
        }
        menu.exec(globalPos);
        });


    // 💥 核心业务逻辑接管：监听 SDK 画图完成信号
    connect(m_view, &TiledGraphicsView::sigGeometryDrawn, this, [this](DrawShape shapeType, QPainterPath path) {

        // 1. 获取当前画的这种形状（点/线/面），在系统里有哪些对应的业务类型？
        QList<DefectTypeConfig> availableConfigs = m_view->defectManager()->getConfigsByShape(shapeType);

        if (availableConfigs.isEmpty()) {
            QMessageBox::warning(this, QString::fromLocal8Bit("警告"),
                QString::fromLocal8Bit("系统未配置该形状对应的病害类型！"));
            return;
        }

        // ==========================================
        // 🟢 2. 动态创建自定义对话框
        // ==========================================
        QDialog dialog(this);
        dialog.setWindowTitle(QString::fromLocal8Bit("添加病害记录"));
        dialog.setMinimumWidth(300);
        QVBoxLayout* layout = new QVBoxLayout(&dialog);

        // 提示文本
        layout->addWidget(new QLabel(QString::fromLocal8Bit("请选择病害业务类型:"), &dialog));

        // 🟢 3. 动态生成 RadioButton (单选按钮)
        QButtonGroup* btnGroup = new QButtonGroup(&dialog);
        for (int i = 0; i < availableConfigs.size(); ++i) {
            const DefectTypeConfig& cfg = availableConfigs[i];
            QRadioButton* radio = new QRadioButton(cfg.typeName, &dialog);

            // 巧妙点：把业务代码 (defectCode) 设为这个按钮的 ID
            btnGroup->addButton(radio, cfg.defectCode);
            layout->addWidget(radio);

            // 默认选中第一个
            if (i == 0) radio->setChecked(true);
        }

        // 4. 加一个可选的备注/具体名称输入框
        layout->addSpacing(10);
        layout->addWidget(new QLabel(QString::fromLocal8Bit("附加备注(可选):"), &dialog));
        QLineEdit* editName = new QLineEdit(&dialog);
        editName->setPlaceholderText(QString::fromLocal8Bit("如果不填，则默认使用类型名称"));
        layout->addWidget(editName);

        // 5. 确定和取消按钮
        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addWidget(buttonBox);
        connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        // ==========================================
        // 🟢 6. 显示对话框并等待用户操作
        // ==========================================
        if (dialog.exec() != QDialog::Accepted) {
            return; // 用户点了取消或右上角红叉，直接丢弃图形
        }

        // 7. 提取用户选中的数据！
        int selectedCode = btnGroup->checkedId(); // 获取选中的那个 RadioButton 的业务代码
        DefectTypeConfig selectedCfg = m_view->defectManager()->getConfig(selectedCode);

        // 如果用户填了备注，名字就叫"纵向裂缝-K100"，否则就叫"纵向裂缝"
        QString finalName = editName->text().trimmed();
        if (finalName.isEmpty()) finalName = selectedCfg.typeName;

        // ==========================================
        // 🟢 8. 生成并交还给 SDK
        // ==========================================
        DefectShapeItem* defectItem = new DefectShapeItem(path, finalName, shapeType, selectedCode);
        
      //  defectItem->setMoveAxis(Axis_Horizontal); // 💥 锁定水平移动！
      ;
      //  画个结界 
      //defectItem->setMoveBounds(QRectF(-999999, 0, 1999998, 1500));
      
      //锁定
      //defectItem->setLocked(true);

        // 💥 关键：把用户选的业务代码塞进去，让引擎知道穿什么衣服！
        defectItem->setProperty("defectCode", selectedCode);

        // (模拟) 塞入里程信息，为了让你测试 loadDefectsByRange
        defectItem->setAttribute("start_mileage", 120.0);
        defectItem->setAttribute("end_mileage", 150.0);

       m_view->defectManager()->addDefect(defectItem);
        });
}

//TODO 新增功能 20260310 导出病害截图
void MainWindow::exportDefects() {
    // 1. 动态获取当前场景里的所有病害
    QList<DefectShapeItem*> defectList;
    for (auto item : m_view->scene()->items()) {
        if (DefectShapeItem* d = dynamic_cast<DefectShapeItem*>(item)) {
            defectList.append(d);
        }
    }

    if (defectList.isEmpty()) {
        QMessageBox::information(this, QString::fromLocal8Bit("提示"), QString::fromLocal8Bit("当前屏幕或数据库中没有病害，无法导出！"));
        return;
    }

    // 2. 自动在程序运行目录下创建一个导出文件夹
    QString exportDir = QApplication::applicationDirPath() + "/ExportedReports/";
    QDir().mkpath(exportDir);

    // 3. 设置边距：病害周围保留 500 像素的底图环境
    double margin = 500.0;

    // 4. 开启专业进度条 (防止 UI 假死)
    QProgressDialog progress(QString::fromLocal8Bit("正在生成病害截图..."),
        QString::fromLocal8Bit("取消"),
        0, defectList.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    // 5. 遍历导出
    int count = 1;
    QElapsedTimer timer;
    timer.start();

    for (int i = 0; i < defectList.size(); ++i) {
        if (progress.wasCanceled()) break;

        DefectShapeItem* defect = defectList[i];

        // A. 获取病害真实的物理包围盒
        QRectF defectRect = defect->sceneBoundingRect();

        // B. 向外扩展环境边距，算出一个大矩形
        QRectF exportRect = defectRect.adjusted(-margin, -margin, margin, margin);

		ExportQuality type = Export_HighRes;
        // C. 💥 调用神级接口，要求返回高清图 (第3个参数 true 代表绘制病害线条)
        QPixmap img = m_view->exportRegionData(exportRect, type, true);

        // D. 保存到硬盘 (文件名加上了病害的真实名字)
        QString fileName = QString("%1/Defect_%2_%3.jpg")
            .arg(exportDir)
            .arg(count++)
            .arg(defect->toData().name);
        if (type == ExportQuality::Export_Thumbnail)
        {
            fileName = QString("%1/Defect_%2_%3_thumbnail.jpg")
                .arg(exportDir)
                .arg(count++)
                .arg(defect->toData().name);
        }

        img.save(fileName, "JPG", 90);

        // E. 刷新 UI 事件循环，防止界面卡顿无响应
        progress.setValue(i + 1);
        QCoreApplication::processEvents();
    }

    double elapsedSec = timer.elapsed() / 1000.0;
    QMessageBox::information(this, QString::fromLocal8Bit("导出完成"),
        QString::fromLocal8Bit("成功导出 %1 张图！\n耗时: %2 秒\n保存在: %3")
        .arg(count - 1).arg(elapsedSec, 0, 'f', 2).arg(exportDir));
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
	//TODO 新增功能 20260228 Ctrl + Z 快捷键撤销上一个绘制点
    if ((event->modifiers() & Qt::ControlModifier) && (event->key() == Qt::Key_Z))
    {
        // 调用我们刚刚写好的撤销大招
        m_view->undoLastDrawPoint();
        return; // 💥 拦截完毕，直接 return，不要往下传了
    }

    if (event->key() == Qt::Key_Escape) {
        m_view->cancelCurrentDrawing();  
        return;
    }

    switch (event->key())
    {
    case Qt::Key_F1:
		m_view->setViewMode(Mode_Browse);
        break;
    case Qt::Key_F2: // 画点
        m_view->startDrawingDefect(Shape_Point);
        break;
    case Qt::Key_F3: // 画线 (裂缝)
        m_view->startDrawingDefect(Shape_Line);
        break;
    case Qt::Key_F4: // 画面 (掉块)
        m_view->startDrawingDefect(Shape_Polygon);
        break;
    case Qt::Key_F9:
    {
        exportDefects();
        break;
    }
    case Qt::Key_G:
        m_view->setViewMode(Mode_ExportBox);
        break;
#ifdef QT_DEBUG 

    // ==========================================
    // 🟢 F5: 测试“全局坐标跳转 + 自动放大”功能
    // ==========================================
    case Qt::Key_F5:
    {
        // 假设这是一个真实病害的中心坐标 (你可以换成你图上实际存在的坐标)
        QPointF targetPos(455238, 13355);

        // 调用我们之前封装的神级接口，跳过去，并且放大 2 倍！
        m_view->focusOnPosition(targetPos, 1);

        qDebug() << QString::fromLocal8Bit("🚀 正在飞梭跳转至坐标:") << targetPos;
        break;
    }

    // ==========================================
     // 🟢 F6: 性能极限测试 (生成 10000 个并瞬间写入 SQLite)
     // ==========================================
    case Qt::Key_F6:
    {
        qDebug() << QString::fromLocal8Bit("🚀 开始在整个隧道范围生成 10000 个测试病害...");
        QElapsedTimer timer;
        timer.start();

        qsrand(QTime::currentTime().msec());
        int codes[] = { 101, 102, 201, 301 };
        double maxX = 700000.0;
        double maxY = 28000.0;

        for (int i = 0; i < 10000; ++i) {
            double x = ((double)qrand() / RAND_MAX) * maxX;
            double y = ((double)qrand() / RAND_MAX) * maxY;
            double defectSize = 50.0 + ((double)qrand() / RAND_MAX) * 100.0;

            QPainterPath path;
            path.moveTo(x, y);
            path.lineTo(x + defectSize, y + defectSize);

            QString testName = QString("Test_%1").arg(i);
            DefectShapeItem* item = new DefectShapeItem(path, testName, Shape_Line, 100000 + i);
            item->setProperty("defectCode", codes[qrand() % 4]);

            // 💥 关键：既然要进数据库，必须赋予里程属性！(这里简单把 X 坐标当做里程)
            item->setAttribute("start_mileage", x);
            item->setAttribute("end_mileage", x + defectSize);

            // 依然传 false，先不要单条保存
            m_view->defectManager()->addDefect(item, false);
        }
        qDebug() << QString::fromLocal8Bit("✅ UI内存生成耗时:") << timer.elapsed() << "ms";

        // ==========================================
        // 💥 见证奇迹的时刻：利用事务，一键全量写入数据库！
        // ==========================================
        timer.restart();
        m_view->defectManager()->saveDefects();

        qDebug() << QString::fromLocal8Bit("💾 10000条数据写入 SQLite 数据库耗时:") << timer.elapsed() << "ms";
        break;
    }

    // ==========================================
    // 🟢 F7: 一键核弹清屏 (同时清空数据库)
    // ==========================================
    case Qt::Key_F7:
    {
        // 1. 先把屏幕和内存里的病害全部干掉
        m_view->defectManager()->clearDefects();

        // 2. 💥 再执行一次全量保存！
        // 既然内存里现在是空的，saveDefects 就会拿着一个空的 List 去找 SQLite。
        // SQLite 底层的 saveAll 会先执行 DELETE FROM defects，然后发现没有新数据要插，直接提交。
        // 完美实现了“顺手清空数据库”！
        m_view->defectManager()->saveDefects();

        qDebug() << QString::fromLocal8Bit("🧹 屏幕与数据库中的所有病害已被瞬间清空！");
        break;
    }
   
    // ==========================================
    // 🟢 F8: 屏幕渲染数据统计雷达
    // ==========================================
    case Qt::Key_T:
    {
        // 1. 获取内存中的总病害数
        int totalLoaded = m_view->defectManager()->getTotalDefectCount();

        // 2. 获取当前屏幕真正看见的病害数
        int visibleCount = 0;

        // m_view->items() 瞬间返回当前屏幕框住的所有元素 (包括底图瓦片和病害)
        
        QRect viewportRect = m_view->viewport()->rect();
        QList<QGraphicsItem*> screenItems = m_view->items(viewportRect);
        for (auto item : screenItems) {
            // 过滤：只要它能转换成 DefectShapeItem，说明它是个病害
            if (dynamic_cast<DefectShapeItem*>(item)) {
                visibleCount++;
            }
        }

        // 3. 弹窗或者打印出来！
        QString msg = QString::fromLocal8Bit("📊 渲染统计报告：\n\n"
            "内存总病害： %1 个\n"
            "当前屏幕可见： %2 个\n\n"
            "显卡只需渲染这 %2 个对象！")
            .arg(totalLoaded).arg(visibleCount);

        QMessageBox::information(this, QString::fromLocal8Bit("统计"), msg);
        break;
    }
#endif
    default:
        break;
    }
	QMainWindow::keyPressEvent(event);
}
