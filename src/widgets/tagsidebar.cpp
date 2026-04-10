/**
 * TagSidebar implementation - Modern tag management with hotkeys
 */

#include "tagsidebar.h"
#include "tagmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QInputDialog>
#include <QApplication>
#include <QScrollBar>
#include <QTimer>
#include <QFileDialog>
#include <algorithm>

namespace FullFrame {

// ============== TagCard Implementation ==============

TagCard::TagCard(const Tag& tag, QWidget* parent)
    : QWidget(parent)
    , m_tagId(tag.id)
    , m_name(tag.name)
    , m_hotkey(tag.hotkey)
    , m_color(tag.color.isEmpty() ? QColor(100, 100, 100) : QColor(tag.color))
    , m_isAlbumTag(tag.isAlbumTag())
{
    setFixedHeight(32);  // Compact height
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
}

void TagCard::setSelected(bool selected)
{
    if (m_selected != selected) {
        m_selected = selected;
        update();
    }
}

void TagCard::setHotkey(const QString& hotkey)
{
    m_hotkey = hotkey;
    update();
}

void TagCard::setAwaitingHotkey(bool awaiting)
{
    m_awaitingHotkey = awaiting;
    update();
}

void TagCard::setAlbumTag(bool isAlbum)
{
    m_isAlbumTag = isAlbum;
    update();
}

void TagCard::setGroupParent(bool isGroup)
{
    m_isGroupParent = isGroup;
    update();
}

void TagCard::setExpanded(bool expanded)
{
    m_expanded = expanded;
    update();
}

void TagCard::setIndented(bool indented)
{
    m_indented = indented;
    update();
}

void TagCard::setCount(int count)
{
    m_count = count;
    update();
}

void TagCard::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    int indent = m_indented ? 16 : 0;
    QRect r = rect().adjusted(2 + indent, 2, -2, -2);
    
    // Background
    QColor bgColor = m_selected ? QColor(0, 120, 215, 50) : 
                     m_hovered ? QColor(255, 255, 255, 12) : 
                     QColor(40, 40, 40);
    
    QPainterPath path;
    path.addRoundedRect(r, 4, 4);
    painter.fillPath(path, bgColor);
    
    // Selection/hover border
    if (m_selected) {
        painter.setPen(QPen(QColor(0, 120, 215), 1.5));
        painter.drawRoundedRect(r, 4, 4);
    } else if (m_hovered) {
        painter.setPen(QPen(QColor(70, 70, 70), 1));
        painter.drawRoundedRect(r, 4, 4);
    }
    
    int contentLeft = r.left();
    
    // Expand/collapse triangle for group parents
    if (m_isGroupParent) {
        int triSize = 8;
        int triX = r.left() + 4;
        int triY = r.top() + (r.height() - triSize) / 2;
        m_expandRect = QRect(triX - 2, triY - 2, triSize + 4, triSize + 4);
        
        QPainterPath triPath;
        if (m_expanded) {
            // Down-pointing triangle
            triPath.moveTo(triX, triY);
            triPath.lineTo(triX + triSize, triY);
            triPath.lineTo(triX + triSize / 2.0, triY + triSize);
        } else {
            // Right-pointing triangle
            triPath.moveTo(triX, triY);
            triPath.lineTo(triX + triSize, triY + triSize / 2.0);
            triPath.lineTo(triX, triY + triSize);
        }
        triPath.closeSubpath();
        
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(160, 160, 160));
        painter.drawPath(triPath);
        
        contentLeft = r.left() + triSize + 6;
    } else {
        m_expandRect = QRect();
    }
    
    // Color dot or folder icon
    int dotSize = 8;
    int dotY = r.top() + (r.height() - dotSize) / 2;
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_color);
    if (m_isAlbumTag) {
        QFont iconFont = painter.font();
        iconFont.setPixelSize(12);
        painter.setFont(iconFont);
        painter.setPen(m_color);
        painter.drawText(QRect(contentLeft + 3, r.top(), 16, r.height()), Qt::AlignCenter, QString::fromUtf8("\xF0\x9F\x93\x81"));
    } else {
        painter.drawEllipse(contentLeft + 6, dotY, dotSize, dotSize);
    }
    
    int dotAreaWidth = 20;
    
    // Hotkey badge (right side)
    int badgeW = 20;
    int badgeH = 18;
    int badgeMargin = 6;
    m_hotkeyRect = QRect(r.right() - badgeW - badgeMargin, 
                         r.top() + (r.height() - badgeH) / 2,
                         badgeW, badgeH);
    
    if (m_awaitingHotkey) {
        painter.setPen(QPen(QColor(255, 193, 7), 1.5));
        painter.setBrush(QColor(255, 193, 7, 30));
        painter.drawRoundedRect(m_hotkeyRect, 3, 3);
        
        painter.setPen(QColor(255, 193, 7));
        QFont font = painter.font();
        font.setPixelSize(10);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(m_hotkeyRect, Qt::AlignCenter, "?");
    } else if (!m_hotkey.isEmpty()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(76, 175, 80));
        painter.drawRoundedRect(m_hotkeyRect, 3, 3);
        
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPixelSize(10);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(m_hotkeyRect, Qt::AlignCenter, m_hotkey.toUpper());
    } else {
        painter.setPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(m_hotkeyRect, 3, 3);
    }
    
    // Delete button (only on hover)
    if (m_hovered) {
        m_deleteRect = QRect(m_hotkeyRect.left() - 18, m_hotkeyRect.top(), 16, badgeH);
        painter.setPen(QColor(160, 70, 70));
        QFont font = painter.font();
        font.setPixelSize(12);
        painter.setFont(font);
        painter.drawText(m_deleteRect, Qt::AlignCenter, "×");
    } else {
        m_deleteRect = QRect();
    }
    
    // Tag name (with optional count)
    int textLeft = contentLeft + dotAreaWidth;
    int textRight = m_hovered ? m_deleteRect.left() - 4 : m_hotkeyRect.left() - 4;
    QRect textRect(textLeft, r.top(), textRight - textLeft, r.height());
    
    QFont font = painter.font();
    font.setPixelSize(11);
    font.setBold(false);
    painter.setFont(font);
    QFontMetrics fm(font);
    
    if (m_count >= 0) {
        QString countStr = QString(" (%1)").arg(m_count);
        int countWidth = fm.horizontalAdvance(countStr);
        int nameAvail = textRect.width() - countWidth;
        QString elidedName = fm.elidedText(m_name, Qt::ElideRight, qMax(nameAvail, 20));
        
        painter.setPen(m_indented ? QColor(170, 170, 170) : QColor(200, 200, 200));
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elidedName);
        
        int nameDrawn = fm.horizontalAdvance(elidedName);
        QRect countRect(textLeft + nameDrawn, r.top(), countWidth, r.height());
        painter.setPen(QColor(120, 120, 120));
        painter.drawText(countRect, Qt::AlignVCenter | Qt::AlignLeft, countStr);
    } else {
        QString elidedText = fm.elidedText(m_name, Qt::ElideRight, textRect.width());
        painter.setPen(m_indented ? QColor(170, 170, 170) : QColor(200, 200, 200));
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elidedText);
    }
}

void TagCard::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_isGroupParent && m_expandRect.isValid() && m_expandRect.contains(event->pos())) {
            Q_EMIT expandToggled(m_tagId);
        } else if (m_hotkeyRect.contains(event->pos())) {
            Q_EMIT hotkeyClicked(m_tagId);
        } else if (m_deleteRect.isValid() && m_deleteRect.contains(event->pos())) {
            Q_EMIT deleteRequested(m_tagId);
        } else {
            Q_EMIT clicked(m_tagId);
        }
    }
}

void TagCard::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    
    QAction* renameAction = menu.addAction("Rename");
    
    QAction* supertagAction = menu.addAction("Toggle Supertag on Selection");
    
    menu.addSeparator();
    
    QAction* linkAction = nullptr;
    QAction* unlinkAction = nullptr;
    if (m_isAlbumTag) {
        unlinkAction = menu.addAction("Unlink from Folder");
    } else {
        linkAction = menu.addAction("Link to Folder...");
    }
    
    menu.addSeparator();
    
    QAction* deleteAction = menu.addAction("Delete");
    
    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == renameAction) {
        Q_EMIT renameRequested(m_tagId);
    } else if (chosen == supertagAction) {
        Q_EMIT supertagToggleRequested(m_tagId);
    } else if (chosen == deleteAction) {
        Q_EMIT deleteRequested(m_tagId);
    } else if (linkAction && chosen == linkAction) {
        Q_EMIT linkToFolderRequested(m_tagId);
    } else if (unlinkAction && chosen == unlinkAction) {
        Q_EMIT unlinkFromFolderRequested(m_tagId);
    }
}

void TagCard::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event)
    m_hovered = true;
    update();
}

void TagCard::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)
    m_hovered = false;
    update();
}

// ============== TagSidebar Implementation ==============

TagSidebar::TagSidebar(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    
    // Connect to TagManager signals
    connect(TagManager::instance(), &TagManager::tagsChanged,
            this, &TagSidebar::loadTags);
}

TagSidebar::~TagSidebar() = default;

void TagSidebar::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 10, 8, 8);
    layout->setSpacing(6);

    // Header row with title and hint
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(0);
    
    QLabel* titleLabel = new QLabel("TAGS", this);
    titleLabel->setStyleSheet(R"(
        font-size: 10px;
        font-weight: bold;
        color: #707070;
        letter-spacing: 1px;
    )");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    
    QLabel* hintLabel = new QLabel("click □ for hotkey", this);
    hintLabel->setStyleSheet("font-size: 9px; color: #505050;");
    headerLayout->addWidget(hintLabel);
    
    layout->addLayout(headerLayout);

    // Button row: Show Untagged + Tagging Mode
    QHBoxLayout* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(4);
    
    // "Show Untagged" button
    m_untaggedButton = new QPushButton("⊘ Untagged", this);
    m_untaggedButton->setCheckable(true);
    m_untaggedButton->setStyleSheet(R"(
        QPushButton {
            background-color: #2a2a2a;
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            padding: 6px 8px;
            color: #a0a0a0;
            font-size: 10px;
        }
        QPushButton:hover {
            background-color: #333333;
            border-color: #4a4a4a;
        }
        QPushButton:checked {
            background-color: rgba(255, 152, 0, 0.15);
            border-color: #ff9800;
            color: #ff9800;
        }
    )");
    connect(m_untaggedButton, &QPushButton::toggled, this, [this](bool checked) {
        m_showUntagged = checked;
        // Clear tag filter when showing untagged
        if (checked) {
            m_selectedTags.clear();
            for (TagCard* card : m_tagCards) {
                card->setSelected(false);
            }
        }
        Q_EMIT showUntaggedChanged(checked);
    });
    buttonRow->addWidget(m_untaggedButton);
    
    // "Tagging Mode" button
    m_taggingModeButton = new QPushButton("🏷️ Tagging", this);
    m_taggingModeButton->setCheckable(true);
    m_taggingModeButton->setStyleSheet(R"(
        QPushButton {
            background-color: #2a2a2a;
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            padding: 6px 8px;
            color: #a0a0a0;
            font-size: 10px;
        }
        QPushButton:hover {
            background-color: #333333;
            border-color: #4a4a4a;
        }
        QPushButton:checked {
            background-color: rgba(0, 120, 215, 0.2);
            border-color: #0078d4;
            color: #4da6ff;
        }
    )");
    connect(m_taggingModeButton, &QPushButton::toggled, this, [this](bool checked) {
        Q_EMIT taggingModeRequested(checked);
    });
    buttonRow->addWidget(m_taggingModeButton);
    
    layout->addLayout(buttonRow);

    // Search row
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search tags...");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet(R"(
        QLineEdit {
            background-color: #2a2a2a;
            border: 1px solid #3a3a3a;
            border-radius: 3px;
            padding: 4px 8px;
            color: #d0d0d0;
            font-size: 11px;
        }
        QLineEdit:focus {
            border-color: #0078d4;
        }
        QLineEdit::placeholder {
            color: #606060;
        }
    )");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &TagSidebar::filterTagCards);
    layout->addWidget(m_searchEdit);

    QString scrollAreaStyle = R"(
        QScrollArea {
            background-color: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background: #2d2d2d;
            width: 8px;
            margin: 0;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: #505050;
            border-radius: 4px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background: #606060;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
    )";

    QString sectionLabelStyle = "font-size: 9px; font-weight: bold; color: #606060; "
                                "letter-spacing: 1px; padding: 2px 0;";

    // === RECENT section ===
    QLabel* recentLabel = new QLabel("RECENT", this);
    recentLabel->setStyleSheet(sectionLabelStyle);
    layout->addWidget(recentLabel);

    m_recentScrollArea = new QScrollArea(this);
    m_recentScrollArea->setWidgetResizable(true);
    m_recentScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_recentScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_recentScrollArea->setStyleSheet(scrollAreaStyle);

    m_recentContainer = new QWidget();
    m_recentContainer->setStyleSheet("background-color: transparent;");
    m_recentLayout = new QVBoxLayout(m_recentContainer);
    m_recentLayout->setContentsMargins(0, 0, 0, 0);
    m_recentLayout->setSpacing(2);
    m_recentLayout->addStretch();

    m_recentScrollArea->setWidget(m_recentContainer);
    layout->addWidget(m_recentScrollArea, 1);

    // === Divider ===
    QFrame* sectionDivider = new QFrame(this);
    sectionDivider->setFrameShape(QFrame::HLine);
    sectionDivider->setStyleSheet("background-color: #333; max-height: 1px;");
    layout->addWidget(sectionDivider);

    // === POPULAR section ===
    QLabel* popularLabel = new QLabel("POPULAR", this);
    popularLabel->setStyleSheet(sectionLabelStyle);
    layout->addWidget(popularLabel);

    m_popularScrollArea = new QScrollArea(this);
    m_popularScrollArea->setWidgetResizable(true);
    m_popularScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_popularScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_popularScrollArea->setStyleSheet(scrollAreaStyle);

    m_popularContainer = new QWidget();
    m_popularContainer->setStyleSheet("background-color: transparent;");
    m_popularLayout = new QVBoxLayout(m_popularContainer);
    m_popularLayout->setContentsMargins(0, 0, 0, 0);
    m_popularLayout->setSpacing(2);
    m_popularLayout->addStretch();

    m_popularScrollArea->setWidget(m_popularContainer);
    layout->addWidget(m_popularScrollArea, 1);

    // Status label for hotkey assignment
    m_statusLabel = new QLabel("", this);
    m_statusLabel->setStyleSheet(R"(
        font-size: 9px;
        color: #ffc107;
        padding: 4px 6px;
        background-color: rgba(255, 193, 7, 0.1);
        border-radius: 3px;
    )");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->hide();
    layout->addWidget(m_statusLabel);

    // Separator
    QFrame* separator1 = new QFrame(this);
    separator1->setFrameShape(QFrame::HLine);
    separator1->setStyleSheet("background-color: #333; max-height: 1px;");
    layout->addWidget(separator1);

    // New tag input row - compact
    QHBoxLayout* newTagLayout = new QHBoxLayout();
    newTagLayout->setSpacing(4);
    
    m_newTagEdit = new QLineEdit(this);
    m_newTagEdit->setPlaceholderText("New tag...");
    m_newTagEdit->setStyleSheet(R"(
        QLineEdit {
            background-color: #2a2a2a;
            border: 1px solid #3a3a3a;
            border-radius: 3px;
            padding: 5px 8px;
            color: #d0d0d0;
            font-size: 11px;
        }
        QLineEdit:focus {
            border-color: #0078d4;
        }
        QLineEdit::placeholder {
            color: #606060;
        }
    )");
    newTagLayout->addWidget(m_newTagEdit);

    m_createButton = new QPushButton("+", this);
    m_createButton->setFixedSize(26, 26);
    m_createButton->setStyleSheet(R"(
        QPushButton {
            background-color: #2d6830;
            border: none;
            border-radius: 3px;
            color: white;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #3a7a3d;
        }
        QPushButton:pressed {
            background-color: #245628;
        }
    )");
    connect(m_createButton, &QPushButton::clicked, this, &TagSidebar::onCreateTag);
    connect(m_newTagEdit, &QLineEdit::returnPressed, this, &TagSidebar::onCreateTag);
    newTagLayout->addWidget(m_createButton);
    
    layout->addLayout(newTagLayout);

    // Selection info label - compact
    m_selectionLabel = new QLabel("No selection", this);
    m_selectionLabel->setStyleSheet("font-size: 9px; color: #606060; padding: 4px 0;");
    m_selectionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_selectionLabel);

    // Set fixed width - narrower
    setFixedWidth(200);

    // Style
    setStyleSheet("background-color: #1e1e1e;");
    
    // Focus policy for keyboard input
    setFocusPolicy(Qt::StrongFocus);
}

void TagSidebar::refresh()
{
    loadTags();
}

void TagSidebar::loadTags()
{
    updateTagCards();
}

TagCard* TagSidebar::createAndConnectCard(const Tag& tag, QWidget* parent)
{
    TagCard* card = new TagCard(tag, parent);
    connect(card, &TagCard::clicked, this, &TagSidebar::onTagCardClicked);
    connect(card, &TagCard::hotkeyClicked, this, &TagSidebar::onHotkeyClicked);
    connect(card, &TagCard::deleteRequested, this, &TagSidebar::onDeleteRequested);
    connect(card, &TagCard::renameRequested, this, &TagSidebar::onRenameRequested);
    connect(card, &TagCard::linkToFolderRequested, this, &TagSidebar::onLinkToFolderRequested);
    connect(card, &TagCard::unlinkFromFolderRequested, this, &TagSidebar::onUnlinkFromFolderRequested);
    connect(card, &TagCard::expandToggled, this, &TagSidebar::onExpandToggled);
    connect(card, &TagCard::supertagToggleRequested, this, &TagSidebar::onSupertagToggleRequested);
    
    card->setSelected(m_selectedTags.contains(tag.id));
    if (m_awaitingHotkeyTagId == tag.id) {
        card->setAwaitingHotkey(true);
    }
    return card;
}

void TagSidebar::populateSection(QVBoxLayout* layout, const QList<Tag>& rootTags,
                                 const QHash<qint64, QList<Tag>>& childMap,
                                 const QHash<qint64, int>& counts)
{
    QWidget* container = qobject_cast<QWidget*>(layout->parentWidget());
    bool showCounts = !counts.isEmpty();
    
    for (const Tag& tag : rootTags) {
        bool hasChildren = childMap.contains(tag.id);
        
        TagCard* card = createAndConnectCard(tag, container);
        if (hasChildren) {
            card->setGroupParent(true);
            card->setExpanded(m_expandedGroups.contains(tag.id));
        }
        if (showCounts)
            card->setCount(counts.value(tag.id, 0));
        layout->addWidget(card);
        m_tagCards.append(card);
        
        if (hasChildren && m_expandedGroups.contains(tag.id)) {
            for (const Tag& child : childMap.value(tag.id)) {
                TagCard* childCard = createAndConnectCard(child, container);
                childCard->setIndented(true);
                if (showCounts)
                    childCard->setCount(counts.value(child.id, 0));
                layout->addWidget(childCard);
                m_tagCards.append(childCard);
            }
        }
    }
}

void TagSidebar::updateTagCards()
{
    // Clear all existing cards from both sections
    for (TagCard* card : m_tagCards) {
        card->deleteLater();
    }
    m_tagCards.clear();
    
    // Clear recent layout
    while (m_recentLayout->count() > 0) {
        QLayoutItem* item = m_recentLayout->takeAt(0);
        delete item;
    }
    // Clear popular layout
    while (m_popularLayout->count() > 0) {
        QLayoutItem* item = m_popularLayout->takeAt(0);
        delete item;
    }
    
    if (!TagManager::instance()->isInitialized()) {
        m_recentLayout->addStretch();
        m_popularLayout->addStretch();
        return;
    }

    QList<Tag> allTags = TagManager::instance()->allTags();
    QHash<qint64, int> counts = TagManager::instance()->tagImageCounts(m_currentDirPaths);
    QHash<qint64, QDateTime> lastUsed = TagManager::instance()->tagLastUsedTimes();
    
    // Separate root tags from children
    QList<Tag> rootTags;
    QHash<qint64, QList<Tag>> childMap;
    
    for (const Tag& tag : allTags) {
        if (tag.parentId >= 0) {
            childMap[tag.parentId].append(tag);
        } else {
            rootTags.append(tag);
        }
    }
    
    const QSet<qint64>& selected = m_selectedTags;

    // --- Recent section: sort by most recently used, then by name ---
    QList<Tag> recentRoots = rootTags;
    auto recentSort = [&selected, &lastUsed](const Tag& a, const Tag& b) {
        bool aSelected = selected.contains(a.id);
        bool bSelected = selected.contains(b.id);
        if (aSelected != bSelected) return aSelected;
        QDateTime ta = lastUsed.value(a.id);
        QDateTime tb = lastUsed.value(b.id);
        if (ta.isValid() != tb.isValid()) return ta.isValid();
        if (ta.isValid() && ta != tb) return ta > tb;
        return a.name.toLower() < b.name.toLower();
    };
    std::sort(recentRoots.begin(), recentRoots.end(), recentSort);
    
    QHash<qint64, QList<Tag>> recentChildMap = childMap;
    for (auto it = recentChildMap.begin(); it != recentChildMap.end(); ++it) {
        std::sort(it.value().begin(), it.value().end(), recentSort);
    }
    
    populateSection(m_recentLayout, recentRoots, recentChildMap);
    m_recentLayout->addStretch();

    // --- Popular section: sort by image count desc, then by name ---
    QList<Tag> popularRoots = rootTags;
    auto popularSort = [&selected, &counts](const Tag& a, const Tag& b) {
        bool aSelected = selected.contains(a.id);
        bool bSelected = selected.contains(b.id);
        if (aSelected != bSelected) return aSelected;
        int ca = counts.value(a.id, 0);
        int cb = counts.value(b.id, 0);
        if (ca != cb) return ca > cb;
        return a.name.toLower() < b.name.toLower();
    };
    std::sort(popularRoots.begin(), popularRoots.end(), popularSort);
    
    QHash<qint64, QList<Tag>> popularChildMap = childMap;
    for (auto it = popularChildMap.begin(); it != popularChildMap.end(); ++it) {
        std::sort(it.value().begin(), it.value().end(), popularSort);
    }
    
    populateSection(m_popularLayout, popularRoots, popularChildMap, counts);
    m_popularLayout->addStretch();

    // Re-apply search filter
    filterTagCards(m_searchEdit->text());
}

void TagSidebar::filterTagCards(const QString& text)
{
    QString filter = text.trimmed().toLower();
    if (filter.isEmpty()) {
        for (TagCard* card : m_tagCards) {
            card->setVisible(true);
        }
        return;
    }
    
    // Build set of IDs that directly match
    QSet<qint64> matchIds;
    for (TagCard* card : m_tagCards) {
        if (card->tagName().toLower().contains(filter)) {
            matchIds.insert(card->tagId());
        }
    }
    
    // Build set of parent IDs whose children match (show parent when child matches)
    // and child IDs whose parent matches (show children when parent matches)
    QSet<qint64> visibleIds = matchIds;
    for (TagCard* card : m_tagCards) {
        qint64 id = card->tagId();
        if (matchIds.contains(id)) {
            // If this is a group parent, show its children too
            if (card->isGroupParent()) {
                for (TagCard* other : m_tagCards) {
                    if (other->isIndented()) {
                        // Find child cards that come after this parent
                        // They share the same parent in the tag manager
                        Tag otherTag = TagManager::instance()->tag(other->tagId());
                        if (otherTag.parentId == id) {
                            visibleIds.insert(other->tagId());
                        }
                    }
                }
            }
            // If this is a child, show its parent too
            if (card->isIndented()) {
                Tag t = TagManager::instance()->tag(id);
                if (t.parentId >= 0) {
                    visibleIds.insert(t.parentId);
                }
            }
        }
    }
    
    for (TagCard* card : m_tagCards) {
        card->setVisible(visibleIds.contains(card->tagId()));
    }
}

void TagSidebar::onExpandToggled(qint64 tagId)
{
    if (m_expandedGroups.contains(tagId)) {
        m_expandedGroups.remove(tagId);
    } else {
        m_expandedGroups.insert(tagId);
    }
    updateTagCards();
}

void TagSidebar::onSupertagToggleRequested(qint64 tagId)
{
    if (m_selectedImagePaths.isEmpty()) {
        m_statusLabel->setStyleSheet(
            "font-size: 9px; color: #ff9800; padding: 4px 6px; "
            "background-color: rgba(255, 152, 0, 0.15); border-radius: 3px;");
        m_statusLabel->setText("Select images first to toggle supertag");
        m_statusLabel->show();
        QTimer::singleShot(2000, this, [this]() {
            if (m_awaitingHotkeyTagId < 0) m_statusLabel->hide();
        });
        return;
    }
    
    Tag tag = TagManager::instance()->tag(tagId);
    if (!tag.isValid()) return;
    
    // Check if all selected images already have this as a supertag
    bool allSupertag = true;
    for (const QString& path : m_selectedImagePaths) {
        if (!TagManager::instance()->isSupertag(path, tagId)) {
            allSupertag = false;
            break;
        }
    }
    
    for (const QString& path : m_selectedImagePaths) {
        TagManager::instance()->setSupertag(path, tagId, !allSupertag);
    }
    
    QString action = allSupertag ? "Removed supertag" : "Set supertag";
    m_statusLabel->setStyleSheet(
        "font-size: 9px; color: #9c27b0; padding: 4px 6px; "
        "background-color: rgba(156, 39, 176, 0.15); border-radius: 3px;");
    m_statusLabel->setText(QString("%1 \"%2\" on %3 image(s)")
        .arg(action).arg(tag.name).arg(m_selectedImagePaths.size()));
    m_statusLabel->show();
    QTimer::singleShot(2000, this, [this]() {
        if (m_awaitingHotkeyTagId < 0) m_statusLabel->hide();
    });
}

QSet<qint64> TagSidebar::selectedTagIds() const
{
    return m_selectedTags;
}

void TagSidebar::setSelectedImagePaths(const QStringList& paths)
{
    m_selectedImagePaths = paths;
    
    if (paths.isEmpty()) {
        m_selectionLabel->setText("No selection");
        m_selectionLabel->setStyleSheet("font-size: 9px; color: #606060; padding: 4px 0;");
    } else {
        m_selectionLabel->setText(QString("%1 selected").arg(paths.size()));
        m_selectionLabel->setStyleSheet("font-size: 9px; color: #4caf50; padding: 4px 0; font-weight: bold;");
    }
}

void TagSidebar::setCurrentDirectoryPaths(const QStringList& paths)
{
    m_currentDirPaths = paths;
    updateTagCards();
}

void TagSidebar::setTaggingModeActive(bool active)
{
    m_taggingModeButton->blockSignals(true);
    m_taggingModeButton->setChecked(active);
    m_taggingModeButton->blockSignals(false);
}

bool TagSidebar::handleHotkey(const QString& key)
{
    // If we're waiting for hotkey assignment
    if (m_awaitingHotkeyTagId >= 0) {
        TagManager::instance()->setTagHotkey(m_awaitingHotkeyTagId, key);
        clearAwaitingHotkey();
        return true;
    }
    
    // Otherwise, try to toggle the tag with this hotkey
    Tag tag = TagManager::instance()->tagByHotkey(key);
    if (tag.isValid()) {
        toggleTagOnSelection(tag.id);
        return true;
    }
    
    return false;
}

void TagSidebar::keyPressEvent(QKeyEvent* event)
{
    if (m_awaitingHotkeyTagId >= 0) {
        // Escape cancels hotkey assignment
        if (event->key() == Qt::Key_Escape) {
            clearAwaitingHotkey();
            return;
        }
        
        // Get the key text
        QString keyText;
        
        // Handle number keys
        if (event->key() >= Qt::Key_0 && event->key() <= Qt::Key_9) {
            keyText = QString::number(event->key() - Qt::Key_0);
        }
        // Handle letter keys
        else if (event->key() >= Qt::Key_A && event->key() <= Qt::Key_Z) {
            keyText = QChar('A' + (event->key() - Qt::Key_A));
        }
        // Handle function keys
        else if (event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F12) {
            keyText = QString("F%1").arg(event->key() - Qt::Key_F1 + 1);
        }
        
        if (!keyText.isEmpty()) {
            TagManager::instance()->setTagHotkey(m_awaitingHotkeyTagId, keyText);
            clearAwaitingHotkey();
        }
        return;
    }
    
    QWidget::keyPressEvent(event);
}

void TagSidebar::onCreateTag()
{
    QString name = m_newTagEdit->text().trimmed();
    if (name.isEmpty()) {
        return;
    }

    // Re-use existing tag if one with this name already exists
    Tag existing = TagManager::instance()->tagByName(name);
    qint64 tagId = existing.isValid() ? existing.id : -1;

    if (tagId < 0) {
        QColor color = generateTagColor();
        tagId = TagManager::instance()->createTag(name, color.name());
    }

    if (tagId < 0) {
        QMessageBox::warning(this, "Error", "Failed to create tag. Name may already exist.");
        return;
    }

    m_newTagEdit->clear();

    if (!m_selectedImagePaths.isEmpty()) {
        applyTagToSelection(tagId);
    }
}

void TagSidebar::onTagCardClicked(qint64 tagId)
{
    // Clear any awaiting state
    clearAwaitingHotkey();
    
    // Deselect "Show Untagged" when selecting a tag
    if (m_showUntagged) {
        m_showUntagged = false;
        m_untaggedButton->setChecked(false);
    }
    
    // Toggle selection
    if (m_selectedTags.contains(tagId)) {
        m_selectedTags.remove(tagId);
    } else {
        m_selectedTags.insert(tagId);
    }
    
    // Re-sort so selected tags appear at top
    updateTagCards();
    
    // Build expanded filter set: include children of any selected group parents
    QSet<qint64> expandedFilter = m_selectedTags;
    for (qint64 id : m_selectedTags) {
        QList<Tag> children = TagManager::instance()->childTags(id);
        for (const Tag& child : children) {
            expandedFilter.insert(child.id);
        }
    }
    
    Q_EMIT tagFilterChanged(expandedFilter);
}

void TagSidebar::onHotkeyClicked(qint64 tagId)
{
    // Clear previous awaiting state
    clearAwaitingHotkey();
    
    // Set this tag as awaiting hotkey
    m_awaitingHotkeyTagId = tagId;
    
    for (TagCard* card : m_tagCards) {
        if (card->tagId() == tagId) {
            card->setAwaitingHotkey(true);
        }
    }
    
    m_statusLabel->setText("Press key (0-9, A-Z) or ESC");
    m_statusLabel->show();
    
    // Grab keyboard focus
    setFocus();
}

void TagSidebar::onDeleteRequested(qint64 tagId)
{
    Tag tag = TagManager::instance()->tag(tagId);
    
    int result = QMessageBox::question(this, "Delete Tag",
        QString("Delete tag \"%1\"?").arg(tag.name),
        QMessageBox::Yes | QMessageBox::No);
    
    if (result == QMessageBox::Yes) {
        m_selectedTags.remove(tagId);
        TagManager::instance()->deleteTag(tagId);
    }
}

void TagSidebar::onRenameRequested(qint64 tagId)
{
    Tag tag = TagManager::instance()->tag(tagId);
    if (!tag.isValid()) {
        return;
    }
    
    bool ok = false;
    QString newName = QInputDialog::getText(this, "Rename Tag",
        QString("New name for \"%1\":").arg(tag.name),
        QLineEdit::Normal, tag.name, &ok);
    
    newName = newName.trimmed();
    
    if (!ok || newName.isEmpty() || newName == tag.name) {
        return;
    }
    
    // Check if a tag with the new name already exists
    Tag existing = TagManager::instance()->tagByName(newName);
    if (existing.isValid()) {
        QMessageBox::warning(this, "Rename Tag",
            QString("A tag named \"%1\" already exists.").arg(newName));
        return;
    }
    
    if (!TagManager::instance()->renameTag(tagId, newName)) {
        QMessageBox::warning(this, "Rename Tag", "Failed to rename tag.");
    }
}

void TagSidebar::onLinkToFolderRequested(qint64 tagId)
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Album Folder",
        QDir::homePath());
    if (!dir.isEmpty()) {
        TagManager::instance()->setTagAlbumPath(tagId, dir);
    }
}

void TagSidebar::onUnlinkFromFolderRequested(qint64 tagId)
{
    TagManager::instance()->clearTagAlbumPath(tagId);
}

void TagSidebar::clearAwaitingHotkey()
{
    if (m_awaitingHotkeyTagId >= 0) {
        for (TagCard* card : m_tagCards) {
            if (card->tagId() == m_awaitingHotkeyTagId) {
                card->setAwaitingHotkey(false);
            }
        }
        m_awaitingHotkeyTagId = -1;
    }
    m_statusLabel->hide();
}

QColor TagSidebar::generateTagColor() const
{
    // Generate a nice saturated color from a curated palette
    static const QList<QColor> palette = {
        QColor("#e91e63"),  // Pink
        QColor("#9c27b0"),  // Purple
        QColor("#673ab7"),  // Deep Purple
        QColor("#3f51b5"),  // Indigo
        QColor("#2196f3"),  // Blue
        QColor("#03a9f4"),  // Light Blue
        QColor("#00bcd4"),  // Cyan
        QColor("#009688"),  // Teal
        QColor("#4caf50"),  // Green
        QColor("#8bc34a"),  // Light Green
        QColor("#cddc39"),  // Lime
        QColor("#ffeb3b"),  // Yellow
        QColor("#ffc107"),  // Amber
        QColor("#ff9800"),  // Orange
        QColor("#ff5722"),  // Deep Orange
    };
    
    return palette[QRandomGenerator::global()->bounded(palette.size())];
}

void TagSidebar::applyTagToSelection(qint64 tagId)
{
    if (m_selectedImagePaths.isEmpty()) {
        return;
    }
    
    Tag tag = TagManager::instance()->tag(tagId);
    TagManager::instance()->tagImages(m_selectedImagePaths, tagId);
    
    // Show brief feedback
    m_statusLabel->setStyleSheet(R"(
        font-size: 9px;
        color: #4caf50;
        padding: 4px 6px;
        background-color: rgba(76, 175, 80, 0.15);
        border-radius: 3px;
    )");
    m_statusLabel->setText(QString("Tagged %1 with \"%2\"").arg(m_selectedImagePaths.size()).arg(tag.name));
    m_statusLabel->show();
    
    // Hide after 2 seconds
    QTimer::singleShot(2000, this, [this]() {
        if (m_awaitingHotkeyTagId < 0) {  // Don't hide if awaiting hotkey
            m_statusLabel->hide();
        }
    });
    
    Q_EMIT tagApplied(tagId);
}

void TagSidebar::removeTagFromSelection(qint64 tagId)
{
    if (m_selectedImagePaths.isEmpty()) {
        return;
    }
    
    TagManager::instance()->untagImages(m_selectedImagePaths, tagId);
    Q_EMIT tagRemoved(tagId);
}

void TagSidebar::toggleTagOnSelection(qint64 tagId)
{
    if (m_selectedImagePaths.isEmpty()) {
        return;
    }
    
    Tag tag = TagManager::instance()->tag(tagId);
    if (!tag.isValid()) {
        return;
    }
    
    // Check if ALL selected images have this tag
    bool allHaveTag = true;
    for (const QString& path : m_selectedImagePaths) {
        if (!TagManager::instance()->hasTag(path, tagId)) {
            allHaveTag = false;
            break;
        }
    }
    
    if (allHaveTag) {
        // Remove tag from all selected images
        TagManager::instance()->untagImages(m_selectedImagePaths, tagId);
        
        // Show feedback
        m_statusLabel->setStyleSheet(R"(
            font-size: 9px;
            color: #f44336;
            padding: 4px 6px;
            background-color: rgba(244, 67, 54, 0.15);
            border-radius: 3px;
        )");
        m_statusLabel->setText(QString("Removed \"%1\" from %2").arg(tag.name).arg(m_selectedImagePaths.size()));
        m_statusLabel->show();
        
        Q_EMIT tagRemoved(tagId);
    } else {
        // Apply tag to all selected images
        TagManager::instance()->tagImages(m_selectedImagePaths, tagId);
        
        // Show feedback
        m_statusLabel->setStyleSheet(R"(
            font-size: 9px;
            color: #4caf50;
            padding: 4px 6px;
            background-color: rgba(76, 175, 80, 0.15);
            border-radius: 3px;
        )");
        m_statusLabel->setText(QString("Tagged %1 with \"%2\"").arg(m_selectedImagePaths.size()).arg(tag.name));
        m_statusLabel->show();
        
        Q_EMIT tagApplied(tagId);
    }
    
    // Hide after 2 seconds
    QTimer::singleShot(2000, this, [this]() {
        if (m_awaitingHotkeyTagId < 0) {
            m_statusLabel->hide();
        }
    });
}

} // namespace FullFrame
