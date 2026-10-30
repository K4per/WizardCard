#pragma once
#include "wizard/core/commands.hpp"
#include <nlohmann/json.hpp>
namespace wizard {
using Json=nlohmann::json;
Json encodeCommand(const Command&);
Command decodeCommand(const Json&);
Json encodeChoice(const ChoiceValue &);
ChoiceValue decodeChoice(const Json &);
Command decodeLegacyCommand(const Json &);
}
