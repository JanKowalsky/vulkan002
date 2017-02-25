#include "VulkanWindow.h"
#include "ui_VulkanWindow.h"

#include "engine.h"
#include <QCursor>

VulkanWindow::VulkanWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::VulkanWindow)
{
    ui->setupUi(this);
    setMouseTracking(true);
}

VulkanWindow::~VulkanWindow()
{
    delete ui;
}

void VulkanWindow::showCursor(bool val)
{
    if(val)
        VulkanEngine::get().getWindow()->setCursor(Qt::ArrowCursor);
    else
        VulkanEngine::get().getWindow()->setCursor(Qt::BlankCursor);
}

std::pair<int, int> VulkanWindow::getCurPosWin()
{
    auto scr_pos = QCursor::pos();
    auto win_pos = mapFromGlobal(scr_pos);

    return std::make_pair(win_pos.x(), win_pos.y());
}

std::pair<float, float> VulkanWindow::getCurPosNDC()
{
    auto win_pos = getCurPosWin();

    std::pair<float, float> res;
    res.first = -1.0f + static_cast<float>(win_pos.first) * 2.0f / width();
    res.second = 1.0f + static_cast<float>(-win_pos.second) * 2.0f / height();

    return res;
}

bool VulkanWindow::getKeyState(Qt::Key k)
{
    return m_keyboard_status[k];
}

Qt::MouseButtons VulkanWindow::getMouseState()
{
    return m_mouse_status;
}

void VulkanWindow::mousePressEvent(QMouseEvent *event)
{
    m_mouse_status |= event->button();

    VulkanEngine::get().getScene().mousePressEvent(event);
}

void VulkanWindow::mouseReleaseEvent(QMouseEvent *event)
{
    m_mouse_status &= ~event->button();

    VulkanEngine::get().getScene().mouseReleaseEvent(event);
}

void VulkanWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    VulkanEngine::get().getScene().mouseDoubleClickEvent(event);
}

void VulkanWindow::mouseMoveEvent(QMouseEvent *event)
{
    VulkanEngine::get().getScene().mouseMoveEvent(event);
}

void VulkanWindow::wheelEvent(QWheelEvent *event)
{
    VulkanEngine::get().getScene().wheelEvent(event);
}

void VulkanWindow::keyPressEvent(QKeyEvent *event)
{
    m_keyboard_status[event->key()] = true;

    VulkanEngine::get().getScene().keyPressEvent(event);
}

void VulkanWindow::keyReleaseEvent(QKeyEvent *event)
{
    m_keyboard_status[event->key()] = false;

    VulkanEngine::get().getScene().keyReleaseEvent(event);
}

void VulkanWindow::resizeEvent(QResizeEvent *event)
{
    VulkanEngine::get().onResize();
}


void VulkanWindow::paintEvent(QPaintEvent *event)
{
    /*Override any default paint operations*/
}
