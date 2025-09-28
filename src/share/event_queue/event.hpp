#pragma once

// `krbn::event_queue::event` can be used safely in a multi-threaded environment.

#include "core_configuration/core_configuration.hpp"
#include "device_properties.hpp"
#include "hash.hpp"
#include "manipulator/manipulator_environment.hpp"
#include "types.hpp"
#include <gsl/gsl>
#include <optional>
#include <pqrs/hash.hpp>
#include <pqrs/osx/system_preferences.hpp>
#include <pqrs/osx/system_preferences/extra/nlohmann_json.hpp>
#include <variant>

namespace krbn {
namespace event_queue {
class event {
public:
  enum class type {
    none,
    momentary_switch_event,
    pointing_motion,
    // virtual events
    shell_command,
    select_input_source,
    set_variable,
    set_notification_message,
    mouse_key,
    sticky_modifier,
    software_function,
    stop_keyboard_repeat,
    // virtual events (passive)
    device_keys_and_pointing_buttons_are_released,
    device_grabbed,
    device_ungrabbed,
    caps_lock_state_changed,
    pointing_device_event_from_event_tap,
    frontmost_application_changed,
    input_source_changed,
    system_preferences_properties_changed,
    virtual_hid_devices_state_changed,
  };

  using value_t = std::variant<momentary_switch_event,                                   // For type::momentary_switch_event
                               pointing_motion,                                          // For type::pointing_motion
                               int64_t,                                                  // For type::caps_lock_state_changed
                               std::string,                                              // For shell_command
                               std::vector<pqrs::osx::input_source_selector::specifier>, // For select_input_source
                               manipulator_environment_variable_set_variable,            // For set_variable
                               notification_message,                                     // For set_notification_message
                               mouse_key,                                                // For mouse_key
                               std::pair<modifier_flag, sticky_modifier_type>,           // For sticky_modifier
                               software_function,                                        // For software_function
                               pqrs::osx::frontmost_application_monitor::application,    // For frontmost_application_changed
                               pqrs::osx::input_source::properties,                      // For input_source_changed
                               pqrs::not_null_shared_ptr_t<device_properties>,           // For device_grabbed
                               pqrs::osx::system_preferences::properties,                // For system_preferences_properties_changed
                               virtual_hid_devices_state,                                // For virtual_hid_devices_state_changed
                               std::monostate>;                                          // For virtual events

  event(void) : type_(type::none),
                value_(std::monostate()) {
  }

  static event make_from_json(const nlohmann::json& json) {
    event result;

    try {
      if (json.is_object()) {
        for (const auto& [key, value] : json.items()) {
          if (key == "type") {
            result.type_ = to_type(value.get<std::string>());
          } else if (key == "momentary_switch_event") {
            result.value_ = value.get<momentary_switch_event>();
          } else if (key == "pointing_motion") {
            result.value_ = value.get<pointing_motion>();
          } else if (key == "caps_lock_state_changed") {
            result.value_ = value.get<int64_t>();
          } else if (key == "shell_command") {
            result.value_ = value.get<std::string>();
          } else if (key == "input_source_specifiers") {
            result.value_ = value.get<std::vector<pqrs::osx::input_source_selector::specifier>>();
          } else if (key == "set_variable") {
            result.value_ = value.get<manipulator_environment_variable_set_variable>();
          } else if (key == "set_notification_message") {
            result.value_ = value.get<notification_message>();
          } else if (key == "mouse_key") {
            result.value_ = value.get<mouse_key>();
          } else if (key == "sticky_modifier") {
            result.value_ = value.get<std::pair<modifier_flag, sticky_modifier_type>>();
          } else if (key == "software_function") {
            result.value_ = value.get<software_function>();
          } else if (key == "frontmost_application") {
            result.value_ = value.get<pqrs::osx::frontmost_application_monitor::application>();
          } else if (key == "input_source_properties") {
            result.value_ = value.get<pqrs::osx::input_source::properties>();
          } else if (key == "system_preferences_properties") {
            result.value_ = value.get<pqrs::osx::system_preferences::properties>();
          } else if (key == "virtual_hid_devices_state") {
            result.value_ = value.get<virtual_hid_devices_state>();
          }
        }
      }
    } catch (...) {
    }

    return result;
  }

  nlohmann::json to_json(void) const {
    nlohmann::json json;
    json["type"] = to_c_string(type_);

    switch (type_) {
      case type::none:
        break;

      case type::momentary_switch_event:
        if (auto v = get_if<momentary_switch_event>()) {
          json["momentary_switch_event"] = *v;
        }
        break;

      case type::pointing_motion:
        if (auto v = get_pointing_motion()) {
          json["pointing_motion"] = *v;
        }
        break;

      case type::caps_lock_state_changed:
        if (auto v = get_integer_value()) {
          json["caps_lock_state_changed"] = *v;
        }
        break;

      case type::shell_command:
        if (auto v = get_shell_command()) {
          json["shell_command"] = *v;
        }
        break;

      case type::select_input_source:
        if (auto v = get_input_source_specifiers()) {
          json["input_source_specifiers"] = *v;
        }
        break;

      case type::set_variable:
        if (auto v = get_set_variable()) {
          json["set_variable"] = *v;
        }
        break;

      case type::set_notification_message:
        if (auto v = get_if<notification_message>()) {
          json["set_notification_message"] = *v;
        }
        break;

      case type::mouse_key:
        if (auto v = get_mouse_key()) {
          json["mouse_key"] = *v;
        }
        break;

      case type::sticky_modifier:
        if (auto v = get_sticky_modifier()) {
          json["sticky_modifier"] = *v;
        }
        break;

      case type::software_function:
        if (auto v = get_if<software_function>()) {
          json["software_function"] = *v;
        }
        break;

      case type::frontmost_application_changed:
        if (auto v = get_frontmost_application()) {
          json["frontmost_application"] = *v;
        }
        break;

      case type::input_source_changed:
        if (auto v = get_input_source_properties()) {
          json["input_source_properties"] = *v;
        }
        break;

      case type::system_preferences_properties_changed:
        if (auto v = std::get_if<pqrs::osx::system_preferences::properties>(&value_)) {
          json["system_preferences_properties"] = *v;
        }
        break;

      case type::virtual_hid_devices_state_changed:
        if (auto v = std::get_if<virtual_hid_devices_state>(&value_)) {
          json["virtual_hid_devices_state"] = *v;
        }
        break;

      case type::stop_keyboard_repeat:
      case type::device_keys_and_pointing_buttons_are_released:
      case type::device_grabbed:
      case type::device_ungrabbed:
      case type::pointing_device_event_from_event_tap:
        break;
    }

    return json;
  }

  explicit event(momentary_switch_event momentary_switch_event) : type_(type::momentary_switch_event),
                                                                  value_(momentary_switch_event) {
  }

  explicit event(const pointing_motion& pointing_motion) : type_(type::pointing_motion),
                                                           value_(pointing_motion) {
  }

  static event make_shell_command_event(const std::string& shell_command) {
    event e;
    e.type_ = type::shell_command;
    e.value_ = shell_command;
    return e;
  }

  static event make_select_input_source_event(const std::vector<pqrs::osx::input_source_selector::specifier>& input_source_specifiers) {
    event e;
    e.type_ = type::select_input_source;
    e.value_ = input_source_specifiers;
    return e;
  }

  static event make_set_variable_event(const manipulator_environment_variable_set_variable& value) {
    event e;
    e.type_ = type::set_variable;
    e.value_ = value;
    return e;
  }

  static event make_set_notification_message_event(const notification_message& value) {
    event e;
    e.type_ = type::set_notification_message;
    e.value_ = value;
    return e;
  }

  static event make_mouse_key_event(const mouse_key& mouse_key) {
    event e;
    e.type_ = type::mouse_key;
    e.value_ = mouse_key;
    return e;
  }

  static event make_sticky_modifier_event(const std::pair<modifier_flag, sticky_modifier_type>& value) {
    event e;
    e.type_ = type::sticky_modifier;
    e.value_ = value;
    return e;
  }

  static event make_sticky_modifier_event(modifier_flag modifier_flag, sticky_modifier_type type) {
    return make_sticky_modifier_event(std::make_pair(modifier_flag, type));
  }

  static event make_software_function_event(const software_function& value) {
    event e;
    e.type_ = type::software_function;
    e.value_ = value;
    return e;
  }

  static event make_stop_keyboard_repeat_event(void) {
    return make_virtual_event(type::stop_keyboard_repeat);
  }

  static event make_device_keys_and_pointing_buttons_are_released_event(void) {
    return make_virtual_event(type::device_keys_and_pointing_buttons_are_released);
  }

  static event make_device_grabbed_event(pqrs::not_null_shared_ptr_t<device_properties> device_properties) {
    event e;
    e.type_ = type::device_grabbed;
    e.value_ = device_properties;
    return e;
  }

  static event make_device_ungrabbed_event(void) {
    return make_virtual_event(type::device_ungrabbed);
  }

  static event make_caps_lock_state_changed_event(int64_t state) {
    event e;
    e.type_ = type::caps_lock_state_changed;
    e.value_ = state;
    return e;
  }

  static event make_pointing_device_event_from_event_tap_event(void) {
    return make_virtual_event(type::pointing_device_event_from_event_tap);
  }

  static event make_frontmost_application_changed_event(const pqrs::osx::frontmost_application_monitor::application& application) {
    event e;
    e.type_ = type::frontmost_application_changed;
    e.value_ = application;
    return e;
  }

  static event make_input_source_changed_event(const pqrs::osx::input_source::properties& properties) {
    event e;
    e.type_ = type::input_source_changed;
    e.value_ = properties;
    return e;
  }

  static event make_system_preferences_properties_changed_event(const pqrs::osx::system_preferences::properties& properties) {
    event e;
    e.type_ = type::system_preferences_properties_changed;
    e.value_ = properties;
    return e;
  }

  static event make_virtual_hid_devices_state_changed_event(const virtual_hid_devices_state& virtual_hid_devices_state) {
    event e;
    e.type_ = type::virtual_hid_devices_state_changed;
    e.value_ = virtual_hid_devices_state;
    return e;
  }

  type get_type(void) const {
    return type_;
  }

  const value_t& get_value(void) const {
    return value_;
  }

  template <typename T>
  const T* get_if(void) const {
    return std::get_if<T>(&value_);
  }

  std::optional<pointing_motion> get_pointing_motion(void) const {
    try {
      if (type_ == type::pointing_motion) {
        return std::get<pointing_motion>(value_);
      }
    } catch (std::bad_variant_access&) {
    }
    return std::nullopt;
  }

  std::optional<int64_t> get_integer_value(void) const {
    try {
      if (type_ == type::caps_lock_state_changed) {
        return std::get<int64_t>(value_);
      }
    } catch (std::bad_variant_access&) {
    }
    return std::nullopt;
  }

  std::optional<std::string> get_shell_command(void) const {
    try {
      if (type_ == type::shell_command) {
        return std::get<std::string>(value_);
      }
    } catch (std::bad_variant_access&) {
    }
    return std::nullopt;
  }

  std::optional<std::vector<pqrs::osx::input_source_selector::specifier>> get_input_source_specifiers(void) const {
    try {
      if (type_ == type::select_input_source) {
        return std::get<std::vector<pqrs::osx::input_source_selector::specifier>>(value_);
      }
    } catch (std::bad_variant_access&) {
    }
    return std::nullopt;
  }

  std::optional<manipulator_environment_variable_set_variable> get_set_variable(void) const {
    try {
      if (type_ == type::set_variable) {
        return std::get<manipulator_environment_variable_set_variable>(value_);
      }
    } catch (std::bad_variant_access&) {
    }
    return std::nullopt;
  }

  std::optional<mouse_key> get_mouse_key(void) const {
    try {
      if (type_ == type::mouse_key) {
        return std::get<mouse_key>(value_);
      }
    } catch (std::bad_variant_access&) {
    }
    return std::nullopt;
  }

  std::optional<std::pair<modifier_flag, sticky_modifier_type>> get_sticky_modifier(void) const {
    try {
      if (type_ == type::sticky_modifier) {
        return std::get<std::pair<modifier_flag, sticky_modifier_type>>(value_);
      }
    } catch (std::bad_variant_access&) {
    }
    return std::nullopt;
  }

  std::optional<pqrs::osx::frontmost_application_monitor::application> get_frontmost_application(void) const {
    try {
      if (type_ == type::frontmost_application_changed) {
        return std::get<pqrs::osx::frontmost_application_monitor::application>(value_);
      }
    } catch (std::bad_variant_access&) {
    }
    return std::nullopt;
  }

  std::optional<pqrs::osx::input_source::properties> get_input_source_properties(void) const {
    try {
      if (type_ == type::input_source_changed) {
        return std::get<pqrs::osx::input_source::properties>(value_);
      }
    } catch (std::bad_variant_access&) {
    }
    return std::nullopt;
  }

  bool operator==(const event& other) const {
    return get_type() == other.get_type() &&
           value_ == other.value_;
  }

private:
  static event make_virtual_event(type type) {
    event e;
    e.type_ = type;
    e.value_ = std::monostate();
    return e;
  }

  static const char* to_c_string(type t) {
#define TO_C_STRING(TYPE) \
  case type::TYPE:        \
    return #TYPE;

    switch (t) {
      TO_C_STRING(none);
      TO_C_STRING(momentary_switch_event);
      TO_C_STRING(pointing_motion);
      TO_C_STRING(shell_command);
      TO_C_STRING(select_input_source);
      TO_C_STRING(set_variable);
      TO_C_STRING(set_notification_message);
      TO_C_STRING(mouse_key);
      TO_C_STRING(sticky_modifier);
      TO_C_STRING(software_function);
      TO_C_STRING(stop_keyboard_repeat);
      TO_C_STRING(device_keys_and_pointing_buttons_are_released);
      TO_C_STRING(device_grabbed);
      TO_C_STRING(device_ungrabbed);
      TO_C_STRING(caps_lock_state_changed);
      TO_C_STRING(pointing_device_event_from_event_tap);
      TO_C_STRING(frontmost_application_changed);
      TO_C_STRING(input_source_changed);
      TO_C_STRING(system_preferences_properties_changed);
      TO_C_STRING(virtual_hid_devices_state_changed);
    }

#undef TO_C_STRING

    return nullptr;
  }

  static type to_type(const std::string& t) {
#define TO_TYPE(TYPE)    \
  {                      \
    if (t == #TYPE) {    \
      return type::TYPE; \
    }                    \
  }

    TO_TYPE(momentary_switch_event);
    TO_TYPE(pointing_motion);
    TO_TYPE(shell_command);
    TO_TYPE(select_input_source);
    TO_TYPE(set_variable);
    TO_TYPE(set_notification_message);
    TO_TYPE(mouse_key);
    TO_TYPE(sticky_modifier);
    TO_TYPE(software_function);
    TO_TYPE(stop_keyboard_repeat);
    TO_TYPE(device_keys_and_pointing_buttons_are_released);
    TO_TYPE(device_grabbed);
    TO_TYPE(device_ungrabbed);
    TO_TYPE(caps_lock_state_changed);
    TO_TYPE(pointing_device_event_from_event_tap);
    TO_TYPE(frontmost_application_changed);
    TO_TYPE(input_source_changed);
    TO_TYPE(system_preferences_properties_changed);
    TO_TYPE(virtual_hid_devices_state_changed);

#undef TO_TYPE

    return type::none;
  }

  type type_;
  value_t value_;
};

inline void to_json(nlohmann::json& json, const event& value) {
  json = value.to_json();
}
} // namespace event_queue
} // namespace krbn

namespace std {
template <>
struct hash<krbn::event_queue::event> final {
  std::size_t operator()(const krbn::event_queue::event& value) const {
    std::size_t h = 0;

    pqrs::hash::combine(h, value.get_type());
    pqrs::hash::combine(h, value.get_value());

    return h;
  }
};
} // namespace std
