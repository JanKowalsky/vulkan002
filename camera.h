#ifndef CAMERA_H
#define CAMERA_H

#include "vulkan_math.h"
#include "VulkanWindow.h"
#include <utility>

class Camera
{
public:
	Camera() {}

    Camera(Camera&) {}
    ~Camera() {}
	Camera(float ratio, float n, float f, float verfovangle);

	void walk(float d);
	void strafe(float d);
	void pitch(float angle);
	void rotate(float angle);
	void upDown(float d);

	void setPos(glm::vec3);
	void setHorizontalFOVAngle(float);
	void setVerticalFOVAngle(float);
	void setNear(float);
	void setFar(float);
	void setAspectRatio(float);

	float getHorizontalFOVAngle() const noexcept;
	float getVerticalFOVAngle() const noexcept;
	float getAspectRatio() const noexcept;
	float getFar() const noexcept;
	float getNear() const noexcept;

	glm::fmat4x4 getViewProj() const noexcept;
	const glm::fmat4x4& getView() const noexcept;
	const glm::fmat4x4& getProj() const noexcept;

	const glm::vec3& getCamPosW() const noexcept;
	const glm::vec3& getCamLookW() const noexcept;
	const glm::vec3& getCamUpW() const noexcept;
	const glm::vec3& getCamRightW() const noexcept;

	glm::vec3 getCurPosProj(const std::pair<float, float>& cur_pos_ndc) const;
	glm::vec3 getCurPosProj(VulkanWindow&) const;
private:

private:
	glm::fmat4x4 m_view;
	glm::fmat4x4 m_proj;

	glm::vec3 m_camPos;
	glm::vec3 m_camLook;
	glm::vec3 m_camUp;
	glm::vec3 m_camRight;

	float m_near;
	float m_far;
	float m_verFovAngle;
	float m_horFovAngle;
	float m_aspectRatio;
};

#endif //CAMERA_H
