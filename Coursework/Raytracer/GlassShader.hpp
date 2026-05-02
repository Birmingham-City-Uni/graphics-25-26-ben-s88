#pragma once
#include "Shader.hpp"

/// <summary>
/// Shader for diffuse, Lambertian surfaces of a single colour.
/// </summary>
class GlassShader : public Shader
{
private:
	float _refractiveIndex;
	Eigen::Vector3f _albedo;
	int _opacity;
public:
	GlassShader(float refractiveIndex = 1.f, Eigen::Vector3f albedo={0.f, 0.f, 0.f}, int opacity=0)
		:_refractiveIndex(refractiveIndex), _albedo(albedo), _opacity(std::min(opacity, 255))
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
		if (_opacity < 255)
		{
			float opacityPercent = (float)_opacity / 255;
			color.x() = (_albedo.x() * opacityPercent) + (color.x() * (1.0f - opacityPercent));
			color.y() = (_albedo.y() * opacityPercent) + (color.y() * (1.0f - opacityPercent));
			color.z() = (_albedo.z() * opacityPercent) + (color.z() * (1.0f - opacityPercent));
		}
		return color;
	}
};

