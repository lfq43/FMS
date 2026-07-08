#ifndef SECURITYSERVICE_H
#define SECURITYSERVICE_H

#include <QObject>
#include <QString>

class SecurityService : public QObject
{
    Q_OBJECT

public:
    explicit SecurityService(QObject* parent = nullptr);

    bool hasFilePassword(int fileId) const;
    bool hasFolderPassword(int folderId) const;
    bool verifyFilePassword(int fileId, const QString& password) const;
    bool verifyFolderPassword(int folderId, const QString& password) const;
    bool setFilePassword(int fileId, const QString& password) const;
    bool setFolderPassword(int folderId, const QString& password) const;
    bool clearFilePassword(int fileId) const;
    bool clearFolderPassword(int folderId) const;

private:
    bool hasPassword(const QString& resourceType, int resourceId) const;
    bool verifyPassword(const QString& resourceType, int resourceId, const QString& password) const;
    bool setPassword(const QString& resourceType, int resourceId, const QString& password) const;
    bool clearPassword(const QString& resourceType, int resourceId) const;
};

#endif // SECURITYSERVICE_H
