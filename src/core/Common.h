#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include <array>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <queue>
#include <memory>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

// Constants
constexpr int CHUNK_SIZE = 32;
constexpr int MICRO_BLOCK_SIZE = 16;
constexpr int MICRO_PALETTE_SIZE = 32;
constexpr size_t MAX_MODEL_INSTANCES = 1000000;
constexpr int MAX_ATLAS_SIZE = 4096;

// Screen dimensions
constexpr unsigned int SCR_WIDTH = 1280;
constexpr unsigned int SCR_HEIGHT = 720;
