#pragma once
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace wizard::runtime {
sf::String utf8(const std::string &);
class Resources {
  public:
    explicit Resources(const std::filesystem::path &root);
    const sf::Font &font() const {
        return font_;
    }
    const sf::Texture &texture(const std::filesystem::path &);
    bool play(const std::filesystem::path &, float gain = 1.f, int priority = 0, float cooldown = .09f);
    void stopSounds();
    std::size_t activeSounds() const;
    bool inspectSound(const std::filesystem::path &, float &seconds, unsigned &rate, unsigned &channels);
    void setSoundVolume(float percent);
    void drawText(sf::RenderTarget &, const sf::String &, sf::Vector2f, unsigned int, sf::Color) const;

  private:
    sf::Font font_;
    std::map<std::string, sf::Texture> textures_;
    struct Voice {
        std::unique_ptr<sf::Sound> sound;
        float gain;
        int priority;
    };
    std::map<std::string, sf::SoundBuffer> sounds_;
    std::vector<Voice> voices_;
    std::map<std::string, std::chrono::steady_clock::time_point> lastPlayed_;
    float soundVolume_{64.f};
    mutable std::map<std::pair<unsigned int, std::u32string>, sf::Text> textCache_;
};
struct Theme {
    sf::Color background{14, 19, 31}, panel{25, 33, 49}, ink{227, 233, 240}, muted{147, 165, 183},
        accent{101, 217, 192}, selected{53, 88, 102};
};
void texturePatch(sf::RenderTarget &, const sf::Texture &, sf::FloatRect,
                  const std::array<int, 4> &insets = {}, sf::Color tint = sf::Color::White);
void box(sf::RenderTarget &, sf::FloatRect, sf::Color, sf::Color border = sf::Color::Transparent);
void text(sf::RenderTarget &, const Resources &, const std::string &, sf::Vector2f, unsigned int, sf::Color);
void wrapped(sf::RenderTarget &, const Resources &, const std::string &, sf::Vector2f, unsigned int,
             sf::Color, std::size_t columns);
void fittedText(sf::RenderTarget &, const Resources &, const std::string &, sf::FloatRect, unsigned int size,
                sf::Color, bool centered = false, bool multiline = false);
bool hit(sf::Vector2f, sf::FloatRect);
enum class Scene { Handoff, Match, Result };
} // namespace wizard::runtime
