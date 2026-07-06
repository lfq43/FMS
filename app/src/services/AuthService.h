#ifndef AUTHSERVICE_H
#define AUTHSERVICE_H

#include <QObject>

class AuthService : public QObject
{
    Q_OBJECT
public:
    AuthService(QObject *parent = nullptr);

    bool login(const QString& username, const QString& password);
    bool registerUser(const QString& username, const QString& password);
private:
    QString hashPassword(const QString& password);
};

#endif // AUTHSERVICE_H
