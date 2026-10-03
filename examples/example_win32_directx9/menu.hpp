#pragma once
#include "gui.hpp"
#include "hashes.hpp"
#include "blur.hpp"
#include "notification.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>

namespace menu {
struct Axis { int mode=0, offset=0; bool jitter=false, avoid=false; };
struct Settings {
    bool enabled=true, silent=true, fire=true, walls=true;
    int fov=180, target=0, hitChance=20, damage=10, history=2;
    std::array<bool,4> hitboxes{true,true,true,false}, multipoint{true,true,true,false};
    bool quickStop=true, quickScope=true, delay=true, recoil=true, spread=false;
    bool duck=false, peek=false, doubleTap=false, anti=true;
    Axis pitch, yaw, freestanding, mouse;
};
struct Preset { int id; std::string name; Settings settings; };
inline Settings settings;
inline std::vector<Preset> presets{{1,"112255",{}},{2,"legit cfg",{}},{3,"hvh master",{}}};
inline int selectedPreset=1, nextPreset=4, weapon=0;
inline int page=0;
inline bool visualsExpanded=false;
inline char search[96]{}, presetSearch[96]{}, rename[64]{};
inline int renameId=-1;
inline float s=1;
inline ImVec2 origin;
inline ImFont* body=nullptr;
inline ImFont* bold=nullptr;
inline ImFont* smallFont=nullptr;

inline float Ease(float value,float target,float speed=15) {
    return value+(target-value)*(-std::expm1(-speed*ImGui::GetIO().DeltaTime));
}
inline float Animate(ImGuiID id,bool target) {
    auto* storage=ImGui::GetStateStorage();
    const ImGuiID key=ImHashStr("##motion",0,id);
    const float a=Ease(storage->GetFloat(key,target?1.f:0.f),target?1.f:0.f);
    storage->SetFloat(key,a); return a;
}
inline ImU32 Color(color_t c,float alpha=1) { return c.to_im_color(alpha); }
inline ImVec2 V(float x,float y) { return ImVec2(x*s,y*s); }
inline void Text(ImVec2 pos,const char* text,ImU32 col,ImFont* font=nullptr,float size=14) {
    ImGui::GetWindowDrawList()->AddText(font?font:body,size*s,pos,col,text,ImGui::FindRenderedTextEnd(text));
}
inline void Chevron(ImVec2 p,ImU32 col,bool down=false) {
    auto* d=ImGui::GetWindowDrawList();
    if(down) { d->AddLine(p+V(-3,-1),p+V(0,2),col,s);d->AddLine(p+V(0,2),p+V(3,-1),col,s); }
    else { d->AddLine(p+V(-2,-4),p+V(2,0),col,s);d->AddLine(p+V(2,0),p+V(-2,4),col,s); }
}
inline bool ButtonAt(const char* id,ImVec2 p,ImVec2 size) {
    ImGui::SetCursorScreenPos(p);
    return ImGui::InvisibleButton(id,size);
}
inline void Dots(ImVec2 p,ImU32 col) {
    auto* d=ImGui::GetWindowDrawList();
    for(int i=-1;i<=1;++i)d->AddCircleFilled(p+V(i*3.f,0),s,col);
}
inline bool IconButton(const char* id,const char* icon,ImVec2 p,float width=26) {
    const bool hit=ButtonAt(id,p,V(width,26));
    const bool hover=ImGui::IsItemHovered();
    if(hover)ImGui::GetWindowDrawList()->AddRectFilled(p,p+V(width,26),Color(gui.tab_active),5*s);
    const ImVec2 sz=body->CalcTextSizeA(13*s,FLT_MAX,0,icon);
    Text(p+ImVec2((width*s-sz.x)/2,(26*s-sz.y)/2),icon,Color(hover?gui.text:gui.text_disabled),body,13);
    return hit;
}
inline bool Popup(const char* id,ImVec2 anchor,float width,float height,const char* title) {
    if(!ImGui::IsPopupOpen(id))return false;
    const auto display=ImGui::GetIO().DisplaySize;
    ImVec2 pos=anchor;
    pos.x=ImClamp(pos.x,8*s,ImMax(8*s,display.x-width*s-8*s));
    pos.y=ImClamp(pos.y,8*s,ImMax(8*s,display.y-height*s-8*s));
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(V(width,height));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,V(12,10));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding,10*s);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize,s);
    ImGui::PushStyleColor(ImGuiCol_PopupBg,gui.group_box_bg.to_vec4(1,false));
    ImGui::PushStyleColor(ImGuiCol_Border,gui.border.to_vec4(1,false));
    bool open=ImGui::BeginPopup(id,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor(2);ImGui::PopStyleVar(3);
    if(open) {
        auto* st=ImGui::GetStateStorage();
        float a=ImGui::IsWindowAppearing()?0:st->GetFloat(0xf003,0);
        a=Ease(a,1,22);st->SetFloat(0xf003,a);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*a);
        if(title&&*title) {
            auto p=ImGui::GetCursorScreenPos();
            Text(p,title,Color(gui.text),bold,14);
            ImGui::GetWindowDrawList()->AddLine(p+V(0,27),p+V(width-24,27),Color(gui.border),s);
            ImGui::SetCursorScreenPos(p+V(0,34));
        }
    }
    return open;
}
inline void EndPopup(){ImGui::PopStyleVar();ImGui::EndPopup();}
inline bool Option(const char* label,bool selected,float width) {
    auto p=ImGui::GetCursorScreenPos();
    bool hit=ButtonAt(label,p,V(width,30));
    bool hover=ImGui::IsItemHovered();
    if(hover||selected)ImGui::GetWindowDrawList()->AddRectFilled(p,p+V(width,30),Color(gui.tab_active,selected?0.8f:0.45f),5*s);
    Text(p+V(8,7),label,Color(selected?gui.text:gui.text_disabled),body,13);
    if(selected)Text(p+V(width-22,8),ICON_FA_CHECK,Color(gui.accent_color),body,11);
    ImGui::SetCursorScreenPos(p+V(0,32));return hit;
}
inline void Field(const char* id,char* value,size_t capacity,float width,const char* hint) {
    const auto p=ImGui::GetCursorScreenPos();
    const ImVec2 z=V(width,29);
    ImGui::GetWindowDrawList()->AddRectFilled(p,p+z,Color(gui.frame_inactive),5*s);
    ImGui::GetWindowDrawList()->AddRect(p,p+z,Color(gui.border),5*s);
    // Isolate the field because this fork's InputText uses its window's right edge.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,V(4,2));
    ImGui::BeginChild(id,z,false,ImGuiWindowFlags_NoScrollbar);
    ImGui::SetNextItemWidth((width-8)*s);
    ImGui::InputTextWithHint("##input",hint,value,capacity);
    ImGui::EndChild();ImGui::PopStyleVar();
    ImGui::SetCursorScreenPos(p+V(0,36));
}
inline bool Matches(const char* label,const char* query) {
    if(!*query)return true;
    std::string a=label,b=query;
    for(auto& c:a)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for(auto& c:b)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return a.find(b)!=std::string::npos;
}
struct Card {
    ImVec2 pos;float width;int row=0,count;
    Card(const char* title,float x,float y,float w,int rows):pos(origin+V(x,y+20)),width(w),count(rows) {
        ImGui::PushID(title);
        Text(origin+V(x+11,y),title,Color(gui.text_disabled,0.78f),smallFont,10);
        AddSquircleFilled(ImGui::GetWindowDrawList(),pos,pos+V(w,rows*38.f),Color(gui.group_box_bg),9*s);
        AddSquircle(ImGui::GetWindowDrawList(),pos,pos+V(w,rows*38.f),Color(gui.border),9*s,s);
    }
    ~Card(){ImGui::PopID();}
    ImVec2 Next(const char* label) {
        auto p=pos+V(12,row*38.f);
        if(row>0)ImGui::GetWindowDrawList()->AddLine(p,p+V(width-24,0),Color(gui.border,0.65f),s);
        ++row;
        if(*search&&!Matches(label,search))ImGui::PushStyleVar(ImGuiStyleVar_Alpha,0.25f);
        return p;
    }
    void Finish(const char* label){if(*search&&!Matches(label,search))ImGui::PopStyleVar();}
};
inline void RowLabel(ImVec2 p,const char* label){Text(p+V(0,11),label,Color(gui.text),body,14);}
inline void ToggleAt(const char* label,bool& value,ImVec2 p,float width,bool dots=false) {
    ImGui::PushID(label);
    const float toggleX=width-34;
    const bool pressed=ButtonAt("##toggle",p,V(width,38));
    const ImGuiID id=ImGui::GetItemID();
    if(pressed)value=!value;
    const float t=Animate(id,value);
    const bool hover=ImGui::IsItemHovered();
    if(hover)ImGui::GetWindowDrawList()->AddRectFilled(p+V(-5,2),p+V(width+5,36),Color(gui.tab_active,0.18f),4*s);
    RowLabel(p,label);
    const ImVec2 sp=p+V(toggleX,9.5f);
    const ImVec4 off(0.020f,0.035f,0.055f,1);
    const ImU32 bg=ImGui::GetColorU32(ImLerp(off,gui.accent_color.to_vec4(1,false),t));
    ImGui::GetWindowDrawList()->AddRectFilled(sp,sp+V(34,19),bg,9.5f*s);
    const ImVec4 knob=ImLerp(ImVec4(0.498f,0.529f,0.557f,1),ImVec4(1,1,1,1),t);
    ImGui::GetWindowDrawList()->AddCircleFilled(sp+V(9.5f+15*t,9.5f),7.5f*s,ImGui::GetColorU32(knob));
    if(dots) {
        Dots(p+V(width-52,19),Color(gui.text_disabled));
        if(ButtonAt("##details",p+V(width-64,7),V(24,24)))ImGui::OpenPopup("Details");
        if(Popup("Details",p+V(width+18,0),220,116,label)) {
            const auto q=ImGui::GetCursorScreenPos();
            ToggleAt("Enabled",value,q,196);
            Text(q+V(0,45),"Click the switch to change this setting.",Color(gui.text_disabled),body,11);
            EndPopup();
        }
    }
    ImGui::PopID();
}
inline void Toggle(Card& card,const char* label,bool& value,bool dots=false) {
    auto p=card.Next(label);ToggleAt(label,value,p,card.width-24,dots);card.Finish(label);
}
inline void SliderAt(const char* label,int& value,int lo,int hi,ImVec2 p,float width,const char* suffix="",bool dots=false) {
    ImGui::PushID(label);RowLabel(p,label);
    auto* window=ImGui::GetCurrentWindow();
    const float trackWidth=width>245?90.f:65.f;
    const float trackX=width-trackWidth-50;
    const ImRect box(p+V(trackX,7),p+V(width-50,31));
    ImGui::SetCursorScreenPos(box.Min);
    const ImGuiID id=window->GetID("##slider");
    ImGui::ItemSize(box);bool visible=ImGui::ItemAdd(box,id);
    if(visible) {
        bool hovered=false,held=false;
        ImGui::ButtonBehavior(box,id,&hovered,&held);
        if(held&&ImGui::IsMouseDown(0))value=lo+static_cast<int>(std::round(ImSaturate((ImGui::GetIO().MousePos.x-box.Min.x)/box.GetWidth())*(hi-lo)));
        if(GImGui->NavId==id) {
            if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow))value=ImMax(lo,value-1);
            if(ImGui::IsKeyPressed(ImGuiKey_RightArrow))value=ImMin(hi,value+1);
        }
        ImGui::RenderNavHighlight(box,id);
        const float fraction=static_cast<float>(value-lo)/ImMax(1,hi-lo);
        auto* d=window->DrawList;
        d->AddRectFilled(p+V(trackX,17.5f),p+V(width-50,21),Color(gui.frame_inactive),2*s);
        d->AddRectFilled(p+V(trackX,17.5f),p+V(trackX+trackWidth*fraction,21),Color(gui.accent_color),2*s);
        d->AddCircleFilled(p+V(trackX+trackWidth*fraction,19),5.5f*s,ImGui::GetColorU32(ImVec4(1,1,1,1)));
    }
    const auto q=p+V(width-39,8);
    ImGui::GetWindowDrawList()->AddRectFilled(q,q+V(39,23),Color(gui.frame_inactive),5*s);
    ImGui::GetWindowDrawList()->AddRect(q,q+V(39,23),Color(gui.border,0.65f),5*s);
    char text[32];std::snprintf(text,sizeof(text),"%d%s",value,suffix);
    auto sz=body->CalcTextSizeA(11*s,FLT_MAX,0,text);
    Text(q+ImVec2((39*s-sz.x)/2,5*s),text,Color(gui.text),body,11);
    if(dots)Dots(p+V(trackX-15,19),Color(gui.text_disabled));
    ImGui::PopID();
}
inline void Slider(Card& card,const char* label,int& value,int lo,int hi,const char* suffix="",bool dots=false) {
    auto p=card.Next(label);SliderAt(label,value,lo,hi,p,card.width-24,suffix,dots);card.Finish(label);
}
inline void ComboAt(const char* label,int& current,const char* const* items,int count,ImVec2 p,float width) {
    ImGui::PushID(label);RowLabel(p,label);
    const float cw=ImMin(138.f,width*0.52f);const auto q=p+V(width-cw,7);
    if(ButtonAt("##combo",q,V(cw,24)))ImGui::OpenPopup("Options");
    auto* d=ImGui::GetWindowDrawList();
    d->AddRectFilled(q,q+V(cw,24),Color(gui.frame_inactive),5*s);
    d->AddRect(q,q+V(cw,24),Color(gui.border,0.65f),5*s);
    const ImVec4 clip(q.x+7*s,q.y,q.x+(cw-22)*s,q.y+24*s);
    current=ImClamp(current,0,count-1);
    d->AddText(body,13*s,q+V(7,5),Color(gui.text,0.85f),items[current],nullptr,0,&clip);
    Chevron(q+V(cw-12,12),Color(gui.text_disabled),true);
    if(Popup("Options",q+V(0,29),ImMax(160.f,cw),count*32.f+20,nullptr)) {
        for(int i=0;i<count;++i)if(Option(items[i],current==i,ImMax(160.f,cw)-24)){current=i;ImGui::CloseCurrentPopup();}
        EndPopup();
    }
    ImGui::PopID();
}
inline void Combo(Card& card,const char* label,int& current,const char* const* items,int count) {
    auto p=card.Next(label);ComboAt(label,current,items,count,p,card.width-24);card.Finish(label);
}
inline void Multi(Card& card,const char* label,std::array<bool,4>& values,bool dots=false) {
    auto p=card.Next(label);ImGui::PushID(label);RowLabel(p,label);
    const float width=card.width-24,cw=138;const auto q=p+V(width-cw,7);
    static constexpr const char* items[]{"Head","Chest","Stomach","Legs"};
    std::string preview;
    for(int i=0;i<4;++i)if(values[i]){if(!preview.empty())preview+=", ";preview+=items[i];}
    if(preview.empty())preview="Select";
    if(ButtonAt("##multi",q,V(cw,24)))ImGui::OpenPopup("Options");
    auto* d=ImGui::GetWindowDrawList();d->AddRectFilled(q,q+V(cw,24),Color(gui.frame_inactive),5*s);
    d->AddRect(q,q+V(cw,24),Color(gui.border,0.65f),5*s);
    const ImVec4 clip(q.x+7*s,q.y,q.x+(cw-22)*s,q.y+24*s);
    d->AddText(body,13*s,q+V(7,5),Color(gui.text,0.85f),preview.c_str(),nullptr,0,&clip);
    Chevron(q+V(cw-12,12),Color(gui.text_disabled),true);
    if(dots)Dots(q+V(-15,12),Color(gui.text_disabled));
    if(Popup("Options",q+V(0,29),180,148,nullptr)) {
        for(int i=0;i<4;++i)if(Option(items[i],values[i],156))values[i]=!values[i];
        EndPopup();
    }
    ImGui::PopID();card.Finish(label);
}
inline void AxisRow(Card& card,const char* label,Axis& axis) {
    auto p=card.Next(label);ImGui::PushID(label);
    const float width=card.width-24;
    if(ButtonAt("##open",p,V(width,38)))ImGui::OpenPopup("Axis");
    const float a=Animate(ImGui::GetItemID(),ImGui::IsItemHovered()||ImGui::IsPopupOpen("Axis"));
    if(a>0.01f)ImGui::GetWindowDrawList()->AddRectFilled(p+V(-5,2),p+V(width+5,36),Color(gui.tab_active,a*0.3f),5*s);
    RowLabel(p,label);Chevron(p+V(width-4,19),Color(gui.text_disabled));
    ImVec2 anchor=p+V(width+20,-8);
    if(anchor.x+260*s>ImGui::GetIO().DisplaySize.x)anchor=p-V(276,8);
    if(Popup("Axis",anchor,260,212,label)) {
        auto q=ImGui::GetCursorScreenPos();
        static constexpr const char* modes[]{"Off","Down","Up","At Targets","Center","Offset"};
        ComboAt(label,axis.mode,modes,6,q,236);
        ToggleAt("Avoid Backstab",axis.avoid,q+V(0,38),236);
        SliderAt("Offset",axis.offset,-180,180,q+V(0,76),236);
        ToggleAt("Jitter",axis.jitter,q+V(0,114),236);
        EndPopup();
    }
    ImGui::PopID();card.Finish(label);
}
inline void Rage() {
    static constexpr const char* targets[]{"Highest Damage","Hit Chance","Distance","Field of View"};
    static constexpr const char* histories[]{"Low","Medium","Maximum"};
    {Card c("MAIN",186,78,296,5);
        Toggle(c,"Enabled",settings.enabled,true);Toggle(c,"Silent Aim",settings.silent,true);
        Toggle(c,"Automatic Fire",settings.fire);Toggle(c,"Aim Through Walls",settings.walls);
        Slider(c,"Field of View",settings.fov,0,180,".0\xc2\xb0");}
    {Card c("SELECTION",186,310,296,7);
        Combo(c,"Target",settings.target,targets,4);Multi(c,"Hitboxes",settings.hitboxes);
        Multi(c,"Multipoint",settings.multipoint,true);Slider(c,"Hit Chance",settings.hitChance,0,100,"%",true);
        Slider(c,"Min Damage",settings.damage,0,100,"",true);Toggle(c,"Quick Stop",settings.quickStop,true);Toggle(c,"Quick Scope",settings.quickScope);}
    {Card c("OTHER",496,78,288,7);
        Combo(c,"History",settings.history,histories,3);Toggle(c,"Delay Shot",settings.delay);
        Toggle(c,"Remove Recoil",settings.recoil);Toggle(c,"Remove Spread",settings.spread);
        Toggle(c,"Duck Peek Assist",settings.duck);Toggle(c,"Quick Peek Assist",settings.peek,true);Toggle(c,"Double Tap",settings.doubleTap);}
    {Card c("ANTI-AIM",496,386,288,5);
        Toggle(c,"Enabled",settings.anti,true);AxisRow(c,"Pitch",settings.pitch);AxisRow(c,"Yaw",settings.yaw);
        AxisRow(c,"Freestanding",settings.freestanding);AxisRow(c,"Mouse Override",settings.mouse);}
}
inline bool Nav(const char* icon,const char* label,int id,float y,float indent=0) {
    auto p=origin+V(12+indent,y);const float width=146-indent;
    const bool hit=ButtonAt(label,p,V(width,31));
    const bool selected=page==id;
    const float a=Animate(ImGui::GetItemID(),selected||ImGui::IsItemHovered());
    if(a>0.01f)ImGui::GetWindowDrawList()->AddRectFilled(p,p+V(width,31),Color(gui.tab_active,a*(selected?1.f:0.4f)),6*s);
    Text(p+V(10,9),icon,Color(selected?gui.accent_color:gui.text_disabled),body,12);
    Text(p+V(32,8),label,Color(selected?gui.text:gui.text_disabled),body,14);
    if(hit)page=id;
    return hit;
}
inline Preset* Selected(){for(auto& p:presets)if(p.id==selectedPreset)return &p;return nullptr;}
inline bool SavePreset(const Preset& p) {
    std::ofstream out("preset_"+std::to_string(p.id)+".ini",std::ios::trunc);
    if(!out)return false;
    out<<p.name<<'\n';const auto& a=p.settings;
    out<<a.enabled<<' '<<a.silent<<' '<<a.fire<<' '<<a.walls<<' '<<a.fov<<' '<<a.target<<' '<<a.hitChance<<' '<<a.damage<<' '<<a.history<<'\n';
    for(bool v:a.hitboxes)out<<v<<' ';for(bool v:a.multipoint)out<<v<<' ';
    out<<a.quickStop<<' '<<a.quickScope<<' '<<a.delay<<' '<<a.recoil<<' '<<a.spread<<' '<<a.duck<<' '<<a.peek<<' '<<a.doubleTap<<' '<<a.anti<<'\n';
    for(const auto* v:{&a.pitch,&a.yaw,&a.freestanding,&a.mouse})out<<v->mode<<' '<<v->offset<<' '<<v->jitter<<' '<<v->avoid<<'\n';
    return static_cast<bool>(out);
}
inline void Presets(ImVec2 anchor) {
    if(!Popup("Presets",anchor,350,300,"Presets"))return;
    auto p=ImGui::GetWindowPos();
    if(IconButton("##add",ICON_FA_PLUS,p+V(312,6)))presets.push_back({nextPreset++,"new config",settings});
    Field("##preset_search",presetSearch,sizeof(presetSearch),326,"Search presets");
    ImGui::BeginChild("##preset_list",V(326,184),false,ImGuiWindowFlags_NoBackground);
    for(auto& preset:presets) {
        if(!Matches(preset.name.c_str(),presetSearch))continue;
        ImGui::PushID(preset.id);
        auto q=ImGui::GetCursorScreenPos();const bool selected=preset.id==selectedPreset;
        if(ButtonAt("##load",q,V(258,34))){selectedPreset=preset.id;settings=preset.settings;}
        if(selected||ImGui::IsItemHovered())ImGui::GetWindowDrawList()->AddRectFilled(q,q+V(326,34),Color(gui.frame_inactive),5*s);
        if(selected)ImGui::GetWindowDrawList()->AddRect(q,q+V(326,34),Color(gui.accent_color,0.8f),5*s);
        Text(q+V(10,10),preset.name.c_str(),Color(selected?gui.text:gui.text_disabled),body,13);
        if(IconButton("##rename",ICON_FA_ELLIPSIS_H,q+V(264,4))) {
            renameId=preset.id;strncpy_s(rename,preset.name.c_str(),_TRUNCATE);ImGui::OpenPopup("Rename");
        }
        if(IconButton("##save",ICON_FA_SAVE,q+V(294,4))) {
            preset.settings=settings;
            bool ok=SavePreset(preset);PushNotification(ok?"Config Saved":"Save Failed",preset.name);
        }
        if(Popup("Rename",q+V(332,0),250,130,"Rename preset")) {
            Field("##name",rename,sizeof(rename),226,"Name");
            if(Option("Apply",false,226)&&*rename){preset.name=rename;ImGui::CloseCurrentPopup();}
            EndPopup();
        }
        ImGui::SetCursorScreenPos(q+V(0,40));ImGui::PopID();
    }
    ImGui::EndChild();EndPopup();
}
inline void Secondary() {
    static std::array<bool,20> values{};static int mode=0,amount=30;
    static constexpr const char* styles[]{"Default","Minimal","Detailed"};
    const char* titles[]{"Rage","Legit","Visuals","Players","World","Inventory","Miscellaneous","Helper"};
    {Card c("GENERAL",186,78,296,5);Toggle(c,"Enabled",values[page],true);
        Combo(c,"Style",mode,styles,3);Slider(c,"Amount",amount,0,100,"%");
        Toggle(c,"Show indicators",values[10]);Toggle(c,"Show preview",values[11]);}
    {Card c("SETTINGS",496,78,288,3);Toggle(c,"Notifications",values[12]);
        Toggle(c,"Compact layout",values[13]);Toggle(c,"Remember selection",values[14]);}
    Text(origin+V(198,330),titles[page],Color(gui.text),bold,17);
    Text(origin+V(198,360),"Interface settings",Color(gui.text_disabled),body,13);
}
inline void Draw(IDirect3DTexture9* avatar,IDirect3DTexture9* background) {
    s=gui.m_scale;
    auto& io=ImGui::GetIO();
    body=io.Fonts->Fonts[0];bold=io.Fonts->Fonts[4];smallFont=io.Fonts->Fonts[6];
    if(background)ImGui::GetBackgroundDrawList()->AddImage(background,ImVec2(0,0),io.DisplaySize);
    ImGui::SetNextWindowSize(V(800,620));
    ImGui::SetNextWindowPos((io.DisplaySize-V(800,620))*0.5f,ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,16*s);
    const bool visible=ImGui::Begin("Neverlose##reference",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoScrollbar);
    if(visible) {
        origin=ImGui::GetWindowPos();auto* d=ImGui::GetWindowDrawList();
        d->PushClipRect(origin,origin+V(800,620),true);draw_blur(d);d->PopClipRect();
        // Original surface colors and accent are preserved.
        d->AddRectFilled(origin,origin+V(170,620),ImColor(0.071f,0.078f,0.114f,0.80f),16*s,ImDrawFlags_RoundCornersLeft);
        d->AddRectFilled(origin+V(170,0),origin+V(800,60),ImColor(0.043f,0.059f,0.090f,0.95f),16*s,ImDrawFlags_RoundCornersTopRight);
        d->AddRectFilled(origin+V(170,60),origin+V(800,620),ImColor(0.043f,0.055f,0.086f,0.95f),16*s,ImDrawFlags_RoundCornersBottomRight);
        d->AddLine(origin+V(170,0),origin+V(170,620),Color(gui.border,0.55f),s);
        d->AddLine(origin+V(180,60),origin+V(784,60),Color(gui.border,0.6f),s);
        d->AddRect(origin,origin+V(800,620),Color(gui.border,0.55f),16*s,0,s);
        auto logo=origin+V(20,18);d->AddRectFilled(logo,logo+V(32,32),Color(gui.frame_active),7*s);
        Text(logo+V(5,7),"NL",Color(gui.accent_color),bold,18);
        Text(origin+V(62,19),"Neverlose",Color(gui.text),bold,14);
        Text(origin+V(62,38),"Counter-Strike 2",Color(gui.text_disabled),body,9);
        d->AddLine(origin+V(16,64),origin+V(154,64),Color(gui.border,0.55f),s);
        Text(origin+V(22,79),"AIMBOT",Color(gui.text_disabled,0.8f),smallFont,10);
        Nav(ICON_FA_CROSSHAIRS,"Rage",0,96);Nav(ICON_FA_MOUSE,"Legit",1,132);
        Text(origin+V(22,180),"COMMON",Color(gui.text_disabled,0.8f),smallFont,10);
        if(Nav(ICON_FA_IMAGE,"Visuals",2,197))visualsExpanded=!visualsExpanded;
        float extra=0;
        if(visualsExpanded){Nav(ICON_FA_USER,"Players",3,232,10);Nav(ICON_FA_GLOBE,"World",4,267,10);extra=70;}
        Nav(ICON_FA_LAYER_GROUP,"Inventory",5,233+extra);Nav(ICON_FA_SLIDERS_H,"Miscellaneous",6,269+extra);
        Text(origin+V(22,320+extra),"EXTENSIONS",Color(gui.text_disabled,0.8f),smallFont,10);
        Nav(ICON_FA_BOMB,"Helper",7,337+extra);
        const auto user=origin+V(14,564);
        d->AddLine(user,user+V(142,0),Color(gui.border,0.6f),s);
        if(avatar)d->AddImageRounded(avatar,user+V(6,12),user+V(38,44),ImVec2(0,0),ImVec2(1,1),IM_COL32_WHITE,16*s);
        Text(user+V(48,14),"kdt255",Color(gui.text),body,14);
        Text(user+V(48,32),"Lifetime",Color(gui.text_disabled),body,11);
        Chevron(user+V(140,29),Color(gui.text_disabled));
        if(ButtonAt("##profile",user,V(142,52)))ImGui::OpenPopup("Profile");
        if(Popup("Profile",user+V(156,-145),240,190,"Profile")) {
            auto p=ImGui::GetCursorScreenPos();
            Text(p,"kdt255",Color(gui.text),bold,15);Text(p+V(0,24),"Subscription: Lifetime",Color(gui.text_disabled),body,12);
            static int scale=1;static constexpr const char* scales[]{"75%","100%","125%","150%","175%","200%"};
            ComboAt("Menu scale",scale,scales,6,p+V(0,52),216);
            static constexpr float scaleValues[]{0.75f,1,1.25f,1.5f,1.75f,2};gui.m_scale=scaleValues[scale];
            EndPopup();
        }
        const auto top=origin+V(188,19);
        d->AddRectFilled(top,top+V(148,28),Color(gui.frame_inactive,0.65f),5*s);
        d->AddRect(top,top+V(148,28),Color(gui.border,0.7f),5*s);
        if(IconButton("##save",ICON_FA_SAVE,top,32))if(auto* p=Selected()) {
            p->settings=settings;bool ok=SavePreset(*p);PushNotification(ok?"Config Saved":"Save Failed",p->name);
        }
        d->AddLine(top+V(33,4),top+V(33,24),Color(gui.border),s);
        if(ButtonAt("##presets",top+V(35,0),V(113,28)))ImGui::OpenPopup("Presets");
        Text(top+V(44,7),Selected()?Selected()->name.c_str():"No preset",Color(gui.text),body,13);
        Chevron(top+V(134,14),Color(gui.text_disabled),true);Presets(top+V(0,35));
        const auto wp=top+V(162,0);
        d->AddRect(wp,wp+V(86,28),Color(gui.border,0.8f),5*s);
        static constexpr const char* weapons[]{"Global","Autosnipers","SSG-08","AWP","Pistols","Rifles"};
        if(ButtonAt("##weapons",wp,V(86,28)))ImGui::OpenPopup("Weapons");
        const ImVec4 clip(wp.x,wp.y,wp.x+66*s,wp.y+28*s);
        d->AddText(body,13*s,wp+V(11,7),Color(gui.text),weapons[weapon],nullptr,0,&clip);
        Chevron(wp+V(75,14),Color(gui.text_disabled),true);
        if(Popup("Weapons",wp+V(0,35),240,256,"Weapon Presets")) {
            for(int i=0;i<6;++i)if(Option(weapons[i],weapon==i,216)){weapon=i;ImGui::CloseCurrentPopup();}
            EndPopup();
        }
        if(IconButton("##search",ICON_FA_SEARCH,origin+V(751,17)))ImGui::OpenPopup("Search");
        if(Popup("Search",origin+V(514,55),268,108,"Find a setting")) {
            Field("##search_text",search,sizeof(search),244,"Search settings");EndPopup();
        }
        if(page==0)Rage();else Secondary();
    }
    ImGui::End();ImGui::PopStyleVar(2);
}
}
