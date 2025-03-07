#include "Events.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string.h>

#include "debug/Logger.hpp"
#include "util/stringutil.hpp"
#include "Window.hpp"

static debug::Logger logger("events");

inline constexpr short _MOUSE_KEYS_OFFSET = 1024;

namespace {
    bool keys[KEYS_BUFFER_SIZE] = {};
    uint frames[KEYS_BUFFER_SIZE] = {};
    uint current_frame = 0;
    bool cursor_drag = false;
    bool cursor_locked = false;
    std::unordered_map<keycode, util::HandlersList<>> key_callbacks;
}

int Events::scroll = 0;

glm::vec2 Events::delta = {};
glm::vec2 Events::cursor = {};

std::vector<uint> Events::codepoints;
std::vector<keycode> Events::pressedKeys;
Bindings Events::bindings {};

int Events::getScroll() {
    return scroll;
}

bool Events::pressed(keycode keycode) {
    return pressed(static_cast<int>(keycode));
}

bool Events::pressed(int keycode) {
    if (keycode < 0 || keycode >= KEYS_BUFFER_SIZE) {
        return false;
    }
    return keys[keycode];
}

bool Events::jpressed(keycode keycode) {
    return jpressed(static_cast<int>(keycode));
}

bool Events::jpressed(int keycode) {
    return Events::pressed(keycode) && frames[keycode] == current_frame;
}

bool Events::clicked(mousecode button) {
    return clicked(static_cast<int>(button));
}

bool Events::clicked(int button) {
    return Events::pressed(_MOUSE_KEYS_OFFSET + button);
}

bool Events::jclicked(mousecode button) {
    return jclicked(static_cast<int>(button));
}

bool Events::jclicked(int button) {
    return Events::jpressed(_MOUSE_KEYS_OFFSET + button);
}

void Events::toggleCursor() {
    cursor_drag = false;
    cursor_locked = !cursor_locked;
    Window::setCursorMode(
        cursor_locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
    );
}

void Events::pollEvents() {
    current_frame++;
    delta.x = 0.f;
    delta.y = 0.f;
    scroll = 0;
    codepoints.clear();
    pressedKeys.clear();
    glfwPollEvents();

    for (auto& entry : bindings.getAll()) {
        auto& binding = entry.second;
        if (!binding.enabled) {
            binding.state = false;
            continue;
        }
        binding.justChange = false;

        bool newstate = false;
        switch (binding.type) {
            case inputtype::keyboard:
                newstate = pressed(binding.code);
                break;
            case inputtype::mouse:
                newstate = clicked(binding.code);
                break;
        }

        if (newstate) {
            if (!binding.state) {
                binding.state = true;
                binding.justChange = true;
                binding.onactived.notify();
            }
        } else {
            if (binding.state) {
                binding.state = false;
                binding.justChange = true;
            }
        }
    }
}

Binding* Events::getBinding(const std::string& name) {
    return bindings.get(name);
}

Binding& Events::requireBinding(const std::string& name) {
    if (const auto found = getBinding(name)) {
        return *found;
    }
    throw std::runtime_error("binding '" + name + "' does not exist");
}

void Events::bind(const std::string& name, inputtype type, keycode code) {
    bind(name, type, static_cast<int>(code));
}

void Events::bind(const std::string& name, inputtype type, mousecode code) {
    bind(name, type, static_cast<int>(code));
}

void Events::bind(const std::string& name, inputtype type, int code) {
    bindings.bind(name, type, code);
}

void Events::rebind(const std::string& name, inputtype type, int code) {
    requireBinding(name) = Binding(type, code);
}

bool Events::active(const std::string& name) {
    return bindings.active(name);
}

bool Events::jactive(const std::string& name) {
    return bindings.jactive(name);
}

void Events::setKey(int key, bool b) {
    ::keys[key] = b;
    ::frames[key] = current_frame;
    if (b) {
        const auto& callbacks = ::key_callbacks.find(static_cast<keycode>(key));
        if (callbacks != ::key_callbacks.end()) {
            callbacks->second.notify();
        }
    }
}

void Events::setButton(int button, bool b) {
    setKey(_MOUSE_KEYS_OFFSET + button, b);
}

void Events::setPosition(float xpos, float ypos) {
    if (::cursor_drag) {
        Events::delta.x += xpos - Events::cursor.x;
        Events::delta.y += ypos - Events::cursor.y;
    } else {
        ::cursor_drag = true;
    }
    Events::cursor.x = xpos;
    Events::cursor.y = ypos;
}

observer_handler Events::addKeyCallback(keycode key, KeyCallback callback) {
    return ::key_callbacks[key].add(std::move(callback));
}

#include "coders/json.hpp"
#include "coders/toml.hpp"

std::string Events::writeBindings() {
    auto obj = dv::object();
    for (auto& entry : bindings.getAll()) {
        const auto& binding = entry.second;
        std::string value;
        switch (binding.type) {
            case inputtype::keyboard:
                value =
                    "key:" +
                    input_util::get_name(static_cast<keycode>(binding.code));
                break;
            case inputtype::mouse:
                value =
                    "mouse:" +
                    input_util::get_name(static_cast<mousecode>(binding.code));
                break;
            default:
                throw std::runtime_error("unsupported control type");
        }
        obj[entry.first] = std::move(value);
    }
    return toml::stringify(obj);
}

void Events::loadBindings(
    const std::string& filename, const std::string& source,
    BindType bindType
) {
    auto map = toml::parse(filename, source);
    for (auto& [sectionName, section] : map.asObject()) {
        for (auto& [name, value] : section.asObject()) {
            auto key = sectionName + "." + name;
            auto [prefix, codename] = util::split_at(value.asString(), ':');
            inputtype type;
            int code;
            if (prefix == "key") {
                type = inputtype::keyboard;
                code = static_cast<int>(input_util::keycode_from(codename));
            } else if (prefix == "mouse") {
                type = inputtype::mouse;
                code = static_cast<int>(input_util::mousecode_from(codename));
            } else {
                logger.error()
                    << "unknown input type: " << prefix << " (binding "
                    << util::quote(key) << ")";
                continue;
            }
            if (bindType == BindType::BIND) {
                Events::bind(key, type, code);
            } else if (bindType == BindType::REBIND) {
                Events::rebind(key, type, code);
            }
        }
    }
}

void Events::enableBindings() {
    for (auto& entry : bindings.getAll()) {
        entry.second.enabled = true;
    }
}

bool Events::isCursorLocked() {
    return cursor_locked;
}
