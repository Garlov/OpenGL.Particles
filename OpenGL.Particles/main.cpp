// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>
#include <vector>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>
GLFWwindow* window;

// Include GLM
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
using namespace glm;

#include "shader.hpp"

struct Particle {
	glm::vec3 pos, speed;
	GLfloat r, g, b, a; // Color
	float size, angle, weight;
	float life; // Remaining life of the particle. if <0 : dead and unused.
	float cameradistance; // *Squared* distance to the camera. if dead : -1.0f

	bool operator<(const Particle& that) const {
		// Sort in reverse order : far particles drawn first.
		return this->cameradistance > that.cameradistance;
	}
};

const int MAX_PARTICLES = 100000;
Particle ParticlesContainer[MAX_PARTICLES];
int LastUsedParticle = 0;

int findUnusedParticle() {

	for (int i = LastUsedParticle; i < MAX_PARTICLES; i++) {
		if (ParticlesContainer[i].life < 0) {
			LastUsedParticle = i;
			return i;
		}
	}

	for (int i = 0; i < LastUsedParticle; i++) {
		if (ParticlesContainer[i].life < 0) {
			LastUsedParticle = i;
			return i;
		}
	}

	return 0; // All particles are taken, override the first one
}

void SortParticles() {
	std::sort(&ParticlesContainer[0], &ParticlesContainer[MAX_PARTICLES]);
}

int main(void)
{
	// Initialise GLFW
	if (!glfwInit())
	{
		std::cout << stderr << "Failed to initialize GLFW" << std::endl;
		getchar();
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(1024, 768, "Particles", NULL, NULL);
	if (window == NULL) {
		std::cout << stderr << "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials." << std::endl;
		getchar();
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK) {
		std::cout << stderr << "Failed to initialize GLEW" << std::endl;
		getchar();
		glfwTerminate();
		return -1;
	}

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	GLuint programID = LoadShaders("shaders/TransformVertexShader.vertexshader", "shaders/ColorFragmentShader.fragmentshader");

	GLuint MatrixID = glGetUniformLocation(programID, "MVP");

	// Projection matrix : 45° Field of View, 4:3 ratio, display range : 0.1 unit <-> 400 units
	glm::mat4 Projection = glm::perspective(glm::radians(45.0f), 4.0f / 3.0f, 0.1f, 200.0f);

	// Camera matrix
	glm::mat4 View = glm::lookAt(
		glm::vec3(10, 10, -10), // Camera pos
		glm::vec3(0, 0, 0), // Camera lookat
		glm::vec3(0, 1, 0)  // Camera up dir
	);

	glm::mat4 Model = glm::mat4(1.0f);
	glm::mat4 MVP = Projection * View * Model;

	glm::vec3 CameraPosition(glm::inverse(View)[3]);

	static const GLfloat g_vertex_buffer_data[] = {
		-0.5f, -0.5f, 0.0f,
		0.5f, -0.5f, 0.0f,
		-0.5f,  0.5f, 0.0f,
		0.5f,  0.5f, 0.0f,
	};
	GLfloat* g_color_buffer_data = new GLfloat[MAX_PARTICLES * 3];
	GLfloat* g_position_buffer_data = new GLfloat[MAX_PARTICLES * 3];

	GLuint vertexBuffer;
	glGenBuffers(1, &vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

	GLuint colorBuffer;
	glGenBuffers(1, &colorBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
	glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);

	GLuint positionBuffer;
	glGenBuffers(1, &positionBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
	glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);

	int ParticlesCount = 0;

	const double FRAMES_PER_SECOND = 60; // this should be able to run the update loop at 60 fps easily
	const double MS_PER_FRAME = 1000 / FRAMES_PER_SECOND;
	const double MAX_FRAMESKIP = 5;

	double nextGameTick = glfwGetTime() * 1000;
	int loops;

	int currentDrawsPerSecond = 0;
	int currentUpdatesPerSecond = 0;
	double lastPrintTime = glfwGetTime();
	//float interpolation;

	while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS && glfwWindowShouldClose(window) == 0) {
		double newTime = glfwGetTime() * 1000;
		loops = 0;
		while (newTime > nextGameTick && loops < MAX_FRAMESKIP) {

			// update stuff
			int newParticles = 1;

			for (int i = 0; i < newParticles; i++) {
				int particleIndex = findUnusedParticle();
				ParticlesContainer[particleIndex].life = 10000.0f; // in seconds
				ParticlesContainer[particleIndex].pos = glm::vec3(0, 0, -20.0f);

				float spread = 1.5f;
				glm::vec3 maindir = glm::vec3(0.0f, 10.0f, 0.0f);
				// Very bad way to generate a random direction; 
				// See for instance http://stackoverflow.com/questions/5408276/python-uniform-spherical-distribution instead,
				// combined with some user-controlled parameters (main direction, spread, etc)
				glm::vec3 randomdir = glm::vec3(
					(rand() % 2000 - 1000.0f) / 1000.0f,
					(rand() % 2000 - 1000.0f) / 1000.0f,
					(rand() % 2000 - 1000.0f) / 1000.0f
				);

				ParticlesContainer[particleIndex].speed = maindir + randomdir * spread;

				// Very bad way to generate a random color
				ParticlesContainer[particleIndex].r = (GLfloat)rand() / RAND_MAX;
				ParticlesContainer[particleIndex].g = (GLfloat)rand() / RAND_MAX;
				ParticlesContainer[particleIndex].b = (GLfloat)rand() / RAND_MAX;
				ParticlesContainer[particleIndex].a = ((GLfloat)rand() / RAND_MAX) / 3;

				ParticlesContainer[particleIndex].size = (rand() % 1000) / 2000.0f + 0.1f;
			}

			// Simulate all particles
			int ParticlesCount = 0;
			for (int i = 0; i < MAX_PARTICLES; i++) {

				Particle& p = ParticlesContainer[i]; // shortcut

				if (p.life > 0.0f) {

					// Decrease life
					p.life -= MS_PER_FRAME;
					if (p.life > 0.0f) {

						// Simulate simple physics : gravity only, no collisions
						p.speed += glm::vec3(0.0f, -9.81f, 0.0f) * (float)MS_PER_FRAME * 0.5f;
						p.pos += p.speed * (float)MS_PER_FRAME;
						p.cameradistance = glm::length2(p.pos - CameraPosition);
						//ParticlesContainer[i].pos += glm::vec3(0.0f,10.0f, 0.0f) * (float)delta;

						// Fill the GPU buffer
						g_position_buffer_data[4 * ParticlesCount + 0] = p.pos.x;
						g_position_buffer_data[4 * ParticlesCount + 1] = p.pos.y;
						g_position_buffer_data[4 * ParticlesCount + 2] = p.pos.z;

						g_position_buffer_data[4 * ParticlesCount + 3] = p.size;

						g_color_buffer_data[4 * ParticlesCount + 0] = p.r;
						g_color_buffer_data[4 * ParticlesCount + 1] = p.g;
						g_color_buffer_data[4 * ParticlesCount + 2] = p.b;
						g_color_buffer_data[4 * ParticlesCount + 3] = p.a;

					}
					else {
						// Particles that just died will be put at the end of the buffer in SortParticles();
						p.cameradistance = -1.0f;
					}

					ParticlesCount++;

				}
			}

			SortParticles();

			currentUpdatesPerSecond += 1;

			nextGameTick += MS_PER_FRAME;
			loops++;
		}
		// No moving objects in this, so no need to use the interpolation
		//interpolation = float(newTime + SKIP_TICKS - next_game_tick) / float(SKIP_TICKS);

		// draw stuff

		currentDrawsPerSecond += 1;

		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
		glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, ParticlesCount * sizeof(GLfloat) * 4, g_position_buffer_data);

		glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
		glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * 4 * sizeof(GLubyte), NULL, GL_STREAM_DRAW); // Buffer orphaning, a common way to improve streaming perf. See above link for details.
		glBufferSubData(GL_ARRAY_BUFFER, 0, ParticlesCount * sizeof(GLubyte) * 4, g_color_buffer_data);

		glUseProgram(programID);

		// Send our transformation to the currently bound shader, 
		// in the "MVP" uniform
		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
		glVertexAttribPointer(
			0,                  // attribute. No particular reason for 0, but must match the layout in the shader.
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
		);

		// 2nd attribute buffer : colors
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
		glVertexAttribPointer(
			1,                              // attribute. No particular reason for 1, but must match the layout in the shader.
			4,                              // size
			GL_FLOAT,						// type
			GL_FALSE,                       // normalized?
			0,                              // stride
			(void*)0                        // array buffer offset
		);

		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
		glVertexAttribPointer(
			2,                                // attribute. No particular reason for 1, but must match the layout in the shader.
			4,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		glVertexAttribDivisor(0, 0); // reuse same vertice on all particles
		glVertexAttribDivisor(1, 1); // colors : one per instance
		glVertexAttribDivisor(2, 1); // positions : one per instance

									 // draw
		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, ParticlesCount);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

		// print fps info
		if (glfwGetTime() - lastPrintTime > 1) {
			lastPrintTime += 1;
			std::cout << currentDrawsPerSecond << " / " << currentUpdatesPerSecond << std::endl;
			currentDrawsPerSecond = 0;
			currentUpdatesPerSecond = 0;
		}
	}

	// Cleanup VBO and shader
	glDeleteBuffers(1, &vertexBuffer);
	glDeleteBuffers(1, &colorBuffer);
	glDeleteBuffers(1, &positionBuffer);
	glDeleteProgram(programID);
	glDeleteVertexArrays(1, &VertexArrayID);


	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

