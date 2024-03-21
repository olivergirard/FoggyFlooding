#include <iostream>
#include <vector>
#include <stdint.h>
#include <tuple>

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_opengl3.h"

using namespace std;

GLuint VAO, VBO;
vector<glm::vec3> colors;

const int screenWidth = 800;
const int screenHeight = 800;
glm::vec3 background = glm::vec3(0, 1, 1);

/* Do not modify these. */
glm::vec3 eye = glm::vec3(0, 0, -screenWidth * 0.6); //change x and y for scaling 
glm::vec3 lookAt = glm::vec3(-100, -100, 0);
glm::vec3 up = glm::vec3(0, 1, 0.0);

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
	//glm::vec3 velocity;
	//glm::vec3 force;
	GLfloat radius = 50.0f;

	glm::vec3 diff = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 spec = glm::vec3(1.0f, 1.0f, 1.0f);
	GLfloat shininess = 20.0f;
};

vector<Light> lights;
vector<Particle> particles;

int CompileShaders() {
	//Vertex Shader
	const char* vsSrc = "#version 330 core\n"
		"layout (location = 0) in vec4 iPos;\n"
		"uniform mat4 modelview;\n"
		"void main()\n"
		"{\n"
		"   vec4 oPos=modelview*iPos;\n"
		"   gl_Position = vec4(oPos.x, oPos.y, oPos.z, oPos.w);\n"
		"}\0";

	//Fragment Shader
	const char* fsSrc = "#version 330 core\n"
		"out vec4 col;\n"
		"uniform vec4 color;\n"
		"void main()\n"
		"{\n"
		"   col = color;\n"
		"}\n\0";

	//Create VS object
	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	//Attach VS src to the Vertex Shader Object
	glShaderSource(vs, 1, &vsSrc, NULL);
	//Compile the vs
	glCompileShader(vs);

	//The same for FS
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &fsSrc, NULL);
	glCompileShader(fs);

	//Get shader program object
	GLuint shaderProg = glCreateProgram();
	//Attach both vs and fs
	glAttachShader(shaderProg, vs);
	glAttachShader(shaderProg, fs);
	//Link all
	glLinkProgram(shaderProg);

	//Clear the VS and FS objects
	glDeleteShader(vs);
	glDeleteShader(fs);
	return shaderProg;
}

void PassVertices(vector <GLfloat>* a)
{
	glm::vec2 v;

	for (GLuint i = 0; i < screenWidth; i++)
	{
		for (GLuint j = 0; j < screenHeight; j++)
		{
			v = glm::vec2(i, j);
			a->push_back(v[0]);
			a->push_back(v[1]);
		}
	}
}

vector<Particle> CreateParticles(GLuint numParticles) {

	vector<Particle> particles;

	for (int i = 0; i < numParticles; i++) {
		Particle p;

		p.position = glm::vec3(0, 0, 0);
		p.radius = 45;

		particles.push_back(p);
	}

	return particles;
}

vector<Light> CreateLights() {
	Light l;

	l.position = glm::vec3(50, 50, 0);
	l.diff = glm::vec3(2, 2, 2);
	l.spec = glm::vec3(10, 10, 10);

	lights.push_back(l);
	return lights;
}

/* Calculates the ray going through the pixel at the given location. */
Ray CalculateRay(GLuint pixelWidth, GLuint pixelHeight) {

	GLfloat fovy = glm::radians(45.0f);
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
std::tuple<GLfloat, Particle> FindFirstIntersection(const Ray& ray) {

	GLfloat firstIntersection = std::numeric_limits<GLfloat>::infinity();
	Particle firstParticle;

	for (const Particle& particle : particles) {

		GLfloat intersection = SphereIntersection(ray, particle);

		if ((intersection > 0.0f) && (intersection < firstIntersection)) {
			firstIntersection = intersection;
			firstParticle = particle;
		}
	}

	/* Indicates an invalid intersection TraceRay(). */
	if (firstIntersection == std::numeric_limits<GLfloat>::infinity()) {
		firstIntersection = -1.0f;
	}

	return std::tuple<GLfloat, Particle>(firstIntersection, firstParticle);
}

/* Calculates the normal vector for a sphere. */
glm::vec3 SphereNormal(const Ray& ray, const Particle& particle, GLfloat t) {

	glm::vec3 intersection = ray.origin + (t * ray.direction);
	glm::vec3 normal = glm::normalize(intersection - particle.position);

	return normal;
}

/* Returns a vector of all of the rays that are visible from the given point on the given surface. */
std::vector<Light> ShadowRays(const Ray& ray, GLfloat t, const Particle& particle) {

	std::vector<Light> visibleLights;
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

glm::vec3 TraceRay(const Ray& ray) {

	std::tuple<GLfloat, Particle> tuple = FindFirstIntersection(ray);
	GLfloat t = get<0>(tuple);
	Particle particle = get<1>(tuple);

	if (t < 0.0f) {
		return background;
	}

	std::vector<Light> contributedLights = ShadowRays(ray, t, particle);
	glm::vec3 color = glm::vec3(0.0f, 0.0f, 0.0f);

	for (Light& light : contributedLights) {
		color += Phong(ray, light, t, particle);
	}

	color = glm::vec3(1, 1, 1);
	return color;
}

vector<glm::vec3> RayCastOutput() {

	glm::vec3 color;

	colors.clear();

	for (GLuint i = 0; i < screenWidth; i++)
	{
		for (GLuint j = 0; j < screenHeight; j++)
		{
			Ray ray = CalculateRay(i, j);
			glm::vec3 color = TraceRay(ray);
			color = glm::clamp(color, 0.0f, 1.0f);

			/* Pushing back twice, once for x and and once for y coordinate. */
			colors.push_back(color);
			colors.push_back(color);
		}
	}

	return colors;
}

int main()
{
	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "CS434 Final Project", NULL, NULL);

	if (window == NULL)
	{
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);

	gladLoadGL();
	glViewport(0, 0, screenWidth, screenHeight);

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	vector <GLfloat> vertices;
	PassVertices(&vertices);
	GLuint points = vertices.size();
	glBufferData(GL_ARRAY_BUFFER, points * sizeof(GLfloat), &vertices[0], GL_STATIC_DRAW);

	glVertexAttribPointer((GLuint)0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	vertices.clear();

	int shaderProg = CompileShaders();
	GLint modelviewParameter = glGetUniformLocation(shaderProg, "modelview");

	glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
	glUseProgram(shaderProg);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	bool drawScene = true;

	particles = CreateParticles(1);
	lights = CreateLights();

	/* Do not modify these. */
	//glm::mat4 proj = glm::ortho(0.0f, (float)screenWidth, 0.0f, (float)screenHeight, 0.01f, 1000.0f);
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.01f, 1000.0f);
	glm::mat4 view = glm::lookAt(glm::vec3(0, 0, -screenWidth * 0.6), glm::vec3(3, 3, 0), glm::vec3(0, 1, 0));
	glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(0.0075));

	glm::mat4 modelView = proj * view * model;
	glUniformMatrix4fv(modelviewParameter, 1, GL_FALSE, glm::value_ptr(modelView));

	while (!glfwWindowShouldClose(window))
	{
		glClear(GL_COLOR_BUFFER_BIT);

		//ImGui_ImplOpenGL3_NewFrame();
		//ImGui_ImplGlfw_NewFrame();
		//ImGui::NewFrame();

		//ImGui::Begin("CS434 Final Project");

		//ImGui::End();

		if (drawScene == true) {

			glBindBuffer(GL_ARRAY_BUFFER, VBO);

			colors = RayCastOutput();

			glPointSize(1);

			for (int i = 0; i < points; i++) {

				GLfloat red = colors[i].r;
				GLfloat green = colors[i].g;
				GLfloat blue = colors[i].b;

				glUniform4f(glGetUniformLocation(shaderProg, "color"), red, green, blue, 1.0f);
				glDrawArrays(GL_POINTS, i, 1);
			}
			
			glBindBuffer(GL_ARRAY_BUFFER, 0);

		}

		//ImGui::Render();
		//ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteProgram(shaderProg);
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}