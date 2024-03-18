#include <iostream>
#include <vector>
#include <stdint.h>

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
vector<glm::vec4> colors;

const int screenWidth = 800;
const int screenHeight = 800;

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

vector<glm::vec4> RayCastOutput() {

	glm::vec4 color;

	colors.clear();

	for (GLuint i = 0; i < screenWidth; i++)
	{
		for (GLuint j = 0; j < screenHeight; j++)
		{
			//color = ray casting functions combined, remove line below
			color = glm::vec4(0, 1, 1, 1);
			
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
	bool filled = false;

	while (!glfwWindowShouldClose(window))
	{
		glClear(GL_COLOR_BUFFER_BIT);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("CS434 Final Project");

		ImGui::End();

		glm::mat4 proj = glm::ortho(0.0f, (float)screenWidth, 0.0f, (float)screenHeight, 0.01f, 1000.0f);
		glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
		glm::mat4 modelView = proj * view;
		glUniformMatrix4fv(modelviewParameter, 1, GL_FALSE, glm::value_ptr(modelView));

		if (drawScene == true) {

			glBindBuffer(GL_ARRAY_BUFFER, VBO);

			colors = RayCastOutput();

			glPointSize(1);

			for (int i = 0; i < points; i++) {

				GLfloat red = colors[i].r;
				GLfloat green = colors[i].g;
				GLfloat blue = colors[i].b;
				GLfloat alpha = colors[i].a;

				glUniform4f(glGetUniformLocation(shaderProg, "color"), red, green, blue, alpha);
				glDrawArrays(GL_POINTS, i, 1);
			}
			
			glBindBuffer(GL_ARRAY_BUFFER, 0);

		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

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