#ifndef ABSTRACTTILESOURCE_H
#define ABSTRACTTILESOURCE_H


#include <QString>
#include<QSize>

class AbstractTileSource
{
public:
	 
virtual ~AbstractTileSource() {};

// 验证数据源是否有效（例如检查 info.json 是否存在）
virtual bool isValid() const = 0;

virtual QString oriImageName() const = 0;

// 返回图片的原始总尺寸 (例如 40000x4000)
virtual QSize totalSize() const = 0;

// 返回切片尺寸 (通常是 1024)
virtual int tileSize() const = 0;

//供底层加载器 (AsyncImageLoader) 获取数据库路径
virtual	 QString getDbPath() const = 0;

// 新增接口：直接返回内存中的缩略图，替代原有的物理文件读取
virtual QString getThumbnailImage() const = 0;
 
 
protected:
	 
private: 

}; 


#endif // !ABSTRACTTILESOURCE_H

