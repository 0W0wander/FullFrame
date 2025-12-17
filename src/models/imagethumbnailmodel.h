#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QPixmap>
#include <QHash>

namespace FullFrame {

class ImageThumbnailModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        FilePathRole = Qt::UserRole + 1,
        FileNameRole,
        ThumbnailRole
    };

    explicit ImageThumbnailModel(QObject* parent = nullptr);
    ~ImageThumbnailModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void setDirectory(const QString& path);
    void clear();
    
    QString filePath(int index) const;
    int indexOf(const QString& path) const;

Q_SIGNALS:
    void loadingStarted(int total);
    void loadingProgress(int current, int total);
    void loadingFinished();

private Q_SLOTS:
    void onThumbnailReady(const QString& filePath, const QPixmap& pixmap);

private:
    void scanDirectory(const QString& path);

    QStringList m_files;
    QHash<QString, QPixmap> m_thumbnails;
    QHash<QString, int> m_pathToIndex;
    int m_thumbnailSize = 256;
};

} // namespace FullFrame
