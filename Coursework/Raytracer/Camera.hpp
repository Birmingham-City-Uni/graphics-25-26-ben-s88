#pragma once
#include "Ray.hpp"
#include <vector>

/// <summary>
/// Movable camera class. Provide the camera location, forward direction and an up
/// vector, along with the image dimensions and vertical Field of View angle (radians).
/// The camera can then produce a ray passing through each pixel location.
/// </summary>
class Camera
{
private:
	Eigen::Vector3f location_, bottomLeftPix_, right1pix_, up1pix_;

public:
	Camera(
		const Eigen::Vector3f& location,
		const Eigen::Vector3f& forward,
		const Eigen::Vector3f& up,
		int pixWidth, int pixHeight,
		float vertFov)
		:location_(location)
	{
		Eigen::Vector3f forwardVec = forward.normalized();
		Eigen::Vector3f rightVec = (up.cross(forwardVec)).normalized();
		Eigen::Vector3f upVec = (forward.cross(rightVec)).normalized();

		float aspect = static_cast<float>(pixWidth) / static_cast<float>(pixHeight);

		float halfHeight = tan(vertFov / 2);
		float halfWidth = aspect * halfHeight;

		bottomLeftPix_ = location + forwardVec - (halfWidth * rightVec + halfHeight * upVec);

		right1pix_ = rightVec * halfWidth * 2.f / static_cast<float>(pixWidth);
		up1pix_ = upVec * halfHeight * 2.f / static_cast<float>(pixHeight);
	}

	Ray getRay(int pixX, int pixY)
	{
		Ray ray;
		ray.origin = location_;
		float jitteredPixX = pixX + (std::rand() / 10.f);
		float jitteredPixY = pixY + (std::rand() / 10.f);
		Eigen::Vector3f pixelPos = bottomLeftPix_ +
			static_cast<float>(pixX) * right1pix_ +
			static_cast<float>(pixY) * up1pix_;

		ray.direction = (pixelPos - location_).normalized();
		return ray;
	}

	Ray getRay(float pixX, float pixY)
	{
		Ray ray;
		ray.origin = location_;
		Eigen::Vector3f pixelPos = bottomLeftPix_ +
			(pixX) * right1pix_ +
			(pixY) * up1pix_;

		ray.direction = (pixelPos - location_).normalized();
		return ray;
	}

	std::vector<Ray> getRays(int pixX, int pixY, int rayGridSize)
	{
		std::vector<Ray> ret;
		float randNum = 0;
		float gridStepAmount = 1.0f / rayGridSize;
		ret.reserve(pow(rayGridSize, 2));
		for (int y = 1; y < rayGridSize + 1; y++)
			for (int x = 1; x < rayGridSize + 1; x++)
			{
				randNum = ((std::rand() % 11) - 5) / 10.f;
				ret.push_back(getRay(pixX + (x * gridStepAmount) + (randNum * gridStepAmount), pixY + (y * gridStepAmount) + (randNum * gridStepAmount)));
			}

		return ret;
	}
};