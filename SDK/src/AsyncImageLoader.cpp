#include "AsyncImageLoader.h"
#include <QtConcurrent>
#include <QThread>
#include <QMetaObject>
#include <QApplication>
#include <QImageReader>
#include <QTimer>
#include <QPointer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QHash>
#include <QList>

namespace {
	const int kMaxSqlConnectionsPerThread = 32;

	QSqlDatabase databaseForCurrentThread(const QString& dbPath, const QString& prefix)
	{
		static thread_local QHash<QString, QString> connectionNames;
		static thread_local QList<QString> lruKeys;

		const QString cacheKey = QString("%1|%2").arg(prefix, dbPath);
		QString connName = connectionNames.value(cacheKey);
		if (connName.isEmpty()) {
			while (connectionNames.size() >= kMaxSqlConnectionsPerThread && !lruKeys.isEmpty()) {
				const QString oldKey = lruKeys.takeFirst();
				const QString oldConnName = connectionNames.take(oldKey);
				if (!oldConnName.isEmpty() && QSqlDatabase::contains(oldConnName)) {
					{
						QSqlDatabase oldDb = QSqlDatabase::database(oldConnName);
						oldDb.close();
					}
					QSqlDatabase::removeDatabase(oldConnName);
				}
			}

			connName = QString("%1_%2_%3")
				.arg(prefix)
				.arg((quintptr)QThread::currentThreadId())
				.arg(qHash(dbPath));
			connectionNames.insert(cacheKey, connName);
		}

		lruKeys.removeAll(cacheKey);
		lruKeys.append(cacheKey);

		QSqlDatabase db;
		if (QSqlDatabase::contains(connName)) {
			db = QSqlDatabase::database(connName);
		}
		else {
			db = QSqlDatabase::addDatabase("QSQLITE", connName);
			db.setDatabaseName(dbPath);
			db.setConnectOptions("QSQLITE_OPEN_READONLY;PRAGMA mmap_size=268435456;");
		}

		if (!db.isOpen()) {
			db.open();
		}
		return db;
	}
}

AsyncImageLoader* AsyncImageLoader::instance()
{
	static AsyncImageLoader inst;
	return &inst;
}

AsyncImageLoader::AsyncImageLoader()
{
	// 高清图缓存：100 MB = 100 * 1024 KB
	// 50 * 1024 大概三张切片
	m_cache.setMaxCost(1000 * 1024);

	m_thumbnailCache.setMaxCost(500 * 1024); // 限制缩略图最大占用 500MB
	m_thumbThreadPool.setMaxThreadCount(2);  // 给缩略图分配 2 个专属的后台线程

	QPointer<QObject> guard(this);
}

void AsyncImageLoader::setBrightness(int value)
{
	m_brightness = value;

	// 注意：修改亮度后，理论上应该清空高清图缓存，让它们重新生成
	// m_cache.clear();
}

int AsyncImageLoader::brightness() const
{
	return m_brightness;
}

QPixmap AsyncImageLoader::getSyncThumbnail(const QString& pathUri)
{
	int br = m_brightness;
	QString key = QString("%1_thumb_br_%2").arg(pathUri).arg(br);

	QMutexLocker locker(&m_mutex);
	if (m_thumbnailCache.contains(key)) {
		return *m_thumbnailCache.object(key);
	}

	return QPixmap(); // 没找到就返回空
}

void AsyncImageLoader::requestThumbnail(const QString& pathUri, const QSize& targetSize /*= QSize()*/)
{
	// 获取主线程当前亮度设置
	int br = m_brightness;

	// 生成带亮度的唯一 Key，比如 "C:/xxx.db_thumb_br_20"
	QString key = QString("%1_thumb_br_%2").arg(pathUri).arg(br);

	// 1. 查缓存，极速返回
	{
		QMutexLocker locker(&m_mutex);
		if (m_thumbnailCache.contains(key)) {
			QPixmap result = *m_thumbnailCache.object(key);
			QTimer::singleShot(0, this, [this, pathUri, result]() {
				emit sigThumbnailLoaded(pathUri, result);
				});
			return;
		}
	}

	// 2. 丢入专属的缩略图后台线程池
	// 核心修正 2：必须捕获当前亮度 br
	QtConcurrent::run(&m_thumbThreadPool, [this, pathUri, key, br]() {
		QImage img;

		QSqlDatabase db = databaseForCurrentThread(pathUri, "ThumbConn");

		// --- B. 极速查询缩略图 ---
		if (db.isOpen()) {
			QSqlQuery query(db);
			if (query.exec("SELECT data FROM thumbnail LIMIT 1") && query.next()) {
				QByteArray thumbData = query.value(0).toByteArray();
				if (thumbData.size() > 0) {
					img = QImage::fromData(thumbData); // 依靠魔数嗅探解码
				}
			}
		}

		// --- C. 兜底机制 ---
		if (img.isNull()) {
			img = QImage(2048, 2048, QImage::Format_RGB888);
			img.fill(QColor(60, 60, 60)); // 灰底
		}

		if (br != 0) {
			// 1. 格式标准化：JPG 解码出来往往是 RGB32，必须转为 RGB888，才能按 byte 处理亮度
			if (img.format() != QImage::Format_RGB888) {
				img = img.convertToFormat(QImage::Format_RGB888);
			}

			// 2. 遍历并钳制修改每个像素
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
			int totalBytes = img.sizeInBytes();
#else
			int totalBytes = img.byteCount();
#endif
			uchar* bits = img.bits();
			for (int i = 0; i < totalBytes; ++i) {
				int val = bits[i] + br;

				// 钳制在 0~255 之间，防止颜色溢出
				bits[i] = (val < 0) ? 0 : (val > 255 ? 255 : val);
			}
		}

		// --- D. 移交主线程组装并缓存 ---
		// 这里需要将 img 捕获进去，因为 Lambda 结束后 img 就析构了
		QTimer::singleShot(0, this, [this, pathUri, key, img]() {
			QPixmap pix = QPixmap::fromImage(img);

			{
				QMutexLocker locker(&m_mutex);
				if (!m_thumbnailCache.contains(key)) {
					// RGB888/ARGB32 显存 cost 计算，单位 KB
					int cost = (pix.width() * pix.height() * 4) / 1024;
					m_thumbnailCache.insert(key, new QPixmap(pix), cost);
				}
			}

			emit sigThumbnailLoaded(pathUri, pix);
			});
		});
}

void AsyncImageLoader::requestImage(const QString& pathUri)
{
	const QString safePathUri = QString::fromLocal8Bit(pathUri.toLocal8Bit().constData());

	// 或者更简单的强行拷贝：
	// const QString safePathUri = pathUri;
	// Qt QString 具有自动写时复制，显式拷贝能降低 Bug 风险。

	// 工业级最稳妥写法：
	const QString threadLocalUri = pathUri;

	threadLocalUri.toUpper(); // 触发无意义的写操作，强制其在内存中生成独立副本
	threadLocalUri.toLower(); // 还原

	// 获取当前亮度设置
	int br = 0;

	// 生成带亮度的唯一 Key
	QString key = QString("%1_br_%2").arg(pathUri).arg(br);

	{
		QMutexLocker locker(&m_mutex);
		if (m_cache.contains(key)) {
			QPixmap result = *m_cache.object(key);

			QTimer::singleShot(0, this, [this, pathUri, result]() {
				emit sigImageLoaded(pathUri, result);
				});
			return;
		}
	}

	QtConcurrent::run([this, threadLocalUri, br, key]() {
		QImage img;

		// A. 解析 URI，判断是数据库直读还是本地文件读取
		QStringList parts = threadLocalUri.split("|");
		if (parts.size() == 3) {
			// 这是从 TunnelSectionItem 传来的数据库切片请求
			QString dbPath = parts[0];
			int col = parts[1].toInt();
			int row = parts[2].toInt();

			QSqlDatabase db = databaseForCurrentThread(dbPath, "ThreadConn");

			if (db.isOpen()) {
				QSqlQuery query(db);
				query.prepare("SELECT data FROM tiles WHERE col = ? AND row = ?");
				query.addBindValue(col);
				query.addBindValue(row);

				if (query.exec() && query.next()) {
					QByteArray imgData = query.value(0).toByteArray();

					// qDebug() << "Thread ID:" << QThread::currentThreadId()
					//          << " decoding tile:" << threadLocalUri
					//          << " size:" << imgData.size();

					img = QImage::fromData(imgData, "JPG"); // 内存解码
				}
			}
		}
		else {
			// 向下兼容：普通的本地图片文件加载
			img = QImage(threadLocalUri);
		}

		// B. 数据损坏或空图的防闪退兜底处理
		if (img.isNull()) {
			img = QImage(1024, 1024, QImage::Format_RGB888);
			img.fill(Qt::gray);
		}

		// C. 格式标准化，为亮度调节算法做准备
		if (img.format() != QImage::Format_RGB888) {
			img = img.convertToFormat(QImage::Format_RGB888);
		}

		// D. 亮度调节计算，CPU 密集型操作
		if (br != 0) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
			int totalBytes = img.sizeInBytes();
#else
			int totalBytes = img.byteCount();
#endif
			uchar* bits = img.bits();
			for (int i = 0; i < totalBytes; ++i) {
				int val = bits[i] + br;
				bits[i] = (val < 0) ? 0 : (val > 255 ? 255 : val);
			}
		}

		// E. 移交主线程组装为 QPixmap，并通知渲染更新
		QObject* safeLoader = AsyncImageLoader::instance();
		QTimer::singleShot(0, safeLoader, [=]() {
			if (!img.isNull()) {
				QPixmap pix = QPixmap::fromImage(img);

				{
					QMutexLocker locker(&AsyncImageLoader::instance()->m_mutex);
					if (!AsyncImageLoader::instance()->m_cache.contains(key)) {
						int cost = (int)(pix.width() * pix.height() * 4 / 1024);
						AsyncImageLoader::instance()->m_cache.insert(key, new QPixmap(pix), cost);
					}
				}

				// 务必使用原始请求字符串返回，
				// 以便接收端能在 Pending 字典中成功核销
				emit AsyncImageLoader::instance()->sigImageLoaded(threadLocalUri, pix);
			}
			});
		});
}