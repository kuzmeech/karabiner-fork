#pragma once

#include "connected_devices/connected_devices.hpp"
#include "constants.hpp"
#include "details/global_configuration.hpp"
#include "details/machine_specific.hpp"
#include "details/profile.hpp"
#include "details/profile/complex_modifications.hpp"
#include "details/profile/device.hpp"
#include "details/profile/simple_modifications.hpp"
#include "details/profile/virtual_hid_keyboard.hpp"
#include "json_utility.hpp"
#include "json_writer.hpp"
#include "logger.hpp"
#include "types.hpp"
#include "vector_utility.hpp"
#include <fstream>
#include <glob/glob.hpp>
#include <pqrs/filesystem.hpp>
#include <pqrs/osx/process_info.hpp>
#include <pqrs/osx/session.hpp>
#include <string>
#include <unordered_map>

// Example: tests/src/core_configuration/json/example.json

namespace krbn {
namespace core_configuration {
using namespace std::string_literals;

class core_configuration final {
public:
  core_configuration(const core_configuration&) = delete;

  core_configuration(void)
      : core_configuration("",
                           0,
                           krbn::core_configuration::error_handling::loose) {
  }

  core_configuration(const std::string& file_path,
                     uid_t expected_file_owner,
                     error_handling error_handling)
      : json_(nlohmann::json::object()),
        error_handling_(error_handling),
        loaded_(false),
        global_configuration_(std::make_shared<details::global_configuration>(nlohmann::json::object(),
                                                                              error_handling)),
        machine_specific_(std::make_shared<details::machine_specific>(nlohmann::json::object(),
                                                                      error_handling)) {
    helper_values_.push_back_object<details::global_configuration>("global",
                                                                   global_configuration_);

    helper_values_.push_back_object<details::machine_specific>("machine_specific",
                                                               machine_specific_);

    helper_values_.push_back_array<details::profile>("profiles",
                                                     profiles_);

    bool valid_file_owner = false;

    // Load karabiner.json only when the owner is root or current session user.
    if (pqrs::filesystem::exists(file_path)) {
      if (pqrs::filesystem::is_owned(file_path, 0)) {
        valid_file_owner = true;
      } else {
        if (pqrs::filesystem::is_owned(file_path, expected_file_owner)) {
          valid_file_owner = true;
        }
      }

      if (!valid_file_owner) {
        logger::get_logger()->warn("{0} is not owned by a valid user.", file_path);

      } else {
        std::ifstream input(file_path);
        if (input) {
          try {
            json_ = json_utility::parse_jsonc(input);

            helper_values_.update_value(json_,
                                        error_handling);

            loaded_ = true;

          } catch (std::exception& e) {
            logger::get_logger()->error("parse error in {0}: {1}", file_path, e.what());
            parse_error_message_ = e.what();
          }
        } else {
          logger::get_logger()->error("failed to open {0}", file_path);
        }
      }
    }

    // Fallbacks
    if (profiles_.empty()) {
      profiles_.push_back(std::make_shared<details::profile>(nlohmann::json({
                                                                 {"name", "Default profile"},
                                                                 {"selected", true},
                                                             }),
                                                             error_handling_));
    }
  }

  nlohmann::json to_json(void) const {
    auto j = json_;

    helper_values_.update_json(j);

    return j;
  }

  bool is_loaded(void) const { return loaded_; }

  const std::string& get_parse_error_message(void) const {
    return parse_error_message_;
  }

  const details::global_configuration& get_global_configuration(void) const {
    return *global_configuration_;
  }
  details::global_configuration& get_global_configuration(void) {
    return *global_configuration_;
  }

  const details::machine_specific& get_machine_specific(void) const {
    return *machine_specific_;
  }
  details::machine_specific& get_machine_specific(void) {
    return *machine_specific_;
  }

  const std::vector<pqrs::not_null_shared_ptr_t<details::profile>>& get_profiles(void) const {
    return profiles_;
  }

  void set_profile_name(size_t index, const std::string name) {
    if (index < profiles_.size()) {
      profiles_[index]->set_name(name);
    }
  }

  void select_profile(size_t index) {
    if (index < profiles_.size()) {
      for (size_t i = 0; i < profiles_.size(); ++i) {
        if (i == index) {
          profiles_[i]->set_selected(true);
        } else {
          profiles_[i]->set_selected(false);
        }
      }
    }
  }

  void push_back_profile(void) {
    profiles_.push_back(std::make_shared<details::profile>(nlohmann::json({
                                                               {"name", "New profile"},
                                                           }),
                                                           error_handling_));
  }

  void duplicate_profile(const details::profile& profile) {
    profiles_.push_back(std::make_shared<details::profile>(nlohmann::json(profile),
                                                           error_handling_));

    auto& p = *(profiles_.back());
    p.set_name(p.get_name() + " (copy)");
    p.set_selected(false);
  }

  void erase_profile(size_t index) {
    if (index < profiles_.size()) {
      if (profiles_.size() > 1) {
        profiles_.erase(profiles_.begin() + index);
      }
    }
  }

  void move_profile(size_t source_index, size_t destination_index) {
    vector_utility::move_element(profiles_, source_index, destination_index);
  }

  const details::profile& get_selected_profile(void) const {
    for (auto&& p : profiles_) {
      if (p->get_selected()) {
        return *p;
      }
    }
    return *(profiles_[0]);
  }
  details::profile& get_selected_profile(void) {
    return const_cast<details::profile&>(static_cast<const core_configuration&>(*this).get_selected_profile());
  }

  // Note:
  // Be careful calling `save` method.
  // If the configuration file is corrupted temporarily (user editing the configuration file in editor),
  // the user data will be lost by the `save` method.
  // Thus, we should call the `save` method only when it is neccessary.

  void sync_save_to_file(void) {
    pqrs::osx::process_info::scoped_sudden_termination_blocker sudden_termination_blocker;

    make_backup_file();
    remove_old_backup_files();

    auto file_path = constants::get_user_core_configuration_file_path();
    json_writer::sync_save_to_file(to_json(), file_path, 0700, 0600);
  }

private:
  void make_backup_file(void) {
    auto file_path = constants::get_user_core_configuration_file_path();

    if (!pqrs::filesystem::exists(file_path)) {
      return;
    }

    auto backups_directory = constants::get_user_core_configuration_automatic_backups_directory();
    if (backups_directory.empty()) {
      return;
    }

    pqrs::filesystem::create_directory_with_intermediate_directories(backups_directory, 0700);

    auto backup_file_path = backups_directory /
                            fmt::format("karabiner_{0}.json", make_current_local_yyyymmdd_string());
    if (pqrs::filesystem::exists(backup_file_path)) {
      return;
    }

    pqrs::filesystem::copy(file_path, backup_file_path);
  }

  void remove_old_backup_files(void) {
    auto backups_directory = constants::get_user_core_configuration_automatic_backups_directory();
    if (backups_directory.empty()) {
      return;
    }

    auto pattern = (backups_directory / "karabiner_????????.json").string();
    auto paths = glob::glob(pattern);
    std::sort(std::begin(paths), std::end(paths));

    const int keep_count = 20;
    if (paths.size() > keep_count) {
      for (int i = 0; i < paths.size() - keep_count; ++i) {
        std::error_code error_code;
        std::filesystem::remove(paths[i], error_code);
      }
    }
  }

  static std::string make_current_local_yyyymmdd_string(void) {
    auto t = time(nullptr);

    tm tm;
    localtime_r(&t, &tm);

    return fmt::format("{0:04d}{1:02d}{2:02d}",
                       tm.tm_year + 1900,
                       tm.tm_mon + 1,
                       tm.tm_mday);
  }

  nlohmann::json json_;
  error_handling error_handling_;
  bool loaded_;
  std::string parse_error_message_;

  pqrs::not_null_shared_ptr_t<details::global_configuration> global_configuration_;
  pqrs::not_null_shared_ptr_t<details::machine_specific> machine_specific_;
  std::vector<pqrs::not_null_shared_ptr_t<details::profile>> profiles_;
  configuration_json_helper::helper_values helper_values_;
};
} // namespace core_configuration
} // namespace krbn
