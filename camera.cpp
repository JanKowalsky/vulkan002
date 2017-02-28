#include "camera.h"

Camera::Camera(float ratio, float n, float f, float verfovangle)
{
    m_camPos = glm::vec3(0.0f, 0.0f, 0.0f);
    m_camLook = glm::vec3(0.0f, 0.0f, 1.0f);
    m_camRight = glm::vec3(1.0f, 0.0f, 0.0f);
    m_camUp = glm::vec3(0.0f, 1.0f, 0.0f);

    m_verFovAngle = verfovangle;
    m_aspectRatio = ratio;
    m_near = n;
    m_far = f;
    m_horFovAngle = 2 * atanf(m_aspectRatio*tanf(m_verFovAngle / 2));

    m_view = glm::lookAtLH<float>(m_camPos, m_camPos + m_camLook, m_camUp);
    m_proj = glm::perspectiveLH(m_verFovAngle, m_aspectRatio, n, f);
}

void Camera::walk(float d)
{
    m_camPos = m_camPos + d*m_camLook;

    m_view = glm::lookAtLH<float>(m_camPos, m_camPos + m_camLook, m_camUp);
}

void Camera::strafe(float d)
{
    m_camPos = m_camPos + d*m_camRight;

    m_view = glm::lookAtLH<float>(m_camPos, m_camPos + m_camLook, m_camUp);
}

void Camera::upDown(float d)
{
    m_camPos = m_camPos + d*m_camUp;

    m_view = glm::lookAtLH<float>(m_camPos, m_camPos + m_camLook, m_camUp);
}

void Camera::setPos(glm::vec3 pos)
{
    m_camPos = pos;

    m_view = glm::lookAtLH<float>(m_camPos, m_camPos + m_camLook, m_camUp);
}

void Camera::setHorizontalFOVAngle(float a)
{
    m_horFovAngle = a;
    m_verFovAngle = 2 * atanf(tanf(a / 2) / m_aspectRatio);

    m_proj = glm::perspectiveLH(m_verFovAngle, m_aspectRatio, m_near, m_far);
}

void Camera::setVerticalFOVAngle(float a)
{
    m_verFovAngle = a;
    m_horFovAngle = 2 * atanf(m_aspectRatio*tanf(m_verFovAngle / 2));

    m_proj = glm::perspectiveLH(m_verFovAngle, m_aspectRatio, m_near, m_far);
}

void Camera::setNear(float n)
{
    m_near = n;

    m_proj = glm::perspectiveLH(m_verFovAngle, m_aspectRatio, m_near, m_far);
}

void Camera::setFar(float f)
{
    m_far = f;

    m_proj = glm::perspectiveLH(m_verFovAngle, m_aspectRatio, m_near, m_far);
}

void Camera::setAspectRatio(float r)
{
    m_aspectRatio = r;

    m_proj = glm::perspectiveLH(m_verFovAngle, m_aspectRatio, m_near, m_far);
}

void Camera::rotate(float angle)
{
    m_camRight = glm::rotateY<float>(m_camRight, angle);
    m_camLook = glm::rotateY<float>(m_camLook, angle);
    m_camUp = glm::rotateY<float>(m_camUp, angle);

    m_view = glm::lookAtLH<float>(m_camPos, m_camPos + m_camLook, m_camUp);
}

void Camera::pitch(float angle)
{
    m_camLook = glm::rotate<float>(m_camLook, angle, m_camRight);
    m_camUp = glm::rotate<float>(m_camUp, angle, m_camRight);

    m_view = glm::lookAtLH<float>(m_camPos, m_camPos + m_camLook, m_camUp);
}

glm::mat4x4 Camera::getViewProj() const noexcept
{
    return m_proj*m_view;
}

float Camera::getHorizontalFOVAngle() const noexcept
{
    return m_horFovAngle;
}

float Camera::getVerticalFOVAngle() const noexcept
{
    return m_verFovAngle;
}

float Camera::getAspectRatio() const noexcept
{
    return m_aspectRatio;
}

float Camera::getFar() const noexcept
{
    return m_far;
}

float Camera::getNear() const noexcept
{
    return m_near;
}

const glm::fmat4x4& Camera::getView() const noexcept
{
    return m_view;
}

const glm::fmat4x4& Camera::getProj() const noexcept
{
    return m_proj;
}

const glm::vec3& Camera::getCamPosW() const noexcept
{
    return m_camPos;
}

const glm::vec3& Camera::getCamLookW() const noexcept
{
    return m_camLook;
}

const glm::vec3& Camera::getCamUpW() const noexcept
{
    return m_camUp;
}

const glm::vec3& Camera::getCamRightW() const noexcept
{
    return m_camRight;
}

glm::vec3 Camera::getCurPosProj(const std::pair<float, float>& cur_pos_ndc) const
{
    /*Calculate distance from camera to projection plane*/
    float d = 1.0f / tanf(m_verFovAngle / 2.0f);

    return glm::vec3(cur_pos_ndc.first * m_aspectRatio, cur_pos_ndc.second, d);
}

glm::vec3 Camera::getCurPosProj(VulkanWindow& win) const
{
    auto cur_pos_ndc = win.getCurPosNDC();
    return getCurPosProj(cur_pos_ndc);
}
