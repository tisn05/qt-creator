/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "fileutils.h"
#include "savefile.h"

#include "algorithm.h"
#include "qtcassert.h"

#include <QDataStream>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QOperatingSystemVersion>
#include <QRegExp>
#include <QTimer>
#include <QUrl>
#include <qplatformdefs.h>

#ifdef QT_GUI_LIB
#include <QMessageBox>
#endif

#ifdef Q_OS_WIN
// We need defines for Windows 8
#undef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WIN8

#include <qt_windows.h>
#include <shlobj.h>
#endif

#ifdef Q_OS_OSX
#include "fileutils_mac.h"
#endif

QT_BEGIN_NAMESPACE
QDebug operator<<(QDebug dbg, const Utils::FileName &c)
{
    return dbg << c.toString();
}

QT_END_NAMESPACE

namespace Utils {

/*! \class Utils::FileUtils

  \brief The FileUtils class contains file and directory related convenience
  functions.

*/

/*!
  Removes the directory \a filePath and its subdirectories recursively.

  \note The \a error parameter is optional.

  Returns whether the operation succeeded.
*/
bool FileUtils::removeRecursively(const FileName &filePath, QString *error)
{
    QFileInfo fileInfo = filePath.toFileInfo();
    if (!fileInfo.exists() && !fileInfo.isSymLink())
        return true;
    QFile::setPermissions(filePath.toString(), fileInfo.permissions() | QFile::WriteUser);
    if (fileInfo.isDir()) {
        QDir dir(filePath.toString());
        dir.setPath(dir.canonicalPath());
        if (dir.isRoot()) {
            if (error) {
                *error = QCoreApplication::translate("Utils::FileUtils",
                    "Refusing to remove root directory.");
            }
            return false;
        }
        if (dir.path() == QDir::home().canonicalPath()) {
            if (error) {
                *error = QCoreApplication::translate("Utils::FileUtils",
                    "Refusing to remove your home directory.");
            }
            return false;
        }

        QStringList fileNames = dir.entryList(QDir::Files | QDir::Hidden
                                              | QDir::System | QDir::Dirs | QDir::NoDotAndDotDot);
        foreach (const QString &fileName, fileNames) {
            if (!removeRecursively(filePath.pathAppended(fileName), error))
                return false;
        }
        if (!QDir::root().rmdir(dir.path())) {
            if (error) {
                *error = QCoreApplication::translate("Utils::FileUtils", "Failed to remove directory \"%1\".")
                        .arg(filePath.toUserOutput());
            }
            return false;
        }
    } else {
        if (!QFile::remove(filePath.toString())) {
            if (error) {
                *error = QCoreApplication::translate("Utils::FileUtils", "Failed to remove file \"%1\".")
                        .arg(filePath.toUserOutput());
            }
            return false;
        }
    }
    return true;
}

/*!
  Copies the directory specified by \a srcFilePath recursively to \a tgtFilePath. \a tgtFilePath will contain
  the target directory, which will be created. Example usage:

  \code
    QString error;
    book ok = Utils::FileUtils::copyRecursively("/foo/bar", "/foo/baz", &error);
    if (!ok)
      qDebug() << error;
  \endcode

  This will copy the contents of /foo/bar into to the baz directory under /foo, which will be created in the process.

  \note The \a error parameter is optional.

  Returns whether the operation succeeded.
*/
bool FileUtils::copyRecursively(const FileName &srcFilePath, const FileName &tgtFilePath,
                                QString *error, const std::function<bool (QFileInfo, QFileInfo, QString *)> &copyHelper)
{
    QFileInfo srcFileInfo = srcFilePath.toFileInfo();
    if (srcFileInfo.isDir()) {
        if (!tgtFilePath.exists()) {
            QDir targetDir(tgtFilePath.toString());
            targetDir.cdUp();
            if (!targetDir.mkdir(tgtFilePath.fileName())) {
                if (error) {
                    *error = QCoreApplication::translate("Utils::FileUtils", "Failed to create directory \"%1\".")
                            .arg(tgtFilePath.toUserOutput());
                }
                return false;
            }
        }
        QDir sourceDir(srcFilePath.toString());
        QStringList fileNames = sourceDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot
                                                    | QDir::Hidden | QDir::System);
        foreach (const QString &fileName, fileNames) {
            const FileName newSrcFilePath = srcFilePath.pathAppended(fileName);
            const FileName newTgtFilePath = tgtFilePath.pathAppended(fileName);
            if (!copyRecursively(newSrcFilePath, newTgtFilePath, error, copyHelper))
                return false;
        }
    } else {
        if (copyHelper) {
            if (!copyHelper(srcFileInfo, tgtFilePath.toFileInfo(), error))
                return false;
        } else {
            if (!QFile::copy(srcFilePath.toString(), tgtFilePath.toString())) {
                if (error) {
                    *error = QCoreApplication::translate("Utils::FileUtils", "Could not copy file \"%1\" to \"%2\".")
                            .arg(srcFilePath.toUserOutput(), tgtFilePath.toUserOutput());
                }
                return false;
            }
        }
    }
    return true;
}

/*!
  If this is a directory, the function will recursively check all files and return
  true if one of them is newer than \a timeStamp. If this is a single file, true will
  be returned if the file is newer than \a timeStamp.

  Returns whether at least one file in \a filePath has a newer date than
  \a timeStamp.
*/
bool FileName::isNewerThan(const QDateTime &timeStamp) const
{
    const QFileInfo fileInfo = toFileInfo();
    if (!fileInfo.exists() || fileInfo.lastModified() >= timeStamp)
        return true;
    if (fileInfo.isDir()) {
        const QStringList dirContents = QDir(toString())
            .entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &curFileName : dirContents) {
            if (pathAppended(curFileName).isNewerThan(timeStamp))
                return true;
        }
    }
    return false;
}

/*!
  Recursively resolves symlinks if \a filePath is a symlink.
  To resolve symlinks anywhere in the path, see canonicalPath.
  Unlike QFileInfo::canonicalFilePath(), this function will still return the expected deepest
  target file even if the symlink is dangling.

  \note Maximum recursion depth == 16.

  Returns the symlink target file path.
*/
FileName FileUtils::resolveSymlinks(const FileName &path)
{
    QFileInfo f = path.toFileInfo();
    int links = 16;
    while (links-- && f.isSymLink())
        f.setFile(f.dir(), f.symLinkTarget());
    if (links <= 0)
        return FileName();
    return FileName::fromString(f.filePath());
}

/*!
  Recursively resolves possibly present symlinks in this file name.
  Unlike QFileInfo::canonicalFilePath(), this function will not return an empty
  string if path doesn't exist.

  Returns the canonical path.
*/
FileName FileName::canonicalPath() const
{
    const QString result = toFileInfo().canonicalFilePath();
    if (result.isEmpty())
        return *this;
    return FileName::fromString(result);
}

/*!
  Like QDir::toNativeSeparators(), but use prefix '~' instead of $HOME on unix systems when an
  absolute path is given.

  Returns the possibly shortened path with native separators.
*/
QString FileName::shortNativePath() const
{
    if (HostOsInfo::isAnyUnixHost()) {
        const FileName home = FileName::fromString(QDir::cleanPath(QDir::homePath()));
        if (isChildOf(home)) {
            return QLatin1Char('~') + QDir::separator()
                + QDir::toNativeSeparators(relativeChildPath(home).toString());
        }
    }
    return toUserOutput();
}

QString FileUtils::fileSystemFriendlyName(const QString &name)
{
    QString result = name;
    result.replace(QRegExp(QLatin1String("\\W")), QLatin1String("_"));
    result.replace(QRegExp(QLatin1String("_+")), QLatin1String("_")); // compact _
    result.remove(QRegExp(QLatin1String("^_*"))); // remove leading _
    result.remove(QRegExp(QLatin1String("_+$"))); // remove trailing _
    if (result.isEmpty())
        result = QLatin1String("unknown");
    return result;
}

int FileUtils::indexOfQmakeUnfriendly(const QString &name, int startpos)
{
    static QRegExp checkRegExp(QLatin1String("[^a-zA-Z0-9_.-]"));
    return checkRegExp.indexIn(name, startpos);
}

QString FileUtils::qmakeFriendlyName(const QString &name)
{
    QString result = name;

    // Remove characters that might trip up a build system (especially qmake):
    int pos = indexOfQmakeUnfriendly(result);
    while (pos >= 0) {
        result[pos] = QLatin1Char('_');
        pos = indexOfQmakeUnfriendly(result, pos);
    }
    return fileSystemFriendlyName(result);
}

bool FileUtils::makeWritable(const FileName &path)
{
    const QString fileName = path.toString();
    return QFile::setPermissions(fileName, QFile::permissions(fileName) | QFile::WriteUser);
}

// makes sure that capitalization of directories is canonical on Windows and OS X.
// This mimics the logic in QDeclarative_isFileCaseCorrect
QString FileUtils::normalizePathName(const QString &name)
{
#ifdef Q_OS_WIN
    const QString nativeSeparatorName(QDir::toNativeSeparators(name));
    const auto nameC = reinterpret_cast<LPCTSTR>(nativeSeparatorName.utf16()); // MinGW
    PIDLIST_ABSOLUTE file;
    HRESULT hr = SHParseDisplayName(nameC, NULL, &file, 0, NULL);
    if (FAILED(hr))
        return name;
    TCHAR buffer[MAX_PATH];
    const bool success = SHGetPathFromIDList(file, buffer);
    ILFree(file);
    return success ? QDir::fromNativeSeparators(QString::fromUtf16(reinterpret_cast<const ushort *>(buffer)))
                   : name;
#elif defined(Q_OS_OSX)
    return Internal::normalizePathName(name);
#else // do not try to handle case-insensitive file systems on Linux
    return name;
#endif
}

bool FileUtils::isRelativePath(const QString &path)
{
    if (path.startsWith(QLatin1Char('/')))
        return false;
    if (HostOsInfo::isWindowsHost()) {
        if (path.startsWith(QLatin1Char('\\')))
            return false;
        // Unlike QFileInfo, this won't accept a relative path with a drive letter.
        // Such paths result in a royal mess anyway ...
        if (path.length() >= 3 && path.at(1) == QLatin1Char(':') && path.at(0).isLetter()
                && (path.at(2) == QLatin1Char('/') || path.at(2) == QLatin1Char('\\')))
            return false;
    }
    return true;
}

QString FileUtils::resolvePath(const QString &baseDir, const QString &fileName)
{
    if (fileName.isEmpty())
        return QString();
    if (isAbsolutePath(fileName))
        return QDir::cleanPath(fileName);
    return QDir::cleanPath(baseDir + QLatin1Char('/') + fileName);
}

FileName FileUtils::commonPath(const FileName &oldCommonPath, const FileName &fileName)
{
    FileName newCommonPath = oldCommonPath;
    while (!newCommonPath.isEmpty() && !fileName.isChildOf(newCommonPath))
        newCommonPath = newCommonPath.parentDir();
    return newCommonPath.canonicalPath();
}

// Copied from qfilesystemengine_win.cpp
#ifdef Q_OS_WIN

// File ID for Windows up to version 7.
static inline QByteArray fileIdWin7(HANDLE handle)
{
    BY_HANDLE_FILE_INFORMATION info;
    if (GetFileInformationByHandle(handle, &info)) {
        char buffer[sizeof "01234567:0123456701234567\0"];
        qsnprintf(buffer, sizeof(buffer), "%lx:%08lx%08lx",
                  info.dwVolumeSerialNumber,
                  info.nFileIndexHigh,
                  info.nFileIndexLow);
        return QByteArray(buffer);
    }
    return QByteArray();
}

// File ID for Windows starting from version 8.
static QByteArray fileIdWin8(HANDLE handle)
{
    QByteArray result;
    FILE_ID_INFO infoEx;
    if (GetFileInformationByHandleEx(handle,
                                     static_cast<FILE_INFO_BY_HANDLE_CLASS>(18), // FileIdInfo in Windows 8
                                     &infoEx, sizeof(FILE_ID_INFO))) {
        result = QByteArray::number(infoEx.VolumeSerialNumber, 16);
        result += ':';
        // Note: MinGW-64's definition of FILE_ID_128 differs from the MSVC one.
        result += QByteArray(reinterpret_cast<const char *>(&infoEx.FileId), int(sizeof(infoEx.FileId))).toHex();
    }
    return result;
}

static QByteArray fileIdWin(HANDLE fHandle)
{
    return QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows8 ?
                fileIdWin8(HANDLE(fHandle)) : fileIdWin7(HANDLE(fHandle));
}
#endif

QByteArray FileUtils::fileId(const FileName &fileName)
{
    QByteArray result;

#ifdef Q_OS_WIN
    const HANDLE handle =
            CreateFile((wchar_t*)fileName.toUserOutput().utf16(), 0,
                       FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (handle != INVALID_HANDLE_VALUE) {
        result = fileIdWin(handle);
        CloseHandle(handle);
    }
#else // Copied from qfilesystemengine_unix.cpp
    if (Q_UNLIKELY(fileName.isEmpty()))
        return result;

    QT_STATBUF statResult;
    if (QT_STAT(fileName.toString().toLocal8Bit().constData(), &statResult))
        return result;
    result = QByteArray::number(quint64(statResult.st_dev), 16);
    result += ':';
    result += QByteArray::number(quint64(statResult.st_ino));
#endif
    return result;
}

QByteArray FileReader::fetchQrc(const QString &fileName)
{
    QTC_ASSERT(fileName.startsWith(QLatin1Char(':')), return QByteArray());
    QFile file(fileName);
    bool ok = file.open(QIODevice::ReadOnly);
    QTC_ASSERT(ok, qWarning() << fileName << "not there!"; return QByteArray());
    return file.readAll();
}

bool FileReader::fetch(const QString &fileName, QIODevice::OpenMode mode)
{
    QTC_ASSERT(!(mode & ~(QIODevice::ReadOnly | QIODevice::Text)), return false);

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | mode)) {
        m_errorString = tr("Cannot open %1 for reading: %2").arg(
                QDir::toNativeSeparators(fileName), file.errorString());
        return false;
    }
    m_data = file.readAll();
    if (file.error() != QFile::NoError) {
        m_errorString = tr("Cannot read %1: %2").arg(
                QDir::toNativeSeparators(fileName), file.errorString());
        return false;
    }
    return true;
}

bool FileReader::fetch(const QString &fileName, QIODevice::OpenMode mode, QString *errorString)
{
    if (fetch(fileName, mode))
        return true;
    if (errorString)
        *errorString = m_errorString;
    return false;
}

#ifdef QT_GUI_LIB
bool FileReader::fetch(const QString &fileName, QIODevice::OpenMode mode, QWidget *parent)
{
    if (fetch(fileName, mode))
        return true;
    if (parent)
        QMessageBox::critical(parent, tr("File Error"), m_errorString);
    return false;
}
#endif // QT_GUI_LIB

FileSaverBase::FileSaverBase() = default;

FileSaverBase::~FileSaverBase() = default;

bool FileSaverBase::finalize()
{
    m_file->close();
    setResult(m_file->error() == QFile::NoError);
    m_file.reset();
    return !m_hasError;
}

bool FileSaverBase::finalize(QString *errStr)
{
    if (finalize())
        return true;
    if (errStr)
        *errStr = errorString();
    return false;
}

#ifdef QT_GUI_LIB
bool FileSaverBase::finalize(QWidget *parent)
{
    if (finalize())
        return true;
    QMessageBox::critical(parent, tr("File Error"), errorString());
    return false;
}
#endif // QT_GUI_LIB

bool FileSaverBase::write(const char *data, int len)
{
    if (m_hasError)
        return false;
    return setResult(m_file->write(data, len) == len);
}

bool FileSaverBase::write(const QByteArray &bytes)
{
    if (m_hasError)
        return false;
    return setResult(m_file->write(bytes) == bytes.count());
}

bool FileSaverBase::setResult(bool ok)
{
    if (!ok && !m_hasError) {
        if (!m_file->errorString().isEmpty()) {
            m_errorString = tr("Cannot write file %1: %2").arg(
                        QDir::toNativeSeparators(m_fileName), m_file->errorString());
        } else {
            m_errorString = tr("Cannot write file %1. Disk full?").arg(
                        QDir::toNativeSeparators(m_fileName));
        }
        m_hasError = true;
    }
    return ok;
}

bool FileSaverBase::setResult(QTextStream *stream)
{
    stream->flush();
    return setResult(stream->status() == QTextStream::Ok);
}

bool FileSaverBase::setResult(QDataStream *stream)
{
    return setResult(stream->status() == QDataStream::Ok);
}

bool FileSaverBase::setResult(QXmlStreamWriter *stream)
{
    return setResult(!stream->hasError());
}


FileSaver::FileSaver(const QString &filename, QIODevice::OpenMode mode)
{
    m_fileName = filename;
    // Workaround an assert in Qt -- and provide a useful error message, too:
    if (HostOsInfo::isWindowsHost()) {
        // Taken from: https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
        static const QStringList reservedNames
                = {"CON", "PRN", "AUX", "NUL",
                   "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                   "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
        const QString fn = QFileInfo(filename).baseName().toUpper();
        for (const QString &rn : reservedNames) {
            if (fn == rn) {
                m_errorString = tr("%1: Is a reserved filename on Windows. Cannot save.").arg(filename);
                m_hasError = true;
                return;
            }
        }
    }
    if (mode & (QIODevice::ReadOnly | QIODevice::Append)) {
        m_file.reset(new QFile{filename});
        m_isSafe = false;
    } else {
        m_file.reset(new SaveFile{filename});
        m_isSafe = true;
    }
    if (!m_file->open(QIODevice::WriteOnly | mode)) {
        QString err = QFile::exists(filename) ?
                tr("Cannot overwrite file %1: %2") : tr("Cannot create file %1: %2");
        m_errorString = err.arg(QDir::toNativeSeparators(filename), m_file->errorString());
        m_hasError = true;
    }
}

bool FileSaver::finalize()
{
    if (!m_isSafe)
        return FileSaverBase::finalize();

    auto sf = static_cast<SaveFile *>(m_file.get());
    if (m_hasError) {
        if (sf->isOpen())
            sf->rollback();
    } else {
        setResult(sf->commit());
    }
    m_file.reset();
    return !m_hasError;
}

TempFileSaver::TempFileSaver(const QString &templ)
{
    m_file.reset(new QTemporaryFile{});
    auto tempFile = static_cast<QTemporaryFile *>(m_file.get());
    if (!templ.isEmpty())
        tempFile->setFileTemplate(templ);
    tempFile->setAutoRemove(false);
    if (!tempFile->open()) {
        m_errorString = tr("Cannot create temporary file in %1: %2").arg(
                QDir::toNativeSeparators(QFileInfo(tempFile->fileTemplate()).absolutePath()),
                tempFile->errorString());
        m_hasError = true;
    }
    m_fileName = tempFile->fileName();
}

TempFileSaver::~TempFileSaver()
{
    m_file.reset();
    if (m_autoRemove)
        QFile::remove(m_fileName);
}

/*! \class Utils::FileName

    \brief The FileName class is a light-weight convenience class for filenames.

    On windows filenames are compared case insensitively.
*/

FileName::FileName()
{
}

/// Constructs a FileName from \a info
FileName FileName::fromFileInfo(const QFileInfo &info)
{
    return FileName::fromString(info.absoluteFilePath());
}

/// \returns a QFileInfo
QFileInfo FileName::toFileInfo() const
{
    return QFileInfo(m_data);
}

FileName FileName::fromUrl(const QUrl &url)
{
    FileName fn;
    fn.m_url = url;
    fn.m_data = url.path();
    return fn;
}

/// \returns a QString for passing on to QString based APIs
const QString &FileName::toString() const
{
    return m_data;
}

QUrl FileName::toUrl() const
{
    return m_url;
}

/// \returns a QString to display to the user
/// Converts the separators to the native format
QString FileName::toUserOutput() const
{
    if (m_url.isEmpty())
        return QDir::toNativeSeparators(toString());
    return m_url.toString();
}

QString FileName::fileName(int pathComponents) const
{
    if (pathComponents < 0)
        return m_data;
    const QChar slash = QLatin1Char('/');
    int i = m_data.lastIndexOf(slash);
    if (pathComponents == 0 || i == -1)
        return m_data.mid(i + 1);
    int component = i + 1;
    // skip adjacent slashes
    while (i > 0 && m_data.at(--i) == slash)
        ;
    while (i >= 0 && --pathComponents >= 0) {
        i = m_data.lastIndexOf(slash, i);
        component = i + 1;
        while (i > 0 && m_data.at(--i) == slash)
            ;
    }

    // If there are no more slashes before the found one, return the entire string
    if (i > 0 && m_data.lastIndexOf(slash, i) != -1)
        return m_data.mid(component);
    return m_data;
}

/// \returns a bool indicating whether a file with this
/// FileName exists.
bool FileName::exists() const
{
    return !isEmpty() && QFileInfo::exists(m_data);
}

/// Find the parent directory of a given directory.

/// Returns an empty FileName if the current directory is already
/// a root level directory.

/// \returns \a FileName with the last segment removed.
FileName FileName::parentDir() const
{
    const QString basePath = toString();
    if (basePath.isEmpty())
        return FileName();

    const QDir base(basePath);
    if (base.isRoot())
        return FileName();

    const QString path = basePath + QLatin1String("/..");
    const QString parent = QDir::cleanPath(path);
    QTC_ASSERT(parent != path, return FileName());

    return FileName::fromString(parent);
}

/// Constructs a FileName from \a filename
/// \a filename is not checked for validity.
FileName FileName::fromString(const QString &filename)
{
    FileName fn;
    fn.m_data = filename;
    return fn;
}

/// Constructs a FileName from \a fileName. The \a defaultExtension is appended
/// to \a filename if that does not have an extension already.
/// \a fileName is not checked for validity.
FileName FileName::fromStringWithExtension(const QString &filename, const QString &defaultExtension)
{
    if (filename.isEmpty() || defaultExtension.isEmpty())
        return FileName::fromString(filename);

    QString rc = filename;
    QFileInfo fi(filename);
    // Add extension unless user specified something else
    const QChar dot = QLatin1Char('.');
    if (!fi.fileName().contains(dot)) {
        if (!defaultExtension.startsWith(dot))
            rc += dot;
        rc += defaultExtension;
    }
    return FileName::fromString(rc);
}

/// Constructs a FileName from \a fileName
/// \a fileName is not checked for validity.
FileName FileName::fromLatin1(const QByteArray &filename)
{
    return FileName::fromString(QString::fromLatin1(filename));
}

/// Constructs a FileName from \a fileName
/// \a fileName is only passed through QDir::cleanPath
FileName FileName::fromUserInput(const QString &filename)
{
    QString clean = QDir::cleanPath(filename);
    if (clean.startsWith(QLatin1String("~/")))
        clean = QDir::homePath() + clean.mid(1);
    return FileName::fromString(clean);
}

/// Constructs a FileName from \a fileName, which is encoded as UTF-8.
/// \a fileName is not checked for validity.
FileName FileName::fromUtf8(const char *filename, int filenameSize)
{
    return FileName::fromString(QString::fromUtf8(filename, filenameSize));
}

FileName FileName::fromVariant(const QVariant &variant)
{
    if (variant.type() == QVariant::Url)
        return FileName::fromUrl(variant.toUrl());
    return FileName::fromString(variant.toString());
}

QVariant FileName::toVariant() const
{
    if (!m_url.isEmpty())
        return m_url;
    return m_data;
}

bool FileName::operator==(const FileName &other) const
{
    if (!m_url.isEmpty())
        return m_url == other.m_url;
    return QString::compare(m_data, other.m_data, HostOsInfo::fileNameCaseSensitivity()) == 0;
}

bool FileName::operator!=(const FileName &other) const
{
    return !(*this == other);
}

bool FileName::operator<(const FileName &other) const
{
    if (!m_url.isEmpty())
        return m_url < other.m_url;
    return QString::compare(m_data, other.m_data, HostOsInfo::fileNameCaseSensitivity()) < 0;
}

bool FileName::operator<=(const FileName &other) const
{
    return !(other < *this);
}

bool FileName::operator>(const FileName &other) const
{
    return other < *this;
}

bool FileName::operator>=(const FileName &other) const
{
    return !(*this < other);
}

FileName FileName::operator+(const QString &s) const
{
    return FileName::fromString(m_data + s);
}

/// \returns whether FileName is a child of \a s
bool FileName::isChildOf(const FileName &s) const
{
    if (s.isEmpty())
        return false;
    if (!m_data.startsWith(s.m_data, HostOsInfo::fileNameCaseSensitivity()))
        return false;
    if (m_data.size() <= s.m_data.size())
        return false;
    // s is root, '/' was already tested in startsWith
    if (s.m_data.endsWith(QLatin1Char('/')))
        return true;
    // s is a directory, next character should be '/' (/tmpdir is NOT a child of /tmp)
    return m_data.at(s.m_data.size()) == QLatin1Char('/');
}

/// \overload
bool FileName::isChildOf(const QDir &dir) const
{
    return isChildOf(FileName::fromString(dir.absolutePath()));
}

/// \returns whether FileName endsWith \a s
bool FileName::endsWith(const QString &s) const
{
    return m_data.endsWith(s, HostOsInfo::fileNameCaseSensitivity());
}

bool FileName::isLocal() const
{
    return m_url.isEmpty() || m_url.isLocalFile();
}

/// \returns the relativeChildPath of FileName to parent if FileName is a child of parent
/// \note returns a empty FileName if FileName is not a child of parent
/// That is, this never returns a path starting with "../"
FileName FileName::relativeChildPath(const FileName &parent) const
{
    if (!isChildOf(parent))
        return FileName();
    return FileName::fromString(m_data.mid(parent.m_data.size() + 1, -1));
}

/// Appends \a s, ensuring a / between the parts
FileName &FileName::appendPath(const QString &s)
{
    if (s.isEmpty())
        return *this;
    if (!isEmpty() && !m_data.endsWith(QLatin1Char('/')))
        m_data.append('/');
    m_data.append(s);
    return *this;
}

FileName FileName::pathAppended(const QString &str) const
{
    FileName fn = *this;
    if (str.isEmpty())
        return fn;
    if (!isEmpty() && !m_data.endsWith(QLatin1Char('/')))
        fn.m_data.append('/');
    fn.m_data.append(str);
    return fn;
}

FileName FileName::stringAppended(const QString &str) const
{
    FileName fn = *this;
    fn.m_data.append(str);
    return fn;
}

uint FileName::hash(uint seed) const
{
    if (Utils::HostOsInfo::fileNameCaseSensitivity() == Qt::CaseInsensitive)
        return qHash(m_data.toUpper(), seed);
    return qHash(m_data, seed);
}

QTextStream &operator<<(QTextStream &s, const FileName &fn)
{
    return s << fn.toString();
}

#ifdef Q_OS_WIN
template <>
void withNtfsPermissions(const std::function<void()> &task)
{
    qt_ntfs_permission_lookup++;
    task();
    qt_ntfs_permission_lookup--;
}
#endif
} // namespace Utils
