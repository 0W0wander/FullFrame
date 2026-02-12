/**
 * TagManager - Image tagging system with SQLite persistence
 * 
 * Features:
 * - Create/delete/rename tags
 * - Assign tags to images
 * - Query images by tag
 * - Tag hierarchy support
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QHash>
#include <QSqlDatabase>

namespace FullFrame {

struct Tag
{
    qint64 id = -1;
    QString name;
    QString color;
    QString hotkey;  // Single key like "1", "A", "F1", etc.
    qint64 parentId = -1;
    
    bool isValid() const { return id >= 0; }
    bool hasHotkey() const { return !hotkey.isEmpty(); }
};

/**
 * Manages tags and image-tag associations
 * Uses SQLite for persistence
 */
class TagManager : public QObject
{
    Q_OBJECT

public:
    static TagManager* instance();
    static void cleanup();

    // Database initialization
    bool initialize(const QString& dbPath);
    bool isInitialized() const { return m_initialized; }

    // Tag management
    qint64 createTag(const QString& name, const QString& color = QString(), qint64 parentId = -1);
    bool deleteTag(qint64 tagId);
    bool renameTag(qint64 tagId, const QString& newName);
    bool setTagColor(qint64 tagId, const QString& color);
    bool setTagHotkey(qint64 tagId, const QString& hotkey);
    bool clearTagHotkey(qint64 tagId);
    
    // Tag queries
    Tag tag(qint64 tagId) const;
    Tag tagByName(const QString& name) const;
    Tag tagByHotkey(const QString& hotkey) const;
    QList<Tag> allTags() const;
    QList<Tag> childTags(qint64 parentId) const;
    
    // Image-tag associations
    bool tagImage(const QString& imagePath, qint64 tagId);
    bool untagImage(const QString& imagePath, qint64 tagId);
    bool hasTag(const QString& imagePath, qint64 tagId) const;
    
    // Get tags for image
    QList<Tag> tagsForImage(const QString& imagePath) const;
    QSet<qint64> tagIdsForImage(const QString& imagePath) const;
    
    // Get images with tag
    QStringList imagesWithTag(qint64 tagId) const;
    QStringList imagesWithAnyTag(const QSet<qint64>& tagIds) const;
    QStringList imagesWithAllTags(const QSet<qint64>& tagIds) const;
    
    // Get image count per tag (for sorting by usage)
    // If imagePaths is provided, only counts within that set of images
    QHash<qint64, int> tagImageCounts(const QStringList& imagePaths = QStringList()) const;
    
    // Bulk operations
    bool tagImages(const QStringList& imagePaths, qint64 tagId);
    bool untagImages(const QStringList& imagePaths, qint64 tagId);

Q_SIGNALS:
    void tagCreated(qint64 tagId, const QString& name);
    void tagDeleted(qint64 tagId);
    void tagRenamed(qint64 tagId, const QString& newName);
    void tagColorChanged(qint64 tagId, const QString& color);
    void tagHotkeyChanged(qint64 tagId, const QString& hotkey);
    void imageTagged(const QString& imagePath, qint64 tagId);
    void imageUntagged(const QString& imagePath, qint64 tagId);
    void tagsChanged();

private:
    explicit TagManager(QObject* parent = nullptr);
    ~TagManager() override;

    // Disable copy
    TagManager(const TagManager&) = delete;
    TagManager& operator=(const TagManager&) = delete;

    bool createTables();
    qint64 imageId(const QString& imagePath) const;
    qint64 getOrCreateImageId(const QString& imagePath);

private:
    static TagManager* s_instance;

    QSqlDatabase m_db;
    bool m_initialized = false;
    
    // In-memory cache
    mutable QHash<qint64, Tag> m_tagCache;
    mutable QHash<QString, QSet<qint64>> m_imageTagCache;
};

} // namespace FullFrame

