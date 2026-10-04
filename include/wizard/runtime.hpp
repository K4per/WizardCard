#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <filesystem>
#include <array>
#include <algorithm>
#include <map>
#include <memory>
#include <string>

namespace wizard::runtime {
sf::String utf8(const std::string&);
class Resources {
public:
    explicit Resources(const std::filesystem::path& root);
    const sf::Font& font() const { return font_; }
    const sf::Texture& texture(const std::filesystem::path&);
    void play(const std::filesystem::path&); // Optional cues, missing files do not affect rules.
    void drawText(sf::RenderTarget&,const sf::String&,sf::Vector2f,unsigned int,sf::Color) const;
private:
    sf::Font font_; std::map<std::string,sf::Texture> textures_;
    std::map<std::string,sf::SoundBuffer> sounds_; std::unique_ptr<sf::Sound> voice_;
    mutable std::map<std::pair<unsigned int,std::u32string>,sf::Text> textCache_;
};
struct Theme {
    sf::Color background{14,19,31}, panel{25,33,49}, ink{227,233,240}, muted{147,165,183}, accent{101,217,192}, selected{53,88,102};
};
void texturePatch(sf::RenderTarget&,const sf::Texture&,sf::FloatRect,const std::array<int,4>& insets={},sf::Color tint=sf::Color::White);
void box(sf::RenderTarget&,sf::FloatRect,sf::Color,sf::Color border=sf::Color::Transparent);
void text(sf::RenderTarget&,const Resources&,const std::string&,sf::Vector2f,unsigned int,sf::Color);
void wrapped(sf::RenderTarget&,const Resources&,const std::string&,sf::Vector2f,unsigned int,sf::Color,std::size_t columns);
bool hit(sf::Vector2f,sf::FloatRect);
struct Animation { float remaining{}; void start(){remaining=0.45f;} void update(float dt){remaining=std::max(0.f,remaining-dt);} };
enum class Scene { Handoff, Match, Result };
}
