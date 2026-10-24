// Geode 5.x / Geometry Dash Android 2.2081 proof-of-concept.
// Original rhythm planner and UI. No cloud AI, copyrighted audio, or playtest bot.
#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/ui/Popup.hpp>
#include "GeneratorCore.hpp"
#include <algorithm>
#include <map>
#include <memory>
#include <string>

using namespace geode::prelude;
using namespace autobuilder;

namespace {
struct Run {
    Plan plan;
    int song = 0;
    Difficulty difficulty = Difficulty::Normal;
    bool generated = false;
};
std::map<LevelEditorLayer*, Run> runs;

// Native game API placement avoids importing the level as a string or altering
// a player's pre-existing objects. Only entries built by the planner are added.
int spawnObjects(LevelEditorLayer* editor, const std::vector<Object>& objects) {
    if(!editor) return 0;
    int ok=0;
    for(const auto& spec : objects) {
        auto* gameObject=editor->createObject(spec.id, {spec.x, spec.y}, false);
        if(!gameObject) continue;
        ++ok;
        if(spec.group>0) editor->addObjectToGroup(gameObject, spec.group);
        if(spec.id==901 || spec.id==1006 || spec.id==1520) {
            auto* fx=typeinfo_cast<EffectGameObject*>(gameObject);
            if(!fx) continue;
            if(spec.targetGroup) fx->m_targetGroupID=spec.targetGroup;
            if(spec.id==901) {
                fx->m_duration=spec.duration;
                fx->m_moveOffset=CCPoint{spec.dx, spec.dy};
            } else if (spec.id==1006) {
                fx->m_triggerTargetColor={spec.r,spec.g,spec.b};
                fx->m_fadeInDuration=spec.fadeIn;
                fx->m_holdDuration=spec.hold;
                fx->m_fadeOutDuration=spec.fadeOut;
                fx->m_pulseTargetType=decltype(fx->m_pulseTargetType)(1); // group
            } else if(spec.id==1520) {
                fx->m_duration=spec.duration;
                fx->m_shakeStrength=0.3f;
                fx->m_shakeInterval=0.08f;
            }
        }
    }
    editor->dirtifyTriggers();
    return ok;
}

CCSprite* makeButton(const char* caption) {
    auto* sprite=CCSprite::createWithSpriteFrameName("GJ_button_01.png");
    if(!sprite) return nullptr;
    sprite->setScale(0.78f);
    auto* text=CCLabelBMFont::create(caption,"bigFont.fnt");
    if(text) {
        text->setScale(0.60f);
        text->setPosition(CCPoint{sprite->getContentSize().width / 2.f, sprite->getContentSize().height / 2.f});
        sprite->addChild(text);
    }
    return sprite;
}

class GeneratorPopup : public Popup {
    LevelEditorLayer* m_editor=nullptr;
    CCLabelBMFont* m_songLabel=nullptr;
    CCLabelBMFont* m_diffLabel=nullptr;
    CCLabelBMFont* m_status=nullptr;
    std::vector<CCMenuItemSpriteExtra*> m_diffButtons;
    int m_song=0;
    Difficulty m_difficulty=Difficulty::Normal;

    void refresh() {
        if(m_songLabel) m_songLabel->setString(officialSongs()[m_song].name);
        if(m_diffLabel) {
            static const char* names[]={"EASY","NORMAL","HARD","HARDER","INSANE","DEMON"};
            m_diffLabel->setString(names[static_cast<int>(m_difficulty)]);
        }
        for(int i=0;i<static_cast<int>(m_diffButtons.size());++i) {
            auto* b=m_diffButtons[i];
            if(b) b->setScale(i==static_cast<int>(m_difficulty)?1.12f:0.86f);
        }
    }

    void onPrev(CCObject*) {
        m_song=(m_song+static_cast<int>(officialSongs().size())-1)%officialSongs().size();
        refresh();
    }
    void onNext(CCObject*) {
        m_song=(m_song+1)%officialSongs().size();
        refresh();
    }
    void onDifficulty(CCObject* sender) {
        auto* node=typeinfo_cast<CCNode*>(sender);
        if(!node) return;
        m_difficulty=static_cast<Difficulty>(std::clamp(node->getTag(),0,5));
        refresh();
    }
    void onGenerate(CCObject*) {
        if(!m_editor || !m_editor->m_level) return;
        auto& run=runs[m_editor];
        if(run.generated) {
            m_status->setString("Already generated. Use IMPROVE.");
            return;
        }
        // A safety guard: never silently overwrite an existing editor layout.
        if(m_editor->m_objectCount>0) {
            m_status->setString("Use a new EMPTY level!");
            return;
        }
        double seconds=0;
        auto audioPath=Mod::get()->getSaveDir() /
           ("song_"+std::to_string(m_song)+".wav");
        auto detected=analyzeWav(audioPath,seconds);
        if(detected.size()<16) {
            detected.clear();
            seconds=officialSongs()[m_song].fallbackSeconds;
        }
        run.song=m_song;
        run.difficulty=m_difficulty;
        run.plan=generate(m_song,m_difficulty,detected,seconds);
        run.generated=true;
        // Selecting an official audio track is separate from effect objects.
        m_editor->m_level->m_audioTrack=m_song;
        m_editor->m_level->m_songID=0;
        m_editor->levelSettingsUpdated();
        int created=spawnObjects(m_editor,run.plan.objects);
        if(created!=static_cast<int>(run.plan.objects.size())) {
            m_status->setString("Partial placement; back up & inspect!");
        } else if(run.plan.waveformAnalyzed) {
            m_status->setString("WAV scanned - layout created!");
        } else {
            m_status->setString("BPM preset used - layout created!");
        }
    }
    void onImprove(CCObject*) {
        auto it=runs.find(m_editor);
        if(it==runs.end() || !it->second.generated) {
            m_status->setString("Press GENERATE first."); return;
        }
        auto& run=it->second;
        auto extra=improve(run.plan,run.difficulty);
        if(extra.empty()) { m_status->setString("Maximum: 3 improvements."); return; }
        int added=spawnObjects(m_editor,extra);
        m_status->setString(("Improve #"+std::to_string(run.plan.improvementPass)+
                             " +"+std::to_string(added)+" objects").c_str());
    }
    bool init(LevelEditorLayer* editor) {
        if(!Popup::init(385.f,255.f)) return false;
        m_editor=editor;
        if(auto it=runs.find(editor);it!=runs.end()) {
            m_song=it->second.song;
            m_difficulty=it->second.difficulty;
        }
        this->setTitle("RHYTHM BUILDER");
        auto* menu=CCMenu::create();
        menu->setPosition({0,0});
        m_mainLayer->addChild(menu,10);
        auto label=[&](const char* txt,float x,float y,float scale) {
            auto* l=CCLabelBMFont::create(txt,"goldFont.fnt");
            l->setScale(scale); l->setPosition({x,y});
            m_mainLayer->addChild(l); return l;
        };
        label("Official song",192,204,0.54f);
        m_songLabel=label("",192,177,0.53f);
        auto* leftSprite=CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        if(leftSprite) {
            leftSprite->setScale(.55f);
            auto* b=CCMenuItemSpriteExtra::create(leftSprite,nullptr,this,menu_selector(GeneratorPopup::onPrev));
            b->setPosition({37,177}); menu->addChild(b);
        }
        auto* rightSprite=CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        if(rightSprite) {
            rightSprite->setFlipX(true); rightSprite->setScale(.55f);
            auto* b=CCMenuItemSpriteExtra::create(rightSprite,nullptr,this,menu_selector(GeneratorPopup::onNext));
            b->setPosition({348,177}); menu->addChild(b);
        }
        label("Difficulty",192,144,.50f);
        const char* icons[]={"difficulty_01_btn_001.png","difficulty_02_btn_001.png",
                              "difficulty_03_btn_001.png","difficulty_04_btn_001.png",
                              "difficulty_05_btn_001.png","difficulty_10_btn_001.png"};
        for(int i=0;i<6;i++) {
            auto* spr=CCSprite::createWithSpriteFrameName(icons[i]);
            if(!spr) continue;
            spr->setScale(.37f);
            auto* btn=CCMenuItemSpriteExtra::create(spr,nullptr,this,menu_selector(GeneratorPopup::onDifficulty));
            btn->setTag(i); btn->setPosition({60.f+i*53.f,108.f});
            menu->addChild(btn);m_diffButtons.push_back(btn);
        }
        m_diffLabel=label("",192,76,.42f);
        if(auto* s=makeButton("GENERATE")) {
            auto* b=CCMenuItemSpriteExtra::create(s,nullptr,this,menu_selector(GeneratorPopup::onGenerate));
            b->setPosition({105,43});menu->addChild(b);
        }
        if(auto* s=makeButton("IMPROVE")) {
            auto* b=CCMenuItemSpriteExtra::create(s,nullptr,this,menu_selector(GeneratorPopup::onImprove));
            b->setPosition({280,43});menu->addChild(b);
        }
        m_status=label("Only for an empty level",192,15,.32f);
        refresh();
        return true;
    }
public:
    static GeneratorPopup* create(LevelEditorLayer* editor) {
        auto* popup=new GeneratorPopup;
        if(popup && popup->init(editor)) {popup->autorelease();return popup;}
        delete popup;
        return nullptr;
    }
};
} // namespace

class $modify(RhythmBuilderUI, EditorUI) {
    bool init(LevelEditorLayer* layer) {
        if(!EditorUI::init(layer)) return false;
        runs.erase(layer); // Reset when this editor instance initializes.
        auto* menu=CCMenu::create();
        menu->setID("rhythm-builder-menu");
        menu->setPosition({0,0});
        this->addChild(menu,200);
        auto* sprite=CCSprite::createWithSpriteFrameName("GJ_button_01.png");
        if(!sprite) return true;
        sprite->setScale(.67f);
        auto* label=CCLabelBMFont::create("GEN","bigFont.fnt");
        label->setScale(.65f);
        label->setPosition(CCPoint{sprite->getContentSize().width / 2.f, sprite->getContentSize().height / 2.f});
        sprite->addChild(label);
        auto* button=CCMenuItemSpriteExtra::create(sprite,nullptr,this,menu_selector(RhythmBuilderUI::onRhythmBuilder));
        const auto win=CCDirector::sharedDirector()->getWinSize();
        button->setPosition({win.width-54.f,win.height-75.f});
        button->setID("rhythm-builder-open-button");
        menu->addChild(button);
        return true;
    }
    void onRhythmBuilder(CCObject*) {
        if(auto* popup=GeneratorPopup::create(m_editorLayer)) popup->show();
    }
};
