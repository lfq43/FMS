#include "AuthService.h"
#include <QCryptographicHash>
#include "core/DatabaseManager.h"
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>

// 认证服务目前负责用户注册和登录校验，所有账号数据都存放在 users 表。
AuthService::AuthService(QObject *parent) : QObject(parent) {}

// 对明文密码做 SHA-256 哈希，数据库只保存哈希值，不直接保存明文密码。
QString AuthService::hashPassword(const QString& password) {
    QByteArray hashed = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    return QString(hashed.toHex());
}

// 根据用户名和密码哈希查询 users 表，存在匹配记录则认为登录成功。
bool AuthService::login(const QString& username, const QString& password) {
    QString hashedPassword = hashPassword(password);
    QString queryStr = QString("SELECT * FROM users WHERE username='%1' AND password_hash='%2'")
                       .arg(username).arg(hashedPassword);

    QSqlQuery query = DatabaseManager::instance().executeSelect(queryStr);
    if (query.next()) {
        qDebug() << "登录成功:" << username;
        return true;
    } else {
        qDebug() << "登录失败:" << username << ", 错误:" << query.lastError().text();
        return false;
    }
}

// 创建新用户记录；后续可在这里同步写入 user_roles，给新用户分配默认角色。
bool AuthService::registerUser(const QString& username, const QString& password) {
    QString hashedPassword = hashPassword(password);
    QString queryStr = QString("INSERT INTO users (username, password_hash, created_at) VALUES ('%1', '%2', datetime('now'))")
                       .arg(username).arg(hashedPassword);

    if (DatabaseManager::instance().executeQuery(queryStr)) {
        qDebug() << "注册成功:" << username;
        return true;
    } else {
        qDebug() << "注册失败:" << username << ", 错误:" << DatabaseManager::instance().lastError();
        return false;
    }
}

