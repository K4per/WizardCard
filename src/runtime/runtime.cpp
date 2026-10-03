#include "wizard/runtime.hpp"
#include <stdexcept>

namespace wizard::runtime {
sf::String utf8(const std::string& s) { return sf::String::fromUtf8(s.begin(),s.end()); }
Resources::Resources(const std::filesystem::path& root) {
    if(!font_.openFromFile(root/"fonts"/"NotoSansCJKsc-Regular.otf")) throw std::runtime_error("Chinese font missing in assets/fonts");
}
const sf::Texture& Resources::texture(const std::filesystem::path& path) {
    auto key=path.string();auto i=textures_.find(key);if(i!=textures_.end()) return i->second;
    sf::Texture texture;if(!texture.loadFromFile(path)) throw std::runtime_error("texture missing: "+key);
    return textures_.emplace(key,std::move(texture)).first->second;
}
void Resources::play(const std::filesystem::path& path) {
    if(!std::filesystem::exists(path)) return;
    auto key=path.string();auto i=sounds_.find(key);
    if(i==sounds_.end()) {sf::SoundBuffer b;if(!b.loadFromFile(path)) return;i=sounds_.emplace(key,std::move(b)).first;}
    voice_=std::make_unique<sf::Sound>(i->second);voice_->play();
}
void box(sf::RenderTarget& target,sf::FloatRect rect,sf::Color color,sf::Color border) {
    sf::RectangleShape shape(rect.size);shape.setPosition(rect.position);shape.setFillColor(color);shape.setOutlineThickness(1.f);shape.setOutlineColor(border);target.draw(shape);
}
void Resources::drawText(sf::RenderTarget& target,const sf::String& value,sf::Vector2f position,unsigned int size,sf::Color color) const {
    if(textCache_.size()>4096)textCache_.clear();
    auto key=std::make_pair(size,value.toUtf32());auto it=textCache_.find(key);
    if(it==textCache_.end())it=textCache_.emplace(std::move(key),sf::Text(font_,value,size)).first;
    auto& label=it->second;label.setPosition(position);label.setFillColor(color);target.draw(label);
}
void text(sf::RenderTarget& target,const Resources& resources,const std::string& value,sf::Vector2f position,unsigned int size,sf::Color color) {
    resources.drawText(target,utf8(value),position,size,color);
}
void wrapped(sf::RenderTarget& target,const Resources& resources,const std::string& value,sf::Vector2f position,unsigned int size,sf::Color color,std::size_t columns) {
    auto s=utf8(value);sf::String line;float y=position.y;std::size_t count=0;
    for(auto ch:s) {if(ch==U'\n' || count>=columns) {resources.drawText(target,line,{position.x,y},size,color);line.clear();y+=static_cast<float>(size)+7;count=0;if(ch==U'\n')continue;}line+=ch;++count;}
    resources.drawText(target,line,{position.x,y},size,color);
}
bool hit(sf::Vector2f point,sf::FloatRect r) { return r.contains(point); }
}
