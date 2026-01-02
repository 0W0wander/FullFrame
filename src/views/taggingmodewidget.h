/**
 * TaggingModeWidget - Focused tagging view with horizontal thumbnails
 * 
 * Layout:
 * - Top: Horizontal scrollable thumbnail strip
 * - Left sidebar: Tags, file info, tag input
 * - Center: Large media preview (image/video/audio)
 */

#pragma once

#include <QWidget>
#include <QListView>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStyledItemDelegate>
#include <QCompleter>
#include <QStringListModel>
#include <QSlider>
#include <QStackedWidget>
#include <QScrollArea>
#include <QFileInfo>
#include <QMovie>

#ifdef HAVE_QT_MULTIMEDIA
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#endif

namespace FullFrame {

class ImageThumbnailModel;
struct Tag;

/**
 * Custom QLineEdit that handles Tab for autocomplete
 */
class AutoCompleteLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit AutoCompleteLineEdit(QWidget* parent = nullptr);
    
Q_SIGNALS:
    void tabPressed();

protected:
    bool event(QEvent* event) override;
};

/**
 * Horizontal thumbnail strip delegate
 */
class HorizontalThumbnailDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit HorizontalThumbnailDelegate(QObject* parent = nullptr);
    
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

private:
    int m_thumbnailHeight = 120;
};

/**
 * Large media preview widget - handles images, videos, and audio
 * Open button is managed externally now
 */
class MediaPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MediaPreviewWidget(QWidget* parent = nullptr);
    ~MediaPreviewWidget();
    
    void setMedia(const QString& filePath);
    void clear();
    void stopPlayback();
    
    QString currentPath() const { return m_currentPath; }
    int mediaType() const { return m_mediaType; }

Q_SIGNALS:
    void mediaLoaded(const QString& filePath, int mediaType);

#ifdef HAVE_QT_MULTIMEDIA
private Q_SLOTS:
    void onPlayPauseClicked();
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onSeekSliderMoved(int position);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
#endif

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void loadImage();
    void loadVideo();
    void loadAudio();
    void updateScaledImage();
    QString formatTime(qint64 ms) const;

private:
    QString m_currentPath;
    int m_mediaType = 0;  // 0=unknown, 1=image, 2=video, 3=audio
    
    // Image display
    QImage m_originalImage;
    QPixmap m_scaledPixmap;
    QWidget* m_imageWidget;
    QLabel* m_imageLabel;
    QMovie* m_gifMovie = nullptr;
    
    // Stacked widget for switching between image/video/audio views
    QStackedWidget* m_stackedWidget;
    
#ifdef HAVE_QT_MULTIMEDIA
    // Video/Audio playback
    QVideoWidget* m_videoWidget;
    QMediaPlayer* m_mediaPlayer;
    QAudioOutput* m_audioOutput;
    
    // Media controls
    QPushButton* m_playPauseButton;
    QSlider* m_seekSlider;
    QLabel* m_timeLabel;
    QWidget* m_controlsWidget;
#endif
    
    // Placeholders for video/audio when multimedia not available
    QWidget* m_videoPlaceholder;
    QWidget* m_audioPlaceholder;
};

/**
 * Sidebar widget showing file info, tags and tag input
 */
class TaggingSidebarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TaggingSidebarWidget(QWidget* parent = nullptr);
    
    void setFilePath(const QString& filePath);
    void refresh();
    void focusTagInput();

Q_SIGNALS:
    void tagAdded(const QString& tagName);
    void tagClicked(qint64 tagId);
    void openRequested(const QString& filePath);
    void nextImageRequested();

private Q_SLOTS:
    void onTagEnterPressed();
    void onTabPressed();

private:
    void updateFileInfo();
    void updateTags();
    void setupUI();
    QString formatFileSize(qint64 bytes) const;

private:
    QString m_filePath;
    
    // File info
    QLabel* m_fileNameLabel;
    QLabel* m_fileInfoLabel;
    
    // Open button
    QPushButton* m_openButton;
    
    // Tags display
    QScrollArea* m_tagsScrollArea;
    QWidget* m_tagsContainer;
    QWidget* m_tagsFlowWidget;
    QLabel* m_noTagsLabel;
    
    // Tag input
    AutoCompleteLineEdit* m_tagInput;
    QCompleter* m_tagCompleter;
    QStringListModel* m_completerModel;
};

/**
 * Main tagging mode widget
 */
class TaggingModeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TaggingModeWidget(QWidget* parent = nullptr);
    ~TaggingModeWidget() override;

    void setModel(ImageThumbnailModel* model);
    
    // Get current image path
    QString currentImagePath() const;
    
    // Navigation
    void selectNext();
    void selectPrevious();
    void selectFirst();
    void selectImage(const QString& filePath);

Q_SIGNALS:
    void imageSelected(const QString& filePath);
    void openRequested(const QString& filePath);

public Q_SLOTS:
    void refresh();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private Q_SLOTS:
    void onThumbnailClicked(const QModelIndex& index);
    void onCurrentChanged(const QModelIndex& current, const QModelIndex& previous);
    void onModelReset();
    void onTagAdded(const QString& tagName);
    void updateTagCompleter();

private:
    void setupUI();
    void updatePreview();

private:
    // Model
    ImageThumbnailModel* m_model = nullptr;
    
    // UI Components
    QListView* m_thumbnailStrip;
    HorizontalThumbnailDelegate* m_delegate;
    MediaPreviewWidget* m_previewWidget;
    TaggingSidebarWidget* m_sidebar;
    
    // Current state
    QString m_currentImagePath;
};

} // namespace FullFrame
