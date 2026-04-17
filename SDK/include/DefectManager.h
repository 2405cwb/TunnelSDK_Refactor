#pragma once
#include <QObject>
#include <QGraphicsScene>
#include "IDefectStorage.h"
#include "items/DefectShapeItem.h"
#include <QMap>

// 定义样式结构体
struct DefectStyle {
    QPen pen;
    QBrush brush;
};

class DefectManager : public QObject
{
    Q_OBJECT
public:
    explicit DefectManager(QGraphicsScene* scene, QObject* parent = nullptr)
        : QObject(parent), m_scene(scene), m_storage(nullptr) {
    }

    ~DefectManager() { if (m_storage) delete m_storage; }

    // 🟢 获取当前内存/场景中加载的病害总数
    int getTotalDefectCount() const {
        return m_items.size();
    }

    // ==========================================
    // 🟢 核心改造：基于业务代码的样式注册表
    // ==========================================

    // 批量注册上端同事传过来的病害配置表
    void registerDefectConfigs(const QList<DefectTypeConfig>& configs) {
        m_configs.clear();
        for (const auto& cfg : configs) {
            m_configs[cfg.defectCode] = cfg;
        }
    }

    // ==========================================
    // 🟢 获取指定几何形状对应的所有业务病害配置
    // ==========================================
    QList<DefectTypeConfig> getConfigsByShape(DrawShape shapeType) const {
        QList<DefectTypeConfig> result;
        // 遍历整个字典
        for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
            if (it.value().defaultShape == shapeType) {
                result.append(it.value());
            }
        }
        return result;
    }


    // 根据业务代码获取完整配置
    DefectTypeConfig getConfig(int defectCode) const {
        if (m_configs.contains(defectCode)) {
            return m_configs.value(defectCode);
        }
        // 兜底默认配置：黄色实线
        return { 0, "未知病害", Shape_Line, QPen(Qt::yellow, 3, Qt::SolidLine), Qt::NoBrush };
    }

 

    // ==========================================
    // 2. 存储引擎注入与连接
    // ==========================================
    void setStorageStrategy(IDefectStorage* storage) {
        if (m_storage) delete m_storage;
        m_storage = storage;
    }

    void setConnectionUri(const QString& uri) { m_currentUri = uri; }
    QString connectionUri() const { return m_currentUri; }

    // ==========================================
    // 3. 核心业务逻辑 (增、删、全量读)
    // ==========================================

    // 🟢 参数 triggerSave：默认 true 表示用户画的，要保存；false 表示从文件读的，别保存！
    void addDefect(DefectShapeItem* item, bool triggerSave = true) {
        // 从 Item 中读取它的业务代码
        int code = item->property("defectCode").toInt();
        // 查字典，获取这件衣服
        DefectTypeConfig cfg = getConfig(code);
        item->setPen(cfg.pen);
        item->setBrush(cfg.brush);
        item->setToolTip(cfg.mark);
        m_items.append(item);
        m_scene->addItem(item);
    
		//TODO 新增功能 20260227 增加病害移动功能：监听 Item 的位置变化信号，自动更新数据库
        connect(item, &DefectShapeItem::sigDefectMoved, this, [this](DefectShapeItem* movedItem) {
            if (m_storage && !m_currentUri.isEmpty()) {
                // 将被拖动的 Item 转成纯数据
                DefectData data = movedItem->toData();

                // 💥 为什么这里调用 addOne？
                // 因为我们在 SQLite 的 addOne 里写的是 "REPLACE INTO"
                // 只要 UUID 没变，SQLite 会自动把旧位置的记录覆盖掉，实现无缝更新！
                m_storage->addOne(data, m_currentUri);

                //qDebug() << QString::fromLocal8Bit(" 病害 [%1] 已被拖动，数据库自动更新完毕！").arg(data.name);
            }
            });

        // 💥 只有用户手动新增的，才触发增量保存
        if (triggerSave && m_storage && !m_currentUri.isEmpty()) {
            m_storage->addOne(item->toData(), m_currentUri);
        }
    }

    void removeDefect(DefectShapeItem* item) {
        QString uuid = item->getUuid();
        if (m_items.removeOne(item)) {
            m_scene->removeItem(item);
            delete item;

            // 💥 触发增量删除
            if (m_storage && !m_currentUri.isEmpty()) {
                m_storage->deleteOne(uuid, m_currentUri);
            }
        }
    }
    // 🟢 清理屏幕上的旧病害
    void clearDefects() {
        for (auto item : m_items) {
            m_scene->removeItem(item);
            delete item;
        }
        m_items.clear();
    }


    // 🟢 触发全量加载 (由 MainWindow 在初始化时调用)
    bool loadDefects() {
        if (!m_storage || m_currentUri.isEmpty()) return false;

        for (auto item : m_items) { m_scene->removeItem(item); delete item; }
        m_items.clear();

        QList<DefectData> dataList = m_storage->loadAll(m_currentUri);
        for (const auto& data : dataList) {
            DefectShapeItem* item = new DefectShapeItem(data.shape, data.name, static_cast<DrawShape>(data.type), data.uuid);
            item->fromData(data);

            // 💥 注意：传 false！阻止加载时无限死循环写入文件！
            addDefect(item, false);
        }
        return true;
    }

    // 🟢 按里程区间加载病害
    bool loadDefectsByRange(double startMile, double endMile) {
        if (!m_storage || m_currentUri.isEmpty()) return false;

        // 1. 先把屏幕上以前的病害清空
        clearDefects();

        // 2. 向存储引擎 (JSON/SQLite) 发起区间查询
        QList<DefectData> dataList = m_storage->loadByRange(m_currentUri, startMile, endMile);

        // 3. 把查出来的病害生成 UI 控件，扔上屏幕
        for (const auto& data : dataList) {
            DefectShapeItem* item = new DefectShapeItem(data.shape, data.name, static_cast<DrawShape>(data.type), data.uuid);
            item->fromData(data);

            // 注意：传 false 防止触发自动保存死循环！
            addDefect(item, false);
        }
        return true;
    }


    // ==========================================
    // 🟢 触发全量保存 (给 MainWindow 的右键菜单手动存盘用)
    // ==========================================
    bool saveDefects(const QString& uri = "") {
        if (!m_storage) return false;

        // 如果没传路径，就用系统当前设置的默认路径
        QString targetUri = uri.isEmpty() ? m_currentUri : uri;
        if (targetUri.isEmpty()) return false;

        QList<DefectData> dataList;
        for (auto item : m_items) {
            dataList.append(item->toData());
        }
        return m_storage->saveAll(dataList, targetUri);
    }
private:
    QGraphicsScene* m_scene;
    QList<DefectShapeItem*> m_items;
    IDefectStorage* m_storage;
    QMap<int, DefectStyle> m_styles;
    QString m_currentUri;

    QMap<int, DefectTypeConfig> m_configs; 
};