#ifndef TUNNELVIEWERCONTROLLER_H
#define TUNNELVIEWERCONTROLLER_H

#include <QObject>
#include <QGraphicsView>
#include <QProgressDialog> // 🟢 引入进度条
#include <QApplication>    // 🟢 引入 App
#include "TunnelGlobal.h"
// 只要前置声明，不需要包含具体头文件，编译更快
class TiledGraphicsView;
class AbstractSourceFactory;

class TunnelViewerController : public QObject
{
    Q_OBJECT
public:
    // 构造函数传入 View，控制器就接管这个 View 的内容了
    explicit TunnelViewerController(TiledGraphicsView* view, QObject* parent = nullptr);
    ~TunnelViewerController();

    // 1. 设置数据源工厂（ 
    void setSourceFactory(AbstractSourceFactory* factory);

    // 2. 核心接口：一键加载整个项目
    // path: 包含所有切片文件夹的根目录，例如 "C:/Projects/Tunnel_01"
	bool loadRoute(const QString& rootPath);
	QList<DbImageInfo> scanDatabaseFolder(const QString& rootFolder);
	void setMaxRouteSections(int maxSections);
	int maxRouteSections() const;

    QString getRootPath()
		const {
		return m_rootPath;
	}
    // 4. 清空场景
    void clear();

	// 区间图像名
	QString m_strBegImageName;
	QString m_strEndImageName; 

    QStringList m_curTunnelNames;

private:
	void restoreViewUpdates(bool blockSignals);

    TiledGraphicsView* m_view;
    AbstractSourceFactory* m_factory;

    //图片根目录
    QString m_rootPath; 
	int m_maxRouteSections;

    // 记录当前加载到了多长（像素），方便追加
    //double m_currentTotalLength;
};

#endif // TUNNELVIEWERCONTROLLER_H
