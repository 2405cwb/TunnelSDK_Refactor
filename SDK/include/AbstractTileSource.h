#ifndef ABSTRACTTILESOURCE_H
#define ABSTRACTTILESOURCE_H

#include <QString>
#include <QSize>
#include <QByteArray>
#include <QImage>
#include <QList>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>
#include <QHash>

struct TileImageData {
	int col;
	int row;
	QByteArray data;
};

class AbstractTileSource
{
public:
	virtual ~AbstractTileSource() {}

	virtual bool isValid() const = 0;
	virtual QString oriImageName() const = 0;
	virtual QSize totalSize() const = 0;
	virtual int tileSize() const = 0;

	// Legacy SQLite path kept for source implementations that already exist.
	// UI and loader code should prefer the tile/thumbnail APIs below.
	virtual QString getDbPath() const = 0;
	virtual QString getThumbnailImage() const = 0;

	virtual QString cacheKey() const {
		return getDbPath();
	}

	virtual QString thumbnailCacheKey() const {
		return QString("%1|thumbnail").arg(cacheKey());
	}

	virtual QString tileCacheKey(int col, int row) const {
		return QString("%1|%2|%3").arg(cacheKey()).arg(col).arg(row);
	}

	virtual QByteArray tileData(int col, int row) const {
		QSqlDatabase db = databaseForCurrentThread();
		if (!db.isOpen()) return QByteArray();

		QSqlQuery query(db);
		query.prepare("SELECT data FROM tiles WHERE col = ? AND row = ?");
		query.addBindValue(col);
		query.addBindValue(row);
		if (query.exec() && query.next()) {
			return query.value(0).toByteArray();
		}
		return QByteArray();
	}

	virtual QList<TileImageData> tileDataRange(int startCol, int endCol, int startRow, int endRow) const {
		QList<TileImageData> result;
		QSqlDatabase db = databaseForCurrentThread();
		if (!db.isOpen()) return result;

		QSqlQuery query(db);
		query.prepare("SELECT col, row, data FROM tiles WHERE col >= ? AND col <= ? AND row >= ? AND row <= ?");
		query.bindValue(0, startCol);
		query.bindValue(1, endCol);
		query.bindValue(2, startRow);
		query.bindValue(3, endRow);
		if (query.exec()) {
			while (query.next()) {
				TileImageData tile;
				tile.col = query.value(0).toInt();
				tile.row = query.value(1).toInt();
				tile.data = query.value(2).toByteArray();
				result.append(tile);
			}
		}
		return result;
	}

	virtual QByteArray thumbnailData() const {
		QSqlDatabase db = databaseForCurrentThread();
		if (!db.isOpen()) return QByteArray();

		QSqlQuery query(db);
		if (query.exec("SELECT data FROM thumbnail LIMIT 1") && query.next()) {
			return query.value(0).toByteArray();
		}
		return QByteArray();
	}

	virtual QImage tileImage(int col, int row) const {
		return QImage::fromData(tileData(col, row), "JPG");
	}

	virtual QImage thumbnailImage() const {
		QImage img = QImage::fromData(thumbnailData());
		if (img.isNull()) {
			img = QImage(getThumbnailImage());
		}
		return img;
	}

protected:
	QSqlDatabase databaseForCurrentThread() const {
		const QString dbPath = getDbPath();
		if (dbPath.isEmpty()) return QSqlDatabase();

		const int maxConnectionsPerThread = 32;
		static thread_local QHash<QString, QString> connectionNames;
		static thread_local QList<QString> lruKeys;

		QString connName = connectionNames.value(dbPath);
		if (connName.isEmpty()) {
			while (connectionNames.size() >= maxConnectionsPerThread && !lruKeys.isEmpty()) {
				const QString oldPath = lruKeys.takeFirst();
				const QString oldConnName = connectionNames.take(oldPath);
				if (!oldConnName.isEmpty() && QSqlDatabase::contains(oldConnName)) {
					{
						QSqlDatabase oldDb = QSqlDatabase::database(oldConnName);
						oldDb.close();
					}
					QSqlDatabase::removeDatabase(oldConnName);
				}
			}

			connName = QString("TileSource_%1_%2")
				.arg((quintptr)QThread::currentThreadId())
				.arg(qHash(dbPath));
			connectionNames.insert(dbPath, connName);
		}

		lruKeys.removeAll(dbPath);
		lruKeys.append(dbPath);

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

private:
};

#endif
