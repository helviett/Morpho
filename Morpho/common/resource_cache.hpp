#pragma once
#include <unordered_map>

namespace Morpho {

template<typename T>
class ResourceCache {
public:
    ResourceCache() = default;
    ~ResourceCache() = default;
    ResourceCache(const ResourceCache &) = delete;
    ResourceCache &operator=(const ResourceCache &) = delete;
    ResourceCache(ResourceCache &&) = delete;
    ResourceCache &operator=(ResourceCache &&) = delete;

    bool try_get(std::size_t hash, T& resource) {
        auto it = resources.find(hash);
        if (it != resources.end()) {
            resource = (*it).second;
            return true;
        }
        return false;
    }

    void add(std::size_t hash, T resource) {
        resources[hash] = resource;
    }
private:
    std::unordered_map<std::size_t, T> resources;
};

}
