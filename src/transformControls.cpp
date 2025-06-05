#include <cmath>
#include <Geode/Geode.hpp>
#include <Geode/modify/GJTransformControl.hpp>
#include <Geode/modify/EditorUI.hpp>
using namespace geode::prelude;

#define SNAP_COL ccc3(255, 135, 0)
#define LOCK_COL ccc3(155, 155, 155)
#define WHITE_COL ccc3(255, 255, 255)

// max error in fp measurements (in points)
// #define MAX_FP_ERROR 0.01f

// struct MyGJTransformControl;

struct {
	ccColor4B m_interfaceCol;
	int m_showInterface; // 1 - never, 2 - always, 3 - on change
	void update() {
		m_interfaceCol = Mod::get()->getSettingValue<cocos2d::ccColor4B>("interface-color");
		m_showInterface = std::clamp(std::atoi(Mod::get()->getSettingValue<std::string>("show-interface").c_str()), 1, 3);
	}
} SETTINGS;

#include "editorUI.cpp"
#include "interface.cpp"

/*
Transform controls scheme:

      |10|
       |
(6)---(4)---(7)
 |           |
 |           |
(2)   (1)   (3) -- |11| -- (12)
 |           |
 |           |
(8)---(5)---(9)

*/


class $modify(MyGJTransformControl, GJTransformControl) {
	struct Fields {
		// float m_lockedRotation = 0; // last value of rotation before it's been locked
		CCMenu* m_menu;
		CCSprite* m_freeRotSprite;
		CCMenuItemSpriteExtra* m_rotBtn;
		CCMenuItemSpriteExtra* m_anchorBtn;

		bool m_enableAnchor = false;
		bool m_freeRot = false;
		bool m_rotDirty = false;
		
		// uint16_t m_disabledSpritesRot = 0;  // sprites disabled because of free rotation or snap
		// uint16_t m_disabledSpritesSnap = 0; // both are bit arrays (lowest 12 bits used - one for each sprite)
		Ref<GJTransformControlInterface> m_interface;

		// ~Fields() {GLOBAL.m_transformControls = nullptr;}
	};

	// inline uint16_t getDisabledSprites() {
	// 	return m_fields->m_disabledSpritesSnap | m_fields->m_disabledSpritesRot;
	// }

	$override 
	bool init() {
		if (!GJTransformControl::init()) return false;
		SETTINGS.update();

		// fix menu sprite 10 and button overlapping 
		m_fields->m_menu = static_cast<CCMenu*>(m_warpLockButton->getParent());
		m_fields->m_menu->setAnchorPoint(ccp(0,0));
		m_warpLockButton->setPosition(ccp(-30, 20));

		// add new buttons to the menu
		m_fields->m_anchorBtn = CCMenuItemSpriteExtra::create(
			CCSprite::createWithSpriteFrameName("snapOffBtn_001.png"_spr), 
			this, menu_selector(MyGJTransformControl::onToggleAnchor)
		);
		m_fields->m_rotBtn = CCMenuItemSpriteExtra::create(
			CCSprite::createWithSpriteFrameName("freeRotOffBtn_001.png"_spr), 
			this, menu_selector(MyGJTransformControl::onRotBtn)
		);
		
		m_fields->m_menu->addChild(m_fields->m_anchorBtn);
		m_fields->m_menu->addChild(m_fields->m_rotBtn);

		m_fields->m_anchorBtn->setPosition(ccp(0, 20));
		m_fields->m_rotBtn->setPosition(ccp(30, 20));
		
		// add labels to the buttons
		auto labelAnchor = CCLabelBMFont::create("Anchor", "bigFont.fnt");
		auto labelPos = CCLabelBMFont::create("ScaleXY", "bigFont.fnt");
		auto labelRot = CCLabelBMFont::create("FreeRot", "bigFont.fnt");

		m_fields->m_anchorBtn->addChildAtPosition(labelAnchor, Anchor::Bottom);
		m_fields->m_rotBtn->addChildAtPosition(labelRot, Anchor::Bottom);
		m_warpLockButton->addChildAtPosition(labelPos, Anchor::Bottom);

		labelAnchor->setScale(.2f);
		labelRot->setScale(.2f);
		labelPos->setScale(.2f);

		// add interface node
		m_fields->m_interface = GJTransformControlInterface::create(this);
		m_mainNode->addChild(m_fields->m_interface);

		// show interface: 1 - never, 2 - always, 3 - on change
		if (SETTINGS.m_showInterface == 2)
			m_fields->m_interface->setInterfaceVisibility(true);
		else m_fields->m_interface->setInterfaceVisibility(false);

		// toggle off anchor
		m_fields->m_enableAnchor = true;
		onToggleAnchor(nullptr);

		// add freeRot sprite
		m_fields->m_freeRotSprite = CCSprite::createWithSpriteFrameName("warpBtn_02_001.png");
		spriteByTag(12)->addChild(m_fields->m_freeRotSprite);
		m_fields->m_freeRotSprite->setID("free-rot"_spr);
		m_fields->m_freeRotSprite->setAnchorPoint(ccp(0, 0.5));

		return true;
	}

	// I call this before EditorUI::activateTransformControls()
	// void prepareToActivate() {
	// 	m_fields->m_disabledSpritesSnap = 0;
	// 	m_fields->m_disabledSpritesRot = 0;
	// 	if (GLOBAL.m_isFreeRot) {
	// 		m_fields->m_rotBtn->setSprite(
	// 			CCSprite::createWithSpriteFrameName("freeRotOffBtn_001.png"_spr));
	// 		GLOBAL.m_isFreeRot = false;
	// 	}
	// }

	void resetAllColoredSprites() {
		for(auto *spr : CCArrayExt<CCSprite*>(m_warpSprites)) {
			spr->setColor(WHITE_COL);
		}
	}


	void moveAnchorToPos(CCPoint positionInLevelCoords) {
		auto editor = EditorUI::get();
		auto level = LevelEditorLayer::get();

		auto undo = editor->createUndoObject(UndoCommand::Transform, /* don't add to undo list */ true);
		undo->m_transformState.m_transformPosition = positionInLevelCoords;

		level->m_undoObjects->addObject(undo);
		level->undoLastAction();
		level->m_redoObjects->removeLastObject();
		// editor->updateButtons();
	}


	void moveAnchorToSprite(int sprIdx, bool colorSprite = true) {
		auto spr = spriteByTag(sprIdx);
		auto worldPoint = spr->convertToWorldSpace(spr->getContentSize() / 2);
		auto levelPoint = LevelEditorLayer::get()->m_objectLayer->convertToNodeSpace(worldPoint);
		moveAnchorToPos(levelPoint);
		if (colorSprite) {
			spr->setColor(SNAP_COL);
		}
	}


	bool tryToBeginFreeRot(CCTouch* touch, CCEvent* event) {
		auto nodePoint = spriteByTag(12)->convertToNodeSpace(touch->getLocation());
		auto box = m_fields->m_freeRotSprite->boundingBox();
		if (!box.containsPoint(nodePoint)) return false;

		// auto deltaTouch = m_fields->m_freeRotSprite->convertToWorldSpace({0,0}) - spriteByTag(12)->convertToWorldSpace({0,0});

		// touch->m_startPoint -= ccp(deltaTouch.x, -deltaTouch.y);
		// touch->m_point -= ccp(deltaTouch.x, -deltaTouch.y);
		// if (GJTransformControl::ccTouchBegan(touch, event)) {
			m_fields->m_freeRot = true;
			m_transformButtonType = 12;
			// m_fields->m_lockedRotation = m_mainNode->getRotation();
		// 	// log::debug("removed");
		// 	return true;
		// }
		return true;
		// return false;
	}


	bool ccTouchBegan(CCTouch* p0, CCEvent* p1) {
		auto editor = EditorUI::get();

		// fix no objects crash
		if (editor->getSelectedObjects()->count() == 0) {
			editor->deactivateTransformControl();
			return false;
		}

		// when anchor enabled
		if (m_fields->m_enableAnchor) {
			if (!GJTransformControl::ccTouchBegan(p0, p1)) {
				return tryToBeginFreeRot(p0, p1); // check freeRot node
			}
			return true;
		}
		
		// when anchor disabled
		auto anchor = spriteByTag(1);
		auto tmp = anchor->getPosition();
		anchor->setPosition(ccp(-999999, -999999));

		bool ret = GJTransformControl::ccTouchBegan(p0, p1);

		anchor->setPosition(tmp);

		if (!ret && !tryToBeginFreeRot(p0, p1)) return false; // check freeRot node

		switch (m_transformButtonType) {
			case 2: moveAnchorToSprite(3); break;
			case 3: moveAnchorToSprite(2); break;
			case 4: moveAnchorToSprite(5); break;
			case 5: moveAnchorToSprite(4); break;
			case 6: moveAnchorToSprite(9); break;
			case 7: moveAnchorToSprite(8); break;
			case 8: moveAnchorToSprite(7); break;
			case 9: moveAnchorToSprite(6); break;
			// case 10: moveAnchorToSprite(5); break; <--- broken
			// case 11: moveAnchorToSprite(2); break;
			case 12: moveAnchorToPos(editor->getGroupCenter(m_objects, true)); break;
			default: break;
		}

		return true;
	}

	
	$override 
	void ccTouchMoved(CCTouch* p0, CCEvent* p1) {
		// if (m_touchID != p0->m_nId) return;

		if (m_transformButtonType == 12 && m_fields->m_freeRot) {
			// EditorUI::get()->transformRotationChanged(m_fields->m_lockedRotation);
			auto location = convertToNodeSpace(p0->getLocation());
			float fVar15 = std::atan2f(location.y + m_cursorDifference.y, location.x + m_cursorDifference.x);
			m_mainNode->setRotation(-fVar15 * 180 / M_PI - m_rotation);
            updateButtons(false, false);
		} else {
			GJTransformControl::ccTouchMoved(p0, p1);
		}
		
		// interface (1 - never, 2 - always, 3 - on change)
		if (SETTINGS.m_showInterface == 3) {
			m_fields->m_interface->setInterfaceVisibility(true);
		}
	}

	$override 
	void ccTouchEnded(CCTouch* p0, CCEvent* p1) {

		if (m_transformButtonType == 12 && m_fields->m_freeRot) {
			auto editor = reinterpret_cast<MyEditorUI*>(EditorUI::get());
			auto rot = m_mainNode->getRotation();
			// m_mainNode->setRotation(m_fields->m_lockedRotation);
			editor->deactivateTransformControl();
			editor->activateTransformControlWithAngle(rot);
			m_fields->m_freeRot = false;
			m_transformButtonType = 0;
			log::debug("my touch ended");
			// LevelEditorLayer::get()->m_undoObjects->removeLastObject();
		} else {
			GJTransformControl::ccTouchEnded(p0, p1);
			log::debug("not my touch ended");
		}
		

		resetAllColoredSprites();

		

		// interface (1 - never, 2 - always, 3 - on change)
		if (SETTINGS.m_showInterface == 3) {
			m_fields->m_interface->setInterfaceVisibility(false);
		}
	}


	$override 
	void ccTouchCancelled(CCTouch* p0, CCEvent* p1) {
		GJTransformControl::ccTouchCancelled(p0, p1);
		// interface (1 - never, 2 - always, 3 - on change)
		if (SETTINGS.m_showInterface == 3) {
			m_fields->m_interface->setInterfaceVisibility(false);
		}
		m_fields->m_freeRot = false;
		resetAllColoredSprites();
	}


	$override 
	void scaleButtons(float scale) {
		GJTransformControl::scaleButtons(scale);

		// fix bug when scaled sprite doesn't match button touch box (scale not btn but menu)
		if (!m_fields->m_menu) return;
		m_fields->m_menu->setScale(scale);
		m_warpLockButton->getChildByTag(1)->setScale(1.f);		

		// distance for freeRot button
		m_fields->m_freeRotSprite->setPosition(ccp(100 / scale, spriteByTag(12)->getContentHeight() / 2));

	}


	$override
	void updateButtons(bool p0, bool p1) {
		GJTransformControl::updateButtons(p0, p1);
		// keep buttons vertical
		const float angle = m_mainNode->getRotation();
		m_fields->m_rotBtn->setRotation(-angle);
		m_fields->m_anchorBtn->setRotation(-angle);
		m_warpLockButton->setRotation(-angle);

		// rotation for rotateSprites
		spriteByTag(12)->setRotation(angle);
	}


	void onToggleAnchor(CCObject*) {
		m_fields->m_enableAnchor = !m_fields->m_enableAnchor;
		auto anchor = spriteByTag(1);
		auto editor = EditorUI::get();

		if (m_fields->m_enableAnchor) {
			anchor->setVisible(true);
			m_fields->m_anchorBtn->setSprite(CCSprite::createWithSpriteFrameName("snapOnBtn_001.png"_spr));

			// fix no objects crash
			if (editor->getSelectedObjects()->count() == 0) {
				editor->deactivateTransformControl();
				return;
			}

			moveAnchorToPos(editor->getGroupCenter(m_objects, true));
			
			auto effect = CCCircleWave::create(0, 45, 1, false, true);
			effect->m_circleMode = CircleMode::Outline;
			anchor->addChildAtPosition(effect, Anchor::Center);

		} else {
			anchor->setVisible(false);
			m_fields->m_anchorBtn->setSprite(CCSprite::createWithSpriteFrameName("snapOffBtn_001.png"_spr));
		}
	}

	void onRotBtn(CCObject* sender) {
		// m_fields->m_freeRot = !m_fields->m_freeRot;
		// if (m_fields->m_freeRot) {
		// 	m_fields->m_rotBtn->setSprite(CCSprite::createWithSpriteFrameName("freeRotOnBtn_001.png"_spr));
			
		// 	m_fields->m_lockedRotation = m_mainNode->getRotation();
			
		// } else {
		// 	m_fields->m_rotBtn->setSprite(CCSprite::createWithSpriteFrameName("freeRotOffBtn_001.png"_spr));
			
			
		// 	// GLOBAL.m_freeRotFinalAngle = m_mainNode->getRotation();
		// 	auto editor = reinterpret_cast<MyEditorUI*>(EditorUI::get());
		// 	editor->deactivateTransformControl();
		// 	editor->activateTransformControlWithAngle(m_mainNode->getRotation());
		// 	return;
		// 	// if (m_fields->m_rotDirty) {
		// 	// }
		// }
	}
};

