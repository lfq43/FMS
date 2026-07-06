#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QString>
#include <QSqlDatabase>
class DatabaseManager
{
public:
    static DatabaseManager& instance();

    bool openDatabase(const QString& dbName = "app.db");
    void closeDatabase();
    bool isOpen() const;

    bool executeQuery(const QString& sql);
    QSqlQuery executeSelect(const QString& sql);

    QString lastError() const;

private:
    DatabaseManager();
    ~DatabaseManager();

    bool initializeDatabase(const QString& dbName);
    bool executeSqlScript(const QString& sqlScript);

    QSqlDatabase m_db;
    QString m_lastError;
};

#endif // DATABASEMANAGER_H
