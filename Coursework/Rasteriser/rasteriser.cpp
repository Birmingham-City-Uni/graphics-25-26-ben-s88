#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>
#include <lodepng.h>
#include "Image.hpp"
#include "LinAlg.hpp"
#include "Light.hpp"
#include "Mesh.hpp"
#include "Shading.hpp"

int timesDraw = 0;

struct Triangle {
	std::array<Eigen::Vector3f, 3> screen; // Coordinates of the triangle in screen space.
	std::array<Eigen::Vector3f, 3> verts; // Vertices of the triangle in world space.
	std::array<Eigen::Vector3f, 3> norms; // Normals of the triangle corners in world space.
	std::array<Eigen::Vector2f, 3> texs; // Texture coordinates of the triangle corners.
};


Eigen::Matrix4f projectionMatrix(int height, int width, float horzFov = 70.f * M_PI / 180.f, float zFar = 1000.f, float zNear = 0.1f)
{
	float vertFov = horzFov * float(height) / width;
	Eigen::Matrix4f projection;
	projection <<
		1.0f / tanf(0.5f * horzFov), 0, 0, 0,
		0, 1.0f / tanf(0.5f * vertFov), 0, 0,
		0, 0, zFar / (zFar - zNear), -zFar * zNear / (zFar - zNear),
		0, 0, 1, 0;
	return projection;
}

void findScreenBoundingBox(const Triangle& t, int width, int height, int& minX, int& minY, int& maxX, int& maxY)
{
	// Find a bounding box around the triangle
	minX = std::min(std::min(t.screen[0].x(), t.screen[1].x()), t.screen[2].x());
	minY = std::min(std::min(t.screen[0].y(), t.screen[1].y()), t.screen[2].y());
	maxX = std::max(std::max(t.screen[0].x(), t.screen[1].x()), t.screen[2].x());
	maxY = std::max(std::max(t.screen[0].y(), t.screen[1].y()), t.screen[2].y());

	// Constrain it to lie within the image.
	minX = std::min(std::max(minX, 0), width - 1);
	maxX = std::min(std::max(maxX, 0), width - 1);
	minY = std::min(std::max(minY, 0), height - 1);
	maxY = std::min(std::max(maxY, 0), height - 1);
}

void drawTriangle(std::vector<uint8_t>& image, int width, int height,
	std::vector<float>& zBuffer,
	const Triangle& t,
	const std::vector<std::unique_ptr<Light>>& lights,
	const Eigen::Vector3f& albedo, const Eigen::Vector3f& specularColor,
	float specularExponent, uint8_t opacity,
	const Eigen::Vector3f& camWorldPos)
{
	int minX, minY, maxX, maxY;
	findScreenBoundingBox(t, width, height, minX, minY, maxX, maxY);

	Eigen::Vector2f edge1 = v2(t.screen[2] - t.screen[0]);
	Eigen::Vector2f edge2 = v2(t.screen[1] - t.screen[0]);
	float triangleArea = 0.5f * vec2Cross(edge2, edge1);
	if (triangleArea < 0) {
		// Triangle is backfacing
		// Exit and quit drawing!
		return;
	}

	for (int x = minX; x <= maxX; ++x)
		for (int y = minY; y <= maxY; ++y) {
			Eigen::Vector2f p(x, y);

			// Find sub-triangle areas
			float a0 = 0.5f * fabsf(vec2Cross(v2(t.screen[1]) - v2(t.screen[2]), p - v2(t.screen[2])));
			float a1 = 0.5f * fabsf(vec2Cross(v2(t.screen[0]) - v2(t.screen[2]), p - v2(t.screen[2])));
			float a2 = 0.5f * fabsf(vec2Cross(v2(t.screen[0]) - v2(t.screen[1]), p - v2(t.screen[1])));

			// find barycentrics
			float b0 = a0 / triangleArea;
			float b1 = a1 / triangleArea;
			float b2 = a2 / triangleArea;

			// If outside triangle, exit early
			float sum = b0 + b1 + b2;
			if (sum > 1.0001) {
				continue;
			}

			Eigen::Vector3f worldP = t.verts[0] * b0 + t.verts[1] * b1 + t.verts[2] * b2;

			float depth = t.screen[0].z() * b0 + t.screen[1].z() * b1 + t.screen[2].z() * b2;
			int depthIdx = static_cast<int>(p.x()) + static_cast<int>(p.y()) * width;
			if (depth > zBuffer[depthIdx]) continue;
			zBuffer[depthIdx] = depth;

			Eigen::Vector3f normP = t.norms[0] * b0 + t.norms[1] * b1 + t.norms[2] * b2;
			normP.normalize();

			// Work out colour at this position.
			Eigen::Vector3f color = Eigen::Vector3f::Zero();

			// Iterate over lights, and sum to find colour.
			for (auto& light : lights) {

				// Work out the contribution from this light source, and add it to the color variable.

				// Work out the intensity of this light source, at the point worldP.
				Eigen::Vector3f lightIntensity = light->getIntensityAt(worldP);

				// We only need to do the following if the light isn't an ambient light.
				if (light->getType() != Light::Type::AMBIENT) {

					// Subtask 3: Work out correct inputs for the phongSpecularTerm function inside drawTriangle, and draw an image!
					// *** YOUR CODE HERE ***
					// Work out the incoming light dir (from the light into the surface point).
					Eigen::Vector3f incomingLightDir = light->getDirection(worldP);
					// Work out the view direction (from surface point towards camera). Make sure it's normalized!
					Eigen::Vector3f viewDir = (camWorldPos - worldP).normalized();
					// Find the specular term by calling phongSpecularTerm.
					float specularTerm = blinnPhongSpecularTerm(incomingLightDir, normP, viewDir, specularExponent);
					// *** END YOUR CODE ***

					Eigen::Vector3f specularOut = specularColor * specularTerm;
					specularOut = coeffWiseMultiply(specularOut, lightIntensity);

					// Take the dot product of the normal with the light direction.
					float dotProd = normP.dot(-incomingLightDir);

					// We don't want negative light - if dot product less than 0, set it to 0.
					dotProd = std::max(dotProd, 0.0f);

					// Multiply the light intensity by the dot product.
					Eigen::Vector3f diffuseOut = lightIntensity * dotProd;
					diffuseOut = coeffWiseMultiply(diffuseOut, albedo);

					// Add both diffuse and specular components to the colour.
					color += specularOut;
					color += diffuseOut;
				}
				else {
					// Light is ambient - just multiply light intensity with albedo.
					color += coeffWiseMultiply(lightIntensity, albedo);
				}
			}

			Color cA;
			// Gamma-correcting colours.
			cA.r = std::min(powf(color.x(), 1 / 2.2f), 1.0f) * 255;
			cA.g = std::min(powf(color.y(), 1 / 2.2f), 1.0f) * 255;
			cA.b = std::min(powf(color.z(), 1 / 2.2f), 1.0f) * 255;
			cA.a = 255;

			if (opacity < 255)
			{
				float opacityPercent = (opacity / 255.f);

				cA.r *= opacityPercent;
				cA.g *= opacityPercent;
				cA.b *= opacityPercent;

				Color cB = getPixel(image, x, y, width, height);
				//Color cB{ 255, 0, 0, 255 };
				cB.r *= 1.0f - opacityPercent;
				cB.g *= 1.f - opacityPercent;
				cB.b *= 1.f - opacityPercent;
			
				Color cOutput{0, 0, 0, 255};
				cOutput.r =  std::min((int)cA.r + (int)cB.r, 255);
				cOutput.g =  std::min((int)cA.g + (int)cB.g, 255);
				cOutput.b =  std::min((int)cA.b + (int)cB.b, 255);

				setPixel(image, x, y, width, height, cOutput);
			}
			else
			{
				setPixel(image, x, y, width, height, cA);
			}

			
			timesDraw++;
		}
}

void drawMesh(std::vector<unsigned char>& image,
	std::vector<float>& zBuffer,
	const Mesh& mesh,
	const Eigen::Vector3f& albedo, const Eigen::Vector3f& specularColor,
	float specularExponent, uint8_t opacity,
	const Eigen::Vector3f& camWorldPos,
	const Eigen::Matrix4f& modelToWorld,
	const Eigen::Matrix4f& worldToClip,
	const std::vector<std::unique_ptr<Light>>& lights,
	int width, int height)
{
	for (int i = 0; i < mesh.vFaces.size(); ++i) {
		Eigen::Vector3f
			v0 = mesh.verts[mesh.vFaces[i][0]],
			v1 = mesh.verts[mesh.vFaces[i][1]],
			v2 = mesh.verts[mesh.vFaces[i][2]];
		Eigen::Vector3f
			n0 = mesh.norms[mesh.nFaces[i][0]],
			n1 = mesh.norms[mesh.nFaces[i][1]],
			n2 = mesh.norms[mesh.nFaces[i][2]];

		Triangle t;
		t.verts[0] = (modelToWorld * vec3ToVec4(v0)).block<3, 1>(0, 0);
		t.verts[1] = (modelToWorld * vec3ToVec4(v1)).block<3, 1>(0, 0);
		t.verts[2] = (modelToWorld * vec3ToVec4(v2)).block<3, 1>(0, 0);

		// Work out the clip space coordinates, by multiplying by worldToClip and doing the 
		// perspective divide.
		Eigen::Vector4f vClip0 = worldToClip * modelToWorld * vec3ToVec4(v0);
		vClip0 /= vClip0.w();
		Eigen::Vector4f vClip1 = worldToClip * modelToWorld * vec3ToVec4(v1);
		vClip1 /= vClip1.w();
		Eigen::Vector4f vClip2 = worldToClip * modelToWorld * vec3ToVec4(v2);
		vClip2 /= vClip2.w();

		// Check that all 3 vertices are in the clip box (-1 to 1 in x, y and z) and if not,
		// skip drawing this triangle.
		if (outsideClipBox(vClip0) && outsideClipBox(vClip1) && outsideClipBox(vClip2)) continue;

		// Work out the screen space coordinates based on the image height and width.
		t.screen[0] = Eigen::Vector3f((vClip0.x() + 1.0f) * width / 2, (-vClip0.y() + 1.0f) * height / 2, vClip0.z());
		t.screen[1] = Eigen::Vector3f((vClip1.x() + 1.0f) * width / 2, (-vClip1.y() + 1.0f) * height / 2, vClip1.z());
		t.screen[2] = Eigen::Vector3f((vClip2.x() + 1.0f) * width / 2, (-vClip2.y() + 1.0f) * height / 2, vClip2.z());

		// transform the normals (using the inverse transpose of the upper 3x3 block)
		t.norms[0] = (modelToWorld.block<3, 3>(0, 0).inverse().transpose() * n0).normalized();
		t.norms[1] = (modelToWorld.block<3, 3>(0, 0).inverse().transpose() * n1).normalized();
		t.norms[2] = (modelToWorld.block<3, 3>(0, 0).inverse().transpose() * n2).normalized();

		t.texs[0] = mesh.texs[mesh.tFaces[i][0]];
		t.texs[1] = mesh.texs[mesh.tFaces[i][1]];
		t.texs[2] = mesh.texs[mesh.tFaces[i][2]];

		drawTriangle(image, width, height, zBuffer, t, lights, albedo, specularColor, specularExponent, opacity, camWorldPos);
	}
}

void drawTriangle(std::vector<uint8_t>& image, int width, int height, // textured
	std::vector<float>& zBuffer,
	const Triangle& t,
	const std::vector<std::unique_ptr<Light>>& lights,
	const Eigen::Vector3f& specularColor,
	float specularExponent,
	const Eigen::Vector3f& camWorldPos,
	const std::vector<uint8_t>& albedoTexture, int texHeight, int texWidth)
{
	int minX, minY, maxX, maxY;
	findScreenBoundingBox(t, width, height, minX, minY, maxX, maxY);

	Eigen::Vector2f edge1 = v2(t.screen[2] - t.screen[0]);
	Eigen::Vector2f edge2 = v2(t.screen[1] - t.screen[0]);
	float triangleArea = 0.5f * vec2Cross(edge2, edge1);
	if (triangleArea < 0) {
		// Triangle is backfacing
		// Exit and quit drawing!
		return;
	}

	for (int x = minX; x <= maxX; ++x)
		for (int y = minY; y <= maxY; ++y) {
			Eigen::Vector2f p(x, y);

			// Find sub-triangle areas
			float a0 = 0.5f * fabsf(vec2Cross(v2(t.screen[1]) - v2(t.screen[2]), p - v2(t.screen[2])));
			float a1 = 0.5f * fabsf(vec2Cross(v2(t.screen[0]) - v2(t.screen[2]), p - v2(t.screen[2])));
			float a2 = 0.5f * fabsf(vec2Cross(v2(t.screen[0]) - v2(t.screen[1]), p - v2(t.screen[1])));

			// find barycentrics
			float b0 = a0 / triangleArea;
			float b1 = a1 / triangleArea;
			float b2 = a2 / triangleArea;

			// If outside triangle, exit early
			float sum = b0 + b1 + b2;
			if (sum > 1.0001) {
				continue;
			}

			Eigen::Vector3f worldP = t.verts[0] * b0 + t.verts[1] * b1 + t.verts[2] * b2;

			float depth = t.screen[0].z() * b0 + t.screen[1].z() * b1 + t.screen[2].z() * b2;
			int depthIdx = static_cast<int>(p.x()) + static_cast<int>(p.y()) * width;
			if (depth > zBuffer[depthIdx]) continue;
			zBuffer[depthIdx] = depth;

			Eigen::Vector3f normP = t.norms[0] * b0 + t.norms[1] * b1 + t.norms[2] * b2;
			normP.normalize();


			Eigen::Vector2f texP = (t.texs[0] * b0) + (t.texs[1] * b1) + (t.texs[2] * b2);

			texP.x() -= floorf(texP.x());
			texP.y() -= floorf(texP.y());

			// Convert this coordinate to a point in texture space
			// To do so, multiply by the texWidth and texHeight to get to the correct range.
			// Don't forget to flip the y coordinates! 
			int texR = (1 - texP.y()) * texHeight;
			int texC = texP.x() * texWidth;

			// Handle the case where texR or texC end up outside the image!
			// There are different ways you could do this - for example using 
			// the modulo (%) operator to wrap around, or clamping to the edges.
			// Write your own code below to do this - once you're done you should be sure 
			// that 0 <= texC < texWidth and 0 <= texR < texHeight.
			if (texR)
			{
				texR -= 1;
			}
			if (texC)
			{
				texC -= 1;
			}

			// Get the value from the texture (hint: use the getPixel function on the albedoTexture).
			Color texColor = getPixel(albedoTexture, texC, texR, texWidth, texHeight); //get pixel of Ts

			// Convert it into an Eigen::Vector3f as an albedo
			// (Optional bonus task, if you checked out the slides on gamma correction:
			// gamma correct this colour, so the texture doesn't appear overly bright.
			// should you raise to the power 1/2.2, or 2.2?)
			Eigen::Vector3f albedo;
			albedo.x() = powf(texColor.r / 255.f, 2.2f);
			albedo.y() = powf(texColor.g / 255.f, 2.2f);
			albedo.z() = powf(texColor.b / 255.f, 2.2f);

			// Work out colour at this position.
			Eigen::Vector3f color = Eigen::Vector3f::Zero();

			// Iterate over lights, and sum to find colour.
			for (auto& light : lights) {

				// Work out the contribution from this light source, and add it to the color variable.

				// Work out the intensity of this light source, at the point worldP.
				Eigen::Vector3f lightIntensity = light->getIntensityAt(worldP);

				// We only need to do the following if the light isn't an ambient light.
				if (light->getType() != Light::Type::AMBIENT) {

					// Subtask 3: Work out correct inputs for the phongSpecularTerm function inside drawTriangle, and draw an image!
					// *** YOUR CODE HERE ***
					// Work out the incoming light dir (from the light into the surface point).
					Eigen::Vector3f incomingLightDir = light->getDirection(worldP);
					// Work out the view direction (from surface point towards camera). Make sure it's normalized!
					Eigen::Vector3f viewDir = (camWorldPos - worldP).normalized();
					// Find the specular term by calling phongSpecularTerm.
					float specularTerm = blinnPhongSpecularTerm(incomingLightDir, normP, viewDir, specularExponent);
					// *** END YOUR CODE ***

					Eigen::Vector3f specularOut = specularColor * specularTerm;
					specularOut = coeffWiseMultiply(specularOut, lightIntensity);

					// Take the dot product of the normal with the light direction.
					float dotProd = normP.dot(-incomingLightDir);

					// We don't want negative light - if dot product less than 0, set it to 0.
					dotProd = std::max(dotProd, 0.0f);

					// Multiply the light intensity by the dot product.
					Eigen::Vector3f diffuseOut = lightIntensity * dotProd;
					diffuseOut = coeffWiseMultiply(diffuseOut, albedo);

					// Add both diffuse and specular components to the colour.
					color += specularOut;
					color += diffuseOut;
				}
				else {
					// Light is ambient - just multiply light intensity with albedo.
					color += coeffWiseMultiply(lightIntensity, albedo);
				}
			}

			Color c;
			// Gamma-correcting colours.
			c.r = std::min(powf(color.x(), 1 / 2.2f), 1.0f) * 255;
			c.g = std::min(powf(color.y(), 1 / 2.2f), 1.0f) * 255;
			c.b = std::min(powf(color.z(), 1 / 2.2f), 1.0f) * 255;

			c.a = 255;

			setPixel(image, x, y, width, height, c);
			timesDraw++;
		}
}

void drawMesh(std::vector<unsigned char>& image, // textured
	std::vector<float>& zBuffer,
	const Mesh& mesh,
	const Eigen::Vector3f& specularColor,
	float specularExponent,
	const Eigen::Vector3f& camWorldPos,
	const Eigen::Matrix4f& modelToWorld,
	const Eigen::Matrix4f& worldToClip,
	const std::vector<std::unique_ptr<Light>>& lights,
	int width, int height,
	const std::vector<uint8_t>& albedoTexture, int texHeight, int texWidth)
{
	for (int i = 0; i < mesh.vFaces.size(); ++i) {
		Eigen::Vector3f
			v0 = mesh.verts[mesh.vFaces[i][0]],
			v1 = mesh.verts[mesh.vFaces[i][1]],
			v2 = mesh.verts[mesh.vFaces[i][2]];
		Eigen::Vector3f
			n0 = mesh.norms[mesh.nFaces[i][0]],
			n1 = mesh.norms[mesh.nFaces[i][1]],
			n2 = mesh.norms[mesh.nFaces[i][2]];

		Triangle t;
		t.verts[0] = (modelToWorld * vec3ToVec4(v0)).block<3, 1>(0, 0);
		t.verts[1] = (modelToWorld * vec3ToVec4(v1)).block<3, 1>(0, 0);
		t.verts[2] = (modelToWorld * vec3ToVec4(v2)).block<3, 1>(0, 0);

		// Work out the clip space coordinates, by multiplying by worldToClip and doing the 
		// perspective divide.
		Eigen::Vector4f vClip0 = worldToClip * modelToWorld * vec3ToVec4(v0);
		vClip0 /= vClip0.w();
		Eigen::Vector4f vClip1 = worldToClip * modelToWorld * vec3ToVec4(v1);
		vClip1 /= vClip1.w();
		Eigen::Vector4f vClip2 = worldToClip * modelToWorld * vec3ToVec4(v2);
		vClip2 /= vClip2.w();

		// Check that all 3 vertices are in the clip box (-1 to 1 in x, y and z) and if not,
		// skip drawing this triangle.
		if (outsideClipBox(vClip0) && outsideClipBox(vClip1) && outsideClipBox(vClip2)) continue;

		// Work out the screen space coordinates based on the image height and width.
		t.screen[0] = Eigen::Vector3f((vClip0.x() + 1.0f) * width / 2, (-vClip0.y() + 1.0f) * height / 2, vClip0.z());
		t.screen[1] = Eigen::Vector3f((vClip1.x() + 1.0f) * width / 2, (-vClip1.y() + 1.0f) * height / 2, vClip1.z());
		t.screen[2] = Eigen::Vector3f((vClip2.x() + 1.0f) * width / 2, (-vClip2.y() + 1.0f) * height / 2, vClip2.z());

		// transform the normals (using the inverse transpose of the upper 3x3 block)
		t.norms[0] = (modelToWorld.block<3, 3>(0, 0).inverse().transpose() * n0).normalized();
		t.norms[1] = (modelToWorld.block<3, 3>(0, 0).inverse().transpose() * n1).normalized();
		t.norms[2] = (modelToWorld.block<3, 3>(0, 0).inverse().transpose() * n2).normalized();

		t.texs[0] = mesh.texs[mesh.tFaces[i][0]];
		t.texs[1] = mesh.texs[mesh.tFaces[i][1]];
		t.texs[2] = mesh.texs[mesh.tFaces[i][2]];

		drawTriangle(image, width, height, zBuffer, t, lights, specularColor, specularExponent, camWorldPos, albedoTexture, texHeight, texWidth);
	}
}

void doSSAA(std::vector<uint8_t>& imgBuffer, int height, int width, std::vector<uint8_t>& outputBuffer)
{
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			if (x % 2 == 0 && y % 2 == 0)
			{
				int pixelIdx = x + y * width;

				int r = getPixel(imgBuffer, x, y, width, height).r + getPixel(imgBuffer, x + 1, y, width, height).r + getPixel(imgBuffer, x, y + 1, width, height).r + getPixel(imgBuffer, x + 1, y + 1, width, height).r;
				r /= 4;
				int g = getPixel(imgBuffer, x, y, width, height).g + getPixel(imgBuffer, x + 1, y, width, height).g + getPixel(imgBuffer, x, y + 1, width, height).g + getPixel(imgBuffer, x + 1, y + 1, width, height).g;
				g /= 4;
				int b = getPixel(imgBuffer, x, y, width, height).b + getPixel(imgBuffer, x + 1, y, width, height).b + getPixel(imgBuffer, x, y + 1, width, height).b + getPixel(imgBuffer, x + 1, y + 1, width, height).b;
				b /= 4;
				Color color{ r, g, b, 255 };
				
				setPixel(outputBuffer, x / 2, y / 2, width / 2, height / 2, color);
			}
		}
	}
}

int main()
{
	std::string outputFilename = "output.png";
	int width = 1920, height = 1080;
	int outputWidth = 1920, outputHeight = 1080;
	const int nChannels = 4;
	bool SSAA = false;

	if (SSAA)
	{
		width *= 2;
		height *= 2;
	}
	

	// Set up an image buffer
	std::vector<uint8_t> imageBuffer(height*width*nChannels);
	std::vector<float> zBuffer(height * width);

    // **** Replace this bit with your lovely rasteriser code ****

	Color black{ 0,0,0,255 };
	for (int r = 0; r < height; ++r) {
		for (int c = 0; c < width; ++c) {
			setPixel(imageBuffer, c, r, width, height, black);
			zBuffer[r * width + c] = 1.0f;
		}
	}

	Eigen::Matrix4f projection = projectionMatrix(height, width);

	// This matrix rotates the camera, tilting it down, then translates it up to make it look down on the scene.
	Eigen::Matrix4f cameraToWorld = translationMatrix(Eigen::Vector3f(0.f, -.3f, 0.f)) * rotateXMatrix(0 * M_PI / 180);

	Eigen::Vector3f camWorldPos = (cameraToWorld * Eigen::Vector4f(0, 0, 0, 1)).block<3, 1>(0, 0);

	// The main important task = set up the worldToCamera and worldToClip matrices here!
	// Set up worldToCamera, based on cameraToWorld above
	Eigen::Matrix4f worldToCamera = cameraToWorld.inverse();
	// Set up worldToClip, using the projection and worldToCamera matrices
	Eigen::Matrix4f worldToClip = projection * worldToCamera;
    
	std::vector<std::unique_ptr<Light>> lights;
	lights.emplace_back(new AmbientLight(Eigen::Vector3f(0.2f, 0.2f, 0.2f)));
	lights.emplace_back(new DirectionalLight(Eigen::Vector3f(0.3f, 0.3f, .3f), Eigen::Vector3f(0.f, -1.f, -1.0f)));
	lights.emplace_back(new SpotLight(Eigen::Vector3f(10.f, 10.f, 10.f), Eigen::Vector3f(2.4f, 0.3f, 5.f), Eigen::Vector3f(-1.f, -0.5f, 0.2f), 40 * M_PI / 180));

	Mesh JamesMesh = loadMeshFile("../../Models/JamesExport3.obj");
	std::vector<uint8_t> jamesTexture;
	unsigned int jamesTexWidth, jamesTexHeight;
	lodepng::decode(jamesTexture, jamesTexWidth, jamesTexHeight, "../../Models/JamesTextureAtlas.png");

	Mesh groundMesh = loadMeshFile("../../Models/CarPark.obj");
	std::vector<uint8_t> groundTexture;
	unsigned int groundTexWidth, groundTexHeight;
	lodepng::decode(groundTexture, groundTexWidth, groundTexHeight, "../../textures/Pavement/worn_tile_floor_diff_4k.png");

	Mesh planeMesh = loadMeshFile("../../Models/PlaneExport.obj");
	std::vector<uint8_t> BGTexture;
	unsigned int BGTexWidth, BGTexHeight;
	lodepng::decode(BGTexture, BGTexWidth, BGTexHeight, "../../textures/SkyboxFlip.png");

	Mesh buildingMesh = loadMeshFile("../../Models/BuildingExport.obj");
	std::vector<uint8_t> BDTexture;
	unsigned int BDTexWidth, BDTexHeight;
	lodepng::decode(BDTexture, BDTexWidth, BDTexHeight, "../../textures/Wall/grey_plaster_03_diff_1k.png");

	Mesh carMesh = loadMeshFile("../../Models/CarExport.obj");
	std::vector<uint8_t> carTexture;
	unsigned int carTexWidth, carTexHeight;
	lodepng::decode(carTexture, carTexWidth, carTexHeight, "../../Models/pontiac-ventura/textures/ventura_d.png");

	Mesh carWindowMesh = loadMeshFile("../../Models/CarWindowExport.obj");

	Eigen::Matrix4f groundTransform;
	groundTransform = translationMatrix(Eigen::Vector3f(1.f, -1.2f, 2.f)) * scaleMatrix(0.8f) ;
	drawMesh(imageBuffer, zBuffer, groundMesh,
		Eigen::Vector3f::Ones() * 1.0f, 100.f, camWorldPos,
		groundTransform, worldToClip, lights, width, height, groundTexture, groundTexHeight, groundTexWidth);

	Eigen::Matrix4f jamesTransform;
	jamesTransform = translationMatrix(Eigen::Vector3f(1.5f, -0.8f, 6.9f)) * scaleMatrix(0.7) * rotateYMatrix(190 * M_PI / 180);
	drawMesh(imageBuffer, zBuffer, JamesMesh,
		Eigen::Vector3f::Ones() * 1.0f, 100.f, camWorldPos,
		jamesTransform, worldToClip, lights, width, height, jamesTexture, jamesTexHeight, jamesTexWidth);

	Eigen::Matrix4f BGTransform;
	BGTransform = translationMatrix(Eigen::Vector3f(-6.5f, -0.3f, 10.f)) * scaleMatrix(11);
	drawMesh(imageBuffer, zBuffer, planeMesh,
		Eigen::Vector3f::Ones() * 1.0f, 100.f, camWorldPos,
		BGTransform, worldToClip, lights, width, height, BGTexture, BGTexHeight, BGTexWidth);

	Eigen::Matrix4f BDTransform;
	BDTransform = translationMatrix(Eigen::Vector3f(3.f, -1.2f, 5.2f)) * scaleMatrix(0.6);
	drawMesh(imageBuffer, zBuffer, buildingMesh,
		Eigen::Vector3f::Ones() * 1.0f, 100.f, camWorldPos,
		BDTransform, worldToClip, lights, width, height, BDTexture, BDTexHeight, BDTexWidth);

	Eigen::Matrix4f carTransform;
    carTransform = translationMatrix(Eigen::Vector3f(0.f, -0.9f, 4.f)) * scaleMatrix(0.7) * rotateYMatrix(-25 * M_PI / 180);
	drawMesh(imageBuffer, zBuffer, carMesh,
		Eigen::Vector3f::Ones() * 1.0f, 100.f, camWorldPos,
		carTransform, worldToClip, lights, width, height, carTexture, carTexHeight, carTexWidth);

	drawMesh(imageBuffer, zBuffer, carWindowMesh, // draw the mesh of the car window using the car transform
		Eigen::Vector3f(1.0f, 1.0f, 1.0f),
		Eigen::Vector3f::Ones() * 1.0f, 100.0f, 100, camWorldPos,
		carTransform, worldToClip, lights, width, height);

	//drawPointLights(imageBuffer, width, height, lights);

	int errorCode = 0;
	if (SSAA)
	{
		std::vector<uint8_t> outputBuffer(outputWidth * outputHeight * nChannels);
		doSSAA(imageBuffer, height, width, outputBuffer);

		errorCode = lodepng::encode(outputFilename, outputBuffer, outputWidth, outputHeight);
	}
	else
	{
		errorCode = lodepng::encode(outputFilename, imageBuffer, outputWidth, outputHeight);
	}
    // **** End lovely rasteriser code ****

    // Save the image
    
    if (errorCode) { // check the error code, in case an error occurred.
        std::cout << "lodepng error encoding image: " << lodepng_error_text(errorCode) << std::endl;
        return errorCode;
    }

	saveZBufferImage("zBuffer.png", zBuffer, width, height);

	std::cout << timesDraw;

    return 0;
}
