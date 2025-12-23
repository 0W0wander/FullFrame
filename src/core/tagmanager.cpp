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

    return true;
}

// ============== Tag Management ==============

qint64 TagManager::createTag(const QString& name, const QString& color, qint64 parentId)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO tags (name, color, parent_id) VALUES (?, ?, ?)");
    query.addBindValue(name);
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
    newTag.name = name;
    newTag.color = color;
    newTag.parentId = parentId;
    m_tagCache.insert(tagId, newTag);

    Q_EMIT tagCreated(tagId, name);
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
    QSqlQuery query(m_db);
    query.prepare("UPDATE tags SET name = ? WHERE id = ?");
    query.addBindValue(newName);
    query.addBindValue(tagId);

    if (!query.exec()) {
        return false;
    }

    if (m_tagCache.contains(tagId)) {
        m_tagCache[tagId].name = newName;
    }

    Q_EMIT tagRenamed(tagId, newName);
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
    query.prepare("SELECT id, name, color, hotkey, parent_id FROM tags WHERE id = ?");
    query.addBindValue(tagId);

    if (query.exec() && query.next()) {
        Tag t;
        t.id = query.value(0).toLongLong();
        t.name = query.value(1).toString();
        t.color = query.value(2).toString();
        t.hotkey = query.value(3).toString();
        t.parentId = query.value(4).toLongLong();
        m_tagCache.insert(tagId, t);
        return t;
    }

    return Tag();
}

Tag TagManager::tagByName(const QString& name) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT id, name, color, hotkey, parent_id FROM tags WHERE name = ?");
    query.addBindValue(name);

    if (query.exec() && query.next()) {
        Tag t;
        t.id = query.value(0).toLongLong();
        t.name = query.value(1).toString();
        t.color = query.value(2).toString();
        t.hotkey = query.value(3).toString();
        t.parentId = query.value(4).toLongLong();
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
    query.prepare("SELECT id, name, color, hotkey, parent_id FROM tags WHERE hotkey = ?");
    query.addBindValue(hotkey);

    if (query.exec() && query.next()) {
        Tag t;
        t.id = query.value(0).toLongLong();
        t.name = query.value(1).toString();
        t.color = query.value(2).toString();
        t.hotkey = query.value(3).toString();
        t.parentId = query.value(4).toLongLong();
        return t;
    }

    return Tag();
}

QList<Tag> TagManager::allTags() const
{
    QList<Tag> tags;
    QSqlQuery query(m_db);
    query.exec("SELECT id, name, color, hotkey, parent_id FROM tags ORDER BY name");

    while (query.next()) {
        Tag t;
        t.id = query.value(0).toLongLong();
        t.name = query.value(1).toString();
        t.color = query.value(2).toString();
        t.hotkey = query.value(3).toString();
        t.parentId = query.value(4).toLongLong();
        tags.append(t);
        m_tagCache.insert(t.id, t);
    }

    return tags;
}

QList<Tag> TagManager::childTags(qint64 parentId) const
{
    QList<Tag> tags;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, name, color, hotkey, parent_id FROM tags WHERE parent_id = ? ORDER BY name");
    query.addBindValue(parentId);

    if (query.exec()) {
        while (query.next()) {
            Tag t;
            t.id = query.value(0).toLongLong();
            t.name = query.value(1).toString();
            t.color = query.value(2).toString();
            t.hotkey = query.value(3).toString();
            t.parentId = query.value(4).toLongLong();
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

bool TagManager::tagImage(const QString& imagePath, qint64 tagId)
{
    qint64 imgId = getOrCreateImageId(imagePath);
    if (imgId < 0) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare("INSERT OR IGNORE INTO image_tags (image_id, tag_id) VALUES (?, ?)");
    query.addBindValue(imgId);
    query.addBindValue(tagId);

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
        SELECT t.id, t.name, t.color, t.hotkey, t.parent_id 
        FROM tags t 
        JOIN image_tags it ON t.id = it.tag_id 
        WHERE it.image_id = ?
        ORDER BY t.name
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

} // namespace FullFrame

