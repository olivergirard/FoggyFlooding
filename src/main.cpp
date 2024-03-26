#include <iostream>
#include <vector>
#include <stdint.h>
#include <tuple>
#include <random>

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_opengl3.h"

using namespace std;

/* Reduce this number to speed up compilation time. Generally 250+. */
const int NUM_PARTICLES = 500;

const int screenWidth = 800;
const int screenHeight = 800;
glm::vec3 background = glm::vec3(0.922, 0.851, 0.737);

/* Do not modify these as they are essential for ray tracing. */
glm::vec3 eye = glm::vec3(0.0f, 0.0f, -screenWidth);
glm::vec3 lookAt = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

const GLfloat EPSILON = 1e-2;

struct Ray {
	glm::vec3 origin;
	glm::vec3 direction;
};

struct Light {
	glm::vec3 position;
	glm::vec3 diff;
	glm::vec3 spec;
};

struct Particle {
	glm::vec3 position;
	glm::vec3 diff;

	/* For now, these values are hard-coded. */
	GLfloat radius = 50.0f;
	glm::vec3 spec = glm::vec3(1.0f, 1.0f, 01.0f);
	GLfloat shininess = 10.0f;
};

vector<Light> lights;
vector<Particle> particles;
vector<glm::vec3> colors;

/* Generates the initial position for the particles that comprise the water. */
glm::vec3 GenerateRightPosition() {

	std::random_device rd;
	std::mt19937 gen(rd());
	
	/* Determines the shape of the water triangle. */
	float minX = -450.0f;
	float maxX = 1000.0f;
	float minY = -200.0f;
	float maxY = 900.0f;
	float minZ = 650.0f;
	float maxZ = 1000.0f;

	std::uniform_real_distribution<float> disX(minX, maxX);
	std::uniform_real_distribution<float> disY(minY, maxY);
	std::uniform_real_distribution<float> disZ(minZ, maxZ);

	GLfloat x = disX(gen);
	GLfloat y = disY(gen);
	GLfloat z = disZ(gen);

	while (x < (-2.0f * y + 800)) {
		x = disX(gen);
		y = disY(gen);
	}

	return glm::vec3(x, y, z);

}

glm::vec3 GenerateLeftPosition() {

	std::random_device rd;
	std::mt19937 gen(rd());

	/* Determines the shape of the water triangle. */
	float minX = -100.0f;
	float maxX = 1000.0f; //more negative, more up
	float minY = -900.0f;
	float maxY = 200.0f;
	float minZ = 650.0f;
	float maxZ = 1000.0f;

	std::uniform_real_distribution<float> disX(minX, maxX);
	std::uniform_real_distribution<float> disY(minY, maxY);
	std::uniform_real_distribution<float> disZ(minZ, maxZ);

	GLfloat x = disX(gen);
	GLfloat y = disY(gen);
	GLfloat z = disZ(gen);

	while (y > (2.0f * x - 1200)) {
		x = disX(gen);
		y = disY(gen);
	}

	return glm::vec3(x, y, z);

}

/* Creates any particles necessary for the scene. */
vector<Particle> CreateParticles(GLuint numParticles) {

	vector<Particle> particles;

	srand(time(NULL));

	Particle p;

	for (int i = 0; i < numParticles; i++) {

		if (i % 2 == 0) {
			p.position = GenerateRightPosition();
		}
		else {
			p.position = GenerateLeftPosition();
		}
		
		p.diff = glm::vec3(0.0f, (double)rand() / RAND_MAX, 1.0f);
		particles.push_back(p);
	}

	return particles;
}

/* Creates any lights necessary to illuminate the scene. */
vector<Light> CreateLights() {

	Light l;

	l.position = glm::vec3(0.0f, 0.0f, -screenWidth * 0.5);
	l.diff = glm::vec3(1, 1, 1);
	l.spec = glm::vec3(0, 0, 0);

	lights.push_back(l);
	return lights;
}

/* Calculates the ray going through the pixel at the given location. */
Ray CalculateRay(GLuint pixelWidth, GLuint pixelHeight) {

	GLfloat fovy = glm::radians(60.0f);
	GLfloat aspectRatio = screenWidth / screenHeight;
	GLfloat focalLength = 1.0f / tan(fovy / 2.0f);

	/* Gramm-Schmidt orthonormalization. */
	glm::vec3 l = glm::normalize(lookAt - eye);
	glm::vec3 v = glm::normalize(glm::cross(l, up));
	glm::vec3 u = glm::cross(v, l);

	glm::vec3 ll = eye + (focalLength * l) - (aspectRatio * v) - u;
	glm::vec3 p = ll + (2.0f * aspectRatio * v * (GLfloat)pixelWidth / (GLfloat)screenWidth) + (2.0f * u * (GLfloat)pixelHeight / (GLfloat)screenHeight);
	glm::vec3 direction = glm::normalize(p - eye);

	Ray ray = { eye, direction };

	return ray;
}

/* Calculates ray-sphere intersection and returns the associated t value. */
GLfloat SphereIntersection(const Ray& ray, const Particle& particle) {

	GLfloat a = glm::dot(ray.direction, ray.direction);
	GLfloat b = 2.0f * glm::dot(ray.origin - particle.position, ray.direction);
	GLfloat c = glm::dot(ray.origin - particle.position, ray.origin - particle.position) - pow(particle.radius, 2);

	GLfloat discriminant = pow(b, 2) - (4.0f * a * c);

	if (discriminant < 0.0f) {
		return -1.0f;
	}

	GLfloat t1 = (-b + sqrt(discriminant)) / (2.0f * a);
	GLfloat t2 = (-b - sqrt(discriminant)) / (2.0f * a);

	if ((t1 > 0.0f) && (t2 > 0.0f)) {
		return fmin(t1, t2);
	}
	else if (t1 > 0.0f) {
		return t1;
	}
	else if (t2 > 0.0f) {
		return t2;
	}

	/* -1.0f indicates no intersection and is checked in FindFirstIntersection(). */
	return -1.0f;
}

/* Finds the first surface intersected by the ray. */
tuple<GLfloat, Particle> FindFirstIntersection(const Ray& ray) {

	GLfloat firstIntersection = numeric_limits<GLfloat>::infinity();
	Particle firstParticle;

	for (const Particle& particle : particles) {

		GLfloat intersection = SphereIntersection(ray, particle);

		if ((intersection > 0.0f) && (intersection < firstIntersection)) {
			firstIntersection = intersection;
			firstParticle = particle;
		}
	}

	/* Indicates an invalid intersection TraceRay(). */
	if (firstIntersection == numeric_limits<GLfloat>::infinity()) {
		firstIntersection = -1.0f;
	}

	return tuple<GLfloat, Particle>(firstIntersection, firstParticle);
}

/* Calculates the normal vector for a sphere. */
glm::vec3 SphereNormal(const Ray& ray, const Particle& particle, GLfloat t) {

	glm::vec3 intersection = ray.origin + (t * ray.direction);
	glm::vec3 normal = glm::normalize(intersection - particle.position);

	return normal;
}

/* Returns a vector of all of the rays that are visible from the given point on the given surface. */
vector<Light> ShadowRays(const Ray& ray, GLfloat t, const Particle& particle) {

	vector<Light> visibleLights;
	glm::vec3 intersection = ray.origin + (t * ray.direction);

	for (Light& light : lights) {

		GLfloat distance = glm::length(light.position - intersection);
		GLfloat t = SphereIntersection(ray, particle);
		
		/* If the distance from the intersection point to the light is less than the distance from the ray to the intersection point. */
		if ((t > 0.0f) && (distance < t)) {
			visibleLights.push_back(light);
		}
	}

	return visibleLights;
}

/* Calculates the color associated with Phong shading for the given point on the surface. */
glm::vec3 Phong(const Ray& ray, const Light& light, GLfloat t, const Particle& particle) {

	glm::vec3 intersection = ray.origin + (ray.direction * t);
	glm::vec3 normal = SphereNormal(ray, particle, t);

	glm::vec3 lightRay = glm::normalize(light.position - intersection);
	glm::vec3 reflectedRay = glm::reflect(lightRay, normal);
	glm::vec3 viewRay = -ray.direction;

	glm::vec3 diffuse = particle.diff * glm::dot(lightRay, normal) * light.diff;
	glm::vec3 specular = particle.spec * pow(glm::dot(reflectedRay, viewRay), particle.shininess) * light.spec;

	glm::vec3 color = diffuse + specular;
	return color;
}

/* Determines whether to return the background color or the calculated Phong shading value per pixel. */
glm::vec3 TraceRay(const Ray& ray) {

	tuple<GLfloat, Particle> tuple = FindFirstIntersection(ray);
	GLfloat t = get<0>(tuple);
	Particle particle = get<1>(tuple);

	if (t < 0.0f) {
		return background;
	}

	vector<Light> contributedLights = ShadowRays(ray, t, particle);
	glm::vec3 color = glm::vec3(0.0f, 0.0f, 0.0f);

	for (Light& light : contributedLights) {
		color += Phong(ray, light, t, particle);
	}

	return color;
}

/* Create a vector of colors used for generating the final scene. */
vector<glm::vec3> RayTraceOutput() {

	glm::vec3 color;

	vector<glm::vec3> colors;

	for (GLuint i = 0; i < screenWidth; i++)
	{
		for (GLuint j = 0; j < screenHeight; j++)
		{
			Ray ray = CalculateRay(i, j);
			glm::vec3 color = TraceRay(ray);
			color = glm::clamp(color, 0.0f, 1.0f);

			colors.push_back(color);
		}
	}

	return colors;
}

int main() {

	glfwInit();
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "CS434 Final Project", NULL, NULL);

	if (window == NULL) {
		cout << "Cannot open GLFW window" << endl;
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);

	gladLoadGL();
	glViewport(0, 0, screenWidth, screenHeight);

	particles = CreateParticles(NUM_PARTICLES);
	lights = CreateLights();

	colors = RayTraceOutput();

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screenWidth, screenHeight, 0, GL_RGB, GL_FLOAT, colors.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	colors.clear();

	while (!glfwWindowShouldClose(window)) {

		glClear(GL_COLOR_BUFFER_BIT);

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, texture);

		glBegin(GL_QUADS);

		/* Interleaving texture coordinates and vertices. */
		glTexCoord2f(0.0f, 0.0f); 
		glVertex2f(-1.0f, -1.0f);

		glTexCoord2f(1.0f, 0.0f); 
		glVertex2f(1.0f, -1.0f);

		glTexCoord2f(1.0f, 1.0f); 
		glVertex2f(1.0f, 1.0f);

		glTexCoord2f(0.0f, 1.0f); 
		glVertex2f(-1.0f, 1.0f);

		glEnd();

		glDisable(GL_TEXTURE_2D);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();
	return 0;
}

