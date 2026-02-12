/**
 * TaggingModeWidget implementation
 */

#include "taggingmodewidget.h"
#include "imagethumbnailmodel.h"
#include "tagmanager.h"
#include "thumbnailcache.h"
#include "thumbnailcreator.h"

#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QKeyEvent>
#include <QShowEvent>
#include <QImageReader>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QRandomGenerator>
#include <QDateTime>
#include <QFileInfo>
#include <QGridLayout>

namespace FullFrame {

// ============== AutoCompleteLineEdit ==============

AutoCompleteLineEdit::AutoCompleteLineEdit(QWidget* parent)
    : QLineEdit(parent)
{
}

bool AutoCompleteLineEdit::event(QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Tab) {
            Q_EMIT tabPressed();
            return true;
        }
    }
    return QLineEdit::event(event);
}

// ============== HorizontalThumbnailDelegate ==============

HorizontalThumbnailDelegate::HorizontalThumbnailDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void HorizontalThumbnailDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                         const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    QRect itemRect = option.rect;
    
    // Background
    bool selected = option.state & QStyle::State_Selected;
    QColor bgColor = selected ? QColor(0, 120, 215, 80) : QColor(30, 30, 30);
    painter->fillRect(itemRect, bgColor);

    // Get thumbnail
    QVariant thumbVar = index.data(Qt::DecorationRole);
    if (thumbVar.canConvert<QPixmap>()) {
        QPixmap pixmap = thumbVar.value<QPixmap>();
        if (!pixmap.isNull()) {
            int margin = 4;
            int availableHeight = itemRect.height() - margin * 2;
            QSize scaledSize = pixmap.size().scaled(pixmap.width(), availableHeight, Qt::KeepAspectRatio);
            
            int x = itemRect.x() + (itemRect.width() - scaledSize.width()) / 2;
            int y = itemRect.y() + margin;
            
            QRect targetRect(x, y, scaledSize.width(), scaledSize.height());
            
            QPainterPath path;
            path.addRoundedRect(targetRect, 4, 4);
            painter->setClipPath(path);
            painter->drawPixmap(targetRect, pixmap);
            painter->setClipping(false);
        }
    }

    // Selection border
    if (selected) {
        painter->setPen(QPen(QColor(0, 120, 215), 3));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(itemRect.adjusted(2, 2, -2, -2), 4, 4);
    }

    painter->restore();
}

QSize HorizontalThumbnailDelegate::sizeHint(const QStyleOptionViewItem& option,
                                             const QModelIndex& index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    return QSize(m_thumbnailHeight * 4 / 3, m_thumbnailHeight);
}

// ============== MediaPreviewWidget ==============

MediaPreviewWidget::MediaPreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(300, 200);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Stacked widget for different media types
    m_stackedWidget = new QStackedWidget(this);
    mainLayout->addWidget(m_stackedWidget, 1);
    
    // Image widget (page 0) - Use QLabel for proper image display
    m_imageWidget = new QWidget(this);
    m_imageWidget->setStyleSheet("background-color: #191919;");
    QVBoxLayout* imageLayout = new QVBoxLayout(m_imageWidget);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    
    m_imageLabel = new QLabel(m_imageWidget);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet("background-color: #191919;");
    m_imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    imageLayout->addWidget(m_imageLabel);
    
    m_stackedWidget->addWidget(m_imageWidget);
    
#ifdef HAVE_QT_MULTIMEDIA
    // Video widget (page 1)
    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->setStyleSheet("background-color: #191919;");
    m_stackedWidget->addWidget(m_videoWidget);
#else
    // Video placeholder (page 1)
    m_videoPlaceholder = new QWidget(this);
    m_videoPlaceholder->setStyleSheet("background-color: #2d3540;");
    QVBoxLayout* vpl = new QVBoxLayout(m_videoPlaceholder);
    QLabel* vi = new QLabel("🎬", m_videoPlaceholder);
    vi->setStyleSheet("font-size: 80px; color: #7090b0;");
    vi->setAlignment(Qt::AlignCenter);
    QLabel* vl = new QLabel("Video File", m_videoPlaceholder);
    vl->setStyleSheet("font-size: 16px; color: #7090b0;");
    vl->setAlignment(Qt::AlignCenter);
    QLabel* vn = new QLabel("(Install Qt Multimedia for playback)", m_videoPlaceholder);
    vn->setStyleSheet("font-size: 11px; color: #5070a0;");
    vn->setAlignment(Qt::AlignCenter);
    vpl->addStretch();
    vpl->addWidget(vi);
    vpl->addWidget(vl);
    vpl->addWidget(vn);
    vpl->addStretch();
    m_stackedWidget->addWidget(m_videoPlaceholder);
#endif
    
    // Audio placeholder (page 2)
    m_audioPlaceholder = new QWidget(this);
    m_audioPlaceholder->setStyleSheet("background-color: #2d3035;");
    QVBoxLayout* apl = new QVBoxLayout(m_audioPlaceholder);
    QLabel* ai = new QLabel("🎵", m_audioPlaceholder);
    ai->setStyleSheet("font-size: 80px; color: #8090a0;");
    ai->setAlignment(Qt::AlignCenter);
    QLabel* al = new QLabel("Audio File", m_audioPlaceholder);
    al->setStyleSheet("font-size: 16px; color: #8090a0;");
    al->setAlignment(Qt::AlignCenter);
#ifndef HAVE_QT_MULTIMEDIA
    QLabel* an = new QLabel("(Install Qt Multimedia for playback)", m_audioPlaceholder);
    an->setStyleSheet("font-size: 11px; color: #5080a0;");
    an->setAlignment(Qt::AlignCenter);
#endif
    apl->addStretch();
    apl->addWidget(ai);
    apl->addWidget(al);
#ifndef HAVE_QT_MULTIMEDIA
    apl->addWidget(an);
#endif
    apl->addStretch();
    m_stackedWidget->addWidget(m_audioPlaceholder);
    
#ifdef HAVE_QT_MULTIMEDIA
    // Media player setup
    m_mediaPlayer = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_mediaPlayer->setAudioOutput(m_audioOutput);
    m_mediaPlayer->setVideoOutput(m_videoWidget);
    m_audioOutput->setVolume(0.7f);
    
    // Controls widget
    m_controlsWidget = new QWidget(this);
    m_controlsWidget->setStyleSheet("background-color: rgba(30, 30, 30, 0.95);");
    m_controlsWidget->setFixedHeight(50);
    m_controlsWidget->hide();
    
    QHBoxLayout* controlsLayout = new QHBoxLayout(m_controlsWidget);
    controlsLayout->setContentsMargins(15, 5, 15, 5);
    controlsLayout->setSpacing(10);
    
    m_playPauseButton = new QPushButton("▶", m_controlsWidget);
    m_playPauseButton->setFixedSize(36, 36);
    m_playPauseButton->setStyleSheet(R"(
        QPushButton {
            background-color: #005a9e;
            border: none;
            border-radius: 18px;
            color: white;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #0068b8; }
        QPushButton:pressed { background-color: #004c87; }
    )");
    connect(m_playPauseButton, &QPushButton::clicked, this, &MediaPreviewWidget::onPlayPauseClicked);
    controlsLayout->addWidget(m_playPauseButton);
    
    m_seekSlider = new QSlider(Qt::Horizontal, m_controlsWidget);
    m_seekSlider->setStyleSheet(R"(
        QSlider::groove:horizontal {
            background: #404040;
            height: 6px;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #005a9e;
            width: 14px;
            margin: -4px 0;
            border-radius: 7px;
        }
        QSlider::sub-page:horizontal {
            background: #005a9e;
            border-radius: 3px;
        }
    )");
    connect(m_seekSlider, &QSlider::sliderMoved, this, &MediaPreviewWidget::onSeekSliderMoved);
    controlsLayout->addWidget(m_seekSlider, 1);
    
    m_timeLabel = new QLabel("0:00 / 0:00", m_controlsWidget);
    m_timeLabel->setStyleSheet("color: #c0c0c0; font-size: 12px; min-width: 90px;");
    controlsLayout->addWidget(m_timeLabel);
    
    mainLayout->addWidget(m_controlsWidget);
    
    connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this, &MediaPreviewWidget::onPositionChanged);
    connect(m_mediaPlayer, &QMediaPlayer::durationChanged, this, &MediaPreviewWidget::onDurationChanged);
    connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, &MediaPreviewWidget::onMediaStatusChanged);
#endif
}

MediaPreviewWidget::~MediaPreviewWidget()
{
    stopPlayback();
}

void MediaPreviewWidget::setMedia(const QString& filePath)
{
    if (m_currentPath == filePath) {
        return;
    }
    
    stopPlayback();
    
    m_currentPath = filePath;
    m_originalImage = QImage();
    m_scaledPixmap = QPixmap();
    
    if (filePath.isEmpty()) {
#ifdef HAVE_QT_MULTIMEDIA
        m_controlsWidget->hide();
#endif
        m_stackedWidget->setCurrentIndex(0);
        m_mediaType = 0;
        update();
        return;
    }
    
    MediaType type = ThumbnailCreator::getMediaType(filePath);
    
    switch (type) {
        case MediaType::Image:
            m_mediaType = 1;
            loadImage();
            m_stackedWidget->setCurrentIndex(0);
#ifdef HAVE_QT_MULTIMEDIA
            m_controlsWidget->hide();
#endif
            break;
            
        case MediaType::Video:
            m_mediaType = 2;
            loadVideo();
            m_stackedWidget->setCurrentIndex(1);
#ifdef HAVE_QT_MULTIMEDIA
            m_controlsWidget->show();
#endif
            break;
            
        case MediaType::Audio:
            m_mediaType = 3;
            loadAudio();
            m_stackedWidget->setCurrentIndex(2);
#ifdef HAVE_QT_MULTIMEDIA
            m_controlsWidget->show();
#endif
            break;
            
        default:
            m_mediaType = 0;
            m_stackedWidget->setCurrentIndex(0);
#ifdef HAVE_QT_MULTIMEDIA
            m_controlsWidget->hide();
#endif
            break;
    }
    
    Q_EMIT mediaLoaded(filePath, m_mediaType);
    update();
}

void MediaPreviewWidget::clear()
{
    setMedia(QString());
}

void MediaPreviewWidget::stopPlayback()
{
    // Stop GIF animation
    if (m_gifMovie) {
        m_gifMovie->stop();
        delete m_gifMovie;
        m_gifMovie = nullptr;
    }
    
#ifdef HAVE_QT_MULTIMEDIA
    if (m_mediaPlayer->playbackState() != QMediaPlayer::StoppedState) {
        m_mediaPlayer->stop();
    }
    m_playPauseButton->setText("▶");
#endif
}

void MediaPreviewWidget::loadImage()
{
    // Stop any existing GIF animation
    if (m_gifMovie) {
        m_gifMovie->stop();
        delete m_gifMovie;
        m_gifMovie = nullptr;
    }
    
    // Check if it's a GIF - use QMovie for animation
    QString ext = QFileInfo(m_currentPath).suffix().toLower();
    if (ext == "gif") {
        m_gifMovie = new QMovie(m_currentPath, QByteArray(), this);
        if (m_gifMovie->isValid()) {
            // Get original GIF size and scale to fit while keeping aspect ratio
            QSize gifSize = m_gifMovie->currentImage().size();
            QSize labelSize = m_imageLabel->size();
            if (!labelSize.isEmpty() && !gifSize.isEmpty()) {
                QSize scaledSize = gifSize.scaled(labelSize, Qt::KeepAspectRatio);
                m_gifMovie->setScaledSize(scaledSize);
            }
            m_imageLabel->setMovie(m_gifMovie);
            m_gifMovie->start();  // Auto-play and loop by default
            return;
        } else {
            delete m_gifMovie;
            m_gifMovie = nullptr;
        }
    }
    
    // Regular static image
    QImageReader reader(m_currentPath);
    reader.setAutoTransform(true);
    m_originalImage = reader.read();
    
    if (!m_originalImage.isNull()) {
        updateScaledImage();
    } else {
        m_imageLabel->setText("Failed to load image");
        m_imageLabel->setStyleSheet("background-color: #191919; color: #808080; font-size: 14px;");
    }
}

void MediaPreviewWidget::loadVideo()
{
#ifdef HAVE_QT_MULTIMEDIA
    m_mediaPlayer->setSource(QUrl::fromLocalFile(m_currentPath));
    m_playPauseButton->setText("⏸");
    m_seekSlider->setValue(0);
    m_timeLabel->setText("0:00 / 0:00");
    // Auto-play videos when loaded
    m_mediaPlayer->play();
#endif
}

void MediaPreviewWidget::loadAudio()
{
#ifdef HAVE_QT_MULTIMEDIA
    m_mediaPlayer->setSource(QUrl::fromLocalFile(m_currentPath));
    m_playPauseButton->setText("⏸");
    m_seekSlider->setValue(0);
    m_timeLabel->setText("0:00 / 0:00");
    // Auto-play audio when loaded
    m_mediaPlayer->play();
#endif
}

void MediaPreviewWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
}

void MediaPreviewWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    
    if (m_mediaType == 1) {
        if (m_gifMovie && m_gifMovie->isValid()) {
            // Resize GIF to fit label while keeping aspect ratio
            QSize gifSize = m_gifMovie->currentImage().size();
            QSize labelSize = m_imageLabel->size();
            if (!labelSize.isEmpty() && !gifSize.isEmpty()) {
                QSize scaledSize = gifSize.scaled(labelSize, Qt::KeepAspectRatio);
                m_gifMovie->setScaledSize(scaledSize);
            }
        } else if (!m_originalImage.isNull()) {
            updateScaledImage();
        }
    }
}

void MediaPreviewWidget::updateScaledImage()
{
    if (m_originalImage.isNull()) {
        return;
    }
    
    QSize targetSize = m_imageLabel->size();
    if (targetSize.isEmpty() || targetSize.width() < 10 || targetSize.height() < 10) {
        // Widget not yet sized properly, schedule update
        QTimer::singleShot(50, this, &MediaPreviewWidget::updateScaledImage);
        return;
    }
    
    QSize scaledSize = m_originalImage.size().scaled(targetSize, Qt::KeepAspectRatio);
    QImage scaled = m_originalImage.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_scaledPixmap = QPixmap::fromImage(scaled);
    m_imageLabel->setPixmap(m_scaledPixmap);
}

#ifdef HAVE_QT_MULTIMEDIA
void MediaPreviewWidget::onPlayPauseClicked()
{
    if (m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
        m_mediaPlayer->pause();
        m_playPauseButton->setText("▶");
    } else {
        m_mediaPlayer->play();
        m_playPauseButton->setText("⏸");
    }
}

void MediaPreviewWidget::onPositionChanged(qint64 position)
{
    if (!m_seekSlider->isSliderDown()) {
        m_seekSlider->setValue(static_cast<int>(position));
    }
    qint64 duration = m_mediaPlayer->duration();
    m_timeLabel->setText(QString("%1 / %2").arg(formatTime(position)).arg(formatTime(duration)));
}

void MediaPreviewWidget::onDurationChanged(qint64 duration)
{
    m_seekSlider->setRange(0, static_cast<int>(duration));
}

void MediaPreviewWidget::onSeekSliderMoved(int position)
{
    m_mediaPlayer->setPosition(position);
}

void MediaPreviewWidget::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia) {
        m_mediaPlayer->setPosition(0);
        m_playPauseButton->setText("▶");
    }
}
#endif

QString MediaPreviewWidget::formatTime(qint64 ms) const
{
    int seconds = static_cast<int>(ms / 1000);
    int minutes = seconds / 60;
    seconds = seconds % 60;
    return QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
}

// ============== TaggingSidebarWidget ==============

TaggingSidebarWidget::TaggingSidebarWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void TaggingSidebarWidget::setupUI()
{
    setFixedWidth(280);
    setStyleSheet("background-color: #1a1a1a;");
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    
    // === TAG INPUT AT TOP ===
    QLabel* addTagLabel = new QLabel("Add Tag", this);
    addTagLabel->setStyleSheet("color: #808080; font-size: 11px; font-weight: bold; text-transform: uppercase;");
    layout->addWidget(addTagLabel);
    
    m_tagInput = new AutoCompleteLineEdit(this);
    m_tagInput->setPlaceholderText("Type tag name and press Enter...");
    m_tagInput->setStyleSheet(R"(
        QLineEdit {
            background-color: #252525;
            border: 2px solid #333;
            border-radius: 6px;
            padding: 10px 12px;
            color: #e0e0e0;
            font-size: 13px;
        }
        QLineEdit:focus {
            border-color: #0078d4;
        }
        QLineEdit::placeholder {
            color: #505050;
        }
    )");
    layout->addWidget(m_tagInput);
    
    // === CURRENT TAGS ===
    QLabel* tagsLabel = new QLabel("Current Tags", this);
    tagsLabel->setStyleSheet("color: #808080; font-size: 11px; font-weight: bold; text-transform: uppercase; margin-top: 4px;");
    layout->addWidget(tagsLabel);
    
    // Tags scroll area (expands to fill available space)
    m_tagsScrollArea = new QScrollArea(this);
    m_tagsScrollArea->setWidgetResizable(true);
    m_tagsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tagsScrollArea->setStyleSheet(R"(
        QScrollArea {
            background-color: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background: #252525;
            width: 8px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #404040;
            border-radius: 4px;
            min-height: 20px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
    )");
    
    m_tagsContainer = new QWidget(m_tagsScrollArea);
    QVBoxLayout* tagsContainerLayout = new QVBoxLayout(m_tagsContainer);
    tagsContainerLayout->setContentsMargins(0, 0, 0, 0);
    tagsContainerLayout->setSpacing(4);
    tagsContainerLayout->setAlignment(Qt::AlignTop);
    
    m_noTagsLabel = new QLabel("No tags yet", m_tagsContainer);
    m_noTagsLabel->setStyleSheet("color: #606060; font-size: 11px; font-style: italic;");
    tagsContainerLayout->addWidget(m_noTagsLabel);
    
    // Flow widget for compact tags (we'll add tags here dynamically)
    m_tagsFlowWidget = new QWidget(m_tagsContainer);
    m_tagsFlowWidget->setStyleSheet("background: transparent;");
    tagsContainerLayout->addWidget(m_tagsFlowWidget);
    tagsContainerLayout->addStretch();
    
    m_tagsScrollArea->setWidget(m_tagsContainer);
    layout->addWidget(m_tagsScrollArea, 1);
    
    // === SEPARATOR ===
    QFrame* sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet("background-color: #333;");
    sep1->setFixedHeight(1);
    layout->addWidget(sep1);
    
    // === FILE INFO ===
    m_fileNameLabel = new QLabel(this);
    m_fileNameLabel->setWordWrap(true);
    m_fileNameLabel->setStyleSheet(R"(
        QLabel {
            color: #e0e0e0;
            font-size: 13px;
            font-weight: bold;
            padding: 6px 8px;
            background-color: #252525;
            border-radius: 6px;
        }
    )");
    layout->addWidget(m_fileNameLabel);
    
    m_fileInfoLabel = new QLabel(this);
    m_fileInfoLabel->setWordWrap(true);
    m_fileInfoLabel->setStyleSheet(R"(
        QLabel {
            color: #808080;
            font-size: 11px;
            padding: 4px 8px;
        }
    )");
    layout->addWidget(m_fileInfoLabel);
    
    // === OPEN BUTTON ===
    m_openButton = new QPushButton("Open in Default App", this);
    m_openButton->setStyleSheet(R"(
        QPushButton {
            background-color: #005a9e;
            border: none;
            border-radius: 6px;
            padding: 10px 16px;
            color: white;
            font-size: 12px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #0068b8;
        }
        QPushButton:pressed {
            background-color: #004c87;
        }
    )");
    connect(m_openButton, &QPushButton::clicked, this, [this]() {
        if (!m_filePath.isEmpty()) {
            Q_EMIT openRequested(m_filePath);
        }
    });
    layout->addWidget(m_openButton);
    
    // Setup autocomplete
    m_completerModel = new QStringListModel(this);
    m_tagCompleter = new QCompleter(m_completerModel, this);
    m_tagCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_tagCompleter->setFilterMode(Qt::MatchContains);
    m_tagCompleter->setCompletionMode(QCompleter::PopupCompletion);
    m_tagCompleter->popup()->setStyleSheet(R"(
        QListView {
            background-color: #2d2d2d;
            color: #e0e0e0;
            border: 1px solid #4d4d4d;
            border-radius: 4px;
            padding: 4px;
            font-size: 13px;
        }
        QListView::item {
            padding: 6px 10px;
            border-radius: 3px;
        }
        QListView::item:selected {
            background-color: #0078d4;
        }
        QListView::item:hover {
            background-color: #3d3d3d;
        }
    )");
    m_tagInput->setCompleter(m_tagCompleter);
    
    connect(m_tagInput, &QLineEdit::returnPressed, this, &TaggingSidebarWidget::onTagEnterPressed);
    connect(m_tagInput, &AutoCompleteLineEdit::tabPressed, this, &TaggingSidebarWidget::onTabPressed);
}

void TaggingSidebarWidget::setFilePath(const QString& filePath)
{
    m_filePath = filePath;
    updateFileInfo();
    updateTags();
    
    // Update completer with all tags
    QStringList tagNames;
    for (const Tag& tag : TagManager::instance()->allTags()) {
        tagNames.append(tag.name);
    }
    m_completerModel->setStringList(tagNames);
}

void TaggingSidebarWidget::refresh()
{
    updateTags();
}

void TaggingSidebarWidget::focusTagInput()
{
    m_tagInput->setFocus();
}

void TaggingSidebarWidget::updateFileInfo()
{
    if (m_filePath.isEmpty()) {
        m_fileNameLabel->setText("No file selected");
        m_fileInfoLabel->setText("");
        return;
    }
    
    QFileInfo info(m_filePath);
    m_fileNameLabel->setText(info.fileName());
    
    QString fileInfo;
    fileInfo += QString("Size: %1\n").arg(formatFileSize(info.size()));
    fileInfo += QString("Modified: %1\n").arg(info.lastModified().toString("MMM d, yyyy h:mm AP"));
    fileInfo += QString("Type: %1").arg(info.suffix().toUpper());
    m_fileInfoLabel->setText(fileInfo);
}

void TaggingSidebarWidget::updateTags()
{
    // Clear existing tags from flow widget
    if (m_tagsFlowWidget->layout()) {
        QLayout* oldLayout = m_tagsFlowWidget->layout();
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete oldLayout;
    }
    
    if (m_filePath.isEmpty()) {
        m_noTagsLabel->show();
        m_tagsFlowWidget->hide();
        return;
    }
    
    QList<Tag> tags = TagManager::instance()->tagsForImage(m_filePath);
    
    if (tags.isEmpty()) {
        m_noTagsLabel->show();
        m_tagsFlowWidget->hide();
        return;
    }
    
    m_noTagsLabel->hide();
    m_tagsFlowWidget->show();
    
    // Use horizontal layout with wrapping behavior
    QHBoxLayout* flowLayout = new QHBoxLayout(m_tagsFlowWidget);
    flowLayout->setContentsMargins(0, 0, 0, 0);
    flowLayout->setSpacing(3);  // Tight spacing like gallery badges
    flowLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    
    for (const Tag& tag : tags) {
        QPushButton* tagBadge = new QPushButton(m_tagsFlowWidget);
        tagBadge->setText(QString("%1 ×").arg(tag.name));
        tagBadge->setCursor(Qt::PointingHandCursor);
        tagBadge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        
        QColor bgColor = QColor(tag.color);
        QColor textColor = (bgColor.lightness() > 128) ? QColor(20, 20, 20) : QColor(255, 255, 255);
        
        // Exact match to gallery badge style from ThumbnailDelegate::paintTagBadges
        // 8pt font, 16px height, 6px padding, 3px radius
        tagBadge->setStyleSheet(QString(R"(
            QPushButton {
                background-color: %1;
                border: none;
                border-radius: 3px;
                padding: 1px 6px;
                min-height: 14px;
                max-height: 16px;
                color: %2;
                font-size: 8pt;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: %3;
            }
        )").arg(bgColor.name())
           .arg(textColor.name())
           .arg(bgColor.darker(115).name()));
        
        qint64 tagId = tag.id;
        connect(tagBadge, &QPushButton::clicked, this, [this, tagId]() {
            Q_EMIT tagClicked(tagId);
        });
        
        flowLayout->addWidget(tagBadge);
    }
    
    flowLayout->addStretch();  // Push tags to the left
}

QString TaggingSidebarWidget::formatFileSize(qint64 bytes) const
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

void TaggingSidebarWidget::onTagEnterPressed()
{
    QString tagName = m_tagInput->text().trimmed();
    
    // If empty, go to next image
    if (tagName.isEmpty()) {
        Q_EMIT nextImageRequested();
        return;
    }
    
    if (m_filePath.isEmpty()) {
        return;
    }
    
    // Create tag if doesn't exist
    Tag existingTag = TagManager::instance()->tagByName(tagName);
    qint64 tagId;
    
    if (!existingTag.isValid()) {
        // Generate a nice color for new tag
        static const QStringList colors = {
            "#e74c3c", "#3498db", "#2ecc71", "#f39c12", "#9b59b6",
            "#1abc9c", "#e91e63", "#00bcd4", "#ff5722", "#607d8b"
        };
        QString color = colors[QRandomGenerator::global()->bounded(colors.size())];
        tagId = TagManager::instance()->createTag(tagName, color);
    } else {
        tagId = existingTag.id;
    }
    
    if (tagId > 0) {
        TagManager::instance()->tagImage(m_filePath, tagId);
        m_tagInput->clear();
        Q_EMIT tagAdded(tagName);
    }
}

void TaggingSidebarWidget::onTabPressed()
{
    if (m_tagCompleter->completionCount() > 0) {
        m_tagCompleter->popup()->hide();
        QString completion = m_tagCompleter->currentCompletion();
        if (!completion.isEmpty()) {
            m_tagInput->setText(completion);
        }
    }
}

// ============== TaggingModeWidget ==============

TaggingModeWidget::TaggingModeWidget(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);  // Enable keyboard focus
    setupUI();
}

TaggingModeWidget::~TaggingModeWidget() = default;

void TaggingModeWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // === Top: Horizontal thumbnail strip ===
    m_thumbnailStrip = new QListView(this);
    m_thumbnailStrip->setFlow(QListView::LeftToRight);
    m_thumbnailStrip->setWrapping(false);
    m_thumbnailStrip->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_thumbnailStrip->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_thumbnailStrip->setSelectionMode(QAbstractItemView::SingleSelection);
    m_thumbnailStrip->setFixedHeight(140);
    m_thumbnailStrip->setMouseTracking(false);
    m_thumbnailStrip->setUniformItemSizes(true);
    
    m_delegate = new HorizontalThumbnailDelegate(this);
    m_thumbnailStrip->setItemDelegate(m_delegate);
    
    m_thumbnailStrip->setStyleSheet(R"(
        QListView {
            background-color: #1a1a1a;
            border: none;
            border-bottom: 1px solid #333;
        }
        QListView::item {
            background: transparent;
            padding: 0px;
        }
        QListView::item:selected {
            background: transparent;
        }
        QScrollBar:horizontal {
            background: #2d2d2d;
            height: 10px;
            margin: 0;
        }
        QScrollBar::handle:horizontal {
            background: #5a5a5a;
            border-radius: 5px;
            min-width: 30px;
            margin: 2px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #6a6a6a;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }
    )");
    
    connect(m_thumbnailStrip, &QListView::clicked, this, &TaggingModeWidget::onThumbnailClicked);
    
    mainLayout->addWidget(m_thumbnailStrip);

    // === Center: Sidebar + Media preview ===
    QWidget* centerWidget = new QWidget(this);
    QHBoxLayout* centerLayout = new QHBoxLayout(centerWidget);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);
    
    // Left sidebar
    m_sidebar = new TaggingSidebarWidget(this);
    m_sidebar->setStyleSheet("border-right: 1px solid #333;");
    connect(m_sidebar, &TaggingSidebarWidget::openRequested, this, &TaggingModeWidget::openRequested);
    connect(m_sidebar, &TaggingSidebarWidget::tagAdded, this, &TaggingModeWidget::onTagAdded);
    connect(m_sidebar, &TaggingSidebarWidget::nextImageRequested, this, &TaggingModeWidget::selectNext);
    connect(m_sidebar, &TaggingSidebarWidget::tagClicked, this, [this](qint64 tagId) {
        // Remove tag when clicked
        if (!m_currentImagePath.isEmpty()) {
            TagManager::instance()->untagImage(m_currentImagePath, tagId);
        }
    });
    centerLayout->addWidget(m_sidebar);
    
    // Media preview
    m_previewWidget = new MediaPreviewWidget(this);
    centerLayout->addWidget(m_previewWidget, 1);
    
    mainLayout->addWidget(centerWidget, 1);

    // Connect to tag manager for updates
    connect(TagManager::instance(), &TagManager::imageTagged, this, [this](const QString& path, qint64) {
        if (path == m_currentImagePath) {
            m_sidebar->refresh();
        }
    });
    connect(TagManager::instance(), &TagManager::imageUntagged, this, [this](const QString& path, qint64) {
        if (path == m_currentImagePath) {
            m_sidebar->refresh();
        }
    });

    setStyleSheet("background-color: #1e1e1e;");
}

void TaggingModeWidget::setModel(ImageThumbnailModel* model)
{
    m_model = model;
    m_thumbnailStrip->setModel(model);
    
    if (m_model) {
        connect(m_model, &ImageThumbnailModel::loadingFinished, this, &TaggingModeWidget::onModelReset);
        connect(m_model, &QAbstractItemModel::modelReset, this, &TaggingModeWidget::onModelReset);
        
        if (m_thumbnailStrip->selectionModel()) {
            connect(m_thumbnailStrip->selectionModel(), &QItemSelectionModel::currentChanged,
                    this, &TaggingModeWidget::onCurrentChanged);
        }
    }
    
    updateTagCompleter();
}

QString TaggingModeWidget::currentImagePath() const
{
    return m_currentImagePath;
}

int TaggingModeWidget::currentRow() const
{
    QModelIndex idx = m_thumbnailStrip->currentIndex();
    return idx.isValid() ? idx.row() : -1;
}

void TaggingModeWidget::setPendingSelectRow(int row)
{
    m_pendingSelectRow = row;
}

void TaggingModeWidget::selectNext()
{
    if (!m_model || m_model->rowCount() == 0) return;
    
    QModelIndex current = m_thumbnailStrip->currentIndex();
    int nextRow = current.isValid() ? current.row() + 1 : 0;
    
    if (nextRow >= m_model->rowCount()) {
        nextRow = 0;
    }
    
    QModelIndex next = m_model->index(nextRow);
    m_thumbnailStrip->setCurrentIndex(next);
    m_thumbnailStrip->scrollTo(next);
    onThumbnailClicked(next);
}

void TaggingModeWidget::selectPrevious()
{
    if (!m_model || m_model->rowCount() == 0) return;
    
    QModelIndex current = m_thumbnailStrip->currentIndex();
    int prevRow = current.isValid() ? current.row() - 1 : m_model->rowCount() - 1;
    
    if (prevRow < 0) {
        prevRow = m_model->rowCount() - 1;
    }
    
    QModelIndex prev = m_model->index(prevRow);
    m_thumbnailStrip->setCurrentIndex(prev);
    m_thumbnailStrip->scrollTo(prev);
    onThumbnailClicked(prev);
}

void TaggingModeWidget::selectFirst()
{
    if (!m_model || m_model->rowCount() == 0) return;
    
    QModelIndex first = m_model->index(0);
    m_thumbnailStrip->setCurrentIndex(first);
    m_thumbnailStrip->scrollTo(first);
    onThumbnailClicked(first);
}

void TaggingModeWidget::selectByRow(int row)
{
    if (!m_model || m_model->rowCount() == 0) return;
    
    int targetRow = qMin(row, m_model->rowCount() - 1);
    if (targetRow < 0) targetRow = 0;
    
    QModelIndex idx = m_model->index(targetRow);
    m_thumbnailStrip->setCurrentIndex(idx);
    m_thumbnailStrip->scrollTo(idx);
    onThumbnailClicked(idx);
}

void TaggingModeWidget::selectImage(const QString& filePath)
{
    if (!m_model || filePath.isEmpty()) return;
    
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QModelIndex index = m_model->index(i);
        QString path = index.data(FilePathRole).toString();
        if (path == filePath) {
            m_thumbnailStrip->setCurrentIndex(index);
            m_thumbnailStrip->scrollTo(index);
            onThumbnailClicked(index);
            break;
        }
    }
}

void TaggingModeWidget::refresh()
{
    m_sidebar->refresh();
    m_previewWidget->update();
}

void TaggingModeWidget::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
        case Qt::Key_Right:
            selectNext();
            event->accept();
            return;
        case Qt::Key_Left:
            selectPrevious();
            event->accept();
            return;
        case Qt::Key_Home:
            selectFirst();
            event->accept();
            return;
        default:
            break;
    }
    
    QWidget::keyPressEvent(event);
}

void TaggingModeWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    
    // Auto-focus the widget so arrow keys work immediately
    setFocus();
    
    // Also ensure thumbnail strip is ready for navigation
    if (m_thumbnailStrip && m_model && m_model->rowCount() > 0) {
        if (!m_thumbnailStrip->currentIndex().isValid()) {
            selectFirst();
        }
    }
}

bool TaggingModeWidget::eventFilter(QObject* obj, QEvent* event)
{
    return QWidget::eventFilter(obj, event);
}

void TaggingModeWidget::onThumbnailClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;
    
    QString filePath = index.data(FilePathRole).toString();
    if (filePath != m_currentImagePath) {
        m_currentImagePath = filePath;
        updatePreview();
        Q_EMIT imageSelected(filePath);
    }
}

void TaggingModeWidget::onCurrentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous)
    if (current.isValid()) {
        onThumbnailClicked(current);
    }
}

void TaggingModeWidget::onModelReset()
{
    m_currentImagePath.clear();
    m_previewWidget->clear();
    m_sidebar->setFilePath(QString());
    
    if (m_model && m_model->rowCount() > 0) {
        if (m_pendingSelectRow >= 0) {
            int row = m_pendingSelectRow;
            // Don't reset m_pendingSelectRow here — this slot fires twice
            // (once for modelReset, once for loadingFinished). Reset it in
            // the timer callback so the second invocation still sees it.
            QTimer::singleShot(100, this, [this, row]() {
                m_pendingSelectRow = -1;
                selectByRow(row);
            });
        } else {
            QTimer::singleShot(100, this, &TaggingModeWidget::selectFirst);
        }
    } else {
        m_pendingSelectRow = -1;
    }
    
    updateTagCompleter();
}

void TaggingModeWidget::onTagAdded(const QString& tagName)
{
    Q_UNUSED(tagName)
    updateTagCompleter();
}

void TaggingModeWidget::updateTagCompleter()
{
    // Sidebar handles its own completer now
}

void TaggingModeWidget::updatePreview()
{
    m_previewWidget->setMedia(m_currentImagePath);
    m_sidebar->setFilePath(m_currentImagePath);
}

} // namespace FullFrame
