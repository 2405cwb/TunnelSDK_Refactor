#ifndef TUNNEL_GLOBAL_H
#define TUNNEL_GLOBAL_H

#include <QPen>
#include <QString>
#include <QPainterPath>
#include <QVariantMap>

// 绘图模式枚举
enum ViewMode {
    Mode_Browse,    // 浏览模式 (默认)：支持拖拽、滚轮缩放
    Mode_Draw   ,    // 绘图模式：支持点击画图 (光标变十字)
	Mode_ExportBox //新增  1680*1680定焦截图模式 
};

// 🟢 布局方向枚举 
enum class LayoutOrientation {
    Vertical,   // 纵向 (里程对应 Y)
    Horizontal  // 横向 (里程对应 X)
};

enum DrawShape {
    Shape_Point,   // 点 (渗漏点等)
    Shape_Line,    // 线 (裂缝等)
    Shape_Polygon  // 面 (剥落、掉块等)
};

enum ElementType {
	Type_Cp3,	   // 里程桩
	Type_Chain,    // 长短链
	Type_Disease,  // 病害
	Type_Ring,     // 环片
	Type_Section,   // 断面
	Type_Platform,   // 站台
	Type_AUTORING,	// 自动识别环片，点击起始，终止位置	
};

struct DbImageInfo {
	QString dbFilePath;   // 数据库文件的绝对路径
	QString originalName; // 原始图片名称
	int width;            // 大图总宽
	int height;           // 大图总高
	int tileSize;         // 切片尺寸 (2048) 
};

// 🟢 业务病害类型配置结构体
struct DefectTypeConfig {
    DefectTypeConfig()
        : defaultShape(Shape_Line), defectCode(0) {}

    DefectTypeConfig(int code, const QString& name, const DrawShape shape,
        const QPen& defectPen, const QBrush& defectBrush)
        : defaultShape(shape), defectCode(code), typeName(name),
          pen(defectPen), brush(defectBrush) {}

    DefectTypeConfig(int code, const QString& name, const DrawShape shape,
        const QPen& defectPen, const QBrush& defectBrush, const QString&)
        : defaultShape(shape), defectCode(code), typeName(name),
          pen(defectPen), brush(defectBrush) {}

    DefectTypeConfig(int code, const QString& name,
        const QPen& defectPen, const QBrush& defectBrush)
        : defaultShape(Shape_Line), defectCode(code), typeName(name),
          pen(defectPen), brush(defectBrush) {}

    DrawShape defaultShape;
    int defectCode;          // 业务代码 (例如：101代表纵向裂缝, 301代表掉块)
    QString typeName;        // 名称 
    // DrawShape defaultShape;  // 该病害对应的默认几何类型 (画线还是画面)
    QPen pen;                // 外框样式 (控制颜色、粗细、实线/虚线)
    QBrush brush;            // 填充样式 (控制填充颜色和透明度)
    //QString mark;            //病害说明
};

struct DefectData {
	int uuid;
	QString name;
	DrawShape type;
	QPainterPath shape; // 如果包含 path，记得 include <QPainterPath> 
	int defectCode = 0;     // 💥 新增：业务病害代码

	QVariantMap attributes; // 💥 核心扩展点：万能属性包
};

enum ExportQuality {
	Export_Thumbnail, // 极速导出缩略图 (速度快，适合概览)
	Export_HighRes    // 纯血无损高清拼接导出 (读取 1:1 硬盘切片，适合报告)
};

#endif // TUNNEL_TYPES_H
