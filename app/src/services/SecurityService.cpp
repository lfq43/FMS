#include "SecurityService.h"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>

namespace {
constexpr const char* kFileResource = "file";
constexpr const char* kFolderResource = "folder";

QString makeSalt()
{
    QByteArray bytes;
    bytes.resize(16);
    QRandomGenerator* generator = QRandomGenerator::global();
    for (int i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<char>(generator->bounded(256));
    }
    return QString::fromLatin1(bytes.toHex());
}

QString hashPassword(const QString& password, const QString& salt)
{
    const QByteArray payload = salt.toUtf8() + ":" + password.toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}
}

SecurityService::SecurityService(QObject* parent)
    : QObject(parent)
{
}

bool SecurityService::hasFilePassword(int fileId) const
{
    return hasPassword(kFileResource, fileId);
}

bool SecurityService::hasFolderPassword(int folderId) const
{
    return hasPassword(kFolderResource, folderId);
}

bool SecurityService::verifyFilePassword(int fileId, const QString& password) const
{
    return verifyPassword(kFileResource, fileId, password);
}

bool SecurityService::verifyFolderPassword(int folderId, const QString& password) const
{
    return verifyPassword(kFolderResource, folderId, password);
}

bool SecurityService::setFilePassword(int fileId, const QString& password) const
{
    return setPassword(kFileResource, fileId, password);
}

bool SecurityService::setFolderPassword(int folderId, const QString& password) const
{
    return setPassword(kFolderResource, folderId, password);
}

bool SecurityService::clearFilePassword(int fileId) const
{
    return clearPassword(kFileResource, fileId);
}

bool SecurityService::clearFolderPassword(int folderId) const
{
    return clearPassword(kFolderResource, folderId);
}

bool SecurityService::hasPassword(const QString& resourceType, int resourceId) const
{
    if (resourceId <= 0) {
        return false;
    }

    QSqlQuery query;
    query.prepare("SELECT 1 FROM resource_passwords "
                  "WHERE resource_type = :resource_type AND resource_id = :resource_id LIMIT 1");
    query.bindValue(":resource_type", resourceType);
    query.bindValue(":resource_id", resourceId);
    return query.exec() && query.next();
}

bool SecurityService::verifyPassword(const QString& resourceType, int resourceId, const QString& password) const
{
    if (resourceId <= 0) {
        return false;
    }

    QSqlQuery query;
    query.prepare("SELECT password_hash, password_salt FROM resource_passwords "
                  "WHERE resource_type = :resource_type AND resource_id = :resource_id");
    query.bindValue(":resource_type", resourceType);
    query.bindValue(":resource_id", resourceId);
    if (!query.exec() || !query.next()) {
        return true;
    }

    const QString expectedHash = query.value("password_hash").toString();
    const QString salt = query.value("password_salt").toString();
    return expectedHash == hashPassword(password, salt);
}

bool SecurityService::setPassword(const QString& resourceType, int resourceId, const QString& password) const
{
    const QString trimmedPassword = password.trimmed();
    if (resourceId <= 0 || trimmedPassword.isEmpty()) {
        return false;
    }

    const QString salt = makeSalt();
    QSqlQuery query;
    query.prepare("INSERT INTO resource_passwords "
                  "(resource_type, resource_id, password_hash, password_salt, updated_at) "
                  "VALUES (:resource_type, :resource_id, :password_hash, :password_salt, datetime('now')) "
                  "ON CONFLICT(resource_type, resource_id) DO UPDATE SET "
                  "password_hash = excluded.password_hash, "
                  "password_salt = excluded.password_salt, "
                  "updated_at = datetime('now')");
    query.bindValue(":resource_type", resourceType);
    query.bindValue(":resource_id", resourceId);
    query.bindValue(":password_hash", hashPassword(trimmedPassword, salt));
    query.bindValue(":password_salt", salt);
    if (!query.exec()) {
        qDebug() << "Set resource password failed:" << query.lastError().text();
        return false;
    }
    return true;
}

bool SecurityService::clearPassword(const QString& resourceType, int resourceId) const
{
    if (resourceId <= 0) {
        return false;
    }

    QSqlQuery query;
    query.prepare("DELETE FROM resource_passwords WHERE resource_type = :resource_type AND resource_id = :resource_id");
    query.bindValue(":resource_type", resourceType);
    query.bindValue(":resource_id", resourceId);
    if (!query.exec()) {
        qDebug() << "Clear resource password failed:" << query.lastError().text();
        return false;
    }
    return true;
}
