#include "wizard/bridge.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <limits>

namespace {
using namespace godot;
using wizard::Json;
std::string utf8(const String &value) {
    const auto bytes = value.utf8();
    return std::string(bytes.get_data(), static_cast<std::size_t>(bytes.length()));
}
Variant variant(const Json &value) {
    if (value.is_null()) return {};
    if (value.is_boolean()) return value.get<bool>();
    if (value.is_string()) return String::utf8(value.get_ref<const std::string &>().c_str());
    if (value.is_number_unsigned()) {
        const auto n = value.get<std::uint64_t>();
        if (n > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            throw std::runtime_error("unsigned ID must be encoded as a string");
        return static_cast<std::int64_t>(n);
    }
    if (value.is_number_integer()) return value.get<std::int64_t>();
    if (value.is_number_float()) return value.get<double>();
    if (value.is_array()) {
        Array out;
        for (const auto &item : value) out.push_back(variant(item));
        return out;
    }
    Dictionary out;
    for (const auto &[key, item] : value.items()) out[String::utf8(key.c_str())] = variant(item);
    return out;
}
Json json(const Variant &value) {
    switch (value.get_type()) {
    case Variant::NIL: return nullptr;
    case Variant::BOOL: return static_cast<bool>(value);
    case Variant::INT: return static_cast<std::int64_t>(value);
    case Variant::FLOAT: return static_cast<double>(value);
    case Variant::STRING: return utf8(static_cast<String>(value));
    case Variant::ARRAY: {
        const Array values = value;
        Json out = Json::array();
        for (std::int64_t i = 0; i < values.size(); ++i) out.push_back(json(values[i]));
        return out;
    }
    case Variant::DICTIONARY: {
        const Dictionary values = value;
        const Array keys = values.keys();
        Json out = Json::object();
        for (std::int64_t i = 0; i < keys.size(); ++i) {
            if (keys[i].get_type() != Variant::STRING)
                throw std::runtime_error("DTO keys must be strings");
            out[utf8(static_cast<String>(keys[i]))] = json(values[keys[i]]);
        }
        return out;
    }
    default: throw std::runtime_error("unsupported DTO value");
    }
}
template<class F> Dictionary bridgeCall(F &&function) {
    try { return variant(function()); }
    catch (const std::exception &e) {
        return variant(Json{{"ok", false}, {"errorCode", "bridge_error"}, {"error", e.what()}});
    }
}
}

class WizardBridge : public godot::RefCounted {
    GDCLASS(WizardBridge, godot::RefCounted)
    wizard::bridge::LocalSession session_;
  protected:
    static void _bind_methods() {
        using namespace godot;
        ClassDB::bind_method(D_METHOD("initialize", "assets", "user_directory"), &WizardBridge::initialize);
        ClassDB::bind_method(D_METHOD("start_match", "options"), &WizardBridge::start_match);
        ClassDB::bind_method(D_METHOD("snapshot", "viewer"), &WizardBridge::snapshot);
        ClassDB::bind_method(D_METHOD("select_action", "viewer", "action_id", "generation", "revision"), &WizardBridge::select_action);
        ClassDB::bind_method(D_METHOD("confirm_action", "generation", "revision"), &WizardBridge::confirm_action);
        ClassDB::bind_method(D_METHOD("cancel_action"), &WizardBridge::cancel_action);
        ClassDB::bind_method(D_METHOD("set_paused", "paused"), &WizardBridge::set_paused);
        ClassDB::bind_method(D_METHOD("step_ai", "generation", "revision"), &WizardBridge::step_ai);
        ClassDB::bind_method(D_METHOD("continue_tutorial", "generation", "revision"), &WizardBridge::continue_tutorial);
        ClassDB::bind_method(D_METHOD("save_replay"), &WizardBridge::save_replay);
        ClassDB::bind_method(D_METHOD("restart_match", "seed"), &WizardBridge::restart_match);
        ClassDB::bind_method(D_METHOD("release_session"), &WizardBridge::release_session);
        ClassDB::bind_method(D_METHOD("read_replay_plan", "path"), &WizardBridge::read_replay_plan);
        ClassDB::bind_method(D_METHOD("verify_replay", "path"), &WizardBridge::verify_replay);
        ClassDB::bind_method(D_METHOD("library"), &WizardBridge::library);
        ClassDB::bind_method(D_METHOD("create_draft", "preset"), &WizardBridge::create_draft);
        ClassDB::bind_method(D_METHOD("validate_draft", "draft"), &WizardBridge::validate_draft);
        ClassDB::bind_method(D_METHOD("save_draft", "draft"), &WizardBridge::save_draft);
        ClassDB::bind_method(D_METHOD("erase_draft", "id"), &WizardBridge::erase_draft);
        ClassDB::bind_method(D_METHOD("query_cards", "query"), &WizardBridge::query_cards);
        ClassDB::bind_method(D_METHOD("apply_settings", "settings"), &WizardBridge::apply_settings);
        ClassDB::bind_method(D_METHOD("leave_match", "viewer"), &WizardBridge::leave_match);
        ClassDB::bind_method(D_METHOD("interact", "viewer", "request", "generation", "revision"), &WizardBridge::interact);
    }
  public:
    Dictionary initialize(const String &assets, const String &user_directory) {
        return bridgeCall([&] { return session_.initialize(std::filesystem::u8path(utf8(assets)),
            user_directory.is_empty() ? wizard::app::userDataDirectory() : std::filesystem::u8path(utf8(user_directory))); });
    }
    Dictionary start_match(const Dictionary &options) { return bridgeCall([&] { return session_.start(json(options)); }); }
    Dictionary snapshot(std::int64_t viewer) {
        return bridgeCall([&] { return session_.snapshot(viewer == 0 || viewer == 1 ? static_cast<int>(viewer) : -1); });
    }
    Dictionary select_action(std::int64_t viewer, const String &id, const String &generation, const String &revision) {
        return bridgeCall([&] { return session_.selectAction(viewer == 0 || viewer == 1 ? static_cast<int>(viewer) : -1,
            utf8(id), utf8(generation), utf8(revision)); });
    }
    Dictionary confirm_action(const String &generation, const String &revision) {
        return bridgeCall([&] { return session_.confirmAction(utf8(generation), utf8(revision)); });
    }
    Dictionary cancel_action() { return bridgeCall([&] { return session_.cancelAction(); }); }
    Dictionary set_paused(bool paused) { return bridgeCall([&] { return session_.pause(paused); }); }
    Dictionary step_ai(const String &generation, const String &revision) {
        return bridgeCall([&] { return session_.stepAi(utf8(generation), utf8(revision)); });
    }
    Dictionary continue_tutorial(const String &generation, const String &revision) {
        return bridgeCall([&] { return session_.continueTutorial(utf8(generation), utf8(revision)); });
    }
    Dictionary save_replay() { return bridgeCall([&] { return session_.saveReplay(); }); }
    Dictionary restart_match(std::int64_t seed) {
        return bridgeCall([&]() -> Json {
            if (seed < 0 || seed > std::numeric_limits<std::uint32_t>::max())
                throw std::runtime_error("seed outside uint32 range");
            return session_.restart(static_cast<std::uint32_t>(seed));
        });
    }
    Dictionary release_session() { return bridgeCall([&] { return session_.release(); }); }
    Dictionary read_replay_plan(const String &path) {
        return bridgeCall([&] { return session_.readReplayPlan(std::filesystem::u8path(utf8(path))); });
    }
    Dictionary verify_replay(const String &path) {
        return bridgeCall([&] { return session_.verifyReplay(std::filesystem::u8path(utf8(path))); });
    }
    Dictionary library() { return bridgeCall([&] { return session_.library(); }); }
    Dictionary create_draft(const String &preset) { return bridgeCall([&] { return session_.createDraft(utf8(preset)); }); }
    Dictionary validate_draft(const Dictionary &draft) { return bridgeCall([&] { return session_.validateDraft(json(draft)); }); }
    Dictionary save_draft(const Dictionary &draft) { return bridgeCall([&] { return session_.saveDraft(json(draft)); }); }
    Dictionary erase_draft(const String &id) { return bridgeCall([&] { return session_.eraseDraft(utf8(id)); }); }
    Dictionary query_cards(const Dictionary &query) { return bridgeCall([&] { return session_.query(json(query)); }); }
    Dictionary apply_settings(const Dictionary &settings) { return bridgeCall([&] { return session_.applySettings(json(settings)); }); }
    Dictionary leave_match(std::int64_t viewer) {
        return bridgeCall([&] { return session_.leaveMatch(viewer == 0 || viewer == 1 ? static_cast<int>(viewer) : -1); });
    }
    Dictionary interact(std::int64_t viewer, const Dictionary &request, const String &generation, const String &revision) {
        return bridgeCall([&] { return session_.interact(viewer == 0 || viewer == 1 ? static_cast<int>(viewer) : -1,
            json(request), utf8(generation), utf8(revision)); });
    }
};

extern "C" GDExtensionBool GDE_EXPORT wizard_bridge_init(GDExtensionInterfaceGetProcAddress get_proc_address,
    GDExtensionClassLibraryPtr library, GDExtensionInitialization *initialization) {
    godot::GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer([](godot::ModuleInitializationLevel level) {
        if (level == godot::MODULE_INITIALIZATION_LEVEL_SCENE) godot::ClassDB::register_class<WizardBridge>();
    });
    init.register_terminator([](godot::ModuleInitializationLevel) {});
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
