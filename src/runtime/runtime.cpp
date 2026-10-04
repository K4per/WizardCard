#include "wizard/runtime.hpp"
#include <stdexcept>
#include <cmath>

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
void texturePatch(sf::RenderTarget& target,const sf::Texture& texture,sf::FloatRect rect,const std::array<int,4>& insets,sf::Color tint) {
    const auto size=texture.getSize();float sw=static_cast<float>(size.x),sh=static_cast<float>(size.y);
    if(rect.size.x<=0 || rect.size.y<=0)return;
    float l=static_cast<float>(insets[0]),u=static_cast<float>(insets[1]),r=static_cast<float>(insets[2]),b=static_cast<float>(insets[3]);
    if(l+r>=sw){float scale=(sw-1)/(l+r);l=std::floor(l*scale);r=std::floor(r*scale);}
    if(u+b>=sh){float scale=(sh-1)/(u+b);u=std::floor(u*scale);b=std::floor(b*scale);}
    float sx[4]={0,l,sw-r,sw},sy[4]={0,u,sh-b,sh};
    float kx=l+r>0?std::min(1.f,rect.size.x/(l+r)):1,ky=u+b>0?std::min(1.f,rect.size.y/(u+b)):1;
    float dx[4]={0,l*kx,rect.size.x-r*kx,rect.size.x},dy[4]={0,u*ky,rect.size.y-b*ky,rect.size.y};
    for(int y=0;y<3;++y)for(int x=0;x<3;++x) {
        float w=sx[x+1]-sx[x],h=sy[y+1]-sy[y];if(w<=0 || h<=0)continue;
        sf::Sprite sprite(texture,sf::IntRect({static_cast<int>(sx[x]),static_cast<int>(sy[y])},{static_cast<int>(w),static_cast<int>(h)}));
        sprite.setPosition(rect.position+sf::Vector2f{dx[x],dy[y]});sprite.setScale({(dx[x+1]-dx[x])/w,(dy[y+1]-dy[y])/h});sprite.setColor(tint);target.draw(sprite);
    }
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
