#include "Rendering.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// number conversion warnings
#pragma warning(disable : 4244)

const char* textVertexShaderSource = R"(
    #version 330 core

    layout (location = 0) in vec4 vertex;
    out vec2 TexCoords;
    
    uniform mat4 projection;
    
    void main() {
        gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
        TexCoords = vertex.zw;
    }
)";

const char* textFragmentShaderSource = R"(
    #version 330 core

    in vec2 TexCoords;
    out vec4 color;
    
    uniform sampler2D text;
    uniform vec4 textColor;
    
    void main() {
        vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
        color = textColor * sampled;
    }
)";

const char* vertexShaderSource = R"(
    #version 330 core

    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec4 aColor;
    
    uniform mat4 projection;
    
    out vec4 fragColor;
    
    void main() {
        gl_Position = projection * vec4(aPos.x, aPos.y, 0.0, 1.0);
        fragColor = aColor;
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core

    in vec4 fragColor;
    out vec4 FragColor;
    
    void main() {
        FragColor = fragColor;
    }
)";

const char* imageVertexShaderSource = R"(
	#version 330 core

	layout (location = 0) in vec2 aPos;
	layout (location = 1) in vec2 aTexCoords;

	out vec2 TexCoords;

    uniform mat4 projection;
    uniform mat4 model;

	void main() {
		gl_Position = projection * model * vec4(aPos.x, aPos.y, 0.0, 1.0);
		TexCoords = aTexCoords;
	}
)";

const char* imageFragmentShaderSource = R"(
	#version 330 core

    in vec2 TexCoords;
    out vec4 color;

    uniform sampler2D image;
    uniform float alpha;
    uniform vec3 blendColor;

    void main() {
        vec4 texColor = texture(image, TexCoords);
        float threshold = 1;
        // Check if the pixel is close to white
        if (length(texColor.rgb - vec3(1.0, 1.0, 1.0)) < threshold) {
            color = vec4(blendColor, texColor.a * alpha);
        } else {
            color = vec4(texColor.rgb, texColor.a * alpha);
        }
    }
)";

Shader::Shader(const char* vertexPath, const char* fragmentPath) {
    GLuint vertex, fragment;

    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vertexPath, NULL);
    glCompileShader(vertex);
    checkCompileErrors(vertex, "VERTEX");

    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fragmentPath, NULL);
    glCompileShader(fragment);
    checkCompileErrors(fragment, "FRAGMENT");

    this->Program = glCreateProgram();
    glAttachShader(this->Program, vertex);
    glAttachShader(this->Program, fragment);
    glLinkProgram(this->Program);
    checkCompileErrors(this->Program, "PROGRAM");

    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

void Shader::Use() {
    glUseProgram(this->Program);
}

void Shader::checkCompileErrors(GLuint shader, std::string type) {
    GLint success;
    GLchar infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, sizeof(infoLog), NULL, infoLog);
            std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << std::endl;
        }
    }
    else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, sizeof(infoLog), NULL, infoLog);
            std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << std::endl;
        }
    }
}

Renderer::Renderer(int width, int height) : width(width), height(height) {
    shader = new Shader(vertexShaderSource, fragmentShaderSource);
    textShader = new Shader(textVertexShaderSource, textFragmentShaderSource);
	imageShader = new Shader(imageVertexShaderSource, imageFragmentShaderSource);

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    projectionMatrix = glm::ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -1.0f, 1.0f);

    initFreeType();
	initTextRendering();
	initRenderData();
}

void Renderer::setProjectionMatrix(const glm::mat4& matrix) {
    projectionMatrix = matrix;
}

void Renderer::drawRectangle(float x, float y, float width, float height, const float color[4]) {
    vertices.clear();

    Vertex v1 = { x, y, color[0], color[1], color[2], color[3] };
    Vertex v2 = { x + width, y, color[0], color[1], color[2], color[3] };
    Vertex v3 = { x + width, y + height, color[0], color[1], color[2], color[3] };
    Vertex v4 = { x, y + height, color[0], color[1], color[2], color[3] };

    vertices = { v1, v2, v3, v4 };

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    shader->Use();
    glBindVertexArray(VAO);

    GLint projLoc = glGetUniformLocation(shader->Program, "projection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projectionMatrix));

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glBindVertexArray(0);
}

void Renderer::drawCircle(float cx, float cy, float r, float* color) {
    vertices.clear();

    int segments = 360;
    float angleStep = 2.0f * M_PI / segments;

    vertices.push_back({ cx, cy, color[0], color[1], color[2], 1.0f });
    for (int i = 0; i <= segments; ++i) {
        float angle = M_PI / 2.0f + i * angleStep;
        vertices.push_back({ cx + cos(angle) * r, cy + sin(angle) * r, color[0], color[1], color[2], 1.0f});
    }

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    shader->Use();
    glBindVertexArray(VAO);

    GLint projLoc = glGetUniformLocation(shader->Program, "projection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projectionMatrix));

    glDrawArrays(GL_TRIANGLE_FAN, 0, segments + 2);

    glBindVertexArray(0);
}

void Renderer::drawParkingSpotTimer(float cx, float cy, float r, float redProgress) {
    vertices.clear();

    int segments = 1000;
    float angleStep = 2.0f * M_PI / segments;

	// Draw the green part of the timer
    vertices.push_back({ cx, cy, 0.0f, 1.0f, 0.0f, 1.0f });
    for (int i = 0; i <= segments * (1.0f - redProgress); ++i) {
        float angle = M_PI / 2.0f + i * angleStep;
        vertices.push_back({ cx + cos(angle) * r, cy + sin(angle) * r, 0.0f, 1.0f, 0.0f, 1.0f });
    }

	// Draw the red part of the timer
    vertices.push_back({ cx, cy, 1.0f, 0.0f, 0.0f, 1.0f });
    for (int i = segments * (1.0f - redProgress); i <= segments; ++i) {
        float angle = M_PI / 2.0f + i * angleStep;
        vertices.push_back({ cx + cos(angle) * r, cy + sin(angle) * r, 1.0f, 0.0f, 0.0f, 1.0f });
    }

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    shader->Use();
    glBindVertexArray(VAO);

    GLint projLoc = glGetUniformLocation(shader->Program, "projection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projectionMatrix));

    glDrawArrays(GL_TRIANGLE_FAN, 0, segments * (1.0f - redProgress) + 2);
    glDrawArrays(GL_TRIANGLE_FAN, segments * (1.0f - redProgress) + 2, segments - segments * (1.0f - redProgress) + 2);

    glBindVertexArray(0);
}

void Renderer::drawText(const std::string& text, float x, float y, float scale, glm::vec4 color) {    
    textShader->Use();
    glUniform4f(glGetUniformLocation(textShader->Program, "textColor"), color.x, color.y, color.z, color.w);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(textVAO);

    GLint projLoc = glGetUniformLocation(textShader->Program, "projection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projectionMatrix));

    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++) {
        Character ch = Characters[*c];

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;
        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }
        };
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        x += (ch.Advance >> 6) * scale;
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

float Renderer::measureTextWidth(const std::string& text, float scale) {
    float width = 0.0f;
    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++) {
        Character ch = Characters[*c];
        width += (ch.Advance >> 6) * scale;
    }
    return width;
}

void Renderer::initTextRendering() {
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Renderer::initFreeType() {
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return;
    }

    FT_Face face;
    if (FT_New_Face(ft, "Gill_Sans.otf", 0, &face)) {
        std::cerr << "ERROR::FREETYPE: Failed to load font" << std::endl;
        return;
    }

    FT_Set_Pixel_Sizes(face, 0, 48);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (GLubyte c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cerr << "ERROR::FREETYPE: Failed to load Glyph" << std::endl;
            continue;
        }
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            face->glyph->advance.x
        };
        Characters.insert(std::pair<GLchar, Character>(c, character));
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}

void Renderer::renderImage(GLuint textureID, float x, float y, float width, float height, float rotation = 0.0f, float alpha = 1.0f, glm::vec3 blendColor = {1.0f, 1.0f, 1.0f}) {
    imageShader->Use();

    // Set the projection matrix
    GLint projLoc = glGetUniformLocation(imageShader->Program, "projection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projectionMatrix));

    // Set the alpha value
    GLint alphaLoc = glGetUniformLocation(imageShader->Program, "alpha");
    glUniform1f(alphaLoc, alpha);

    // Set the blend color
    GLint blendColorLoc = glGetUniformLocation(imageShader->Program, "blendColor");
    glUniform3fv(blendColorLoc, 1, glm::value_ptr(blendColor));

    // Create transformation matrix
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(x + width / 2, y + height / 2, 0.0f)); // Move to the center of the quad
    model = glm::rotate(model, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f)); // Rotate around the center
    model = glm::translate(model, glm::vec3(-width / 2, -height / 2, 0.0f)); // Move back to the original position
    model = glm::scale(model, glm::vec3(width, height, 1.0f)); // Scale to the desired size

    GLint modelLoc = glGetUniformLocation(imageShader->Program, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    // Bind the texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set up the vertices and texture coordinates
    float vertices[] = {
        // Positions          // Texture Coords
        0.0f, 0.0f,           0.0f, 0.0f,
        1.0f, 0.0f,           1.0f, 0.0f,
        1.0f, 1.0f,           1.0f, 1.0f,
        0.0f, 1.0f,           0.0f, 1.0f
    };

    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    // Bind and set vertex buffer(s) and attribute pointer(s)
    glBindVertexArray(imageVAO);
    glBindBuffer(GL_ARRAY_BUFFER, imageVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, imageEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Set the vertex attribute pointers
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Draw the textured quad
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}

void Renderer::initRenderData() {
    // Generate and bind the VAO, VBO, and EBO
    glGenVertexArrays(1, &imageVAO);
    glGenBuffers(1, &imageVBO);
    glGenBuffers(1, &imageEBO);

    glBindVertexArray(imageVAO);

    glBindBuffer(GL_ARRAY_BUFFER, imageVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, imageEBO);

    // Set the vertex attribute pointers
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}