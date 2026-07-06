#ifndef FILE_SERVICE_H
#define FILE_SERVICE_H

#include <QObject>
#include <QFile>
#include <QFileInfo>
#include <QString>

struct LocalFileInfo {
    int id;
    QString fileName;
    QString filePath;
    qint64 fileSize;
    QString fileType;
    QString addedAt;
    QString uploadStatus;
};

class FileService : public QObject {
    Q_OBJECT

public:
    explicit FileService(QObject *parent = nullptr);
    ~FileService();

    bool addlocalFile(const QString &filePath);
    bool removeFile(int fileId);
    bool updateFileStatus(int fileId, const QString &status);

    QList<LocalFileInfo> getAllFiles() const;
};

#endif // FILE_SERVICE_H
