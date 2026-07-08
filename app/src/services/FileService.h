#ifndef FILE_SERVICE_H
#define FILE_SERVICE_H

#include <QDateTime>
#include <QFileInfo>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

class QSqlQuery;

struct LocalFileInfo {
    int fileId = -1;
    int ownerId = -1;
    int folderId = -1;
    int categoryId = -1;
    int currentVersionId = -1;

    QString fileName;
    QString baseName;
    QString filePath;
    QString storagePath;
    QString extension;
    QString mimeType;
    qint64 fileSize = 0;
    QString checksumSha256;
    QStringList tags;

    QDateTime addedAt;
    QDateTime modifiedAt;
    bool isTrashed = false;
    bool isBackedUp = false;
};

struct FileVersionInfo {
    int versionId = -1;
    int fileId = -1;
    int versionNo = 0;
    QString originalName;
    QString storagePath;
    qint64 sizeBytes = 0;
    QString checksumSha256;
    QString changeNote;
    int createdBy = -1;
    QDateTime createdAt;
};

class FileService : public QObject {
    Q_OBJECT

public:
    explicit FileService(QObject *parent = nullptr);
    ~FileService();

    static bool backupOnAddEnabled();
    static void setBackupOnAddEnabled(bool enabled);
    static QString backupRootPath();
    static bool setBackupRootPath(const QString& path);

    bool addLocalFile(int ownerId, int folderId, const QString& filePath, bool backupFile, int* outFileId = nullptr);
    bool addlocalFile(const QString& filePath);

    bool createNewVersion(int ownerId, int fileId, const QString& filePath, const QString& changeNote, bool backupFile);
    QList<FileVersionInfo> listVersions(int ownerId, int fileId) const;
    bool restoreVersion(int ownerId, int fileId, int versionId);

    QList<LocalFileInfo> listFiles(int ownerId, int folderId = -1) const;
    QList<LocalFileInfo> listSharedFiles(int userId, int folderId = -1) const;
    QList<LocalFileInfo> getAllFiles() const;
    QStringList tagsForFile(int ownerId, int fileId) const;
    bool setFileTags(int ownerId, int fileId, const QStringList& tags);

    bool moveFile(int ownerId, int fileId, int targetFolderId);
    bool copyFile(int ownerId, int fileId, int targetFolderId, int* outFileId = nullptr);
    bool trashFile(int ownerId, int fileId);
    bool removeFile(int fileId);
    bool updateFileStatus(int fileId, const QString& status);

    QString calculateChecksum(const QString& filePath) const;

private:
    QString backupFileToStorage(int ownerId, int fileId, int versionNo, const QString& filePath) const;
    QString ownerNameForPath(int ownerId) const;
    QString detectMimeType(const QFileInfo& fileInfo) const;
    int nextVersionNo(int fileId) const;
    LocalFileInfo readFileInfoFromQuery(const QSqlQuery& query) const;
};

#endif // FILE_SERVICE_H
