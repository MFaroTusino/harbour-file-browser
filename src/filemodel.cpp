#include "filemodel.h"
#include <QDateTime>
#include "globals.h"
#include <QDebug>

enum {
    FilenameRole = Qt::UserRole + 1,
    FileKindRole = Qt::UserRole + 2,
    FileIconRole = Qt::UserRole + 3,
    PermissionsRole = Qt::UserRole + 4,
    SizeRole = Qt::UserRole + 5,
    LastModifiedRole = Qt::UserRole + 6,
    CreatedRole = Qt::UserRole + 7
};

FileModel::FileModel(QObject *parent) :
    QAbstractListModel(parent),
    m_active(false),
    m_dirty(false)
{
    m_dir = "";
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, SIGNAL(directoryChanged(const QString&)), this, SLOT(refresh()));
    connect(m_watcher, SIGNAL(fileChanged(const QString&)), this, SLOT(refresh()));
}

FileModel::~FileModel()
{
}

int FileModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_files.count();
}

QVariant FileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() > m_files.size()-1)
        return QVariant();

    QFileInfo info = m_files.at(index.row()).info;
    switch (role) {

    case Qt::DisplayRole:
    case FilenameRole:
        return info.fileName();

    case FileKindRole:
        if (info.isDir()) return "d";
        if (info.isSymLink()) return "l";
        if (info.isFile()) return "-";
        return "?";

    case FileIconRole:
        if (info.isDir()) return "folder";
        if (info.isSymLink()) return "link";
        if (info.isFile()) {
            QString suffix = info.suffix().toLower();
            return suffixToIconName(suffix);
        }
        return "file";

    case PermissionsRole:
        return permissionsToString(info.permissions());

    case SizeRole:
        if (info.isDir()) return "";
        return filesizeToString(info.size());

    case LastModifiedRole: {
        return datetimeToString(info.lastModified());
    }

    case CreatedRole: {
        return datetimeToString(info.created());
    }

    default:
        return QVariant();
    }
}

QHash<int, QByteArray> FileModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
    roles.insert(FilenameRole, QByteArray("filename"));
    roles.insert(FileKindRole, QByteArray("filekind"));
    roles.insert(FileIconRole, QByteArray("fileIcon"));
    roles.insert(PermissionsRole, QByteArray("permissions"));
    roles.insert(SizeRole, QByteArray("size"));
    roles.insert(LastModifiedRole, QByteArray("modified"));
    roles.insert(CreatedRole, QByteArray("created"));
    return roles;
}

int FileModel::fileCount() const
{
    return m_files.count();
}

QString FileModel::errorMessage() const
{
    return m_errorMessage;
}

void FileModel::setDir(QString dir)
{
    if (m_dir == dir)
        return;

    // update watcher to watch the new directory
    if (!m_dir.isEmpty())
        m_watcher->removePath(m_dir);
    if (!dir.isEmpty())
        m_watcher->addPath(dir);

    m_dir = dir;

    readDirectory();
    m_dirty = false;

    emit dirChanged();
}

QString FileModel::appendPath(QString dirName)
{
    return QDir::cleanPath(QDir(m_dir).absoluteFilePath(dirName));
}

void FileModel::setActive(bool active)
{
    if (m_active == active)
        return;

    m_active = active;
    emit activeChanged();

    if (m_dirty)
        readDirectory();

    m_dirty = false;
}

QString FileModel::parentPath()
{
    return QDir::cleanPath(QDir(m_dir).absoluteFilePath(".."));
}

QString FileModel::fileNameAt(int fileIndex)
{
    if (fileIndex < 0 || fileIndex >= m_files.count())
        return QString();

    return m_files.at(fileIndex).info.absoluteFilePath();
}

void FileModel::refresh()
{
    if (!m_active) {
        m_dirty = true;
        return;
    }

    readDirectory();
    m_dirty = false;
}

void FileModel::readDirectory()
{
    // wrapped in reset model methods to get views notified
    beginResetModel();

    m_files.clear();
    m_errorMessage = "";

    if (!m_dir.isEmpty())
        readEntries();

    endResetModel();
    emit fileCountChanged();
    emit errorMessageChanged();
}

void FileModel::readEntries()
{
    QDir dir(m_dir);
    if (!dir.exists()) {
        m_errorMessage = tr("Directory does not exist");
        return;
    }
    QByteArray ba = m_dir.toLatin1();
    char *dirname = ba.data();
    if (access(dirname, R_OK) == -1) {
        m_errorMessage = tr("No permission to read the directory");
        return;
    }

    QFileInfoList infoList = dir.entryInfoList();
    foreach (QFileInfo info, infoList) {
        QString filename = info.fileName();
        if (filename == "." || filename == "..")
            continue;

        FileData data;
        data.info = info;
        m_files.append(data);
    }
}
