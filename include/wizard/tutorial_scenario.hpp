#pragma once
#include "wizard/content.hpp"
#include "wizard/core/engine.hpp"

namespace wizard {
// Only a local caller supplies the trusted course directory; no config/replay
// deserializer can manufacture this validated capability from embedded state.
class TutorialScenario {
  public:
    static TutorialScenario load(const std::filesystem::path &trustedDirectory,
                                 const std::string &lessonId, const CardCatalog &);
    const std::string &id() const { return id_; }
    const std::string &digest() const { return digest_; }
    const Json &document() const { return document_; }
    GameEngine start(const CardCatalog &) const;

  private:
    TutorialScenario() = default;
    std::string id_, digest_, contentHash_, rulesVersion_;
    int cardRevision_{};
    Json document_;
    GameState initial_;
};
}
