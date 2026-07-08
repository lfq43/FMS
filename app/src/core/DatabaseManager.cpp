#include "DatabaseManager.h"

#include <QDebug>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

// 返回全局唯一的数据库管理器，确保整个程序复用同一个 SQLite 连接。
DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager instance;
    return instance;
}

// 构造时注册 Qt SQLite 驱动，真正的数据库文件路径在 openDatabase 中设置。
DatabaseManager::DatabaseManager() {
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    qDebug() << "DatabaseManager created";
}

// 析构时关闭数据库连接，避免应用退出时连接资源泄漏。
DatabaseManager::~DatabaseManager() {
    if (m_db.isOpen()) {
        m_db.close();
    }
}

// 打开指定 SQLite 文件，并在连接成功后执行 schema.sql 初始化表结构。
bool DatabaseManager::openDatabase(const QString& dbName) {
    m_db.setDatabaseName(dbName);
    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        qDebug() << "Failed to open database:" << m_lastError;
        return false;
    }

    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA foreign_keys = ON");

    qDebug() << "Database opened:" << dbName;
    return initializeDatabase(dbName);
}

// 主动关闭当前数据库连接，通常在程序退出或切换数据库文件时使用。
void DatabaseManager::closeDatabase() {
    if (m_db.isOpen()) {
        m_db.close();
        qDebug() << "Database closed";
    }
}

// 判断当前数据库连接是否处于打开状态。
bool DatabaseManager::isOpen() const {
    return m_db.isOpen();
}

// 从 Qt 资源系统读取 database/schema.sql，并执行其中的建表和初始化语句。
bool DatabaseManager::initializeDatabase(const QString& dbName) {
    Q_UNUSED(dbName);

    if (!m_db.isOpen()) {
        m_lastError = QStringLiteral("Database is not open");
        return false;
    }

    QFile schemaFile(":/database/schema.sql");
    if (!schemaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = QStringLiteral("Cannot read :/database/schema.sql");
        qDebug() << m_lastError;
        return false;
    }

    const QString schemaSql = QString::fromUtf8(schemaFile.readAll());
    if (!executeSqlScript(schemaSql)) {
        return false;
    }

    QSqlQuery columnQuery(m_db);
    bool hasOriginalPath = false;
    if (columnQuery.exec("PRAGMA table_info(files)")) {
        while (columnQuery.next()) {
            if (columnQuery.value("name").toString() == "original_path") {
                hasOriginalPath = true;
                break;
            }
        }
    }
    if (!hasOriginalPath && !executeQuery("ALTER TABLE files ADD COLUMN original_path TEXT")) {
        return false;
    }
    if (!executeQuery("UPDATE files "
                      "SET original_path = (SELECT storage_path FROM file_versions WHERE id = files.current_version_id) "
                      "WHERE original_path IS NULL OR original_path = ''")) {
        return false;
    }

    QSqlQuery folderColumnQuery(m_db);
    bool hasFolderLocalPath = false;
    if (folderColumnQuery.exec("PRAGMA table_info(folders)")) {
        while (folderColumnQuery.next()) {
            if (folderColumnQuery.value("name").toString() == "local_path") {
                hasFolderLocalPath = true;
                break;
            }
        }
    }
    if (!hasFolderLocalPath && !executeQuery("ALTER TABLE folders ADD COLUMN local_path TEXT")) {
        return false;
    }

    const QString accessGrantTableSql =
        "CREATE TABLE IF NOT EXISTS access_grants ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "resource_type TEXT NOT NULL CHECK (resource_type IN ('file', 'folder')),"
                      "resource_id INTEGER NOT NULL,"
                      "grantee_user_id INTEGER NOT NULL,"
                      "granted_by_user_id INTEGER NOT NULL,"
                      "access_level TEXT NOT NULL DEFAULT 'view' CHECK (access_level IN ('view', 'edit', 'share')),"
                      "inherit_to_children INTEGER NOT NULL DEFAULT 1 CHECK (inherit_to_children IN (0, 1)),"
                      "created_at TEXT NOT NULL DEFAULT (datetime('now')),"
                      "updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
                      "expires_at TEXT,"
                      "UNIQUE (resource_type, resource_id, grantee_user_id),"
                      "FOREIGN KEY (grantee_user_id) REFERENCES users(id) ON DELETE CASCADE,"
                      "FOREIGN KEY (granted_by_user_id) REFERENCES users(id) ON DELETE RESTRICT"
                      ")";
    if (!executeQuery(accessGrantTableSql)) {
        return false;
    }

    QSqlQuery accessGrantSchema(m_db);
    accessGrantSchema.prepare("SELECT sql FROM sqlite_master WHERE type = 'table' AND name = 'access_grants'");
    if (!accessGrantSchema.exec()) {
        m_lastError = accessGrantSchema.lastError().text();
        return false;
    }
    if (accessGrantSchema.next() && !accessGrantSchema.value(0).toString().contains("'edit'")) {
        if (!m_db.transaction()) {
            m_lastError = m_db.lastError().text();
            return false;
        }

        QSqlQuery migrationQuery(m_db);
        const QStringList statements = {
            "ALTER TABLE access_grants RENAME TO access_grants_old",
            accessGrantTableSql,
            "INSERT INTO access_grants "
            "(id, resource_type, resource_id, grantee_user_id, granted_by_user_id, access_level, inherit_to_children, created_at, updated_at, expires_at) "
            "SELECT id, resource_type, resource_id, grantee_user_id, granted_by_user_id, access_level, inherit_to_children, created_at, updated_at, expires_at "
            "FROM access_grants_old",
            "DROP TABLE access_grants_old"
        };

        for (const QString& statement : statements) {
            if (!migrationQuery.exec(statement)) {
                m_lastError = migrationQuery.lastError().text();
                qDebug() << "Migrate access grants failed:" << m_lastError << statement;
                m_db.rollback();
                return false;
            }
        }

        if (!m_db.commit()) {
            m_lastError = m_db.lastError().text();
            m_db.rollback();
            return false;
        }
    }
    if (!executeQuery("CREATE INDEX IF NOT EXISTS idx_access_grants_grantee "
                      "ON access_grants(grantee_user_id, resource_type, access_level)")) {
        return false;
    }
    if (!executeQuery("CREATE INDEX IF NOT EXISTS idx_access_grants_resource "
                      "ON access_grants(resource_type, resource_id)")) {
        return false;
    }
    if (!executeQuery("CREATE TABLE IF NOT EXISTS resource_passwords ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "resource_type TEXT NOT NULL CHECK (resource_type IN ('file', 'folder')),"
                      "resource_id INTEGER NOT NULL,"
                      "password_hash TEXT NOT NULL,"
                      "password_salt TEXT NOT NULL,"
                      "created_at TEXT NOT NULL DEFAULT (datetime('now')),"
                      "updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
                      "UNIQUE (resource_type, resource_id))")) {
        return false;
    }
    if (!executeQuery("CREATE INDEX IF NOT EXISTS idx_resource_passwords_resource "
                      "ON resource_passwords(resource_type, resource_id)")) {
        return false;
    }

    qDebug() << "Database initialized";
    return true;
}

// 执行一整段 SQL 脚本，能正确跳过注释，并避免把字符串内部的分号拆成多条语句。
bool DatabaseManager::executeSqlScript(const QString& sqlScript) {
    QString currentStatement;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool inLineComment = false;
    bool inBlockComment = false;

    for (int i = 0; i < sqlScript.size(); ++i) {
        const QChar ch = sqlScript.at(i);
        const QChar next = (i + 1 < sqlScript.size()) ? sqlScript.at(i + 1) : QChar();

        if (inLineComment) {
            if (ch == '\n') {
                inLineComment = false;
            }
            continue;
        }

        if (inBlockComment) {
            if (ch == '*' && next == '/') {
                inBlockComment = false;
                ++i;
            }
            continue;
        }

        if (!inSingleQuote && !inDoubleQuote && ch == '-' && next == '-') {
            inLineComment = true;
            ++i;
            continue;
        }

        if (!inSingleQuote && !inDoubleQuote && ch == '/' && next == '*') {
            inBlockComment = true;
            ++i;
            continue;
        }

        if (ch == '\'' && !inDoubleQuote) {
            currentStatement += ch;
            if (inSingleQuote && next == '\'') {
                currentStatement += next;
                ++i;
            } else {
                inSingleQuote = !inSingleQuote;
            }
            continue;
        }

        if (ch == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            currentStatement += ch;
            continue;
        }

        if (ch == ';' && !inSingleQuote && !inDoubleQuote) {
            const QString statement = currentStatement.trimmed();
            currentStatement.clear();
            if (!statement.isEmpty() && !executeQuery(statement)) {
                return false;
            }
            continue;
        }

        currentStatement += ch;
    }

    const QString statement = currentStatement.trimmed();
    return statement.isEmpty() || executeQuery(statement);
}

// 执行不返回结果集的 SQL，例如建表、插入、更新和删除。
bool DatabaseManager::executeQuery(const QString& sql) {
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        qDebug() << "SQL failed:" << m_lastError << sql;
        return false;
    }
    return true;
}

// 执行查询 SQL 并返回 QSqlQuery，调用方通过 query.next() 读取结果。
QSqlQuery DatabaseManager::executeSelect(const QString& sql) {
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        qDebug() << "Select failed:" << m_lastError << sql;
    }
    return query;
}

// 返回最近一次数据库操作失败时记录的错误文本。
QString DatabaseManager::lastError() const {
    return m_lastError;
}
