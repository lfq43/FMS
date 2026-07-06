#include "FileService.h"
#include <QDebug>

FileService::FileService(QObject *parent) : QObject(parent) {
    qDebug() << "FileService 创建完成";
}

FileService::~FileService() {
    qDebug() << "FileService 销毁";
}

bool FileService::addlocalFile(const QString &filePath) {
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        qDebug() << "文件不存在或不是有效的文件:" << filePath;
        return false;
    }

    QFileInfo info(filePath);
    LocalFileInfo localFile;
    localFile.fileName = info.fileName();  
    localFile.filePath = info.absoluteFilePath();
    localFile.fileSize = info.size();
    localFile.fileType = info.suffix();
    localFile.addedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    localFile.uploadStatus = "pending"; // 初始状态为待上传

    QString insertQuery = QString("INSERT INTO local_files (file_name, file_path, file_size, file_type, added_at, upload_status) "
                                "VALUES ('%1', '%2', %3, '%4', '%5', '%6')")
                                .arg(localFile.fileName)
                                .arg(localFile.filePath)
                                .arg(localFile.fileSize)
                                .arg(localFile.fileType)
                                .arg(localFile.addedAt)
                                .arg(localFile.uploadStatus);

    qDebug() << "添加本地文件:" << filePath;
    return true;
}

bool FileService::removeFile(int fileId) {
    QString deleteQuery = QString("DELETE FROM local_files WHERE id = %1").arg(fileId);
    qDebug() << "删除文件ID:" << fileId;
    return true;
}

bool FileService::updateFileStatus(int fileId, const QString &status) {
    QString updateQuery = QString("UPDATE local_files SET upload_status = '%1' WHERE id = %2").arg(status).arg(fileId);
    qDebug() << "更新文件状态 ID:" << fileId << "状态:" << status;
    return true;
}
