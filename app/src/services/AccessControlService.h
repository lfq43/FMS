#ifndef ACCESSCONTROLSERVICE_H
#define ACCESSCONTROLSERVICE_H

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

enum class AccessLevel {
    View = 1,
    Edit = 2,
    Share = 3
};

struct AccessGrantInfo {
    int grantId = -1;
    QString resourceType;
    int resourceId = -1;
    int granteeUserId = -1;
    QString granteeUsername;
    int grantedByUserId = -1;
    AccessLevel accessLevel = AccessLevel::View;
    bool inheritToChildren = true;
    QDateTime createdAt;
    QDateTime updatedAt;
    QDateTime expiresAt;
};

class AccessControlService : public QObject
{
    Q_OBJECT

public:
    explicit AccessControlService(QObject* parent = nullptr);

    bool canViewFile(int userId, int fileId) const;
    bool canEditFile(int userId, int fileId) const;
    bool canShareFile(int userId, int fileId) const;
    bool canViewFolder(int userId, int folderId) const;
    bool canEditFolder(int userId, int folderId) const;
    bool canShareFolder(int userId, int folderId) const;

    bool grantFileAccess(int actorUserId, int fileId, int granteeUserId, AccessLevel level) const;
    bool grantFolderAccess(int actorUserId, int folderId, int granteeUserId, AccessLevel level) const;
    bool revokeFileAccess(int actorUserId, int fileId, int granteeUserId) const;
    bool revokeFolderAccess(int actorUserId, int folderId, int granteeUserId) const;

    QList<AccessGrantInfo> listFileGrants(int actorUserId, int fileId) const;
    QList<AccessGrantInfo> listFolderGrants(int actorUserId, int folderId) const;
    int userIdForUsername(const QString& username) const;

private:
    bool canAccessFile(int userId, int fileId, AccessLevel requiredLevel) const;
    bool canAccessFolder(int userId, int folderId, AccessLevel requiredLevel) const;
    bool hasDirectGrant(const QString& resourceType, int resourceId, int userId, AccessLevel requiredLevel, bool requireInherited) const;
    bool upsertGrant(const QString& resourceType, int resourceId, int actorUserId, int granteeUserId, AccessLevel level, bool inheritToChildren) const;
    bool revokeGrant(const QString& resourceType, int resourceId, int actorUserId, int granteeUserId) const;
    QList<AccessGrantInfo> listGrants(const QString& resourceType, int resourceId, int actorUserId) const;

    int fileOwnerId(int fileId) const;
    int folderOwnerId(int folderId) const;
    int fileFolderId(int fileId) const;
    int parentFolderId(int folderId) const;
    bool userExists(int userId) const;
};

QString accessLevelToString(AccessLevel level);
AccessLevel accessLevelFromString(const QString& value);

#endif // ACCESSCONTROLSERVICE_H
