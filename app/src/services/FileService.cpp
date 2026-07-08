#include "FileService.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMimeDatabase>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

#include "AuthService.h"

namespace {
constexpr const char* kBackupOnAddKey = "file/backupOnAdd";
constexpr const char* kBackupRootPathKey = "file/backupRootPath";
constexpr const char* kSettingsOrg = "FMS";
constexpr const char* kSettingsApp = "FMS";

QSettings appSettings()
{
    return QSettings(kSettingsOrg, kSettingsApp);
}

QVariant nullableId(int id)
{
    return id > 0 ? QVariant(id) : QVariant();
}

QString safePathPart(QString value)
{
    value = value.trimmed();
    if (value.isEmpty()) {
        return "unknown";
    }

    const QString invalidChars = "\\/:*?\"<>|";
    for (const QChar ch : invalidChars) {
        value.replace(ch, "_");
    }
    return value;
}
}

FileService::FileService(QObject *parent) : QObject(parent)
{
    qDebug() << "FileService created";
}

FileService::~FileService()
{
    qDebug() << "FileService destroyed";
}

bool FileService::backupOnAddEnabled()
{
    return appSettings().value(kBackupOnAddKey, true).toBool();
    //返回kBackupOnAddKey对应的QVariant变量，true是没有找到时的默认返回值
}

void FileService::setBackupOnAddEnabled(bool enabled)
{
    QSettings settings = appSettings();
    settings.setValue(kBackupOnAddKey, enabled);
    settings.sync();
    //强制设置从内存写入磁盘
}

QString FileService::backupRootPath()
{
    const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/storage";
    const QString savedPath = appSettings().value(kBackupRootPathKey, defaultPath).toString().trimmed();
    return QDir::fromNativeSeparators(savedPath.isEmpty() ? defaultPath : savedPath);//转换分隔符
}

bool FileService::setBackupRootPath(const QString& path)
{
    const QString normalizedPath = QDir::fromNativeSeparators(path.trimmed());
    if (normalizedPath.isEmpty()) {
        qDebug() << "Set backup path failed: empty path";
        return false;
    }

    QDir dir(normalizedPath);
    if (!dir.exists() && !QDir().mkpath(normalizedPath)) {
        qDebug() << "Set backup path failed: cannot create directory" << normalizedPath;
        return false;
    }

    QSettings settings = appSettings();
    settings.setValue(kBackupRootPathKey, QDir(normalizedPath).absolutePath());
    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool FileService::addLocalFile(int ownerId, int folderId, const QString& filePath, bool backupFile, int* outFileId)
{
    if (ownerId <= 0) {
        qDebug() << "Add file failed: invalid owner id";
        return false;
    }

    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        qDebug() << "Add file failed: invalid file" << filePath;
        return false;
    }

    const QString checksum = calculateChecksum(filePath);
    if (checksum.isEmpty()) {
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) {
        qDebug() << "Start transaction failed:" << db.lastError().text();
        return false;
    }

    QSqlQuery insertFile(db);
    insertFile.prepare("INSERT INTO files "
                       "(owner_id, folder_id, name, original_path, extension, mime_type, size_bytes, checksum_sha256, created_at, updated_at) "
                       "VALUES (:owner_id, :folder_id, :name, :original_path, :extension, :mime_type, :size_bytes, :checksum, datetime('now'), datetime('now'))");
    insertFile.bindValue(":owner_id", ownerId);
    insertFile.bindValue(":folder_id", nullableId(folderId));
    insertFile.bindValue(":name", info.fileName());
    insertFile.bindValue(":original_path", info.absoluteFilePath());
    insertFile.bindValue(":extension", info.suffix());
    insertFile.bindValue(":mime_type", detectMimeType(info));
    insertFile.bindValue(":size_bytes", info.size());
    insertFile.bindValue(":checksum", checksum);

    if (!insertFile.exec()) {
        qDebug() << "Insert file failed:" << insertFile.lastError().text();
        db.rollback();
        return false;
    }

    const int fileId = insertFile.lastInsertId().toInt();
    const int versionNo = 1;
    const QString storagePath = backupFile ? backupFileToStorage(ownerId, fileId, versionNo, filePath) : info.absoluteFilePath();
    if (storagePath.isEmpty()) {
        db.rollback();
        return false;
    }

    QSqlQuery insertVersion(db);
    insertVersion.prepare("INSERT INTO file_versions "
                          "(file_id, version_no, storage_path, original_name, size_bytes, checksum_sha256, change_note, created_by, created_at) "
                          "VALUES (:file_id, :version_no, :storage_path, :original_name, :size_bytes, :checksum, :change_note, :created_by, datetime('now'))");
    insertVersion.bindValue(":file_id", fileId);
    insertVersion.bindValue(":version_no", versionNo);
    insertVersion.bindValue(":storage_path", storagePath);
    insertVersion.bindValue(":original_name", info.fileName());
    insertVersion.bindValue(":size_bytes", info.size());
    insertVersion.bindValue(":checksum", checksum);
    insertVersion.bindValue(":change_note", backupFile ? "添加文件并备份原始内容" : "添加文件，仅记录本地路径");
    insertVersion.bindValue(":created_by", ownerId);

    if (!insertVersion.exec()) {
        qDebug() << "Insert file version failed:" << insertVersion.lastError().text();
        db.rollback();
        return false;
    }

    const int versionId = insertVersion.lastInsertId().toInt();
    QSqlQuery updateFile(db);
    updateFile.prepare("UPDATE files SET current_version_id = :version_id WHERE id = :file_id");
    updateFile.bindValue(":version_id", versionId);
    updateFile.bindValue(":file_id", fileId);
    if (!updateFile.exec()) {
        qDebug() << "Update current version failed:" << updateFile.lastError().text();
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        qDebug() << "Commit add file failed:" << db.lastError().text();
        db.rollback();
        return false;
    }

    if (outFileId) {
        *outFileId = fileId;
    }
    return true;
}

bool FileService::addlocalFile(const QString& filePath)
{
    return addLocalFile(AuthService::currentUserId(), -1, filePath, backupOnAddEnabled());
}

bool FileService::createNewVersion(int ownerId, int fileId, const QString& filePath, const QString& changeNote, bool backupFile)
{
    if (ownerId <= 0 || fileId <= 0) {
        return false;
    }

    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        return false;
    }

    const QString checksum = calculateChecksum(filePath);
    const int versionNo = nextVersionNo(fileId);
    const QString storagePath = backupFile ? backupFileToStorage(ownerId, fileId, versionNo, filePath) : info.absoluteFilePath();
    if (checksum.isEmpty() || storagePath.isEmpty()) {
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) {
        return false;
    }

    QSqlQuery insertVersion(db);
    insertVersion.prepare("INSERT INTO file_versions "
                          "(file_id, version_no, storage_path, original_name, size_bytes, checksum_sha256, change_note, created_by, created_at) "
                          "VALUES (:file_id, :version_no, :storage_path, :original_name, :size_bytes, :checksum, :change_note, :created_by, datetime('now'))");
    insertVersion.bindValue(":file_id", fileId);
    insertVersion.bindValue(":version_no", versionNo);
    insertVersion.bindValue(":storage_path", storagePath);
    insertVersion.bindValue(":original_name", info.fileName());
    insertVersion.bindValue(":size_bytes", info.size());
    insertVersion.bindValue(":checksum", checksum);
    insertVersion.bindValue(":change_note", changeNote.isEmpty() ? (backupFile ? "创建新备份版本" : "创建新路径版本") : changeNote);
    insertVersion.bindValue(":created_by", ownerId);
    if (!insertVersion.exec()) {
        qDebug() << "Create version failed:" << insertVersion.lastError().text();
        db.rollback();
        return false;
    }

    const int versionId = insertVersion.lastInsertId().toInt();
    QSqlQuery updateFile(db);
    updateFile.prepare("UPDATE files "
                       "SET current_version_id = :version_id, name = :name, original_path = :original_path, extension = :extension, mime_type = :mime_type, "
                       "size_bytes = :size_bytes, checksum_sha256 = :checksum, updated_at = datetime('now') "
                       "WHERE id = :file_id AND owner_id = :owner_id");
    updateFile.bindValue(":version_id", versionId);
    updateFile.bindValue(":name", info.fileName());
    updateFile.bindValue(":original_path", info.absoluteFilePath());
    updateFile.bindValue(":extension", info.suffix());
    updateFile.bindValue(":mime_type", detectMimeType(info));
    updateFile.bindValue(":size_bytes", info.size());
    updateFile.bindValue(":checksum", checksum);
    updateFile.bindValue(":file_id", fileId);
    updateFile.bindValue(":owner_id", ownerId);
    if (!updateFile.exec() || updateFile.numRowsAffected() <= 0) {
        qDebug() << "Update file version pointer failed:" << updateFile.lastError().text();
        db.rollback();
        return false;
    }

    return db.commit();
}

QList<FileVersionInfo> FileService::listVersions(int ownerId, int fileId) const
{
    QList<FileVersionInfo> versions;
    QSqlQuery query;
    query.prepare("SELECT v.id, v.file_id, v.version_no, v.storage_path, v.original_name, v.size_bytes, "
                  "v.checksum_sha256, v.change_note, v.created_by, v.created_at "
                  "FROM file_versions v "
                  "JOIN files f ON f.id = v.file_id "
                  "WHERE f.owner_id = :owner_id AND v.file_id = :file_id "
                  "ORDER BY v.version_no DESC");
    query.bindValue(":owner_id", ownerId);
    query.bindValue(":file_id", fileId);
    if (!query.exec()) {
        qDebug() << "List versions failed:" << query.lastError().text();
        return versions;
    }

    while (query.next()) {
        FileVersionInfo version;
        version.versionId = query.value("id").toInt();
        version.fileId = query.value("file_id").toInt();
        version.versionNo = query.value("version_no").toInt();
        version.storagePath = query.value("storage_path").toString();
        version.originalName = query.value("original_name").toString();
        version.sizeBytes = query.value("size_bytes").toLongLong();
        version.checksumSha256 = query.value("checksum_sha256").toString();
        version.changeNote = query.value("change_note").toString();
        version.createdBy = query.value("created_by").toInt();
        version.createdAt = QDateTime::fromString(query.value("created_at").toString(), Qt::ISODate);
        versions.append(version);
    }
    return versions;
}

bool FileService::restoreVersion(int ownerId, int fileId, int versionId)
{
    QSqlQuery query;
    query.prepare("SELECT storage_path, original_name, size_bytes, checksum_sha256 "
                  "FROM file_versions WHERE id = :version_id AND file_id = :file_id");
    query.bindValue(":version_id", versionId);
    query.bindValue(":file_id", fileId);
    if (!query.exec() || !query.next()) {
        return false;
    }

    const QString storagePath = query.value("storage_path").toString();
    const QString originalName = query.value("original_name").toString();
    const qint64 sizeBytes = query.value("size_bytes").toLongLong();
    const QString checksum = query.value("checksum_sha256").toString();

    QSqlQuery updateQuery;
    updateQuery.prepare("UPDATE files "
                        "SET current_version_id = :version_id, name = :name, size_bytes = :size_bytes, "
                        "checksum_sha256 = :checksum, updated_at = datetime('now') "
                        "WHERE id = :file_id AND owner_id = :owner_id");
    updateQuery.bindValue(":version_id", versionId);
    updateQuery.bindValue(":name", originalName);
    updateQuery.bindValue(":size_bytes", sizeBytes);
    updateQuery.bindValue(":checksum", checksum);
    updateQuery.bindValue(":file_id", fileId);
    updateQuery.bindValue(":owner_id", ownerId);

    if (!updateQuery.exec()) {
        qDebug() << "Restore version failed:" << updateQuery.lastError().text() << storagePath;
        return false;
    }
    return updateQuery.numRowsAffected() > 0;
}

QList<LocalFileInfo> FileService::listFiles(int ownerId, int folderId) const
{
    QList<LocalFileInfo> files;
    if (ownerId <= 0) {
        return files;
    }

    QSqlQuery query;
    const QString folderFilter = folderId > 0 ? "f.folder_id = :folder_id" : "f.folder_id IS NULL";
    query.prepare(QString("SELECT f.id, f.owner_id, f.folder_id, f.category_id, f.current_version_id, "
                          "f.name, f.original_path, f.extension, f.mime_type, f.size_bytes, f.checksum_sha256, f.created_at, f.updated_at, "
                          "f.is_trashed, v.storage_path, v.change_note "
                          "FROM files f "
                          "LEFT JOIN file_versions v ON v.id = f.current_version_id "
                          "WHERE f.owner_id = :owner_id AND %1 AND f.is_trashed = 0 "
                          "ORDER BY f.updated_at DESC").arg(folderFilter));
    query.bindValue(":owner_id", ownerId);
    if (folderId > 0) {
        query.bindValue(":folder_id", folderId);
    }

    if (!query.exec()) {
        qDebug() << "List files failed:" << query.lastError().text();
        return files;
    }

    while (query.next()) {
        files.append(readFileInfoFromQuery(query));
    }
    return files;
}

QList<LocalFileInfo> FileService::getAllFiles() const
{
    return listFiles(AuthService::currentUserId());
}

bool FileService::moveFile(int ownerId, int fileId, int targetFolderId)
{
    if (ownerId <= 0 || fileId <= 0) {
        return false;
    }

    QSqlQuery duplicateQuery;
    const QString folderFilter = targetFolderId > 0 ? "folder_id = :folder_id" : "folder_id IS NULL";
    duplicateQuery.prepare(QString("SELECT 1 FROM files "
                                   "WHERE owner_id = :owner_id AND %1 AND name = ("
                                   "SELECT name FROM files WHERE id = :file_id AND owner_id = :owner_id AND is_trashed = 0"
                                   ") AND id != :file_id AND is_trashed = 0").arg(folderFilter));
    duplicateQuery.bindValue(":owner_id", ownerId);
    duplicateQuery.bindValue(":file_id", fileId);
    if (targetFolderId > 0) {
        duplicateQuery.bindValue(":folder_id", targetFolderId);
    }
    if (!duplicateQuery.exec() || duplicateQuery.next()) {
        return false;
    }

    QSqlQuery query;
    query.prepare("UPDATE files SET folder_id = :folder_id, updated_at = datetime('now') "
                  "WHERE id = :file_id AND owner_id = :owner_id AND is_trashed = 0");
    query.bindValue(":folder_id", nullableId(targetFolderId));
    query.bindValue(":file_id", fileId);
    query.bindValue(":owner_id", ownerId);
    if (!query.exec()) {
        qDebug() << "Move file failed:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool FileService::copyFile(int ownerId, int fileId, int targetFolderId, int* outFileId)
{
    if (ownerId <= 0 || fileId <= 0) {
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) {
        return false;
    }

    QSqlQuery sourceQuery(db);
    sourceQuery.prepare("SELECT f.name, f.original_path, f.extension, f.mime_type, f.size_bytes, f.checksum_sha256, "
                        "v.storage_path, v.original_name, v.change_note "
                        "FROM files f "
                        "LEFT JOIN file_versions v ON v.id = f.current_version_id "
                        "WHERE f.id = :file_id AND f.owner_id = :owner_id AND f.is_trashed = 0");
    sourceQuery.bindValue(":file_id", fileId);
    sourceQuery.bindValue(":owner_id", ownerId);
    if (!sourceQuery.exec() || !sourceQuery.next()) {
        db.rollback();
        return false;
    }

    QString targetName = sourceQuery.value("name").toString();
    QSqlQuery duplicateQuery(db);
    const QString folderFilter = targetFolderId > 0 ? "folder_id = :folder_id" : "folder_id IS NULL";
    duplicateQuery.prepare(QString("SELECT 1 FROM files "
                                   "WHERE owner_id = :owner_id AND %1 AND name = :name AND is_trashed = 0").arg(folderFilter));
    duplicateQuery.bindValue(":owner_id", ownerId);
    duplicateQuery.bindValue(":name", targetName);
    if (targetFolderId > 0) {
        duplicateQuery.bindValue(":folder_id", targetFolderId);
    }
    if (!duplicateQuery.exec()) {
        db.rollback();
        return false;
    }
    if (duplicateQuery.next()) {
        const QFileInfo nameInfo(targetName);
        const QString suffix = nameInfo.suffix();
        const QString baseName = suffix.isEmpty() ? targetName : targetName.left(targetName.size() - suffix.size() - 1);
        targetName = suffix.isEmpty() ? baseName + " - 副本" : QString("%1 - 副本.%2").arg(baseName, suffix);
    }

    QSqlQuery insertFile(db);
    insertFile.prepare("INSERT INTO files "
                       "(owner_id, folder_id, name, original_path, extension, mime_type, size_bytes, checksum_sha256, created_at, updated_at) "
                       "VALUES (:owner_id, :folder_id, :name, :original_path, :extension, :mime_type, :size_bytes, :checksum, datetime('now'), datetime('now'))");
    insertFile.bindValue(":owner_id", ownerId);
    insertFile.bindValue(":folder_id", nullableId(targetFolderId));
    insertFile.bindValue(":name", targetName);
    insertFile.bindValue(":original_path", sourceQuery.value("original_path"));
    insertFile.bindValue(":extension", sourceQuery.value("extension"));
    insertFile.bindValue(":mime_type", sourceQuery.value("mime_type"));
    insertFile.bindValue(":size_bytes", sourceQuery.value("size_bytes"));
    insertFile.bindValue(":checksum", sourceQuery.value("checksum_sha256"));
    if (!insertFile.exec()) {
        qDebug() << "Copy file insert failed:" << insertFile.lastError().text();
        db.rollback();
        return false;
    }

    const int newFileId = insertFile.lastInsertId().toInt();
    QSqlQuery insertVersion(db);
    insertVersion.prepare("INSERT INTO file_versions "
                          "(file_id, version_no, storage_path, original_name, size_bytes, checksum_sha256, change_note, created_by, created_at) "
                          "VALUES (:file_id, 1, :storage_path, :original_name, :size_bytes, :checksum, :change_note, :created_by, datetime('now'))");
    insertVersion.bindValue(":file_id", newFileId);
    insertVersion.bindValue(":storage_path", sourceQuery.value("storage_path"));
    insertVersion.bindValue(":original_name", sourceQuery.value("original_name"));
    insertVersion.bindValue(":size_bytes", sourceQuery.value("size_bytes"));
    insertVersion.bindValue(":checksum", sourceQuery.value("checksum_sha256"));
    insertVersion.bindValue(":change_note", "复制文件");
    insertVersion.bindValue(":created_by", ownerId);
    if (!insertVersion.exec()) {
        qDebug() << "Copy file version failed:" << insertVersion.lastError().text();
        db.rollback();
        return false;
    }

    QSqlQuery updateFile(db);
    updateFile.prepare("UPDATE files SET current_version_id = :version_id WHERE id = :file_id");
    updateFile.bindValue(":version_id", insertVersion.lastInsertId().toInt());
    updateFile.bindValue(":file_id", newFileId);
    if (!updateFile.exec()) {
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        return false;
    }
    if (outFileId) {
        *outFileId = newFileId;
    }
    return true;
}

bool FileService::trashFile(int ownerId, int fileId)
{
    QSqlQuery query;
    query.prepare("UPDATE files SET is_trashed = 1, trashed_at = datetime('now'), updated_at = datetime('now') "
                  "WHERE id = :file_id AND owner_id = :owner_id");
    query.bindValue(":file_id", fileId);
    query.bindValue(":owner_id", ownerId);
    if (!query.exec()) {
        qDebug() << "Trash file failed:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool FileService::removeFile(int fileId)
{
    return trashFile(AuthService::currentUserId(), fileId);
}

bool FileService::updateFileStatus(int fileId, const QString& status)
{
    Q_UNUSED(status);
    QSqlQuery query;
    query.prepare("UPDATE files SET updated_at = datetime('now') WHERE id = :file_id AND owner_id = :owner_id");
    query.bindValue(":file_id", fileId);
    query.bindValue(":owner_id", AuthService::currentUserId());
    return query.exec() && query.numRowsAffected() > 0;
}

QString FileService::calculateChecksum(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot open file for checksum:" << filePath;
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        qDebug() << "Cannot calculate checksum:" << filePath;
        return QString();
    }

    return QString(hash.result().toHex());
}

QString FileService::backupFileToStorage(int ownerId, int fileId, int versionNo, const QString& filePath) const
{
    QFileInfo info(filePath);
    const QString ownerName = safePathPart(ownerNameForPath(ownerId));
    const QString fileFolderName = QString("%1_%2_%3")
                                       .arg(ownerName)
                                       .arg(fileId)
                                       .arg(safePathPart(info.completeBaseName()));
    const QString baseDir = QDir(backupRootPath()).filePath(fileFolderName);

    QDir dir;
    if (!dir.mkpath(baseDir)) {
        qDebug() << "Create backup directory failed:" << baseDir;
        return QString();
    }

    const QString targetFileName = QString("v%1_%2")
                                       .arg(versionNo, 3, 10, QLatin1Char('0'))
                                       .arg(safePathPart(info.fileName()));
    const QString targetPath = QDir(baseDir).filePath(targetFileName);
    if (QFile::exists(targetPath)) {
        QFile::remove(targetPath);
    }

    if (!QFile::copy(info.absoluteFilePath(), targetPath)) {
        qDebug() << "Backup file copy failed:" << info.absoluteFilePath() << targetPath;
        return QString();
    }
    return targetPath;
}

QString FileService::ownerNameForPath(int ownerId) const
{
    if (AuthService::currentUserId() == ownerId && !AuthService::currentUsername().isEmpty()) {
        return AuthService::currentUsername();
    }

    QSqlQuery query;
    query.prepare("SELECT username FROM users WHERE id = :id");
    query.bindValue(":id", ownerId);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }

    return QString("user_%1").arg(ownerId);
}

QString FileService::detectMimeType(const QFileInfo& fileInfo) const
{
    QMimeDatabase mimeDb;
    return mimeDb.mimeTypeForFile(fileInfo).name();
}

int FileService::nextVersionNo(int fileId) const
{
    QSqlQuery query;
    query.prepare("SELECT COALESCE(MAX(version_no), 0) + 1 FROM file_versions WHERE file_id = :file_id");
    query.bindValue(":file_id", fileId);
    if (!query.exec() || !query.next()) {
        return 1;
    }
    return query.value(0).toInt();
}

LocalFileInfo FileService::readFileInfoFromQuery(const QSqlQuery& query) const
{
    LocalFileInfo info;
    info.fileId = query.value("id").toInt();
    info.ownerId = query.value("owner_id").toInt();
    info.folderId = query.value("folder_id").isNull() ? -1 : query.value("folder_id").toInt();
    info.categoryId = query.value("category_id").isNull() ? -1 : query.value("category_id").toInt();
    info.currentVersionId = query.value("current_version_id").isNull() ? -1 : query.value("current_version_id").toInt();
    info.fileName = query.value("name").toString();
    info.baseName = QFileInfo(info.fileName).baseName();
    info.extension = query.value("extension").toString();
    info.mimeType = query.value("mime_type").toString();
    info.fileSize = query.value("size_bytes").toLongLong();
    info.checksumSha256 = query.value("checksum_sha256").toString();
    info.addedAt = QDateTime::fromString(query.value("created_at").toString(), Qt::ISODate);
    info.modifiedAt = QDateTime::fromString(query.value("updated_at").toString(), Qt::ISODate);
    info.isTrashed = query.value("is_trashed").toBool();
    info.storagePath = query.value("storage_path").toString();
    info.filePath = query.value("original_path").toString();
    if (info.filePath.isEmpty()) {
        info.filePath = info.storagePath;
    }
    info.isBackedUp = !info.storagePath.isEmpty() && info.storagePath != info.filePath;
    return info;
}
