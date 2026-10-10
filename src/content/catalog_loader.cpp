#include "wizard/content.hpp"
#include <set>
#include <stdexcept>

namespace wizard {
Json readCatalogJson(const std::filesystem::path &directory) {
    const auto manifestPath = directory / "catalog.json";
    if (!std::filesystem::exists(manifestPath))
        return readJson(directory / "cards.json");
    auto manifest = readJson(manifestPath);
    auto fail = [&](const std::string &message) -> void {
        throw std::runtime_error(manifestPath.string() + ": " + message);
    };
    if (!manifest.is_object() || manifest.value("format", 0) != 1 ||
        !manifest.contains("metadata") || !manifest.at("metadata").is_object() ||
        manifest.at("metadata").contains("cards") || !manifest.contains("definitions") ||
        !manifest.at("definitions").is_array() || manifest.at("definitions").empty())
        fail("invalid catalog manifest");
    Json catalog = manifest.at("metadata");
    catalog["cards"] = Json::array();
    const auto root = std::filesystem::weakly_canonical(directory);
    std::set<std::string> ids;
    std::set<std::filesystem::path> files;
    for (const auto &entry : manifest.at("definitions")) {
        if (!entry.is_object() || !entry.contains("id") || !entry.at("id").is_string() ||
            !entry.contains("file") || !entry.at("file").is_string())
            fail("definition requires string id and file");
        const auto id = entry.at("id").get<std::string>();
        const auto filename = entry.at("file").get<std::string>();
        const auto relative = std::filesystem::u8path(filename);
        if (id.empty() || !ids.insert(id).second)
            fail("duplicate or empty definition id: " + id);
        if (filename.empty() || relative.is_absolute() || relative.has_root_path() ||
            filename.find(':') != std::string::npos || filename.find('\\') != std::string::npos ||
            relative.extension() != ".json")
            fail("definition path must be a relative JSON path");
        for (const auto &component : relative)
            if (component == ".." || component == ".")
                fail("definition path cannot traverse directories");
        const auto file = std::filesystem::weakly_canonical(root / relative);
        const auto contained = file.lexically_relative(root);
        if (contained.empty() || contained.is_absolute() || *contained.begin() == ".." ||
            !files.insert(file).second)
            fail("definition path escapes catalog or repeats a file");
        auto definition = readJson(file);
        if (!definition.is_object() || definition.value("id", std::string{}) != id)
            fail("definition id disagrees with manifest: " + id);
        catalog["cards"].push_back(std::move(definition));
    }
    return catalog;
}
} // namespace wizard
