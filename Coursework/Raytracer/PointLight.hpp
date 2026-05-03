#pragma once
#include "Light.hpp"

class PointLight : public Light
{
private:
	Eigen::Vector3f location_, intensity_;
public:
	PointLight(const Eigen::Vector3f& location, const Eigen::Vector3f& intensity)
		:location_(location), intensity_(intensity)
	{}
	Type type = POINT;

	virtual bool visibilityCheck(const Eigen::Vector3f& location, const Renderable* renderable) const override
	{
		Ray shadowRay;
		shadowRay.origin = location;
		shadowRay.direction = (location_ - location).normalized();
		float maxT = (location_ - location).norm();
		HitInfo info;
		return !renderable->intersect(shadowRay, 1e-4f, maxT, info, SHADOW_BITMASK);
	}

	virtual Eigen::Vector3f getIntensity(const Eigen::Vector3f& location) const override
	{
		float dist = (location_ - location).norm();
		return intensity_ / (dist * dist);
	}

	virtual Eigen::Vector3f getVecToLight(const Eigen::Vector3f& location) const override
	{
		return (location_ - location).normalized();
	}

	bool visibilityCheckWithOffset(const Eigen::Vector3f& location, const Renderable* renderable, Eigen::Vector3f offset) const
	{
		Ray shadowRay;
		shadowRay.origin = location;
		shadowRay.direction = ((location_ + offset) - location).normalized();
		float maxT = ((location_ + offset) - location).norm();
		HitInfo info;
		return !renderable->intersect(shadowRay, 1e-4f, maxT, info, SHADOW_BITMASK);
	}
};

