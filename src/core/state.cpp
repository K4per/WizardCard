#include "wizard/core/rules.hpp"
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace wizard {
const CardDefinition &CardCatalog::at(const std::string &id) const {
    return cards.at(id);
}
void EffectQueue::append(Trigger t) {
    items.push_back(std::move(t));
}
} // namespace wizard
