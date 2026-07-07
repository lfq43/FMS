#include "FolderService.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
QVariant nullableFolderId(int folderId)
{
    return folderId > 0 ? QVariant(folderId) : QVariant();
}

QString normalizedFolderName(const QString& name)
{
    return name.trimmed();
}
}

FolderService::FolderService(QObject* parent) : QObject(parent)
{
}

bool FolderService::createFolder(int ownerId, int parentId, const QString& name, int* outFolderId, const QString& localPath)
{
    const QString folderName = normalizedFolderName(name);
    if (ownerId <= 0 || folderName.isEmpty()) {
        return false;
    }

    if (parentId > 0 && !folderExists(ownerId, parentId)) {
        return false;
    }

    if (hasDuplicateName(ownerId, parentId, folderName)) {
        return false;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO folders (owner_id, parent_id, name, local_path, created_at, updated_at) "
                  "VALUES (:owner_id, :parent_id, :name, :local_path, datetime('now'), datetime('now'))");
    query.bindValue(":owner_id", ownerId);
    query.bindValue(":parent_id", nullableFolderId(parentId));
    query.bindValue(":name", folderName);
    query.bindValue(":local_path", localPath.trimmed());

    if (!query.exec()) {
        qDebug() << "Create folder failed:" << query.lastError().text();
        return false;
    }

    if (outFolderId) {
        *outFolderId = query.lastInsertId().toInt();
    }
    return true;
}

QList<FolderInfo> FolderService::listFolders(int ownerId, int parentId) const
{
    QList<FolderInfo> folders;
    if (ownerId <= 0) {
        return folders;
    }

    QSqlQuery query;
    const QString parentFilter = parentId > 0 ? "parent_id = :parent_id" : "parent_id IS NULL";
    query.prepare(QString("SELECT id, owner_id, parent_id, name, local_path, description, is_trashed, created_at, updated_at "
                          "FROM folders "
                          "WHERE owner_id = :owner_id AND %1 AND is_trashed = 0 "
                          "ORDER BY name COLLATE NOCASE ASC").arg(parentFilter));
    query.bindValue(":owner_id", ownerId);
    if (parentId > 0) {
        query.bindValue(":parent_id", parentId);
    }

    if (!query.exec()) {
        qDebug() << "List folders failed:" << query.lastError().text();
        return folders;
    }

    while (query.next()) {
        folders.append(readFolderInfoFromQuery(query));
    }
    return folders;
}

bool FolderService::renameFolder(int ownerId, int folderId, const QString& newName)
{
    const QString folderName = normalizedFolderName(newName);
    if (ownerId <= 0 || folderId <= 0 || folderName.isEmpty()) {
        return false;
    }

    QSqlQuery parentQuery;
    parentQuery.prepare("SELECT parent_id FROM folders WHERE id = :id AND owner_id = :owner_id AND is_trashed = 0");
    parentQuery.bindValue(":id", folderId);
    parentQuery.bindValue(":owner_id", ownerId);
    if (!parentQuery.exec() || !parentQuery.next()) {
        return false;
    }

    const int parentId = parentQuery.value("parent_id").isNull() ? -1 : parentQuery.value("parent_id").toInt();
    if (hasDuplicateName(ownerId, parentId, folderName, folderId)) {
        return false;
    }

    QSqlQuery query;
    query.prepare("UPDATE folders SET name = :name, updated_at = datetime('now') "
                  "WHERE id = :id AND owner_id = :owner_id AND is_trashed = 0");
    query.bindValue(":name", folderName);
    query.bindValue(":id", folderId);
    query.bindValue(":owner_id", ownerId);
    if (!query.exec()) {
        qDebug() << "Rename folder failed:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool FolderService::trashFolder(int ownerId, int folderId)
{
    if (ownerId <= 0 || folderId <= 0) {
        return false;
    }

    QSqlQuery query;
    query.prepare("UPDATE folders SET is_trashed = 1, updated_at = datetime('now') "
                  "WHERE id = :id AND owner_id = :owner_id AND is_trashed = 0");
    query.bindValue(":id", folderId);
    query.bindValue(":owner_id", ownerId);
    if (!query.exec()) {
        qDebug() << "Trash folder failed:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool FolderService::moveFolder(int ownerId, int folderId, int newParentId)
{
    if (ownerId <= 0 || folderId <= 0 || folderId == newParentId) {
        return false;
    }

    if (!folderExists(ownerId, folderId)) {
        return false;
    }

    if (newParentId > 0 && !folderExists(ownerId, newParentId)) {
        return false;
    }

    if (newParentId > 0 && isDescendantFolder(ownerId, folderId, newParentId)) {
        return false;
    }

    QSqlQuery nameQuery;
    nameQuery.prepare("SELECT name FROM folders WHERE id = :id AND owner_id = :owner_id AND is_trashed = 0");
    nameQuery.bindValue(":id", folderId);
    nameQuery.bindValue(":owner_id", ownerId);
    if (!nameQuery.exec() || !nameQuery.next()) {
        return false;
    }

    if (hasDuplicateName(ownerId, newParentId, nameQuery.value("name").toString(), folderId)) {
        return false;
    }

    QSqlQuery query;
    query.prepare("UPDATE folders SET parent_id = :parent_id, updated_at = datetime('now') "
                  "WHERE id = :id AND owner_id = :owner_id AND is_trashed = 0");
    query.bindValue(":parent_id", nullableFolderId(newParentId));
    query.bindValue(":id", folderId);
    query.bindValue(":owner_id", ownerId);
    if (!query.exec()) {
        qDebug() << "Move folder failed:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

QList<FolderInfo> FolderService::folderPath(int ownerId, int folderId) const
{
    QList<FolderInfo> path;
    int currentId = folderId;
    while (ownerId > 0 && currentId > 0) {
        QSqlQuery query;
        query.prepare("SELECT id, owner_id, parent_id, name, local_path, description, is_trashed, created_at, updated_at "
                      "FROM folders WHERE id = :id AND owner_id = :owner_id AND is_trashed = 0");
        query.bindValue(":id", currentId);
        query.bindValue(":owner_id", ownerId);
        if (!query.exec() || !query.next()) {
            path.clear();
            return path;
        }

        const FolderInfo info = readFolderInfoFromQuery(query);
        path.prepend(info);
        currentId = info.parentId;
    }
    return path;
}

bool FolderService::folderExists(int ownerId, int folderId) const
{
    QSqlQuery query;
    query.prepare("SELECT 1 FROM folders WHERE id = :id AND owner_id = :owner_id AND is_trashed = 0");
    query.bindValue(":id", folderId);
    query.bindValue(":owner_id", ownerId);
    return query.exec() && query.next();
}

bool FolderService::hasDuplicateName(int ownerId, int parentId, const QString& name, int excludeFolderId) const
{
    QSqlQuery query;
    const QString parentFilter = parentId > 0 ? "parent_id = :parent_id" : "parent_id IS NULL";
    query.prepare(QString("SELECT 1 FROM folders "
                          "WHERE owner_id = :owner_id AND %1 AND name = :name AND is_trashed = 0 "
                          "AND (:exclude_id <= 0 OR id != :exclude_id)").arg(parentFilter));
    query.bindValue(":owner_id", ownerId);
    query.bindValue(":name", name);
    query.bindValue(":exclude_id", excludeFolderId);
    if (parentId > 0) {
        query.bindValue(":parent_id", parentId);
    }
    return query.exec() && query.next();
}

bool FolderService::isDescendantFolder(int ownerId, int folderId, int possibleChildId) const
{
    int currentId = possibleChildId;
    while (currentId > 0) {
        if (currentId == folderId) {
            return true;
        }

        QSqlQuery query;
        query.prepare("SELECT parent_id FROM folders WHERE id = :id AND owner_id = :owner_id AND is_trashed = 0");
        query.bindValue(":id", currentId);
        query.bindValue(":owner_id", ownerId);
        if (!query.exec() || !query.next()) {
            return false;
        }
        currentId = query.value("parent_id").isNull() ? -1 : query.value("parent_id").toInt();
    }
    return false;
}

FolderInfo FolderService::readFolderInfoFromQuery(const QSqlQuery& query) const
{
    FolderInfo info;
    info.folderId = query.value("id").toInt();
    info.ownerId = query.value("owner_id").toInt();
    info.parentId = query.value("parent_id").isNull() ? -1 : query.value("parent_id").toInt();
    info.name = query.value("name").toString();
    info.localPath = query.value("local_path").toString();
    info.description = query.value("description").toString();
    info.isTrashed = query.value("is_trashed").toBool();
    info.createdAt = QDateTime::fromString(query.value("created_at").toString(), Qt::ISODate);
    info.updatedAt = QDateTime::fromString(query.value("updated_at").toString(), Qt::ISODate);
    return info;
}
