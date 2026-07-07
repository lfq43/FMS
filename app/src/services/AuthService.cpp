#include "AuthService.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QSettings>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
constexpr const char* kSettingsOrg = "FMS";
constexpr const char* kSettingsApp = "FMS";
constexpr const char* kRememberLoginKey = "auth/rememberLogin";
constexpr const char* kRememberUserIdKey = "auth/rememberUserId";
constexpr const char* kRememberUsernameKey = "auth/rememberUsername";

QSettings authSettings()
{
    return QSettings(kSettingsOrg, kSettingsApp);
}
}

int AuthService::s_currentUserId = -1;
QString AuthService::s_currentUsername;
QString AuthService::s_currentDisplayName;
qint64 AuthService::s_currentStorageQuotaBytes = 0;
qint64 AuthService::s_currentStorageUsedBytes = 0;
QString AuthService::s_currentLastLoginAt;

AuthService::AuthService(QObject *parent) : QObject(parent) {}

QString AuthService::hashPassword(const QString& password)
{
    const QByteArray hashed = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    return QString(hashed.toHex());
}

bool AuthService::login(const QString& username, const QString& password)
{
    QSqlQuery query;
    query.prepare("SELECT id, username, display_name, storage_quota_bytes, storage_used_bytes, last_login_at "
                  "FROM users "
                  "WHERE username = :username AND password_hash = :password_hash AND status = 'active'");
    query.bindValue(":username", username);
    query.bindValue(":password_hash", hashPassword(password));

    if (!query.exec()) {
        qDebug() << "登录查询失败:" << query.lastError().text();
        return false;
    }

    if (!query.next()) {
        logout();
        qDebug() << "登录失败:" << username;
        return false;
    }

    s_currentUserId = query.value("id").toInt();
    s_currentUsername = query.value("username").toString();
    s_currentDisplayName = query.value("display_name").toString();
    s_currentStorageQuotaBytes = query.value("storage_quota_bytes").toLongLong();
    s_currentStorageUsedBytes = query.value("storage_used_bytes").toLongLong();
    s_currentLastLoginAt = query.value("last_login_at").toString();

    QSqlQuery updateQuery;
    updateQuery.prepare("UPDATE users SET last_login_at = datetime('now'), updated_at = datetime('now') WHERE id = :id");
    updateQuery.bindValue(":id", s_currentUserId);
    if (!updateQuery.exec()) {
        qDebug() << "更新最后登录时间失败:" << updateQuery.lastError().text();
    }

    qDebug() << "登录成功:" << username;
    return true;
}

bool AuthService::registerUser(const QString& username, const QString& password)
{
    QSqlQuery checkQuery;
    checkQuery.prepare("SELECT EXISTS(SELECT 1 FROM users WHERE username = :username)");
    checkQuery.bindValue(":username", username);
    if (!checkQuery.exec()) {
        qDebug() << "注册检查失败:" << checkQuery.lastError().text();
        return false;
    }

    if (checkQuery.next() && checkQuery.value(0).toBool()) {
        qDebug() << "注册失败: 用户名已存在:" << username;
        return false;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO users (username, password_hash, display_name, created_at, updated_at) "
                  "VALUES (:username, :password_hash, :display_name, datetime('now'), datetime('now'))");
    query.bindValue(":username", username);
    query.bindValue(":password_hash", hashPassword(password));
    query.bindValue(":display_name", username);

    if (!query.exec()) {
        qDebug() << "注册失败:" << username << ", 错误:" << query.lastError().text();
        return false;
    }

    qDebug() << "注册成功:" << username;
    return true;
}

bool AuthService::restoreRememberedLogin()
{
    QSettings settings = authSettings();
    if (!settings.value(kRememberLoginKey, false).toBool()) {
        return false;
    }

    const int userId = settings.value(kRememberUserIdKey, -1).toInt();
    if (userId <= 0) {
        return false;
    }

    QSqlQuery query;
    query.prepare("SELECT id, username, display_name, storage_quota_bytes, storage_used_bytes, last_login_at "
                  "FROM users "
                  "WHERE id = :id AND status = 'active'");
    query.bindValue(":id", userId);

    if (!query.exec()) {
        qDebug() << "恢复登录状态失败:" << query.lastError().text();
        return false;
    }

    if (!query.next()) {
        rememberCurrentLogin(false);
        logout();
        return false;
    }

    s_currentUserId = query.value("id").toInt();
    s_currentUsername = query.value("username").toString();
    s_currentDisplayName = query.value("display_name").toString();
    s_currentStorageQuotaBytes = query.value("storage_quota_bytes").toLongLong();
    s_currentStorageUsedBytes = query.value("storage_used_bytes").toLongLong();
    s_currentLastLoginAt = query.value("last_login_at").toString();
    qDebug() << "已恢复登录状态:" << s_currentUsername;
    return true;
}

void AuthService::rememberCurrentLogin(bool remember)
{
    QSettings settings = authSettings();
    settings.setValue(kRememberLoginKey, remember);
    if (remember && isLoggedIn()) {
        settings.setValue(kRememberUserIdKey, s_currentUserId);
        settings.setValue(kRememberUsernameKey, s_currentUsername);
    }
    else {
        settings.remove(kRememberUserIdKey);
        settings.remove(kRememberUsernameKey);
    }
    settings.sync();
}

bool AuthService::rememberedLoginEnabled()
{
    return authSettings().value(kRememberLoginKey, false).toBool();
}

QString AuthService::rememberedUsername()
{
    return authSettings().value(kRememberUsernameKey).toString();
}

void AuthService::logout()
{
    rememberCurrentLogin(false);
    s_currentUserId = -1;
    s_currentUsername.clear();
    s_currentDisplayName.clear();
    s_currentStorageQuotaBytes = 0;
    s_currentStorageUsedBytes = 0;
    s_currentLastLoginAt.clear();
}

bool AuthService::isLoggedIn()
{
    return s_currentUserId > 0;
}

int AuthService::currentUserId()
{
    return s_currentUserId;
}

QString AuthService::currentUsername()
{
    return s_currentUsername;
}

QString AuthService::currentDisplayName()
{
    return s_currentDisplayName.isEmpty() ? s_currentUsername : s_currentDisplayName;
}

qint64 AuthService::currentStorageQuotaBytes()
{
    return s_currentStorageQuotaBytes;
}

qint64 AuthService::currentStorageUsedBytes()
{
    return s_currentStorageUsedBytes;
}

QString AuthService::currentLastLoginAt()
{
    return s_currentLastLoginAt;
}
