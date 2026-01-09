#pragma once

#include "../core/Common.h"

class BlockHighlight {
public:
    GLuint VAO = 0, VBO = 0;
    GLuint shaderProgram = 0;

    void init() {
        // Wireframe cube vertices (12 edges = 24 vertices for GL_LINES)
        float vertices[] = {
            // Bottom face edges
            0, 0, 0,  1, 0, 0,
            1, 0, 0,  1, 0, 1,
            1, 0, 1,  0, 0, 1,
            0, 0, 1,  0, 0, 0,
            // Top face edges
            0, 1, 0,  1, 1, 0,
            1, 1, 0,  1, 1, 1,
            1, 1, 1,  0, 1, 1,
            0, 1, 1,  0, 1, 0,
            // Vertical edges
            0, 0, 0,  0, 1, 0,
            1, 0, 0,  1, 1, 0,
            1, 0, 1,  1, 1, 1,
            0, 0, 1,  0, 1, 1,
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);

        // Simple shader for wireframe
        const char* vertSrc = R"(
            #version 460 core
            layout (location = 0) in vec3 aPos;
            uniform mat4 mvp;
            uniform vec3 blockPos;
            void main() {
                gl_Position = mvp * vec4(aPos + blockPos, 1.0);
            }
        )";

        const char* fragSrc = R"(
            #version 460 core
            out vec4 FragColor;
            uniform vec4 color;
            void main() {
                FragColor = color;
            }
        )";

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vertSrc, nullptr);
        glCompileShader(vs);

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fragSrc, nullptr);
        glCompileShader(fs);

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vs);
        glAttachShader(shaderProgram, fs);
        glLinkProgram(shaderProgram);

        glDeleteShader(vs);
        glDeleteShader(fs);
    }

    void render(const glm::mat4& vp, const glm::ivec3& blockPos, const glm::vec4& color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) {
        glUseProgram(shaderProgram);

        // Slight offset to prevent z-fighting
        glm::vec3 pos = glm::vec3(blockPos) - 0.001f;

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "mvp"), 1, GL_FALSE, glm::value_ptr(vp));
        glUniform3fv(glGetUniformLocation(shaderProgram, "blockPos"), 1, glm::value_ptr(pos));
        glUniform4fv(glGetUniformLocation(shaderProgram, "color"), 1, glm::value_ptr(color));

        glBindVertexArray(VAO);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, 24);
        glBindVertexArray(0);
    }

    void cleanup() {
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (shaderProgram) glDeleteProgram(shaderProgram);
    }
};
