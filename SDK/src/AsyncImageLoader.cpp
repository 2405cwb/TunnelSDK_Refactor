#include "AsyncImageLoader.h" 
#include <QtConcurrent>  
#include <QThread>
#include<QMetaObject>
#include<QApplication>
#include <QImageReader>
#include <QTimer>
#include <QPointer>  
#include<QSqlDatabase>
#include<QSqlQuery>
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
	// 楂樻竻鍥剧紦瀛橈細100 MB = 100 * 1024 KB
	//50*1024 澶ф涓夊紶鍒囩墖
	m_cache.setMaxCost(1000 * 1024);

	m_thumbnailCache.setMaxCost(500 * 1024); // 闄愬埗缂╃暐鍥炬渶澶у崰鐢?256MB
	m_thumbThreadPool.setMaxThreadCount(2);  // 缁欑缉鐣ュ浘鍒嗛厤 2 涓笓灞炵殑鍚庡彴绾跨▼
	QPointer<QObject> guard(this);
}

void AsyncImageLoader::setBrightness(int value)
{
    m_brightness = value;
    // 娉ㄦ剰锛氫慨鏀逛寒搴﹀悗锛岀悊璁轰笂搴旇娓呯┖楂樻竻鍥剧紦瀛橈紝璁╁畠浠噸鏂扮敓鎴?
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
	return QPixmap(); // 娌℃壘鍒板氨杩斿洖绌?
}

void AsyncImageLoader::requestThumbnail(const QString& pathUri, const QSize& targetSize /*= QSize()*/)
{
	// 鑾峰彇涓荤嚎绋嬪綋鍓嶄寒搴﹁缃?
	int br = m_brightness;
	// 鐢熸垚甯︿寒搴︾殑鍞竴 Key (姣斿 "C:/xxx.db_thumb_br_20")
	QString key = QString("%1_thumb_br_%2").arg(pathUri).arg(br);

	// 1. 鏌ョ紦瀛?(鏋侀€熻繑鍥?
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

	// 2. 涓㈠叆涓撳睘鐨勭缉鐣ュ浘鍚庡彴绾跨▼姹?
	// 馃挜 鏍稿績淇 2锛氬繀椤绘崟鑾峰綋鍓嶄寒搴?br
	QtConcurrent::run(&m_thumbThreadPool, [this, pathUri, key, br]() {
		QImage img;

		QSqlDatabase db = databaseForCurrentThread(pathUri, "ThumbConn");

		// --- B. 鏋侀€熸煡璇㈢缉鐣ュ浘 ---
		if (db.isOpen()) {
			QSqlQuery query(db);
			if (query.exec("SELECT data FROM thumbnail LIMIT 1") && query.next()) {
				QByteArray thumbData = query.value(0).toByteArray();
				if (thumbData.size() > 0) {
					img = QImage::fromData(thumbData); // 渚濋潬榄旀暟鍡呮帰瑙ｇ爜
				}
			}
		}

		// --- C. 鍏滃簳鏈哄埗 ---
		if (img.isNull()) {
			img = QImage(2048, 2048, QImage::Format_RGB888);
			img.fill(QColor(60, 60, 60)); // 鐏板簳
		}
		if (br != 0) {
			// 1. 鏍煎紡鏍囧噯鍖栵細JPG 瑙ｇ爜鍑烘潵寰€寰€鏄?RGB32锛屽繀椤昏浆涓?RGB888 鎵嶈兘鎸?byte 澶勭悊浜害
			if (img.format() != QImage::Format_RGB888) {
				img = img.convertToFormat(QImage::Format_RGB888);
			}

			// 2. 閬嶅巻骞堕挸鍒朵慨鏀规瘡涓儚绱?
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
			int totalBytes = img.sizeInBytes();
#else
			int totalBytes = img.byteCount();
#endif
			uchar* bits = img.bits();
			for (int i = 0; i < totalBytes; ++i) {
				int val = bits[i] + br;
				// 閽冲埗鍦?0~255 涔嬮棿锛岄槻姝㈤鑹叉孩鍑?
				bits[i] = (val < 0) ? 0 : (val > 255 ? 255 : val);
			}
		}

		// --- D. 绉讳氦涓荤嚎绋嬬粍瑁呭苟缂撳瓨 ---
		// 杩欓噷闇€瑕佸皢 img 鎹曡幏杩涘幓锛屽洜涓?Lambda 缁撴潫 img 灏辨瀽鏋勪簡
		QTimer::singleShot(0, this, [this, pathUri, key, img]() {
			QPixmap pix = QPixmap::fromImage(img);

			{
				QMutexLocker locker(&m_mutex);
				if (!m_thumbnailCache.contains(key)) {
					// RGB888/ARGB32 鏄惧瓨 cost 璁＄畻 (KB)
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
	// 鎴栬€呮洿绠€鍗曠殑寮鸿鎷疯礉锛歝onst QString safePathUri = pathUri; (Qt QString 鍏锋湁鑷姩鍐欐椂澶嶅埗锛屾樉寮忔嫹璐濊兘闄嶄綆Bug椋庨櫓)
	// 宸ヤ笟绾ф渶绋冲Ε鍐欐硶锛?
	const QString threadLocalUri = pathUri;
	threadLocalUri.toUpper(); // 瑙﹀彂鏃犳剰涔夌殑鍐欐搷浣滐紝寮哄埗鍏跺湪鍐呭瓨涓敓鎴愮嫭绔嬪壇鏈?
	threadLocalUri.toLower(); // 杩樺師

							  // 鑾峰彇褰撳墠浜害璁剧疆
	int br = 0;
	// 鐢熸垚甯︿寒搴︾殑鍞竴 Key 
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

		// A. 瑙ｆ瀽 URI 鍒ゆ柇鏄暟鎹簱鐩磋杩樻槸鏈湴鏂囦欢璇诲彇
		QStringList parts = threadLocalUri.split("|");
		if (parts.size() == 3) {
			// 杩欐槸浠?TunnelSectionItem 浼犳潵鐨勬暟鎹簱鍒囩墖璇锋眰
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
					//    qDebug() << "Thread ID:" << QThread::currentThreadId() << " decoding tile:" << threadLocalUri << " size:" << imgData.size();
					img = QImage::fromData(imgData, "JPG"); // 鍐呭瓨瑙ｇ爜
				}
			}
		}
		else {
			// 鍚戜笅鍏煎锛氭櫘閫氱殑鏈湴鍥剧墖鏂囦欢鍔犺浇
			img = QImage(threadLocalUri);
		}

		// B. 鏁版嵁鎹熷潖鎴栫┖鍥剧殑闃查棯閫€鍏滃簳澶勭悊
		if (img.isNull()) {
			img = QImage(1024, 1024, QImage::Format_RGB888);
			img.fill(Qt::gray);
		}

		// C. 鏍煎紡鏍囧噯鍖栵紝涓轰寒搴﹁皟鑺傜畻娉曞仛鍑嗗
		if (img.format() != QImage::Format_RGB888) {
			img = img.convertToFormat(QImage::Format_RGB888);
		}

		// D. 浜害璋冭妭璁＄畻 (CPU瀵嗛泦鍨嬫搷浣?
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

		// E. 绉讳氦涓荤嚎绋嬬粍瑁呬负 QPixmap 骞堕€氱煡娓叉煋鏇存柊
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

				// 鍔″繀浣跨敤鍘熷璇锋眰瀛楃涓茶繑鍥烇紝浠ヤ究鎺ユ敹绔兘鍦?Pending 瀛楀吀涓垚鍔熸牳閿€
				emit AsyncImageLoader::instance()->sigImageLoaded(threadLocalUri, pix);
			}
		});
	});
}

