#pragma once 
// 存储抽象接口
#include "DefectSystem.h"
class IDefectStorage {
public:
    virtual ~IDefectStorage() = default;

    // --- 批量操作 (用于初始化加载) ---
    virtual QList<DefectData> loadAll(const QString& uri) = 0;
    virtual bool saveAll(const QList<DefectData>& defects, const QString& uri) = 0;

    // --- 🟢 增量操作 (用于日常自动保存) ---
    // 默认返回 true，如果具体的存储类不打算实现增量（比如极端简单的格式），就降级处理
    virtual bool addOne(const DefectData& defect, const QString& uri) { return true; }
    virtual bool deleteOne(const QString& uuid, const QString& uri) { return true; }
    virtual bool updateOne(const DefectData& defect, const QString& uri) { return true; }

    // ==========================================
    // 🟢  按里程区间加载 
    // ==========================================
    virtual QList<DefectData> loadByRange(const QString& uri, double startMile, double endMile) {
         
        return QList<DefectData>();
    }
};