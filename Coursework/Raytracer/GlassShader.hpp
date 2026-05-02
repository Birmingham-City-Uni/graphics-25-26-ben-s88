#pragma once
#include "Shader.hpp"

/// <summary>
/// Shader for diffuse, Lambertian surfaces of a single colour.
/// </summary>
class GlassShader : public Shader
{
private:
	float _refractiveIndex;
public:
	GlassShader(float refractiveIndex)
		:_refractiveIndex(refractiveIndex)
	{
	}

	virtual Eigen::Vector3f getColor(const HitInfo& hitInfo,
		const Renderable* scene,
		const std::vector<std::unique_ptr<Light>>& lights,
		const Eigen::Vector3f& ambientLight,
		int currBounceCount,
		const int maxBounces) const
	{
		if (currBounceCount >= maxBounces) return Eigen::Vector3f::Zero();

		Ray refractionRay;
		refractionRay.direction = refract(hitInfo.inDirection.normalized(), hitInfo.normal.normalized(), _refractiveIndex);
		refractionRay.origin = hitInfo.location;

		Eigen::Vector3f color = Eigen::Vector3f::Zero();

		HitInfo refractionHit;
		if (scene->intersect(refractionRay, 1e-6f, 1e4f, refractionHit, VISIBLE_BITMASK)) {
			//std::cout << "intersected";
			color = refractionHit.shader->getColor(
				refractionHit, scene,
				lights, ambientLight,
				currBounceCount + 1, maxBounces);
		}

		return color;
	}
};

