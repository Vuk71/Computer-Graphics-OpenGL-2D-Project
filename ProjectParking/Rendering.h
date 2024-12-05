#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

#ifndef RENDERING_H
#define RENDERING_H

class Shader {
public:
    GLuint Program;

    Shader(const char* vertexPath, const char* fragmentPath);
    void Use();

private:
    void checkCompileErrors(GLuint shader, std::string type);
};

class Renderer {
private:
    GLuint VBO, VAO;
	GLuint textVAO, textVBO;
    Shader* shader;
	Shader* textShader;
	Shader* imageShader;
    glm::mat4 projectionMatrix;
    int width, height;
    GLuint imageVAO, imageVBO, imageEBO;

    struct Vertex {
        float x, y;
        float r, g, b, a;
    };

    std::vector<Vertex> vertices;

    struct Character {
        GLuint TextureID;
        glm::ivec2 Size;
        glm::ivec2 Bearing;
        GLuint Advance;
    };

    std::map<GLchar, Character> Characters;

    void initFreeType();
	void initTextRendering();
    void initRenderData();

public:
    Renderer(int width, int height);
    void setProjectionMatrix(const glm::mat4& matrix);
    void drawRectangle(float x, float y, float width, float height, const float color[4]);
    void drawCircle(float cx, float cy, float r, float* color);
    void drawParkingSpotTimer(float cx, float cy, float r, float redProgress);
    void drawText(const std::string& text, float x, float y, float scale, glm::vec4 color);
    float measureTextWidth(const std::string& text, float scale);
	void renderImage(GLuint texture, float x, float y, float width, float height, float rotation, float alpha, glm::vec3 blendColor);
};

#endif
