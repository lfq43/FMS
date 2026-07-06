#ifndef FILEMANAGERSERVICE_H
#define FILEMANAGERSERVICE_H

#include <QObject>

class FileManagerService : public QObject
{
    Q_OBJECT
public:
    FileManagerService();

    bool createDirectory(const QString &path);
    bool removeDirectory(const QString &path);
    bool renameDirectory(const QString &oldPath, const QString &newPath);
    bool moveDirectory(const QString &sourcePath, const QString &destinationPath);
};

#endif // FILEMANAGERSERVICE_H
