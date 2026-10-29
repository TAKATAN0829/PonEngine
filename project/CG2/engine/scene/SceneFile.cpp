#include "SceneFile.h"
#include "IEditableScene.h"

#include <Windows.h>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <unordered_set>
#include <vector>

namespace {
struct Record {
    SceneObjectId id{};
    unsigned mask{};
    std::array<float, 14> values{}; // scale, rotation, position, color, shininess
    int lighting{};
};

unsigned Mask(const EditableObjectView& view) {
    return (view.transform ? 1u : 0u) | (view.color ? 2u : 0u) |
        (view.enableLighting ? 4u : 0u) | (view.shininess ? 8u : 0u);
}

bool Valid(const Record& record) {
    for (float value : record.values) {
        if (!std::isfinite(value)) { return false; }
    }
    return record.id != kInvalidSceneObjectId && record.mask <= 15 &&
        (record.lighting == 0 || record.lighting == 1) &&
        (!(record.mask & 8) || (record.values[13] >= 1.0f && record.values[13] <= 500.0f));
}
}

bool SceneFile::Save(IEditableScene& scene, const std::filesystem::path& path, std::string& error) {
    error.clear();
    if (scene.GetEditableObjectCount() > 100000) {
        error = "Scene exceeds the supported object count.";
        return false;
    }
    std::vector<Record> records;
    std::unordered_set<SceneObjectId> ids;
    for (size_t i = 0; i < scene.GetEditableObjectCount(); ++i) {
        const auto view = scene.GetEditableObject(i);
        Record record{};
        record.id = view.id;
        record.mask = Mask(view);
        if (view.transform) {
            const auto& t = *view.transform;
            record.values = { t.scale.x, t.scale.y, t.scale.z, t.rotate.x, t.rotate.y, t.rotate.z,
                t.translate.x, t.translate.y, t.translate.z };
        }
        if (view.color) {
            record.values[9] = view.color->x; record.values[10] = view.color->y;
            record.values[11] = view.color->z; record.values[12] = view.color->w;
        }
        if (view.enableLighting) { record.lighting = *view.enableLighting ? 1 : 0; }
        if (view.shininess) { record.values[13] = *view.shininess; }
        if (!Valid(record) || !ids.insert(record.id).second) {
            error = "Scene contains invalid values or duplicate IDs.";
            return false;
        }
        records.push_back(record);
    }

    std::error_code ec;
    if (!path.parent_path().empty()) { std::filesystem::create_directories(path.parent_path(), ec); }
    if (ec) { error = "Cannot create scene directory: " + ec.message(); return false; }
    auto temporary = path;
    temporary += L".tmp";
    std::ofstream out(temporary, std::ios::trunc);
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<float>::max_digits10);
    out << "PON_SCENE 1\n" << records.size() << '\n';
    for (const auto& record : records) {
        out << record.id << ' ' << record.mask;
        for (float value : record.values) { out << ' ' << value; }
        out << ' ' << record.lighting << '\n';
    }
    out.close();
    if (!out) {
        error = "Cannot write scene file.";
        std::filesystem::remove(temporary, ec);
        return false;
    }
    // Replace only after the entire temporary file was written successfully.
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = "Cannot replace scene file (Windows error " + std::to_string(GetLastError()) + ").";
        std::filesystem::remove(temporary, ec);
        return false;
    }
    return true;
}

bool SceneFile::Load(IEditableScene& scene, const std::filesystem::path& path, std::string& error) {
    error.clear();
    std::ifstream in(path);
    in.imbue(std::locale::classic());
    if (!in) { error = "Cannot open scene file."; return false; }
    std::string magic;
    unsigned version{};
    size_t count{};
    if (!(in >> magic >> version >> count) || magic != "PON_SCENE" || version != 1 ||
        count != scene.GetEditableObjectCount() || count > 100000) {
        error = "Unsupported scene format or object count mismatch.";
        return false;
    }

    std::vector<EditableObjectView> views;
    std::unordered_set<SceneObjectId> sceneIds;
    for (size_t i = 0; i < count; ++i) {
        auto view = scene.GetEditableObject(i);
        if (view.id == kInvalidSceneObjectId || !sceneIds.insert(view.id).second) {
            error = "Scene contains invalid or duplicate IDs.";
            return false;
        }
        views.push_back(view);
    }
    std::vector<Record> records(count);
    std::vector<size_t> targets;
    std::unordered_set<SceneObjectId> fileIds;
    for (auto& record : records) {
        in >> record.id >> record.mask;
        for (float& value : record.values) { in >> value; }
        in >> record.lighting;
        if (!in || !Valid(record) || !fileIds.insert(record.id).second) {
            error = "Invalid or incomplete scene data.";
            return false;
        }
        size_t target = 0;
        while (target < views.size() && views[target].id != record.id) { ++target; }
        if (target == views.size() || Mask(views[target]) != record.mask) {
            error = "Scene object ID or property mismatch.";
            return false;
        }
        targets.push_back(target);
    }
    in >> std::ws;
    if (in.bad() || !in.eof()) { error = "Unexpected data after scene records."; return false; }

    // Validate everything before changing any live data.
    for (size_t i = 0; i < records.size(); ++i) {
        const auto& record = records[i];
        const auto& v = record.values;
        auto& view = views[targets[i]];
        if (view.transform) { *view.transform = { {v[0], v[1], v[2]}, {v[3], v[4], v[5]}, {v[6], v[7], v[8]} }; }
        if (view.color) { *view.color = {v[9], v[10], v[11], v[12]}; }
        if (view.enableLighting) { *view.enableLighting = record.lighting != 0; }
        if (view.shininess) { *view.shininess = v[13]; }
    }
    return true;
}
