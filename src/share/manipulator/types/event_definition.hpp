#pragma once

#include "event_queue.hpp"
#include <nlohmann/json.hpp>
#include <optional>
#include <pqrs/json.hpp>
#include <variant>

namespace krbn {
namespace manipulator {
class event_definition final {
public:
  enum class type {
    none,
    momentary_switch_event,
    any,
    shell_command,
    select_input_source,
    set_variable,
    set_notification_message,
    mouse_key,
    sticky_modifier,
    software_function,
  };

  enum class any_type {
    key_code,
    consumer_key_code,
    apple_vendor_keyboard_key_code,
    apple_vendor_top_case_key_code,
    pointing_button,
  };

  using value_t = std::variant<momentary_switch_event,                                   // For type::momentary_switch_event
                               any_type,                                                 // For type::any
                               std::string,                                              // For type::shell_command
                               std::vector<pqrs::osx::input_source_selector::specifier>, // For type::select_input_source
                               manipulator_environment_variable_set_variable,            // For type::set_variable
                               notification_message,                                     // For type::set_notification_message
                               mouse_key,                                                // For type::mouse_key
                               std::pair<modifier_flag, sticky_modifier_type>,           // For type::sticky_modifier
                               software_function,                                        // For type::software_function
                               std::monostate>;                                          // For type::none

  event_definition(void) : type_(type::none),
                           value_(std::monostate()) {
  }

  ~event_definition(void) {
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

  std::optional<std::string> get_shell_command(void) const {
    if (type_ == type::shell_command) {
      return std::get<std::string>(value_);
    }
    return std::nullopt;
  }

  std::optional<std::vector<pqrs::osx::input_source_selector::specifier>> get_input_source_specifiers(void) const {
    if (type_ == type::select_input_source) {
      return std::get<std::vector<pqrs::osx::input_source_selector::specifier>>(value_);
    }
    return std::nullopt;
  }

  std::optional<manipulator_environment_variable_set_variable> get_set_variable(void) const {
    if (type_ == type::set_variable) {
      return std::get<manipulator_environment_variable_set_variable>(value_);
    }
    return std::nullopt;
  }

  std::optional<event_queue::event> to_event(void) const {
    switch (type_) {
      case type::none:
        return std::nullopt;
      case type::momentary_switch_event:
        return event_queue::event(std::get<momentary_switch_event>(value_));
      case type::any:
        return std::nullopt;
      case type::shell_command:
        return event_queue::event::make_shell_command_event(std::get<std::string>(value_));
      case type::select_input_source:
        return event_queue::event::make_select_input_source_event(std::get<std::vector<pqrs::osx::input_source_selector::specifier>>(value_));
      case type::set_variable:
        return event_queue::event::make_set_variable_event(std::get<manipulator_environment_variable_set_variable>(value_));
      case type::set_notification_message:
        return event_queue::event::make_set_notification_message_event(std::get<notification_message>(value_));
      case type::mouse_key:
        return event_queue::event::make_mouse_key_event(std::get<mouse_key>(value_));
      case type::sticky_modifier:
        return event_queue::event::make_sticky_modifier_event(std::get<std::pair<modifier_flag, sticky_modifier_type>>(value_));
      case type::software_function:
        return event_queue::event::make_software_function_event(std::get<software_function>(value_));
    }
  }

  bool handle_json(const std::string& key,
                   const nlohmann::json& value,
                   const nlohmann::json& json) {
    // Set type_ and values.

    //
    // momentary_switch_event
    //

    if (key == "key_code" ||
        key == "consumer_key_code" ||
        key == "apple_vendor_keyboard_key_code" ||
        key == "apple_vendor_top_case_key_code" ||
        key == "generic_desktop" ||
        key == "pointing_button") {
      check_type(json);

      try {
        type_ = type::momentary_switch_event;
        value_ = nlohmann::json::object({{key, value}}).get<momentary_switch_event>();
      } catch (const pqrs::json::unmarshal_error& e) {
        throw pqrs::json::unmarshal_error(fmt::format("`{0}` error: {1}", key, e.what()));
      }

      return true;
    }

    //
    // any
    //

    if (key == "any") {
      check_type(json);

      pqrs::json::requires_string(value, "`" + key + "`");

      if (value == "key_code") {
        type_ = type::any;
        value_ = any_type::key_code;
      } else if (value == "consumer_key_code") {
        type_ = type::any;
        value_ = any_type::consumer_key_code;
      } else if (value == "apple_vendor_keyboard_key_code") {
        type_ = type::any;
        value_ = any_type::apple_vendor_keyboard_key_code;
      } else if (value == "apple_vendor_top_case_key_code") {
        type_ = type::any;
        value_ = any_type::apple_vendor_top_case_key_code;
      } else if (value == "pointing_button") {
        type_ = type::any;
        value_ = any_type::pointing_button;
      } else {
        throw pqrs::json::unmarshal_error(fmt::format("unknown `{0}`: `{1}`", key, pqrs::json::dump_for_error_message(value)));
      }

      return true;
    }

    //
    // shell_command
    //

    if (key == "shell_command") {
      check_type(json);

      pqrs::json::requires_string(value, "`" + key + "`");

      type_ = type::shell_command;
      value_ = value.get<std::string>();

      return true;
    }

    //
    // select_input_source
    //

    if (key == "select_input_source") {
      check_type(json);

      std::vector<pqrs::osx::input_source_selector::specifier> input_source_specifiers;

      if (value.is_object()) {
        try {
          input_source_specifiers.push_back(value.get<pqrs::osx::input_source_selector::specifier>());
        } catch (const pqrs::json::unmarshal_error& e) {
          throw pqrs::json::unmarshal_error(fmt::format("`{0}` error: {1}", key, e.what()));
        }
      } else if (value.is_array()) {
        try {
          input_source_specifiers = value.get<std::vector<pqrs::osx::input_source_selector::specifier>>();
        } catch (const pqrs::json::unmarshal_error& e) {
          throw pqrs::json::unmarshal_error(fmt::format("`{0}` error: {1}", key, e.what()));
        }
      } else {
        throw pqrs::json::unmarshal_error(fmt::format("`{0}` must be object or array of objects, but is `{1}`", key, pqrs::json::dump_for_error_message(value)));
      }

      type_ = type::select_input_source;
      value_ = input_source_specifiers;

      return true;
    }

    //
    // set_variable
    //

    if (key == "set_variable") {
      check_type(json);

      manipulator_environment_variable_set_variable v;

      try {
        v = value.get<manipulator_environment_variable_set_variable>();
      } catch (const pqrs::json::unmarshal_error& e) {
        throw pqrs::json::unmarshal_error(fmt::format("`{0}` error: {1}", key, e.what()));
      }

      if (v.get_name() == std::nullopt) {
        throw pqrs::json::unmarshal_error(fmt::format("`{0}.name` is not found in `{1}`", key, pqrs::json::dump_for_error_message(value)));
      }

      if (v.get_type() == manipulator_environment_variable_set_variable::type::set) {
        if (v.get_value() == std::nullopt &&
            !(v.get_expression()) &&
            v.get_key_up_value() == std::nullopt &&
            !(v.get_key_up_expression())) {
          throw pqrs::json::unmarshal_error(fmt::format("none of `{0}.value`, `{0}.expression`, `{0}.key_up_value`, or `{0}.key_up_expression` are not found in `{1}`",
                                                        key,
                                                        pqrs::json::dump_for_error_message(value)));
        }
      }

      type_ = type::set_variable;
      value_ = v;

      return true;
    }

    //
    // set_notification_message
    //

    if (key == "set_notification_message") {
      check_type(json);

      type_ = type::set_notification_message;

      try {
        value_ = value.get<notification_message>();
      } catch (const pqrs::json::unmarshal_error& e) {
        throw pqrs::json::unmarshal_error(fmt::format("`{0}` error: {1}", key, e.what()));
      }

      return true;
    }

    //
    // mouse_key
    //

    if (key == "mouse_key") {
      check_type(json);

      type_ = type::mouse_key;

      try {
        value_ = value.get<mouse_key>();
      } catch (const pqrs::json::unmarshal_error& e) {
        throw pqrs::json::unmarshal_error(fmt::format("`{0}` error: {1}", key, e.what()));
      }

      return true;
    }

    //
    // sticky_modifier
    //

    if (key == "sticky_modifier") {
      check_type(json);

      pqrs::json::requires_object(value, "`" + key + "`");

      type_ = type::sticky_modifier;

      for (const auto& [k, v] : value.items()) {
        // k is always std::string.

        try {
          value_ = std::make_pair(
              nlohmann::json(k).get<modifier_flag>(),
              v.get<sticky_modifier_type>());
        } catch (const pqrs::json::unmarshal_error& e) {
          throw pqrs::json::unmarshal_error(fmt::format("`{0}` error: {1}", key, e.what()));
        }
      }

      return true;
    }

    //
    // software_function
    //

    if (key == "software_function") {
      check_type(json);

      type_ = type::software_function;

      try {
        value_ = value.get<software_function>();
      } catch (const pqrs::json::unmarshal_error& e) {
        throw pqrs::json::unmarshal_error(fmt::format("`{0}` error: {1}", key, e.what()));
      }

      return true;
    }

    //
    // description
    //

    if (key == "description") {
      // Do nothing

      return true;
    }

    return false;
  }

private:
  void check_type(const nlohmann::json& json) const {
    if (type_ != type::none) {
      throw pqrs::json::unmarshal_error(fmt::format("multiple types are specified: `{0}`", pqrs::json::dump_for_error_message(json)));
    }
  }

  type type_;
  value_t value_;
};
} // namespace manipulator
} // namespace krbn
