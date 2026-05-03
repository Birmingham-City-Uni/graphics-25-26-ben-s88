#pragma once
#include "Renderable.hpp"

class Renderable;

class Light
{
public:
	virtual ~Light() throw()
	{}
	enum Type { POINT, DIRECTIONAL, AMBIENT, AREA };
	Type type;
	virtual bool visibilityCheck(const Eigen::Vector3f& location, const Renderable* renderable) const = 0;
	virtual Eigen::Vector3f getIntensity(const Eigen::Vector3f& location) const = 0;
	virtual Eigen::Vector3f getVecToLight(const Eigen::Vector3f& location) const = 0;
};

