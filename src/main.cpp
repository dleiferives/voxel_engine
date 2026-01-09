// Core headers
#include "core/Common.h"
#include "core/Camera.h"

// Entity headers
#include "entity/Player.h"

// World headers
#include "world/Block.h"
#include "world/Chunk.h"
#include "world/Lighting.h"
#include "world/TerrainGen.h"

// Render headers
#include "render/TextureAtlas.h"
#include "render/ChunkMesher.h"
#include "render/ModelRenderer.h"
#include "render/BlockHighlight.h"

// Physics headers
#include "physics/Raycast.h"

// ImGui
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

// Embedded shaders
const char chunkVertSource[] = {
#embed "shaders/chunk.vert"
    , 0
};
const char chunkFragSource[] = {
#embed "shaders/chunk.frag"
    , 0
};
const char modelVertSource[] = {
#embed "shaders/model.vert"
    , 0
};
const char modelFragSource[] = {
#embed "shaders/model.frag"
    , 0
};
const char cullingCompSource[] = {
#embed "shaders/culling.comp"
    , 0
};

// Global definitions (declared as extern in headers)
MicroBlockPalette g_microPalette;
BlockTextureInfo g_blockTextures[BLOCK_COUNT];
TextureAtlas g_textureAtlas;

// Global player and UI state
Player* g_player = nullptr;
bool g_uiMode = false;
float g_deltaTime = 0.0f;

void initBlockTextures() {
    g_textureAtlas.init(16);

    // Add textures to atlas
    uint16_t stoneTop = g_textureAtlas.addTexture("textures/stone.png");
    uint16_t stoneSide = stoneTop;
    uint16_t dirtTex = g_textureAtlas.addTexture("textures/dirt.png");
    uint16_t grassTop = g_textureAtlas.addTexture("textures/grass_top.png");
    uint16_t grassSide = g_textureAtlas.addTexture("textures/grass_side.png");
    uint16_t sandTex = g_textureAtlas.addTexture("textures/sand.png");
    uint16_t woodTop = g_textureAtlas.addTexture("textures/wood_top.png");
    uint16_t woodSide = g_textureAtlas.addTexture("textures/wood_side.png");
    uint16_t leavesTex = g_textureAtlas.addTexture("textures/leaves.png");
    uint16_t glassTex = g_textureAtlas.addTexture("textures/glass.png");
    uint16_t brickTex = g_textureAtlas.addTexture("textures/brick.png");
    uint16_t logTop = g_textureAtlas.addTexture("textures/log_top.png");
    uint16_t logSide = g_textureAtlas.addTexture("textures/log_side.png");

    // Fallback colors if textures don't exist
    uint16_t stoneColor = g_textureAtlas.addColor(glm::vec3(0.5f));
    uint16_t dirtColor = g_textureAtlas.addColor(glm::vec3(0.45f, 0.3f, 0.15f));
    uint16_t grassColor = g_textureAtlas.addColor(glm::vec3(0.2f, 0.6f, 0.2f));
    uint16_t sandColor = g_textureAtlas.addColor(glm::vec3(0.9f, 0.85f, 0.6f));
    uint16_t woodColor = g_textureAtlas.addColor(glm::vec3(0.55f, 0.35f, 0.15f));
    uint16_t leavesColor = g_textureAtlas.addColor(glm::vec3(0.1f, 0.5f, 0.1f));
    uint16_t glassColor = g_textureAtlas.addColor(glm::vec3(0.8f, 0.9f, 1.0f));
    uint16_t brickColor = g_textureAtlas.addColor(glm::vec3(0.7f, 0.3f, 0.2f));
    uint16_t logColor = g_textureAtlas.addColor(glm::vec3(0.4f, 0.25f, 0.1f));

    // Stone - same texture all sides
    g_blockTextures[BLOCK_STONE] = {
        {{stoneColor, 0}, {stoneColor, 0}, {stoneColor, 0},
         {stoneColor, 0}, {stoneColor, 0}, {stoneColor, 0}},
        false, false
    };

    // Dirt - same texture all sides
    g_blockTextures[BLOCK_DIRT] = {
        {{dirtColor, 0}, {dirtColor, 0}, {dirtColor, 0},
         {dirtColor, 0}, {dirtColor, 0}, {dirtColor, 0}},
        false, false
    };

    // Grass - different top/side/bottom
    g_blockTextures[BLOCK_GRASS] = {
        {{grassColor, 0}, {grassColor, 0}, {dirtColor, 0},
         {grassColor, 0}, {grassColor, 0}, {grassColor, 0}},
        false, false
    };

    // Sand
    g_blockTextures[BLOCK_SAND] = {
        {{sandColor, 0}, {sandColor, 0}, {sandColor, 0},
         {sandColor, 0}, {sandColor, 0}, {sandColor, 0}},
        false, false
    };

    // Wood - different top/bottom vs sides
    g_blockTextures[BLOCK_WOOD] = {
        {{woodColor, 0}, {woodColor, 0}, {woodColor, 0},
         {woodColor, 0}, {woodColor, 0}, {woodColor, 0}},
        false, false
    };

    // Log - different top/bottom vs sides
    g_blockTextures[BLOCK_LOG] = {
        {{logColor, 0}, {logColor, 0}, {logColor, 0},
         {logColor, 0}, {logColor, 0}, {logColor, 0}},
        false, false
    };

    // Leaves - transparent
    g_blockTextures[BLOCK_LEAVES] = {
        {{leavesColor, 0}, {leavesColor, 0}, {leavesColor, 0},
         {leavesColor, 0}, {leavesColor, 0}, {leavesColor, 0}},
        true, false
    };

    // Glass - transparent
    g_blockTextures[BLOCK_GLASS] = {
        {{glassTex, 0}, {glassTex, 0}, {glassTex, 0},
         {glassTex, 0}, {glassTex, 0}, {glassTex, 0}},
        true, false
    };

    // Brick
    g_blockTextures[BLOCK_BRICK] = {
        {{brickColor, 0}, {brickColor, 0}, {brickColor, 0},
         {brickColor, 0}, {brickColor, 0}, {brickColor, 0}},
        false, false
    };

    // Glowstone - bright yellow/orange
    uint16_t glowstoneColor = g_textureAtlas.addColor(glm::vec3(1.0f, 0.9f, 0.5f));
    g_blockTextures[BLOCK_GLOWSTONE] = {
        {{glowstoneColor, 0}, {glowstoneColor, 0}, {glowstoneColor, 0},
         {glowstoneColor, 0}, {glowstoneColor, 0}, {glowstoneColor, 0}},
        false, true  // fullBright = true (ignores lighting)
    };

    // Lava - orange/red
    uint16_t lavaColor = g_textureAtlas.addColor(glm::vec3(1.0f, 0.4f, 0.1f));
    g_blockTextures[BLOCK_LAVA] = {
        {{lavaColor, 0}, {lavaColor, 0}, {lavaColor, 0},
         {lavaColor, 0}, {lavaColor, 0}, {lavaColor, 0}},
        false, true  // fullBright = true
    };

    g_textureAtlas.build();
}

// Block info array
// Format: {category, color, scale, modelId, textureInfo, lightEmission, blocksLight}
BlockInfo BLOCK_INFO[] = {
    // BLOCK_AIR - doesn't block light
    {CATEGORY_AIR, glm::vec3(0.0f), 0.0f, 0, nullptr, 0, false},
    // Solid blocks - all block light
    {CATEGORY_SOLID, glm::vec3(0.5f), 1.0f, 0, &g_blockTextures[BLOCK_STONE], 0, true},           // STONE
    {CATEGORY_SOLID, glm::vec3(0.45f, 0.3f, 0.15f), 1.0f, 0, &g_blockTextures[BLOCK_DIRT], 0, true},  // DIRT
    {CATEGORY_SOLID, glm::vec3(0.2f, 0.6f, 0.2f), 1.0f, 0, &g_blockTextures[BLOCK_GRASS], 0, true},   // GRASS
    {CATEGORY_SOLID, glm::vec3(0.9f, 0.85f, 0.6f), 1.0f, 0, &g_blockTextures[BLOCK_SAND], 0, true},   // SAND
    {CATEGORY_SOLID, glm::vec3(0.55f, 0.35f, 0.15f), 1.0f, 0, &g_blockTextures[BLOCK_WOOD], 0, true}, // WOOD
    {CATEGORY_SOLID, glm::vec3(0.1f, 0.5f, 0.1f), 1.0f, 0, &g_blockTextures[BLOCK_LEAVES], 0, false}, // LEAVES - doesn't fully block light
    {CATEGORY_SOLID, glm::vec3(0.8f, 0.9f, 1.0f), 1.0f, 0, &g_blockTextures[BLOCK_GLASS], 0, false},  // GLASS - doesn't block light
    {CATEGORY_SOLID, glm::vec3(0.7f, 0.3f, 0.2f), 1.0f, 0, &g_blockTextures[BLOCK_BRICK], 0, true},   // BRICK
    // Light-emitting solid blocks
    {CATEGORY_SOLID, glm::vec3(1.0f, 0.9f, 0.5f), 1.0f, 0, &g_blockTextures[BLOCK_GLOWSTONE], 15, true}, // GLOWSTONE - max light
    {CATEGORY_SOLID, glm::vec3(1.0f, 0.4f, 0.1f), 1.0f, 0, &g_blockTextures[BLOCK_LAVA], 15, true},      // LAVA - max light
    // Model blocks - don't block light
    {CATEGORY_MODEL, glm::vec3(0.9f, 0.2f, 0.2f), 0.4f, 0, nullptr, 0, false},  // FLOWER_RED
    {CATEGORY_MODEL, glm::vec3(0.9f, 0.9f, 0.2f), 0.4f, 0, nullptr, 0, false},  // FLOWER_YELLOW
    {CATEGORY_MODEL, glm::vec3(0.3f, 0.7f, 0.3f), 0.6f, 1, nullptr, 0, false},  // TALL_GRASS
    {CATEGORY_MODEL, glm::vec3(0.8f, 0.7f, 0.6f), 0.3f, 2, nullptr, 0, false},  // MUSHROOM
    {CATEGORY_MODEL, glm::vec3(0.6f, 0.8f, 1.0f), 0.5f, 3, nullptr, 7, false},  // CRYSTAL - emits some light
    {CATEGORY_MODEL, glm::vec3(1.0f, 0.8f, 0.3f), 0.3f, 4, nullptr, 14, false}, // TORCH - bright light
    // LOG block
    {CATEGORY_SOLID, glm::vec3(0.4f, 0.25f, 0.1f), 1.0f, 0, &g_blockTextures[BLOCK_LOG], 0, true}, // LOG
    // Micro blocks - block light
    {CATEGORY_MICRO, glm::vec3(0.7f, 0.5f, 0.3f), 1.0f, 0, nullptr, 0, true},   // MICRO_TERRAIN
    {CATEGORY_MICRO, glm::vec3(0.8f, 0.8f, 0.8f), 1.0f, 1, nullptr, 0, true},   // MICRO_SCULPTURE
};

class ChunkManager {
public:
    std::unordered_map<int64_t, std::unique_ptr<Chunk>> chunks;
    std::vector<ChunkVertex> opaqueBuffer;
    std::vector<ChunkVertex> transparentBuffer;

    // Streaming state
    std::vector<glm::ivec3> chunksToGenerate;
    std::vector<glm::ivec3> chunksToMesh;
    glm::ivec3 lastPlayerChunk = glm::ivec3(INT_MAX);

    int64_t hashPos(glm::ivec3 pos) const {
        return (int64_t(pos.x) & 0xFFFFF) |
               ((int64_t(pos.y) & 0xFFFFF) << 20) |
               ((int64_t(pos.z) & 0xFFFFF) << 40);
    }

    Chunk* getChunk(glm::ivec3 pos) {
        auto it = chunks.find(hashPos(pos));
        return it != chunks.end() ? it->second.get() : nullptr;
    }

    Chunk* createChunk(glm::ivec3 pos) {
        auto chunk = std::make_unique<Chunk>(pos);
        Chunk* ptr = chunk.get();
        chunks[hashPos(pos)] = std::move(chunk);
        return ptr;
    }

    void generateChunkTerrain(glm::ivec3 chunkPos) {
        Chunk* chunk = createChunk(chunkPos);
        g_terrainGen.generateChunk(*chunk, chunkPos.x, chunkPos.y, chunkPos.z);
        chunk->needsRebuild = true;
        chunk->needsLightRebuild = true;
        chunksToMesh.push_back(chunkPos);
    }

    // Update chunks around player
    void updateAroundPlayer(const glm::ivec3& playerChunk, int renderDistance) {
        // Check if player moved to a new chunk
        if (playerChunk == lastPlayerChunk) return;
        lastPlayerChunk = playerChunk;

        // Find chunks that need to be loaded
        chunksToGenerate.clear();

        for (int dy = -renderDistance; dy <= renderDistance; dy++) {
            for (int dz = -renderDistance; dz <= renderDistance; dz++) {
                for (int dx = -renderDistance; dx <= renderDistance; dx++) {
                    // Spherical-ish distance check
                    float dist = sqrtf(dx*dx + dy*dy + dz*dz);
                    if (dist > renderDistance) continue;

                    glm::ivec3 chunkPos = playerChunk + glm::ivec3(dx, dy, dz);
                    if (!getChunk(chunkPos)) {
                        chunksToGenerate.push_back(chunkPos);
                    }
                }
            }
        }

        // Sort by distance to player (closest first)
        std::sort(chunksToGenerate.begin(), chunksToGenerate.end(),
            [&playerChunk](const glm::ivec3& a, const glm::ivec3& b) {
                float distA = glm::length(glm::vec3(a - playerChunk));
                float distB = glm::length(glm::vec3(b - playerChunk));
                return distA < distB;
            });
    }

    // Unload distant chunks
    void unloadDistantChunks(const glm::ivec3& playerChunk, int maxDistance) {
        std::vector<int64_t> toRemove;

        for (auto& [hash, chunk] : chunks) {
            glm::ivec3 diff = chunk->chunkPos - playerChunk;
            float dist = glm::length(glm::vec3(diff));
            if (dist > maxDistance + 2) {
                // Cleanup OpenGL resources
                if (chunk->VAO) {
                    glDeleteVertexArrays(1, &chunk->VAO);
                    glDeleteBuffers(1, &chunk->VBO);
                }
                if (chunk->transparentVAO) {
                    glDeleteVertexArrays(1, &chunk->transparentVAO);
                    glDeleteBuffers(1, &chunk->transparentVBO);
                }
                toRemove.push_back(hash);
            }
        }

        for (int64_t hash : toRemove) {
            chunks.erase(hash);
        }
    }

    // Process chunk generation queue
    void processGenerationQueue(int maxPerFrame = 1) {
        int processed = 0;
        while (!chunksToGenerate.empty() && processed < maxPerFrame) {
            glm::ivec3 pos = chunksToGenerate.back();
            chunksToGenerate.pop_back();

            if (!getChunk(pos)) {
                generateChunkTerrain(pos);
                processed++;
            }
        }
    }

    // Process mesh building queue
    void processMeshQueue(int maxPerFrame = 2) {
        int processed = 0;
        while (!chunksToMesh.empty() && processed < maxPerFrame) {
            glm::ivec3 pos = chunksToMesh.back();
            chunksToMesh.pop_back();

            Chunk* chunk = getChunk(pos);
            if (chunk && chunk->needsRebuild) {
                rebuildChunkMesh(chunk);
                processed++;
            }
        }
    }

    void rebuildChunkMesh(Chunk* chunk) {
        if (!chunk->needsRebuild) return;

        // Calculate lighting before meshing (light values are baked into vertices)
        if (chunk->needsLightRebuild) {
            calculateChunkLighting(*chunk);
        }

        GreedyMesher::mesh(*chunk, opaqueBuffer, transparentBuffer);

        chunk->vertexCount = opaqueBuffer.size();
        chunk->transparentVertexCount = transparentBuffer.size();

        if (chunk->vertexCount == 0 && chunk->transparentVertexCount == 0) {
            chunk->isEmpty = true;
            chunk->needsRebuild = false;
            return;
        }

        chunk->isEmpty = false;

        // Upload opaque mesh
        if (chunk->vertexCount > 0) {
            if (!chunk->VAO) {
                glGenVertexArrays(1, &chunk->VAO);
                glGenBuffers(1, &chunk->VBO);
            }

            glBindVertexArray(chunk->VAO);
            glBindBuffer(GL_ARRAY_BUFFER, chunk->VBO);
            glBufferData(GL_ARRAY_BUFFER,
                         opaqueBuffer.size() * sizeof(ChunkVertex),
                         opaqueBuffer.data(), GL_STATIC_DRAW);

            setupChunkVertexAttribs();
            glBindVertexArray(0);
        }

        // Upload transparent mesh
        if (chunk->transparentVertexCount > 0) {
            if (!chunk->transparentVAO) {
                glGenVertexArrays(1, &chunk->transparentVAO);
                glGenBuffers(1, &chunk->transparentVBO);
            }

            glBindVertexArray(chunk->transparentVAO);
            glBindBuffer(GL_ARRAY_BUFFER, chunk->transparentVBO);
            glBufferData(GL_ARRAY_BUFFER,
                         transparentBuffer.size() * sizeof(ChunkVertex),
                         transparentBuffer.data(), GL_STATIC_DRAW);

            setupChunkVertexAttribs();
            glBindVertexArray(0);
        }

        chunk->needsRebuild = false;
        chunk->meshUploaded = true;
    }

    void setupChunkVertexAttribs() {
        // Position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, position));
        glEnableVertexAttribArray(0);

        // Normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, normal));
        glEnableVertexAttribArray(1);

        // Color
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, color));
        glEnableVertexAttribArray(2);

        // UV
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, uv));
        glEnableVertexAttribArray(3);

        // Texture layer
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, textureLayer));
        glEnableVertexAttribArray(4);

        // AO
        glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(ChunkVertex),
                              (void*)offsetof(ChunkVertex, ao));
        glEnableVertexAttribArray(5);
    }

    void collectAllModelBlocks(ModelRenderer& renderer) {
        renderer.allInstances.clear();

        for (auto& [hash, chunk] : chunks) {
            chunk->collectModelBlocks();
            renderer.allInstances.insert(
                renderer.allInstances.end(),
                chunk->modelBlocks.begin(),
                chunk->modelBlocks.end()
            );
        }

        renderer.uploadInstances();
    }
};

// Block interaction globals
ChunkManager* g_chunkManager = nullptr;
int g_selectedBlockType = BLOCK_STONE;
bool g_leftMousePressed = false;
bool g_rightMousePressed = false;
bool g_needsModelUpdate = false;

// Available blocks for hotbar
const uint8_t HOTBAR_BLOCKS[] = {
    BLOCK_STONE, BLOCK_DIRT, BLOCK_GRASS, BLOCK_SAND,
    BLOCK_WOOD, BLOCK_LOG, BLOCK_LEAVES, BLOCK_GLASS, BLOCK_BRICK,
    BLOCK_GLOWSTONE, BLOCK_LAVA
};
const int HOTBAR_SIZE = sizeof(HOTBAR_BLOCKS) / sizeof(HOTBAR_BLOCKS[0]);
int g_hotbarIndex = 0;

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (g_player) {
        g_player->processMouse((float)xpos, (float)ypos, g_uiMode);
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (g_uiMode) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        g_leftMousePressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        g_rightMousePressed = true;
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (g_uiMode) return;

    g_hotbarIndex -= (int)yoffset;
    if (g_hotbarIndex < 0) g_hotbarIndex = HOTBAR_SIZE - 1;
    if (g_hotbarIndex >= HOTBAR_SIZE) g_hotbarIndex = 0;
    g_selectedBlockType = HOTBAR_BLOCKS[g_hotbarIndex];
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    static bool tab_pressed = false;
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
        if (!tab_pressed) {
            g_uiMode = !g_uiMode;
            glfwSetInputMode(window, GLFW_CURSOR,
                             g_uiMode ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            if (!g_uiMode && g_player) {
                g_player->resetMouse();
            }
            tab_pressed = true;
        }
    } else {
        tab_pressed = false;
    }

    if (g_player) {
        g_player->processKeyboard(window, g_deltaTime, g_uiMode);
    }
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader error:\n" << infoLog << std::endl;
    }
    return shader;
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "Voxel World - Infinite Streaming", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Initialize texture atlas and block textures
    initBlockTextures();

    // Compile chunk shaders
    GLuint chunkVS = compileShader(GL_VERTEX_SHADER, chunkVertSource);
    GLuint chunkFS = compileShader(GL_FRAGMENT_SHADER, chunkFragSource);
    GLuint chunkProgram = glCreateProgram();
    glAttachShader(chunkProgram, chunkVS);
    glAttachShader(chunkProgram, chunkFS);
    glLinkProgram(chunkProgram);
    glDeleteShader(chunkVS);
    glDeleteShader(chunkFS);

    // Initialize model renderer
    ModelRenderer modelRenderer;
    modelRenderer.init(cullingCompSource, modelVertSource, modelFragSource);

    // Initialize block highlight renderer
    BlockHighlight blockHighlight;
    blockHighlight.init();

    // Create player
    Player player(glm::vec3(0.0f, 64.0f, 0.0f));
    player.yaw = -135.0f;
    player.pitch = -20.0f;
    player.updateVectors();
    g_player = &player;

    // Create chunk manager
    ChunkManager chunkManager;
    g_chunkManager = &chunkManager;

    std::cout << "Starting infinite world generation..." << std::endl;

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    Frustum frustum;
    uint32_t totalChunks = 0, visibleChunks = 0;
    uint32_t totalOpaqueVerts = 0, visibleOpaqueVerts = 0;
    uint32_t totalTransparentVerts = 0, visibleTransparentVerts = 0;
    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        g_deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // Update chunk streaming
        glm::ivec3 playerChunk = player.getChunkPosition();
        chunkManager.updateAroundPlayer(playerChunk, player.chunkRenderDistance);
        chunkManager.processGenerationQueue(200);  // Generate up to 2 chunks per frame
        chunkManager.processMeshQueue(3);        // Mesh up to 4 chunks per frame
        chunkManager.unloadDistantChunks(playerChunk, player.chunkRenderDistance);

        // Raycast for block targeting
        auto getWorldBlock = [&](int x, int y, int z) -> BlockData {
            glm::ivec3 chunkPos = glm::ivec3(
                (x >= 0 ? x : x - CHUNK_SIZE + 1) / CHUNK_SIZE,
                (y >= 0 ? y : y - CHUNK_SIZE + 1) / CHUNK_SIZE,
                (z >= 0 ? z : z - CHUNK_SIZE + 1) / CHUNK_SIZE
            );
            Chunk* chunk = chunkManager.getChunk(chunkPos);
            if (!chunk) return {BLOCK_AIR, ROT_Y_UP_Z_FWD, 0};

            int lx = ((x % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
            int ly = ((y % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
            int lz = ((z % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
            return chunk->getBlock(lx, ly, lz);
        };

        RaycastHit hit = raycast(player.position, player.front, 8.0f, getWorldBlock);

        // Helper lambda to rebuild chunk and neighbors if on boundary
        auto rebuildChunkAndNeighbors = [&](Chunk* chunk, int lx, int ly, int lz) {
            chunk->needsLightRebuild = true;
            chunkManager.rebuildChunkMesh(chunk);

            // Check if block is on chunk boundary and rebuild neighbor chunks
            glm::ivec3 cpos = chunk->chunkPos;
            if (lx == 0) {
                Chunk* neighbor = chunkManager.getChunk(cpos + glm::ivec3(-1, 0, 0));
                if (neighbor) { neighbor->needsRebuild = true; neighbor->needsLightRebuild = true; chunkManager.rebuildChunkMesh(neighbor); }
            }
            if (lx == CHUNK_SIZE - 1) {
                Chunk* neighbor = chunkManager.getChunk(cpos + glm::ivec3(1, 0, 0));
                if (neighbor) { neighbor->needsRebuild = true; neighbor->needsLightRebuild = true; chunkManager.rebuildChunkMesh(neighbor); }
            }
            if (ly == 0) {
                Chunk* neighbor = chunkManager.getChunk(cpos + glm::ivec3(0, -1, 0));
                if (neighbor) { neighbor->needsRebuild = true; neighbor->needsLightRebuild = true; chunkManager.rebuildChunkMesh(neighbor); }
            }
            if (ly == CHUNK_SIZE - 1) {
                Chunk* neighbor = chunkManager.getChunk(cpos + glm::ivec3(0, 1, 0));
                if (neighbor) { neighbor->needsRebuild = true; neighbor->needsLightRebuild = true; chunkManager.rebuildChunkMesh(neighbor); }
            }
            if (lz == 0) {
                Chunk* neighbor = chunkManager.getChunk(cpos + glm::ivec3(0, 0, -1));
                if (neighbor) { neighbor->needsRebuild = true; neighbor->needsLightRebuild = true; chunkManager.rebuildChunkMesh(neighbor); }
            }
            if (lz == CHUNK_SIZE - 1) {
                Chunk* neighbor = chunkManager.getChunk(cpos + glm::ivec3(0, 0, 1));
                if (neighbor) { neighbor->needsRebuild = true; neighbor->needsLightRebuild = true; chunkManager.rebuildChunkMesh(neighbor); }
            }
        };

        // Handle block breaking (left click)
        if (g_leftMousePressed && hit.hit) {
            g_leftMousePressed = false;

            glm::ivec3 chunkPos = glm::ivec3(
                (hit.blockPos.x >= 0 ? hit.blockPos.x : hit.blockPos.x - CHUNK_SIZE + 1) / CHUNK_SIZE,
                (hit.blockPos.y >= 0 ? hit.blockPos.y : hit.blockPos.y - CHUNK_SIZE + 1) / CHUNK_SIZE,
                (hit.blockPos.z >= 0 ? hit.blockPos.z : hit.blockPos.z - CHUNK_SIZE + 1) / CHUNK_SIZE
            );
            Chunk* chunk = chunkManager.getChunk(chunkPos);
            if (chunk) {
                int lx = ((hit.blockPos.x % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
                int ly = ((hit.blockPos.y % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
                int lz = ((hit.blockPos.z % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;

                chunk->setBlock(lx, ly, lz, BLOCK_AIR);
                rebuildChunkAndNeighbors(chunk, lx, ly, lz);
                g_needsModelUpdate = true;
            }
        }
        g_leftMousePressed = false;

        // Handle block placing (right click)
        if (g_rightMousePressed && hit.hit) {
            g_rightMousePressed = false;

            glm::ivec3 placePos = hit.placePos;
            glm::ivec3 chunkPos = glm::ivec3(
                (placePos.x >= 0 ? placePos.x : placePos.x - CHUNK_SIZE + 1) / CHUNK_SIZE,
                (placePos.y >= 0 ? placePos.y : placePos.y - CHUNK_SIZE + 1) / CHUNK_SIZE,
                (placePos.z >= 0 ? placePos.z : placePos.z - CHUNK_SIZE + 1) / CHUNK_SIZE
            );
            Chunk* chunk = chunkManager.getChunk(chunkPos);
            if (chunk) {
                int lx = ((placePos.x % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
                int ly = ((placePos.y % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
                int lz = ((placePos.z % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;

                chunk->setBlock(lx, ly, lz, g_selectedBlockType);
                rebuildChunkAndNeighbors(chunk, lx, ly, lz);
                g_needsModelUpdate = true;
            }
        }
        g_rightMousePressed = false;

        // Update model instances if blocks changed
        if (g_needsModelUpdate) {
            chunkManager.collectAllModelBlocks(modelRenderer);
            g_needsModelUpdate = false;
        }

        float aspectRatio = (float)SCR_WIDTH / (float)SCR_HEIGHT;
        glm::mat4 projection = player.getProjectionMatrix(aspectRatio);
        glm::mat4 view = player.getViewMatrix();
        glm::mat4 vp = projection * view;

        frustum.extract(vp);

        glClearColor(0.4f, 0.6f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Bind texture atlas
        g_textureAtlas.bind(0);

        // Setup chunk shader
        glUseProgram(chunkProgram);
        glUniformMatrix4fv(glGetUniformLocation(chunkProgram, "view"), 1,
                           GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(chunkProgram, "projection"), 1,
                           GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(chunkProgram, "viewPos"), 1,
                     glm::value_ptr(player.position));
        glUniform1i(glGetUniformLocation(chunkProgram, "textureArray"), 0);

        totalChunks = 0;
        visibleChunks = 0;
        totalOpaqueVerts = 0;
        visibleOpaqueVerts = 0;
        totalTransparentVerts = 0;
        visibleTransparentVerts = 0;

        // Render opaque geometry first
        for (auto& [hash, chunk] : chunkManager.chunks) {
            if (chunk->isEmpty || !chunk->meshUploaded) continue;

            totalChunks++;
            totalOpaqueVerts += chunk->vertexCount;
            totalTransparentVerts += chunk->transparentVertexCount;

            glm::vec3 minPos, maxPos;
            chunk->getBoundingBox(minPos, maxPos);

            if (!frustum.isBoxVisible(minPos, maxPos)) continue;

            visibleChunks++;

            if (chunk->vertexCount > 0) {
                visibleOpaqueVerts += chunk->vertexCount;
                glBindVertexArray(chunk->VAO);
                glDrawArrays(GL_TRIANGLES, 0,
                             static_cast<GLsizei>(chunk->vertexCount));
            }
        }

        // Render block selection highlight
        if (hit.hit && !g_uiMode) {
            glDisable(GL_DEPTH_TEST);
            blockHighlight.render(vp, hit.blockPos, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            glEnable(GL_DEPTH_TEST);
        }

        // Render GPU-culled model blocks
        glDisable(GL_CULL_FACE);
        modelRenderer.cullAndRender(frustum.planes, view, projection, player.position);
        glEnable(GL_CULL_FACE);

        // Render transparent geometry last (with blending)
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        glUseProgram(chunkProgram);

        // Collect chunks with transparent geometry
        struct TransparentJob {
            Chunk* chunk;
            float distanceSq;
        };
        std::vector<TransparentJob> transparentJobs;

        for (auto& [hash, chunk] : chunkManager.chunks) {
            if (chunk->isEmpty || !chunk->meshUploaded || chunk->transparentVertexCount == 0)
                continue;

            glm::vec3 minPos, maxPos;
            chunk->getBoundingBox(minPos, maxPos);

            if (frustum.isBoxVisible(minPos, maxPos)) {
                glm::vec3 chunkCenter = minPos + glm::vec3(CHUNK_SIZE / 2.0f);
                float distSq = glm::distance2(player.position, chunkCenter);
                transparentJobs.push_back({chunk.get(), distSq});
            }
        }

        // Sort Back-to-Front (highest distance first)
        std::sort(transparentJobs.begin(), transparentJobs.end(),
            [](const TransparentJob& a, const TransparentJob& b) {
                return a.distanceSq > b.distanceSq;
            });

        // Draw sorted chunks
        for (auto& job : transparentJobs) {
            visibleTransparentVerts += job.chunk->transparentVertexCount;
            glBindVertexArray(job.chunk->transparentVAO);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(job.chunk->transparentVertexCount));
        }

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        // ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (g_uiMode) {
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        } else {
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
        }

        // Draw crosshair in game mode
        if (!g_uiMode) {
            ImDrawList* drawList = ImGui::GetBackgroundDrawList();
            float cx = SCR_WIDTH / 2.0f;
            float cy = SCR_HEIGHT / 2.0f;
            float size = 10.0f;
            ImU32 color = IM_COL32(255, 255, 255, 200);
            drawList->AddLine(ImVec2(cx - size, cy), ImVec2(cx + size, cy), color, 2.0f);
            drawList->AddLine(ImVec2(cx, cy - size), ImVec2(cx, cy + size), color, 2.0f);
        }

        ImGui::Begin("Debug Menu");
        if (g_uiMode) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "MENU MODE (TAB to close)");
        } else {
            ImGui::Text("GAME MODE (TAB for menu)");
        }
        ImGui::Separator();
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Frame Time: %.3f ms", 1000.0f / io.Framerate);

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 1.0f, 1.0f), "Player:");
        ImGui::Text("  Position: %.1f, %.1f, %.1f",
                    player.position.x, player.position.y, player.position.z);
        ImGui::Text("  Chunk: %d, %d, %d",
                    playerChunk.x, playerChunk.y, playerChunk.z);
        ImGui::SliderFloat("Move Speed", &player.moveSpeed, 10.0f, 200.0f);
        ImGui::SliderInt("Chunk Render Distance", &player.chunkRenderDistance, 2, 16);
        ImGui::SliderFloat("View Distance", &player.viewDistance, 100.0f, 2000.0f);

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "World Streaming:");
        ImGui::Text("  Loaded Chunks: %zu", chunkManager.chunks.size());
        ImGui::Text("  Generation Queue: %zu", chunkManager.chunksToGenerate.size());
        ImGui::Text("  Mesh Queue: %zu", chunkManager.chunksToMesh.size());

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
                           "Solid Blocks (Textured Greedy Mesh):");
        ImGui::Text("  Chunks: %u / %u visible", visibleChunks, totalChunks);
        ImGui::Text("  Opaque Verts: %u / %u", visibleOpaqueVerts, totalOpaqueVerts);
        ImGui::Text("  Transparent Verts: %u / %u", visibleTransparentVerts,
                    totalTransparentVerts);
        ImGui::Text("  Triangles: %u",
                    (visibleOpaqueVerts + visibleTransparentVerts) / 3);

        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.5f, 1.0f),
                           "Model Blocks (GPU Culled):");
        ImGui::Text("  Total: %zu", modelRenderer.allInstances.size());
        ImGui::Text("  Visible: %u", modelRenderer.visibleCount);
        ImGui::Text("  Culled: %.1f%%",
                    modelRenderer.allInstances.size() > 0
                    ? 100.0f * (1.0f - (float)modelRenderer.visibleCount /
                               modelRenderer.allInstances.size())
                    : 0.0f);

        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Block Interaction:");
        if (hit.hit) {
            ImGui::Text("  Target: %d, %d, %d", hit.blockPos.x, hit.blockPos.y, hit.blockPos.z);
        } else {
            ImGui::Text("  Target: None");
        }
        ImGui::Text("  Selected: %s",
            g_selectedBlockType == BLOCK_STONE ? "Stone" :
            g_selectedBlockType == BLOCK_DIRT ? "Dirt" :
            g_selectedBlockType == BLOCK_GRASS ? "Grass" :
            g_selectedBlockType == BLOCK_SAND ? "Sand" :
            g_selectedBlockType == BLOCK_WOOD ? "Wood" :
            g_selectedBlockType == BLOCK_LOG ? "Log" :
            g_selectedBlockType == BLOCK_LEAVES ? "Leaves" :
            g_selectedBlockType == BLOCK_GLASS ? "Glass" :
            g_selectedBlockType == BLOCK_BRICK ? "Brick" :
            g_selectedBlockType == BLOCK_GLOWSTONE ? "Glowstone" :
            g_selectedBlockType == BLOCK_LAVA ? "Lava" : "Unknown");
        ImGui::Text("  Scroll to change block");
        ImGui::Text("  Left click to break");
        ImGui::Text("  Right click to place");
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteProgram(chunkProgram);
    modelRenderer.cleanup();
    blockHighlight.cleanup();
    g_textureAtlas.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
