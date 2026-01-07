#pragma once

#include "../world/Block.h"

class ModelGeometry {
public:
    static std::vector<float> cubeVertices;

    static void init() {
        cubeVertices = {
            -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
            0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
            0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
            0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
            -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
            -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,

            -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,

            -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
            -0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
            -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
            -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
            -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
            -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,

            0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
            0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
            0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
            0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
            0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
            0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,

            -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
            0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
            0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
            0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
            -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
            -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,

            -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
            0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
            -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
            -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
        };
    }
};

inline std::vector<float> ModelGeometry::cubeVertices;

class ModelRenderer {
public:
    GLuint modelVAO = 0, modelVBO = 0;
    GLuint inputSSBO = 0;
    GLuint outputSSBO = 0;
    GLuint indirectBuffer = 0;
    GLuint atomicBuffer = 0;
    GLuint computeProgram = 0;
    GLuint renderProgram = 0;

    std::vector<ModelInstance> allInstances;
    GLuint visibleCount = 0;

    void init(const char* computeSource, const char* vertSource, const char* fragSource) {
        ModelGeometry::init();

        GLuint cs = compileShader(GL_COMPUTE_SHADER, computeSource);
        computeProgram = glCreateProgram();
        glAttachShader(computeProgram, cs);
        glLinkProgram(computeProgram);
        glDeleteShader(cs);

        GLuint vs = compileShader(GL_VERTEX_SHADER, vertSource);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSource);
        renderProgram = glCreateProgram();
        glAttachShader(renderProgram, vs);
        glAttachShader(renderProgram, fs);
        glLinkProgram(renderProgram);
        glDeleteShader(vs);
        glDeleteShader(fs);

        glGenVertexArrays(1, &modelVAO);
        glGenBuffers(1, &modelVBO);

        glBindVertexArray(modelVAO);
        glBindBuffer(GL_ARRAY_BUFFER, modelVBO);
        glBufferData(GL_ARRAY_BUFFER, ModelGeometry::cubeVertices.size() * sizeof(float),
                     ModelGeometry::cubeVertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glGenBuffers(1, &inputSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, inputSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_MODEL_INSTANCES * sizeof(ModelInstance), nullptr, GL_DYNAMIC_DRAW);

        glGenBuffers(1, &outputSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, outputSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_MODEL_INSTANCES * sizeof(ModelInstance), nullptr, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, outputSSBO);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ModelInstance),
                              (void*)offsetof(ModelInstance, positionAndScale));
        glEnableVertexAttribArray(2);
        glVertexAttribDivisor(2, 1);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(ModelInstance),
                              (void*)offsetof(ModelInstance, colorAndType));
        glEnableVertexAttribArray(3);
        glVertexAttribDivisor(3, 1);

        glGenBuffers(1, &indirectBuffer);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
        DrawArraysIndirectCommand cmd = {36, 0, 0, 0};
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(cmd), &cmd, GL_DYNAMIC_DRAW);

        glGenBuffers(1, &atomicBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, atomicBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

        glBindVertexArray(0);
    }

    void uploadInstances() {
        if (allInstances.empty()) return;

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, inputSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                        allInstances.size() * sizeof(ModelInstance), allInstances.data());
    }

    void cullAndRender(const glm::vec4 frustumPlanes[6], const glm::mat4& view,
                       const glm::mat4& projection, const glm::vec3& viewPos) {
        if (allInstances.empty()) {
            visibleCount = 0;
            return;
        }

        GLuint totalInstances = static_cast<GLuint>(allInstances.size());

        GLuint zero = 0;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, atomicBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &zero);

        glUseProgram(computeProgram);
        glUniform4fv(glGetUniformLocation(computeProgram, "frustumPlanes"), 6,
                     glm::value_ptr(frustumPlanes[0]));
        glUniform1ui(glGetUniformLocation(computeProgram, "totalInstances"), totalInstances);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, inputSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, outputSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, indirectBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, atomicBuffer);

        GLuint numGroups = (totalInstances + 255) / 256;
        glDispatchCompute(numGroups, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, atomicBuffer);
        glBindBuffer(GL_COPY_WRITE_BUFFER, indirectBuffer);
        glCopyBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_COPY_WRITE_BUFFER,
                            0, offsetof(DrawArraysIndirectCommand, instanceCount), sizeof(GLuint));

        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &visibleCount);

        glUseProgram(renderProgram);
        glUniformMatrix4fv(glGetUniformLocation(renderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(renderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(renderProgram, "viewPos"), 1, glm::value_ptr(viewPos));

        glBindVertexArray(modelVAO);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
        glDrawArraysIndirect(GL_TRIANGLES, nullptr);
    }

    void cleanup() {
        glDeleteVertexArrays(1, &modelVAO);
        glDeleteBuffers(1, &modelVBO);
        glDeleteBuffers(1, &inputSSBO);
        glDeleteBuffers(1, &outputSSBO);
        glDeleteBuffers(1, &indirectBuffer);
        glDeleteBuffers(1, &atomicBuffer);
        glDeleteProgram(computeProgram);
        glDeleteProgram(renderProgram);
    }

private:
    GLuint compileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Shader error: " << infoLog << std::endl;
        }
        return shader;
    }
};
