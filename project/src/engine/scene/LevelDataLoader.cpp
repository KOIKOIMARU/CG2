#include "engine/scene/LevelDataLoader.h"

#include <nlohmann/json.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <sstream>
#include <stdexcept>

namespace {

using Json = nlohmann::json;

constexpr float kDegreesToRadians =
    std::numbers::pi_v<float> / 180.0f;

std::filesystem::path PathFromUtf8(const char* path)
{
    const auto* begin = reinterpret_cast<const char8_t*>(path);
    return std::filesystem::path(
        std::u8string(begin, begin + std::strlen(path)));
}

void SetError(std::string* errorMessage, const std::string& message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

Math::Vector3 ReadVector3(const Json& value, const std::string& fieldName)
{
    if (!value.is_array() || value.size() != 3) {
        throw std::runtime_error(fieldName + " must be an array of three numbers.");
    }

    for (const Json& component : value) {
        if (!component.is_number()) {
            throw std::runtime_error(fieldName + " must contain only numbers.");
        }
    }

    return {
        value.at(0).get<float>(),
        value.at(1).get<float>(),
        value.at(2).get<float>()
    };
}

Math::Vector3 ConvertAxis(const Math::Vector3& blender)
{
    return { blender.x, blender.z, blender.y };
}

Math::Vector3 ConvertRotation(const Math::Vector3& blenderDegrees)
{
    return {
        -blenderDegrees.x * kDegreesToRadians,
        -blenderDegrees.z * kDegreesToRadians,
        -blenderDegrees.y * kDegreesToRadians
    };
}

LevelDataLoader::ObjectData ParseObject(
    const Json& objectJson,
    const std::string& location)
{
    if (!objectJson.is_object()) {
        throw std::runtime_error(location + " must be an object.");
    }
    if (!objectJson.contains("type") || !objectJson.at("type").is_string()) {
        throw std::runtime_error(location + ".type must be a string.");
    }
    if (!objectJson.contains("name") || !objectJson.at("name").is_string()) {
        throw std::runtime_error(location + ".name must be a string.");
    }
    if (!objectJson.contains("transform") ||
        !objectJson.at("transform").is_object()) {
        throw std::runtime_error(location + ".transform must be an object.");
    }

    const Json& transformJson = objectJson.at("transform");
    for (const char* key : { "translation", "rotation", "scaling" }) {
        if (!transformJson.contains(key)) {
            throw std::runtime_error(
                location + ".transform." + key + " is required.");
        }
    }

    LevelDataLoader::ObjectData objectData{};
    objectData.type = objectJson.at("type").get<std::string>();
    objectData.name = objectJson.at("name").get<std::string>();

    if (objectJson.contains("disabled")) {
        if (!objectJson.at("disabled").is_boolean()) {
            throw std::runtime_error(location + ".disabled must be a boolean.");
        }
        objectData.disabled = objectJson.at("disabled").get<bool>();
    }

    if (objectJson.contains("file_name")) {
        if (!objectJson.at("file_name").is_string()) {
            throw std::runtime_error(location + ".file_name must be a string.");
        }
        objectData.fileName = objectJson.at("file_name").get<std::string>();
    }

    objectData.translation = ConvertAxis(ReadVector3(
        transformJson.at("translation"),
        location + ".transform.translation"));
    objectData.rotation = ConvertRotation(ReadVector3(
        transformJson.at("rotation"),
        location + ".transform.rotation"));
    objectData.scaling = ConvertAxis(ReadVector3(
        transformJson.at("scaling"),
        location + ".transform.scaling"));

    if (objectJson.contains("collider")) {
        const Json& colliderJson = objectJson.at("collider");
        if (!colliderJson.is_object()) {
            throw std::runtime_error(location + ".collider must be an object.");
        }
        if (!colliderJson.contains("type") ||
            !colliderJson.at("type").is_string() ||
            !colliderJson.contains("center") ||
            !colliderJson.contains("size")) {
            throw std::runtime_error(
                location + ".collider requires type, center, and size.");
        }

        objectData.collider.enabled = true;
        objectData.collider.type = colliderJson.at("type").get<std::string>();
        objectData.collider.center = ConvertAxis(ReadVector3(
            colliderJson.at("center"),
            location + ".collider.center"));
        objectData.collider.size = ConvertAxis(ReadVector3(
            colliderJson.at("size"),
            location + ".collider.size"));
    }

    if (objectJson.contains("children")) {
        const Json& childrenJson = objectJson.at("children");
        if (!childrenJson.is_array()) {
            throw std::runtime_error(location + ".children must be an array.");
        }

        objectData.children.reserve(childrenJson.size());
        for (size_t index = 0; index < childrenJson.size(); ++index) {
            objectData.children.push_back(ParseObject(
                childrenJson.at(index),
                location + ".children[" + std::to_string(index) + "]"));
        }
    }

    return objectData;
}

void CollectSpawnPoints(
    const LevelDataLoader::ObjectData& objectData,
    LevelDataLoader::LevelData& levelData)
{
    if (objectData.disabled) {
        return;
    }

    if (objectData.type == "PlayerSpawn") {
        LevelDataLoader::PlayerSpawnData playerSpawn{};
        playerSpawn.translation = objectData.translation;
        playerSpawn.rotation = objectData.rotation;
        levelData.players.push_back(playerSpawn);
    } else if (objectData.type == "EnemySpawn") {
        LevelDataLoader::EnemySpawnData enemySpawn{};
        enemySpawn.fileName = objectData.fileName;
        enemySpawn.translation = objectData.translation;
        enemySpawn.rotation = objectData.rotation;
        levelData.enemies.push_back(enemySpawn);
    }

    for (const LevelDataLoader::ObjectData& child : objectData.children) {
        CollectSpawnPoints(child, levelData);
    }
}

} // namespace

bool LevelDataLoader::LoadFile(
    const char* path,
    LevelData& levelData,
    std::string* errorMessage)
{
    levelData = LevelData{};
    if (errorMessage) {
        errorMessage->clear();
    }

    if (!path || path[0] == '\0') {
        SetError(errorMessage, "The level JSON path is empty.");
        return false;
    }

    std::ifstream file(PathFromUtf8(path));
    if (!file) {
        SetError(errorMessage, std::string("Could not open level JSON: ") + path);
        return false;
    }

    try {
        Json root;
        file >> root;

        if (!root.is_object()) {
            throw std::runtime_error("The JSON root must be an object.");
        }
        if (!root.contains("name") || !root.at("name").is_string()) {
            throw std::runtime_error("The root name must be a string.");
        }
        if (root.at("name").get<std::string>() != "scene") {
            throw std::runtime_error("The root name must be \"scene\".");
        }
        if (!root.contains("objects") || !root.at("objects").is_array()) {
            throw std::runtime_error("The root objects field must be an array.");
        }

        levelData.name = "scene";
        const Json& objectsJson = root.at("objects");
        levelData.objects.reserve(objectsJson.size());
        for (size_t index = 0; index < objectsJson.size(); ++index) {
            levelData.objects.push_back(ParseObject(
                objectsJson.at(index),
                "objects[" + std::to_string(index) + "]"));
        }
        for (const ObjectData& objectData : levelData.objects) {
            CollectSpawnPoints(objectData, levelData);
        }
    } catch (const Json::parse_error& exception) {
        std::ostringstream message;
        message << "JSON parse error at byte " << exception.byte
            << ": " << exception.what();
        SetError(errorMessage, message.str());
        levelData = LevelData{};
        return false;
    } catch (const std::exception& exception) {
        SetError(errorMessage, exception.what());
        levelData = LevelData{};
        return false;
    }

    return true;
}
