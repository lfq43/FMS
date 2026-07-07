#ifndef AUTHSERVICE_H
#define AUTHSERVICE_H

#include <QObject>
#include <QString>

class AuthService : public QObject
{
    Q_OBJECT
public:
    explicit AuthService(QObject *parent = nullptr);

    bool login(const QString& username, const QString& password);
    bool registerUser(const QString& username, const QString& password);

    static bool restoreRememberedLogin();
    static void rememberCurrentLogin(bool remember);
    static bool rememberedLoginEnabled();
    static QString rememberedUsername();
    static void logout();
    static bool isLoggedIn();
    static int currentUserId();
    static QString currentUsername();
    static QString currentDisplayName();
    static qint64 currentStorageQuotaBytes();
    static qint64 currentStorageUsedBytes();
    static QString currentLastLoginAt();

private:
    QString hashPassword(const QString& password);

    static int s_currentUserId;
    static QString s_currentUsername;
    static QString s_currentDisplayName;
    static qint64 s_currentStorageQuotaBytes;
    static qint64 s_currentStorageUsedBytes;
    static QString s_currentLastLoginAt;
};

#endif // AUTHSERVICE_H
