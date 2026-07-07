#ifndef FOLDERSERVICE_H
#define FOLDERSERVICE_H

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

class QSqlQuery;

struct FolderInfo {
    int folderId = -1;
    int ownerId = -1;
    int parentId = -1;
    QString name;
    QString localPath;
    QString description;
    QDateTime createdAt;
    QDateTime updatedAt;
    bool isTrashed = false;
};

class FolderService : public QObject
{
    Q_OBJECT
public:
    explicit FolderService(QObject* parent = nullptr);

    bool createFolder(int ownerId, int parentId, const QString& name, int* outFolderId = nullptr, const QString& localPath = QString());
    QList<FolderInfo> listFolders(int ownerId, int parentId = -1) const;
    bool renameFolder(int ownerId, int folderId, const QString& newName);
    bool trashFolder(int ownerId, int folderId);
    bool moveFolder(int ownerId, int folderId, int newParentId);
    QList<FolderInfo> folderPath(int ownerId, int folderId) const;

private:
    bool folderExists(int ownerId, int folderId) const;
    bool hasDuplicateName(int ownerId, int parentId, const QString& name, int excludeFolderId = -1) const;
    bool isDescendantFolder(int ownerId, int folderId, int possibleChildId) const;
    FolderInfo readFolderInfoFromQuery(const QSqlQuery& query) const;
};

#endif // FOLDERSERVICE_H
