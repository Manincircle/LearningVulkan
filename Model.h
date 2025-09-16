#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <functional>
#include <vulkan/vulkan.h>

struct Vertex{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;

    bool operator==(const Vertex& other) const {
        return position == other.position && normal == other.normal && texCoord == other.texCoord;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const noexcept {
            size_t seed = 0;

            // 一个通用的 hash 组合方法（boost::hash_combine 的思路）
            auto hashCombine = [&seed](size_t v) {
                seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            };

            hashCombine(std::hash<float>()(vertex.position.x));
            hashCombine(std::hash<float>()(vertex.position.y));
            hashCombine(std::hash<float>()(vertex.position.z));

            hashCombine(std::hash<float>()(vertex.normal.x));
            hashCombine(std::hash<float>()(vertex.normal.y));
            hashCombine(std::hash<float>()(vertex.normal.z));

            hashCombine(std::hash<float>()(vertex.texCoord.x));
            hashCombine(std::hash<float>()(vertex.texCoord.y));

            return seed;
        }
    };
}



class Model {
public:
    Model(const std::string& filePath);
    ~Model();
    std::vector<Vertex>& getVertices() { return vertices; }
    std::vector<uint32_t>& getIndices() { return indices; }
    std::vector<VkVertexInputBindingDescription> getVertexBindingDescription();
    std::vector<VkVertexInputAttributeDescription> getVertexAttributeDescription();
private:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};