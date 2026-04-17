#include <QApplication>
#include <QFileDialog>
#include "SqliteImageSlicer.h"
#include "OpenCvImageSlicer.h"


void sqlitImage(QString srcFolder, QString dstFolder)
{ 

    // 实例化切片工具类
    SqliteImageSlicer slicer;
    // 设置参数 (切片尺寸设为 2048，质量 80)
    slicer.setConfig(2048, 80);

    // 开始执行全自动流程
    slicer.processDirectory(srcFolder, dstFolder);
}

void opencvImage(QString srcFolder, QString dstFolder)
{ 

    // 实例化切片工具类
    OpenCvImageSlicer slicer;

    // 设置参数 (切片尺寸设为 2048，质量 80)
    slicer.setConfig(2048, 95);

    // 开始执行全自动流程
    slicer.processDirectory(srcFolder, dstFolder);
}

int main(int argc, char* argv[])
{

    // 解除 Qt 底层的图片内存分配限制
    qputenv("QT_IMAGEIO_MAX_ALLOC", "0");
    QApplication a(argc, argv);
    QString srcFolder = QFileDialog::getExistingDirectory(nullptr, "请选择包含原始大图的文件夹", "C:/");
    if (srcFolder.isEmpty()) return 0;

    QString dstFolder = QFileDialog::getExistingDirectory(nullptr, "请选择数据库输出文件夹", srcFolder);
    if (dstFolder.isEmpty()) return 0;
	opencvImage(srcFolder, dstFolder);

    return 0;
}