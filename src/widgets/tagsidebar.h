/**
 * TagSidebar - Modern sidebar for tag management with hotkey support
 * 
 * Features:
 * - Visual tag cards with hotkey badges
 * - Click-to-assign hotkeys (1-9, A-Z)
 * - Quick tag application via keyboard
 * - Filter images by tag
 */

#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSet>
#include <QKeyEvent>

namespace FullFrame {

struct Tag;

/**
 * Individual tag card widget with hotkey support
 */
class TagCard : public QWidget
{
    Q_OBJECT

public:
    explicit TagCard(const Tag& tag, QWidget* parent = nullptr);
    
    qint64 tagId() const { return m_tagId; }
    QString tagName() const { return m_name; }
    QString hotkey() const { return m_hotkey; }
    bool isAlbumTag() const { return m_isAlbumTag; }
    
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }
    
    void setHotkey(const QString& hotkey);
    void setAwaitingHotkey(bool awaiting);
    void setAlbumTag(bool isAlbum);
    
    void setGroupParent(bool isGroup);
    bool isGroupParent() const { return m_isGroupParent; }
    void setExpanded(bool expanded);
    bool isExpanded() const { return m_expanded; }
    void setIndented(bool indented);
    bool isIndented() const { return m_indented; }

Q_SIGNALS:
    void clicked(qint64 tagId);
    void hotkeyClicked(qint64 tagId);
    void deleteRequested(qint64 tagId);
    void renameRequested(qint64 tagId);
    void linkToFolderRequested(qint64 tagId);
    void unlinkFromFolderRequested(qint64 tagId);
    void expandToggled(qint64 tagId);
    void supertagToggleRequested(qint64 tagId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    qint64 m_tagId;
    QString m_name;
    QString m_hotkey;
    QColor m_color;
    bool m_selected = false;
    bool m_hovered = false;
    bool m_awaitingHotkey = false;
    bool m_isAlbumTag = false;
    bool m_isGroupParent = false;
    bool m_expanded = false;
    bool m_indented = false;
    
    QRect m_hotkeyRect;
    QRect m_deleteRect;
    QRect m_expandRect;
};

/**
 * Main sidebar widget
 */
class TagSidebar : public QWidget
{
    Q_OBJECT

public:
    explicit TagSidebar(QWidget* parent = nullptr);
    ~TagSidebar() override;

    // Get selected tag IDs for filtering
    QSet<qint64> selectedTagIds() const;
    
    // Refresh tag list from database
    void refresh();
    
    // Handle hotkey press - returns true if handled
    bool handleHotkey(const QString& key);
    
    // Check if sidebar is awaiting a hotkey assignment
    qint64 awaitingHotkeyTagId() const { return m_awaitingHotkeyTagId; }

Q_SIGNALS:
    void tagFilterChanged(const QSet<qint64>& tagIds);
    void showUntaggedChanged(bool showUntagged);
    void taggingModeRequested(bool enabled);
    void tagApplied(qint64 tagId);
    void tagRemoved(qint64 tagId);

public Q_SLOTS:
    void setSelectedImagePaths(const QStringList& paths);
    void setCurrentDirectoryPaths(const QStringList& paths);
    void setTaggingModeActive(bool active);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private Q_SLOTS:
    void onCreateTag();
    void onTagCardClicked(qint64 tagId);
    void onHotkeyClicked(qint64 tagId);
    void onDeleteRequested(qint64 tagId);
    void onRenameRequested(qint64 tagId);
    void onLinkToFolderRequested(qint64 tagId);
    void onUnlinkFromFolderRequested(qint64 tagId);
    void onExpandToggled(qint64 tagId);
    void onSupertagToggleRequested(qint64 tagId);

private:
    enum SortMode { SortByCount, SortAlphabetic };
    
    void setupUI();
    void loadTags();
    void updateTagCards();
    void filterTagCards(const QString& text);
    void clearAwaitingHotkey();
    QColor generateTagColor() const;
    void applyTagToSelection(qint64 tagId);
    void removeTagFromSelection(qint64 tagId);
    void toggleTagOnSelection(qint64 tagId);
    TagCard* createAndConnectCard(const Tag& tag);

private:
    QScrollArea* m_scrollArea;
    QWidget* m_tagContainer;
    QVBoxLayout* m_tagLayout;
    QLineEdit* m_searchEdit;
    QLineEdit* m_newTagEdit;
    QPushButton* m_createButton;
    QPushButton* m_untaggedButton;
    QPushButton* m_taggingModeButton;
    QPushButton* m_sortButton;
    QLabel* m_statusLabel;
    QLabel* m_selectionLabel;
    
    QList<TagCard*> m_tagCards;
    QSet<qint64> m_selectedTags;
    QSet<qint64> m_expandedGroups;
    QStringList m_selectedImagePaths;
    QStringList m_currentDirPaths;
    
    qint64 m_awaitingHotkeyTagId = -1;
    bool m_showUntagged = false;
    SortMode m_sortMode = SortByCount;
};

} // namespace FullFrame
