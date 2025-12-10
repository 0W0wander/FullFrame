#pragma once

#include <QMainWindow>

class QLabel;

namespace FullFrame {

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void setupUI();
    
private:
    QLabel* m_statusLabel = nullptr;
};

} // namespace FullFrame
