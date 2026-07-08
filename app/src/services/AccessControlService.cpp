#include "AccessControlService.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
constexpr const char* kFileResource = "file";
constexpr const char* kFolderResource = "folder";

bool grantCoversRequired(AccessLevel grantedLevel, AccessLevel requiredLevel)
{
    return static_cast<int>(grantedLevel) >= static_cast<int>(requiredLevel);
}

AccessGrantInfo readGrantInfo(const QSqlQuery& query)
{
    AccessGrantInfo info;
    info.grantId = query.value("id").toInt();
    info.resourceType = query.value("resource_type").toString();
    info.resourceId = query.value("resource_id").toInt();
    info.granteeUserId = query.value("grantee_user_id").toInt();
    info.granteeUsername = query.value("username").toString();
    info.grantedByUserId = query.value("granted_by_user_id").toInt();
    info.accessLevel = accessLevelFromString(query.value("access_level").toString());
    info.inheritToChildren = query.value("inherit_to_children").toBool();
    info.createdAt = QDateTime::fromString(query.value("created_at").toString(), Qt::ISODate);
    info.updatedAt = QDateTime::fromString(query.value("updated_at").toString(), Qt::ISODate);
    if (!query.value("expires_at").isNull()) {
        info.expiresAt = QDateTime::fromString(query.value("expires_at").toString(), Qt::ISODate);
    }
    return info;
}
}

QString accessLevelToString(AccessLevel level)
{
    switch (level) {
    case AccessLevel::Share:
        return QStringLiteral("share");
    case AccessLevel::Edit:
        return QStringLiteral("edit");
    case AccessLevel::View:
    default:
        return QStringLiteral("view");
    }
}

AccessLevel accessLevelFromString(const QString& value)
{
    if (value.compare(QStringLiteral("share"), Qt::CaseInsensitive) == 0) {
        return AccessLevel::Share;
    }
    if (value.compare(QStringLiteral("edit"), Qt::CaseInsensitive) == 0) {
        return AccessLevel::Edit;
    }
    return AccessLevel::View;
}

AccessControlService::AccessControlService(QObject* parent)
    : QObject(parent)
{
}

bool AccessControlService::canViewFile(int userId, int fileId) const
{
    return canAccessFile(userId, fileId, AccessLevel::View);
}

bool AccessControlService::canEditFile(int userId, int fileId) const
{
    return canAccessFile(userId, fileId, AccessLevel::Edit);
}

bool AccessControlService::canShareFile(int userId, int fileId) const
{
    return canAccessFile(userId, fileId, AccessLevel::Share);
}

bool AccessControlService::canViewFolder(int userId, int folderId) const
{
    return canAccessFolder(userId, folderId, AccessLevel::View);
}

bool AccessControlService::canEditFolder(int userId, int folderId) const
{
    return canAccessFolder(userId, folderId, AccessLevel::Edit);
}

bool AccessControlService::canShareFolder(int userId, int folderId) const
{
    return canAccessFolder(userId, folderId, AccessLevel::Share);
}

bool AccessControlService::grantFileAccess(int actorUserId, int fileId, int granteeUserId, AccessLevel level) const
{
    if (!canShareFile(actorUserId, fileId) || !userExists(granteeUserId)) {
        return false;
    }
    return upsertGrant(kFileResource, fileId, actorUserId, granteeUserId, level, true);
}

bool AccessControlService::grantFolderAccess(int actorUserId, int folderId, int granteeUserId, AccessLevel level) const
{
    if (!canShareFolder(actorUserId, folderId) || !userExists(granteeUserId)) {
        return false;
    }
    return upsertGrant(kFolderResource, folderId, actorUserId, granteeUserId, level, true);
}

bool AccessControlService::revokeFileAccess(int actorUserId, int fileId, int granteeUserId) const
{
    return revokeGrant(kFileResource, fileId, actorUserId, granteeUserId);
}

bool AccessControlService::revokeFolderAccess(int actorUserId, int folderId, int granteeUserId) const
{
    return revokeGrant(kFolderResource, folderId, actorUserId, granteeUserId);
}

QList<AccessGrantInfo> AccessControlService::listFileGrants(int actorUserId, int fileId) const
{
    return listGrants(kFileResource, fileId, actorUserId);
}

QList<AccessGrantInfo> AccessControlService::listFolderGrants(int actorUserId, int folderId) const
{
    return listGrants(kFolderResource, folderId, actorUserId);
}

int AccessControlService::userIdForUsername(const QString& username) const
{
    const QString normalizedUsername = username.trimmed();
    if (normalizedUsername.isEmpty()) {
        return -1;
    }

    QSqlQuery query;
    query.prepare("SELECT id FROM users WHERE username = :username AND status = 'active'");
    query.bindValue(":username", normalizedUsername);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return -1;
}

bool AccessControlService::canAccessFile(int userId, int fileId, AccessLevel requiredLevel) const
{
    if (userId <= 0 || fileId <= 0) {
        return false;
    }

    if (fileOwnerId(fileId) == userId) {
        return true;
    }

    if (hasDirectGrant(kFileResource, fileId, userId, requiredLevel, false)) {
        return true;
    }

    int folderId = fileFolderId(fileId);
    while (folderId > 0) {
        if (folderOwnerId(folderId) == userId) {
            return true;
        }
        if (hasDirectGrant(kFolderResource, folderId, userId, requiredLevel, true)) {
            return true;
        }
        folderId = parentFolderId(folderId);
    }
    return false;
}

bool AccessControlService::canAccessFolder(int userId, int folderId, AccessLevel requiredLevel) const
{
    if (userId <= 0 || folderId <= 0) {
        return false;
    }

    if (folderOwnerId(folderId) == userId) {
        return true;
    }

    int currentFolderId = folderId;
    bool isCurrentFolder = true;
    while (currentFolderId > 0) {
        if (hasDirectGrant(kFolderResource, currentFolderId, userId, requiredLevel, !isCurrentFolder)) {
            return true;
        }
        currentFolderId = parentFolderId(currentFolderId);
        isCurrentFolder = false;
    }
    return false;
}

bool AccessControlService::hasDirectGrant(const QString& resourceType, int resourceId, int userId, AccessLevel requiredLevel, bool requireInherited) const
{
    QSqlQuery query;
    QString inheritFilter;
    if (requireInherited) {
        inheritFilter = "AND inherit_to_children = 1 ";
    }
    query.prepare(QString("SELECT access_level FROM access_grants "
                          "WHERE resource_type = :resource_type "
                          "AND resource_id = :resource_id "
                          "AND grantee_user_id = :user_id "
                          "AND (expires_at IS NULL OR expires_at > datetime('now')) %1"
                          "LIMIT 1").arg(inheritFilter));
    query.bindValue(":resource_type", resourceType);
    query.bindValue(":resource_id", resourceId);
    query.bindValue(":user_id", userId);

    if (!query.exec()) {
        qDebug() << "Check access grant failed:" << query.lastError().text();
        return false;
    }
    if (!query.next()) {
        return false;
    }
    return grantCoversRequired(accessLevelFromString(query.value("access_level").toString()), requiredLevel);
}

bool AccessControlService::upsertGrant(const QString& resourceType, int resourceId, int actorUserId, int granteeUserId, AccessLevel level, bool inheritToChildren) const
{
    if (resourceId <= 0 || actorUserId <= 0 || granteeUserId <= 0) {
        return false;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO access_grants "
                  "(resource_type, resource_id, grantee_user_id, granted_by_user_id, access_level, inherit_to_children, created_at, updated_at) "
                  "VALUES (:resource_type, :resource_id, :grantee_user_id, :granted_by_user_id, :access_level, :inherit_to_children, datetime('now'), datetime('now')) "
                  "ON CONFLICT(resource_type, resource_id, grantee_user_id) DO UPDATE SET "
                  "granted_by_user_id = excluded.granted_by_user_id, "
                  "access_level = excluded.access_level, "
                  "inherit_to_children = excluded.inherit_to_children, "
                  "updated_at = datetime('now'), "
                  "expires_at = NULL");
    query.bindValue(":resource_type", resourceType);
    query.bindValue(":resource_id", resourceId);
    query.bindValue(":grantee_user_id", granteeUserId);
    query.bindValue(":granted_by_user_id", actorUserId);
    query.bindValue(":access_level", accessLevelToString(level));
    query.bindValue(":inherit_to_children", inheritToChildren ? 1 : 0);

    if (!query.exec()) {
        qDebug() << "Grant access failed:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool AccessControlService::revokeGrant(const QString& resourceType, int resourceId, int actorUserId, int granteeUserId) const
{
    const bool allowed = resourceType == kFileResource
        ? canShareFile(actorUserId, resourceId)
        : canShareFolder(actorUserId, resourceId);
    if (!allowed) {
        return false;
    }

    QSqlQuery query;
    query.prepare("DELETE FROM access_grants "
                  "WHERE resource_type = :resource_type "
                  "AND resource_id = :resource_id "
                  "AND grantee_user_id = :grantee_user_id");
    query.bindValue(":resource_type", resourceType);
    query.bindValue(":resource_id", resourceId);
    query.bindValue(":grantee_user_id", granteeUserId);

    if (!query.exec()) {
        qDebug() << "Revoke access failed:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

QList<AccessGrantInfo> AccessControlService::listGrants(const QString& resourceType, int resourceId, int actorUserId) const
{
    QList<AccessGrantInfo> grants;
    const bool allowed = resourceType == kFileResource
        ? canShareFile(actorUserId, resourceId)
        : canShareFolder(actorUserId, resourceId);
    if (!allowed) {
        return grants;
    }

    QSqlQuery query;
    query.prepare("SELECT g.id, g.resource_type, g.resource_id, g.grantee_user_id, u.username, "
                  "g.granted_by_user_id, g.access_level, g.inherit_to_children, "
                  "g.created_at, g.updated_at, g.expires_at "
                  "FROM access_grants g "
                  "LEFT JOIN users u ON u.id = g.grantee_user_id "
                  "WHERE g.resource_type = :resource_type AND g.resource_id = :resource_id "
                  "ORDER BY u.username COLLATE NOCASE ASC");
    query.bindValue(":resource_type", resourceType);
    query.bindValue(":resource_id", resourceId);

    if (!query.exec()) {
        qDebug() << "List grants failed:" << query.lastError().text();
        return grants;
    }

    while (query.next()) {
        grants.append(readGrantInfo(query));
    }
    return grants;
}

int AccessControlService::fileOwnerId(int fileId) const
{
    QSqlQuery query;
    query.prepare("SELECT owner_id FROM files WHERE id = :id AND is_trashed = 0");
    query.bindValue(":id", fileId);
    if (query.exec() && query.next()) {
        return query.value("owner_id").toInt();
    }
    return -1;
}

int AccessControlService::folderOwnerId(int folderId) const
{
    QSqlQuery query;
    query.prepare("SELECT owner_id FROM folders WHERE id = :id AND is_trashed = 0");
    query.bindValue(":id", folderId);
    if (query.exec() && query.next()) {
        return query.value("owner_id").toInt();
    }
    return -1;
}

int AccessControlService::fileFolderId(int fileId) const
{
    QSqlQuery query;
    query.prepare("SELECT folder_id FROM files WHERE id = :id AND is_trashed = 0");
    query.bindValue(":id", fileId);
    if (query.exec() && query.next()) {
        return query.value("folder_id").isNull() ? -1 : query.value("folder_id").toInt();
    }
    return -1;
}

int AccessControlService::parentFolderId(int folderId) const
{
    QSqlQuery query;
    query.prepare("SELECT parent_id FROM folders WHERE id = :id AND is_trashed = 0");
    query.bindValue(":id", folderId);
    if (query.exec() && query.next()) {
        return query.value("parent_id").isNull() ? -1 : query.value("parent_id").toInt();
    }
    return -1;
}

bool AccessControlService::userExists(int userId) const
{
    QSqlQuery query;
    query.prepare("SELECT 1 FROM users WHERE id = :id AND status = 'active'");
    query.bindValue(":id", userId);
    return query.exec() && query.next();
}
