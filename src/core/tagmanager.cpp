/**
 * TagManager implementation
 * 
 * SQLite-based tag storage with in-memory caching
 */

#include "tagmanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QFileInfo>
#include <QDebug>

namespace FullFrame {

TagManager* TagManager::s_instance = nullptr;

TagManager* TagManager::instance()
{
    if (!s_instance) {
        s_instance = new TagManager();
    }
    return s_instance;
}

void TagManager::cleanup()
{
    delete s_instance;
    s_instance = nullptr;
}

TagManager::TagManager(QObject* parent)
    : QObject(parent)
{
}

TagManager::~TagManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool TagManager::initialize(const QString& dbPath)
{
    if (m_initialized) {
        return true;
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", "fullframe_tags");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "Failed to open tag database:" << m_db.lastError().text();
        return false;
    }

    if (!createTables()) {
        qWarning() << "Failed to create tag tables";
        return false;
    }

    m_initialized = true;
    return true;
}

bool TagManager::createTables()
{
    QSqlQuery query(m_db);

    // Tags table
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS tags (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            color TEXT,
            hotkey TEXT,
            parent_id INTEGER DEFAULT -1,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )")) {
        qWarning() << "Failed to create tags table:" << query.lastError().text();
        return false;
    }
    
    // Add hotkey column if it doesn't exist (migration for existing databases)
    query.exec("ALTER TABLE tags ADD COLUMN hotkey TEXT");
    
    // Add album_path column if it doesn't exist (migration for existing databases)
    query.exec("ALTER TABLE tags ADD COLUMN album_path TEXT");

    // Add is_supertag column to image_tags if it doesn't exist (migration for existing databases)
    query.exec("ALTER TABLE image_tags ADD COLUMN is_supertag INTEGER DEFAULT 0");

    // Images table (stores image paths)
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS images (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            path TEXT NOT NULL UNIQUE,
            added_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )")) {
        qWarning() << "Failed to create images table:" << query.lastError().text();
        return false;
    }

    // Image-tag junction table
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS image_tags (
            image_id INTEGER NOT NULL,
            tag_id INTEGER NOT NULL,
            tagged_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (image_id, tag_id),
            FOREIGN KEY (image_id) REFERENCES images(id) ON DELETE CASCADE,
            FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
        )
    )")) {
        qWarning() << "Failed to create image_tags table:" << query.lastError().text();
        return false;
    }

    // Create indexes
    query.exec("CREATE INDEX IF NOT EXISTS idx_image_tags_image ON image_tags(image_id)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_image_tags_tag ON image_tags(tag_id)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_images_path ON images(path)");

    // Sequence tables
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS sequences (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )")) {
        qWarning() << "Failed to create sequences table:" << query.lastError().text();
    }
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS sequence_items (
            sequence_id INTEGER NOT NULL,
            image_path TEXT NOT NULL,
            position INTEGER NOT NULL DEFAULT 0,
            is_cover INTEGER NOT NULL DEFAULT 0,
            PRIMARY KEY (sequence_id, image_path),
            FOREIGN KEY (sequence_id) REFERENCES sequences(id) ON DELETE CASCADE
        )
    )")) {
        qWarning() << "Failed to create sequence_items table:" << query.lastError().text();
    }
    query.exec("CREATE INDEX IF NOT EXISTS idx_seq_items_path ON sequence_items(image_path)");

    return true;
}

// ============== Tag Management ==============

qint64 TagManager::createTag(const QString& name, const QString& color, qint64 parentId)
{
    QString lowerName = name.toLower();
    
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO tags (name, color, parent_id) VALUES (?, ?, ?)");
    query.addBindValue(lowerName);
    query.addBindValue(color.isEmpty() ? QVariant() : color);
    query.addBindValue(parentId);

    if (!query.exec()) {
        qWarning() << "Failed to create tag:" << query.lastError().text();
        return -1;
    }

    qint64 tagId = query.lastInsertId().toLongLong();
    
    // Update cache
    Tag newTag;
    newTag.id = tagId;
    newTag.name = lowerName;
    newTag.color = color;
    newTag.parentId = parentId;
    m_tagCache.insert(tagId, newTag);

    Q_EMIT tagCreated(tagId, lowerName);
    Q_EMIT tagsChanged();
    return tagId;
}

bool TagManager::deleteTag(qint64 tagId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM tags WHERE id = ?");
    query.addBindValue(tagId);

    if (!query.exec()) {
        return false;
    }

    m_tagCache.remove(tagId);
    Q_EMIT tagDeleted(tagId);
    Q_EMIT tagsChanged();
    return true;
}

bool TagManager::renameTag(qint64 tagId, const QString& newName)
{
    QString lowerName = newName.toLower();
    
    QSqlQuery query(m_db);
    query.prepare("UPDATE tags SET name = ? WHERE id = ?");
    query.addBindValue(lowerName);
    query.addBindValue(tagId);

    if (!query.exec()) {
        return false;
    }

    if (m_tagCache.contains(tagId)) {
        m_tagCache[tagId].name = lowerName;
    }

    Q_EMIT tagRenamed(tagId, lowerName);
    Q_EMIT tagsChanged();
    return true;
}

bool TagManager::setTagColor(qint64 tagId, const QString& color)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE tags SET color = ? WHERE id = ?");
    query.addBindValue(color);
    query.addBindValue(tagId);

    if (!query.exec()) {
        return false;
    }

    if (m_tagCache.contains(tagId)) {
        m_tagCache[tagId].color = color;
    }

    Q_EMIT tagColorChanged(tagId, color);
    return true;
}

bool TagManager::setTagHotkey(qint64 tagId, const QString& hotkey)
{
    // First clear any existing tag with this hotkey
    if (!hotkey.isEmpty()) {
        QSqlQuery clearQuery(m_db);
        clearQuery.prepare("UPDATE tags SET hotkey = NULL WHERE hotkey = ?");
        clearQuery.addBindValue(hotkey);
        clearQuery.exec();
        
        // Update cache for cleared tags
        for (auto& cachedTag : m_tagCache) {
            if (cachedTag.hotkey == hotkey) {
                cachedTag.hotkey.clear();
            }
        }
    }
    
    QSqlQuery query(m_db);
    query.prepare("UPDATE tags SET hotkey = ? WHERE id = ?");
    query.addBindValue(hotkey.isEmpty() ? QVariant() : hotkey);
    query.addBindValue(tagId);

    if (!query.exec()) {
        return false;
    }

    if (m_tagCache.contains(tagId)) {
        m_tagCache[tagId].hotkey = hotkey;
    }

    Q_EMIT tagHotkeyChanged(tagId, hotkey);
    Q_EMIT tagsChanged();
    return true;
}

bool TagManager::clearTagHotkey(qint64 tagId)
{
    return setTagHotkey(tagId, QString());
}

// ============== Tag Queries ==============

Tag TagManager::tag(qint64 tagId) const
{
    if (m_tagCache.contains(tagId)) {
        return m_tagCache.value(tagId);
    }

    QSqlQuery query(m_db);
    query.prepare("SELECT id, name, color, hotkey, parent_id, album_path FROM tags WHERE id = ?");
    query.addBindValue(tagId);

    if (query.exec() && query.next()) {
        Tag t;
        t.id = query.value(0).toLongLong();
        t.name = query.value(1).toString();
        t.color = query.value(2).toString();
        t.hotkey = query.value(3).toString();
        t.parentId = query.value(4).toLongLong();
        t.albumPath = query.value(5).toString();
        m_tagCache.insert(tagId, t);
        return t;
    }

    return Tag();
}

Tag TagManager::tagByName(const QString& name) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT id, name, color, hotkey, parent_id, album_path FROM tags WHERE name = ?");
    query.addBindValue(name);

    if (query.exec() && query.next()) {
        Tag t;
        t.id = query.value(0).toLongLong();
        t.name = query.value(1).toString();
        t.color = query.value(2).toString();
        t.hotkey = query.value(3).toString();
        t.parentId = query.value(4).toLongLong();
        t.albumPath = query.value(5).toString();
        return t;
    }

    return Tag();
}

Tag TagManager::tagByHotkey(const QString& hotkey) const
{
    if (hotkey.isEmpty()) {
        return Tag();
    }
    
    QSqlQuery query(m_db);
    query.prepare("SELECT id, name, color, hotkey, parent_id, album_path FROM tags WHERE hotkey = ?");
    query.addBindValue(hotkey);

    if (query.exec() && query.next()) {
        Tag t;
        t.id = query.value(0).toLongLong();
        t.name = query.value(1).toString();
        t.color = query.value(2).toString();
        t.hotkey = query.value(3).toString();
        t.parentId = query.value(4).toLongLong();
        t.albumPath = query.value(5).toString();
        return t;
    }

    return Tag();
}

QList<Tag> TagManager::allTags() const
{
    QList<Tag> tags;
    QSqlQuery query(m_db);
    query.exec("SELECT id, name, color, hotkey, parent_id, album_path FROM tags ORDER BY name");

    while (query.next()) {
        Tag t;
        t.id = query.value(0).toLongLong();
        t.name = query.value(1).toString();
        t.color = query.value(2).toString();
        t.hotkey = query.value(3).toString();
        t.parentId = query.value(4).toLongLong();
        t.albumPath = query.value(5).toString();
        tags.append(t);
        m_tagCache.insert(t.id, t);
    }

    return tags;
}

QList<Tag> TagManager::childTags(qint64 parentId) const
{
    QList<Tag> tags;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, name, color, hotkey, parent_id, album_path FROM tags WHERE parent_id = ? ORDER BY name");
    query.addBindValue(parentId);

    if (query.exec()) {
        while (query.next()) {
            Tag t;
            t.id = query.value(0).toLongLong();
            t.name = query.value(1).toString();
            t.color = query.value(2).toString();
            t.hotkey = query.value(3).toString();
            t.parentId = query.value(4).toLongLong();
            t.albumPath = query.value(5).toString();
            tags.append(t);
        }
    }

    return tags;
}

// ============== Image-Tag Associations ==============

qint64 TagManager::imageId(const QString& imagePath) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM images WHERE path = ?");
    query.addBindValue(imagePath);

    if (query.exec() && query.next()) {
        return query.value(0).toLongLong();
    }
    return -1;
}

qint64 TagManager::getOrCreateImageId(const QString& imagePath)
{
    qint64 id = imageId(imagePath);
    if (id >= 0) {
        return id;
    }

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO images (path) VALUES (?)");
    query.addBindValue(imagePath);

    if (query.exec()) {
        return query.lastInsertId().toLongLong();
    }
    return -1;
}

bool TagManager::tagImage(const QString& imagePath, qint64 tagId, bool asSupertag)
{
    qint64 imgId = getOrCreateImageId(imagePath);
    if (imgId < 0) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO image_tags (image_id, tag_id, is_supertag) VALUES (?, ?, ?)");
    query.addBindValue(imgId);
    query.addBindValue(tagId);
    query.addBindValue(asSupertag ? 1 : 0);

    if (query.exec()) {
        // Update cache
        m_imageTagCache[imagePath].insert(tagId);
        Q_EMIT imageTagged(imagePath, tagId);
        return true;
    }
    return false;
}

bool TagManager::untagImage(const QString& imagePath, qint64 tagId)
{
    qint64 imgId = imageId(imagePath);
    if (imgId < 0) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM image_tags WHERE image_id = ? AND tag_id = ?");
    query.addBindValue(imgId);
    query.addBindValue(tagId);

    if (query.exec()) {
        m_imageTagCache[imagePath].remove(tagId);
        Q_EMIT imageUntagged(imagePath, tagId);
        return true;
    }
    return false;
}

bool TagManager::setSupertag(const QString& imagePath, qint64 tagId, bool isSupertag)
{
    qint64 imgId = imageId(imagePath);
    if (imgId < 0) {
        return false;
    }

    // First check if the tag is already associated with the image
    if (!hasTag(imagePath, tagId)) {
        // If not, add it first
        if (!tagImage(imagePath, tagId, isSupertag)) {
            return false;
        }
    } else {
        // Update the supertag status
        QSqlQuery query(m_db);
        query.prepare("UPDATE image_tags SET is_supertag = ? WHERE image_id = ? AND tag_id = ?");
        query.addBindValue(isSupertag ? 1 : 0);
        query.addBindValue(imgId);
        query.addBindValue(tagId);

        if (!query.exec()) {
            return false;
        }
    }

    Q_EMIT imageTagged(imagePath, tagId);
    return true;
}

bool TagManager::isSupertag(const QString& imagePath, qint64 tagId) const
{
    qint64 imgId = imageId(imagePath);
    if (imgId < 0) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare("SELECT is_supertag FROM image_tags WHERE image_id = ? AND tag_id = ?");
    query.addBindValue(imgId);
    query.addBindValue(tagId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() != 0;
    }
    return false;
}

bool TagManager::hasTag(const QString& imagePath, qint64 tagId) const
{
    if (m_imageTagCache.contains(imagePath)) {
        return m_imageTagCache.value(imagePath).contains(tagId);
    }

    qint64 imgId = imageId(imagePath);
    if (imgId < 0) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare("SELECT 1 FROM image_tags WHERE image_id = ? AND tag_id = ?");
    query.addBindValue(imgId);
    query.addBindValue(tagId);

    return query.exec() && query.next();
}

QList<Tag> TagManager::tagsForImage(const QString& imagePath) const
{
    QList<Tag> tags;
    qint64 imgId = imageId(imagePath);
    if (imgId < 0) {
        return tags;
    }

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT t.id, t.name, t.color, t.hotkey, t.parent_id, t.album_path, it.is_supertag
        FROM tags t 
        JOIN image_tags it ON t.id = it.tag_id 
        WHERE it.image_id = ?
        ORDER BY it.is_supertag DESC, t.name
    )");
    query.addBindValue(imgId);

    if (query.exec()) {
        while (query.next()) {
            Tag t;
            t.id = query.value(0).toLongLong();
            t.name = query.value(1).toString();
            t.color = query.value(2).toString();
            t.hotkey = query.value(3).toString();
            t.parentId = query.value(4).toLongLong();
            t.albumPath = query.value(5).toString();
            t.isSupertag = query.value(6).toInt() != 0;
            tags.append(t);
        }
    }

    return tags;
}

QSet<qint64> TagManager::tagIdsForImage(const QString& imagePath) const
{
    if (m_imageTagCache.contains(imagePath)) {
        return m_imageTagCache.value(imagePath);
    }

    QSet<qint64> tagIds;
    qint64 imgId = imageId(imagePath);
    if (imgId < 0) {
        return tagIds;
    }

    QSqlQuery query(m_db);
    query.prepare("SELECT tag_id FROM image_tags WHERE image_id = ?");
    query.addBindValue(imgId);

    if (query.exec()) {
        while (query.next()) {
            tagIds.insert(query.value(0).toLongLong());
        }
    }

    m_imageTagCache.insert(imagePath, tagIds);
    return tagIds;
}

QStringList TagManager::imagesWithTag(qint64 tagId) const
{
    QStringList paths;
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT i.path 
        FROM images i 
        JOIN image_tags it ON i.id = it.image_id 
        WHERE it.tag_id = ?
    )");
    query.addBindValue(tagId);

    if (query.exec()) {
        while (query.next()) {
            paths.append(query.value(0).toString());
        }
    }

    return paths;
}

QStringList TagManager::imagesWithAnyTag(const QSet<qint64>& tagIds) const
{
    if (tagIds.isEmpty()) {
        return QStringList();
    }

    QStringList placeholders;
    for (int i = 0; i < tagIds.size(); ++i) {
        placeholders << "?";
    }

    QSqlQuery query(m_db);
    query.prepare(QString(R"(
        SELECT DISTINCT i.path 
        FROM images i 
        JOIN image_tags it ON i.id = it.image_id 
        WHERE it.tag_id IN (%1)
    )").arg(placeholders.join(",")));

    for (qint64 tagId : tagIds) {
        query.addBindValue(tagId);
    }

    QStringList paths;
    if (query.exec()) {
        while (query.next()) {
            paths.append(query.value(0).toString());
        }
    }

    return paths;
}

QStringList TagManager::imagesWithAllTags(const QSet<qint64>& tagIds) const
{
    if (tagIds.isEmpty()) {
        return QStringList();
    }

    QStringList placeholders;
    for (int i = 0; i < tagIds.size(); ++i) {
        placeholders << "?";
    }

    QSqlQuery query(m_db);
    query.prepare(QString(R"(
        SELECT i.path 
        FROM images i 
        JOIN image_tags it ON i.id = it.image_id 
        WHERE it.tag_id IN (%1)
        GROUP BY i.id
        HAVING COUNT(DISTINCT it.tag_id) = ?
    )").arg(placeholders.join(",")));

    for (qint64 tagId : tagIds) {
        query.addBindValue(tagId);
    }
    query.addBindValue(tagIds.size());

    QStringList paths;
    if (query.exec()) {
        while (query.next()) {
            paths.append(query.value(0).toString());
        }
    }

    return paths;
}

bool TagManager::tagImages(const QStringList& imagePaths, qint64 tagId)
{
    bool success = true;
    m_db.transaction();
    
    for (const QString& path : imagePaths) {
        if (!tagImage(path, tagId)) {
            success = false;
        }
    }
    
    m_db.commit();
    return success;
}

bool TagManager::untagImages(const QStringList& imagePaths, qint64 tagId)
{
    bool success = true;
    m_db.transaction();
    
    for (const QString& path : imagePaths) {
        if (!untagImage(path, tagId)) {
            success = false;
        }
    }
    
    m_db.commit();
    return success;
}

// ============== Album Tag Support ==============

bool TagManager::setTagAlbumPath(qint64 tagId, const QString& albumPath)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE tags SET album_path = ? WHERE id = ?");
    query.addBindValue(albumPath.isEmpty() ? QVariant() : albumPath);
    query.addBindValue(tagId);

    if (!query.exec()) {
        return false;
    }

    if (m_tagCache.contains(tagId)) {
        m_tagCache[tagId].albumPath = albumPath;
    }

    Q_EMIT tagAlbumPathChanged(tagId, albumPath);
    Q_EMIT tagsChanged();
    return true;
}

bool TagManager::clearTagAlbumPath(qint64 tagId)
{
    return setTagAlbumPath(tagId, QString());
}

bool TagManager::updateImagePath(const QString& oldPath, const QString& newPath)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE images SET path = ? WHERE path = ?");
    query.addBindValue(newPath);
    query.addBindValue(oldPath);

    if (!query.exec()) {
        qWarning() << "Failed to update image path:" << query.lastError().text();
        return false;
    }

    // Update cache
    if (m_imageTagCache.contains(oldPath)) {
        QSet<qint64> tags = m_imageTagCache.take(oldPath);
        m_imageTagCache.insert(newPath, tags);
    }

    // Update sequence_items
    QSqlQuery seqQ(m_db);
    seqQ.prepare("UPDATE sequence_items SET image_path = ? WHERE image_path = ?");
    seqQ.addBindValue(newPath);
    seqQ.addBindValue(oldPath);
    seqQ.exec();

    Q_EMIT imagePathUpdated(oldPath, newPath);
    return true;
}

// ============== Tag Image Counts ==============

QHash<qint64, int> TagManager::tagImageCounts(const QStringList& imagePaths) const
{
    QHash<qint64, int> counts;
    QSqlQuery query(m_db);

    if (imagePaths.isEmpty()) {
        // Count across entire database
        query.exec("SELECT tag_id, COUNT(*) FROM image_tags GROUP BY tag_id");
    } else {
        // Count only within the given set of image paths
        QStringList placeholders;
        placeholders.reserve(imagePaths.size());
        for (int i = 0; i < imagePaths.size(); ++i) {
            placeholders << "?";
        }

        query.prepare(QString(R"(
            SELECT it.tag_id, COUNT(*)
            FROM image_tags it
            JOIN images i ON it.image_id = i.id
            WHERE i.path IN (%1)
            GROUP BY it.tag_id
        )").arg(placeholders.join(",")));

        for (const QString& path : imagePaths) {
            query.addBindValue(path);
        }
        query.exec();
    }

    while (query.next()) {
        counts.insert(query.value(0).toLongLong(), query.value(1).toInt());
    }

    return counts;
}

QHash<qint64, QDateTime> TagManager::tagLastUsedTimes() const
{
    QHash<qint64, QDateTime> times;
    QSqlQuery query(m_db);
    query.exec("SELECT tag_id, MAX(tagged_at) FROM image_tags GROUP BY tag_id");
    while (query.next()) {
        times.insert(query.value(0).toLongLong(),
                     query.value(1).toDateTime());
    }
    return times;
}

// ============== Tag Hierarchy / Combining ==============

bool TagManager::setTagParent(qint64 tagId, qint64 parentId)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE tags SET parent_id = ? WHERE id = ?");
    query.addBindValue(parentId);
    query.addBindValue(tagId);

    if (!query.exec()) {
        qWarning() << "Failed to set tag parent:" << query.lastError().text();
        return false;
    }

    if (m_tagCache.contains(tagId)) {
        m_tagCache[tagId].parentId = parentId;
    }

    Q_EMIT tagsChanged();
    return true;
}

bool TagManager::groupTagsUnderParent(const QString& parentName, const QStringList& childNames)
{
    QString lowerParent = parentName.trimmed().toLower();
    if (lowerParent.isEmpty() || childNames.isEmpty()) {
        return false;
    }

    Tag parent = tagByName(lowerParent);
    qint64 parentId;
    if (parent.isValid()) {
        parentId = parent.id;
    } else {
        parentId = createTag(lowerParent);
        if (parentId < 0) {
            return false;
        }
    }

    bool allOk = true;
    for (const QString& childName : childNames) {
        QString lowerChild = childName.trimmed().toLower();
        if (lowerChild.isEmpty()) continue;

        Tag child = tagByName(lowerChild);
        if (!child.isValid()) {
            qWarning() << "groupTagsUnderParent: child tag not found:" << lowerChild;
            allOk = false;
            continue;
        }
        if (!setTagParent(child.id, parentId)) {
            allOk = false;
        }
    }

    return allOk;
}

bool TagManager::mergeTags(const QString& targetName, const QStringList& sourceNames)
{
    QString lowerTarget = targetName.trimmed().toLower();
    if (lowerTarget.isEmpty() || sourceNames.isEmpty()) {
        return false;
    }

    Tag target = tagByName(lowerTarget);
    qint64 targetId;
    if (target.isValid()) {
        targetId = target.id;
    } else {
        targetId = createTag(lowerTarget);
        if (targetId < 0) {
            return false;
        }
    }

    m_db.transaction();

    bool allOk = true;
    for (const QString& srcName : sourceNames) {
        QString lowerSrc = srcName.trimmed().toLower();
        if (lowerSrc.isEmpty()) continue;

        Tag src = tagByName(lowerSrc);
        if (!src.isValid()) {
            qWarning() << "mergeTags: source tag not found:" << lowerSrc;
            allOk = false;
            continue;
        }
        if (src.id == targetId) continue;

        // Move image associations: ignore conflicts (image already has target tag)
        QSqlQuery moveQuery(m_db);
        moveQuery.prepare("UPDATE OR IGNORE image_tags SET tag_id = ? WHERE tag_id = ?");
        moveQuery.addBindValue(targetId);
        moveQuery.addBindValue(src.id);
        moveQuery.exec();

        // Remove any leftover rows that conflicted (duplicates)
        QSqlQuery cleanQuery(m_db);
        cleanQuery.prepare("DELETE FROM image_tags WHERE tag_id = ?");
        cleanQuery.addBindValue(src.id);
        cleanQuery.exec();

        // Delete the source tag
        QSqlQuery delQuery(m_db);
        delQuery.prepare("DELETE FROM tags WHERE id = ?");
        delQuery.addBindValue(src.id);
        delQuery.exec();

        m_tagCache.remove(src.id);
    }

    m_db.commit();

    m_imageTagCache.clear();
    Q_EMIT tagsChanged();
    return allOk;
}

// ============== Image Sequences ==============

qint64 TagManager::createSequence(const QStringList& imagePaths, const QString& coverPath)
{
    if (imagePaths.size() < 2) return -1;

    // Remove any of these images from existing sequences first
    for (const QString& p : imagePaths) {
        qint64 existing = sequenceForImage(p);
        if (existing >= 0) breakSequence(existing);
    }

    m_db.transaction();

    QSqlQuery query(m_db);
    query.exec("INSERT INTO sequences DEFAULT VALUES");
    qint64 seqId = query.lastInsertId().toLongLong();

    query.prepare("INSERT INTO sequence_items (sequence_id, image_path, position, is_cover) VALUES (?, ?, ?, ?)");
    for (int i = 0; i < imagePaths.size(); ++i) {
        query.addBindValue(seqId);
        query.addBindValue(imagePaths[i]);
        query.addBindValue(i);
        query.addBindValue(imagePaths[i] == coverPath ? 1 : 0);
        query.exec();
    }

    m_db.commit();
    Q_EMIT sequencesChanged();
    return seqId;
}

bool TagManager::breakSequence(qint64 sequenceId)
{
    m_db.transaction();
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM sequence_items WHERE sequence_id = ?");
    query.addBindValue(sequenceId);
    query.exec();
    query.prepare("DELETE FROM sequences WHERE id = ?");
    query.addBindValue(sequenceId);
    query.exec();
    m_db.commit();
    Q_EMIT sequencesChanged();
    return true;
}

bool TagManager::setSequenceCover(qint64 sequenceId, const QString& coverPath)
{
    m_db.transaction();
    QSqlQuery query(m_db);
    query.prepare("UPDATE sequence_items SET is_cover = 0 WHERE sequence_id = ?");
    query.addBindValue(sequenceId);
    query.exec();
    query.prepare("UPDATE sequence_items SET is_cover = 1 WHERE sequence_id = ? AND image_path = ?");
    query.addBindValue(sequenceId);
    query.addBindValue(coverPath);
    query.exec();
    m_db.commit();
    Q_EMIT sequencesChanged();
    return true;
}

qint64 TagManager::sequenceForImage(const QString& imagePath) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT sequence_id FROM sequence_items WHERE image_path = ?");
    query.addBindValue(imagePath);
    if (query.exec() && query.next())
        return query.value(0).toLongLong();
    return -1;
}

QString TagManager::sequenceCover(qint64 sequenceId) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT image_path FROM sequence_items WHERE sequence_id = ? AND is_cover = 1");
    query.addBindValue(sequenceId);
    if (query.exec() && query.next())
        return query.value(0).toString();
    // Fallback to first item
    query.prepare("SELECT image_path FROM sequence_items WHERE sequence_id = ? ORDER BY position LIMIT 1");
    query.addBindValue(sequenceId);
    if (query.exec() && query.next())
        return query.value(0).toString();
    return {};
}

QStringList TagManager::sequenceImages(qint64 sequenceId) const
{
    QStringList paths;
    QSqlQuery query(m_db);
    query.prepare("SELECT image_path FROM sequence_items WHERE sequence_id = ? ORDER BY position");
    query.addBindValue(sequenceId);
    if (query.exec()) {
        while (query.next())
            paths.append(query.value(0).toString());
    }
    return paths;
}

int TagManager::sequenceCount(qint64 sequenceId) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM sequence_items WHERE sequence_id = ?");
    query.addBindValue(sequenceId);
    if (query.exec() && query.next())
        return query.value(0).toInt();
    return 0;
}

QHash<QString, int> TagManager::allSequenceCovers() const
{
    QHash<QString, int> result;
    QSqlQuery query(m_db);
    query.exec(R"(
        SELECT si.image_path, cnt.c
        FROM sequence_items si
        JOIN (SELECT sequence_id, COUNT(*) as c FROM sequence_items GROUP BY sequence_id) cnt
            ON si.sequence_id = cnt.sequence_id
        WHERE si.is_cover = 1
    )");
    while (query.next())
        result.insert(query.value(0).toString(), query.value(1).toInt());
    return result;
}

QSet<QString> TagManager::hiddenSequenceMembers() const
{
    QSet<QString> hidden;
    QSqlQuery query(m_db);
    query.exec("SELECT image_path FROM sequence_items WHERE is_cover = 0");
    while (query.next())
        hidden.insert(query.value(0).toString());
    return hidden;
}

QHash<QString, qint64> TagManager::allImageSequenceIds() const
{
    QHash<QString, qint64> result;
    QSqlQuery query(m_db);
    query.exec("SELECT image_path, sequence_id FROM sequence_items");
    while (query.next())
        result.insert(query.value(0).toString(), query.value(1).toLongLong());
    return result;
}

bool TagManager::removeFromSequence(const QString& imagePath)
{
    qint64 seqId = sequenceForImage(imagePath);
    if (seqId < 0) return false;

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM sequence_items WHERE image_path = ?");
    query.addBindValue(imagePath);
    query.exec();

    // If 1 or fewer members remain, discard the sequence
    if (sequenceCount(seqId) <= 1) {
        breakSequence(seqId);
    } else {
        // If deleted image was the cover, pick a new one
        QString cover = sequenceCover(seqId);
        if (cover.isEmpty()) {
            QStringList remaining = sequenceImages(seqId);
            if (!remaining.isEmpty())
                setSequenceCover(seqId, remaining.first());
        }
        Q_EMIT sequencesChanged();
    }
    return true;
}

} // namespace FullFrame

