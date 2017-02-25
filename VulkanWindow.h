#ifndef VULKANWINDOW_H
#define VULKANWINDOW_H

#include <unordered_map>

#include <QWidget>
#include <QKeyEvent>

namespace Ui {
class VulkanWindow;
}

class VulkanWindow : public QWidget
{
    Q_OBJECT

public:
    explicit VulkanWindow(QWidget *parent = 0);
    ~VulkanWindow();

    std::pair<int, int> getCurPosWin();
    std::pair<float, float> getCurPosNDC();
    bool getKeyState(Qt::Key);
    Qt::MouseButtons getMouseState();

    void showCursor(bool);

private:
    Ui::VulkanWindow *ui;

    /*Input management*/
    std::unordered_map<int, bool> m_keyboard_status;
    Qt::MouseButtons m_mouse_status;

    /*QWidget interface*/
protected:
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void wheelEvent(QWheelEvent *event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void keyReleaseEvent(QKeyEvent *event) override;
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void paintEvent(QPaintEvent *event) override;
};

#endif // VULKANWINDOW_H
