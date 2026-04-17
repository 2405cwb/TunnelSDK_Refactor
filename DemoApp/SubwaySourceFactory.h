#ifndef SUBWAY_SOURCE_FACTORY_H
#define SUBWAY_SOURCE_FACTORY_H

#include "AbstractSourceFactory.h"
#include "SubwayTileSource.h" // 你之前写的那个具体实现类

class SubwaySourceFactory : public AbstractSourceFactory {
public:
    // 核心：控制器会调用这个方法来创建每一段的数据源
    AbstractTileSource* create(const QString& folderPath) override {
        // 直接返回具体的地铁数据源
        return new SubwayTileSource(folderPath);
    }

    LayoutOrientation layoutOrientation() const override {
        // 默认是纵向拼接
        return LayoutOrientation::Horizontal;
	}
};

#endif // SUBWAY_SOURCE_FACTORY_H