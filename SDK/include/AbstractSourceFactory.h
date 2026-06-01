#ifndef ABSTRAC_TSOURCE_FACTORY_H
#define ABSTRAC_TSOURCE_FACTORY_H 
#include <AbstractTileSource.h>
#include<QCollator> 
#include "TunnelGlobal.h"




class AbstractSourceFactory {
public:
	virtual ~AbstractSourceFactory() {}

    // 给定一个文件夹路径，返回一个构造好的数据源
    virtual AbstractTileSource* create(const QString& folderPath) = 0;

    // 🟢 2. 新增：自定义排序策略 (虚方法，提供默认实现)
    // 默认使用 Windows 风格的自然排序
    virtual void sortDatabaseFolder(QList<DbImageInfo>& names) {
		QCollator collator;
		collator.setNumericMode(true); // 默认开启智能数字排序
		std::sort(names.begin(), names.end(), [&collator](const DbImageInfo& a, const DbImageInfo& b) {
			return collator.compare(a.originalName, b.originalName) < 0;
		});
    }

  
    // 默认是纵向 
    virtual LayoutOrientation layoutOrientation() const {
        return LayoutOrientation::Vertical;
    }

    //浏览步长
    virtual int  scrollSpeed() const {
        return 50;
    }
    
}; 
#endif // !ABSTRAC_TSOURCE_FACTORY_H
