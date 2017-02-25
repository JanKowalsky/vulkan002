#ifndef SCENE_H
#define SCENE_H

class Timer;
class QMouseEvent;
class QWheelEvent;
class QKeyEvent;

class Scene
{
public:
	virtual void render() = 0;
	virtual void onResize() = 0;
	virtual Timer& getTimer() = 0;

    virtual void mousePressEvent(QMouseEvent*){}
    virtual void mouseReleaseEvent(QMouseEvent*){}
    virtual void mouseDoubleClickEvent(QMouseEvent*){}
    virtual void mouseMoveEvent(QMouseEvent*){}
    virtual void wheelEvent(QWheelEvent*){}
    virtual void keyPressEvent(QKeyEvent*){}
    virtual void keyReleaseEvent(QKeyEvent*){}
};

#endif //SCENE_H
