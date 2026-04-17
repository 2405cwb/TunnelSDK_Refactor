#ifndef TUNNEL_GLOBAL_H
#define TUNNEL_GLOBAL_H

// 绘图模式枚举
enum ViewMode {
    Mode_Browse,    // 浏览模式 (默认)：支持拖拽、滚轮缩放
    Mode_Draw ,      // 绘图模式：支持点击画图 (光标变十字)
    Mode_ExportBox  // 🟢 新增：1680 定焦截取模式
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
#include <QPen>

// 在类定义外面，或者 public 区域加上这个结构体
struct DbImageInfo {
    QString dbFilePath;   // 数据库文件的绝对路径
    QString originalName; // 原始图片名称
    int width;            // 大图总宽
    int height;           // 大图总高
    int tileSize;         // 切片尺寸 (2048) 
};
 

// 🟢 业务病害类型配置结构体
struct DefectTypeConfig {
    int defectCode;          // 业务代码 (例如：101代表纵向裂缝, 301代表掉块)
    QString typeName;        // 病害名称 (例如："纵向裂缝")
    DrawShape defaultShape;  // 该病害对应的默认几何类型 (画线还是画面)
    QPen pen;                // 外框样式 (控制颜色、粗细、实线/虚线)
    QBrush brush;            // 填充样式 (控制填充颜色和透明度)
    QString mark;            //病害说明
}; 

enum ExportQuality {
    Export_Thumbnail, // 极速导出缩略图 (速度快，适合概览)
    Export_HighRes    // 纯血无损高清拼接导出 (读取 1:1 硬盘切片，适合报告)
};

#endif // TUNNEL_TYPES_H