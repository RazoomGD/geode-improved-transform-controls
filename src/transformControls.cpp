#include <cmath>
#include <Geode/Geode.hpp>
#include <Geode/modify/GJTransformControl.hpp>
#include <Geode/modify/EditorUI.hpp>
using namespace geode::prelude;

#define ANCHOR_COL ccc3(255, 135, 0)
#define WHITE_COL ccc3(255, 255, 255)
#define SNAP_COL ccc3(255, 135, 0)

#define ANCHOR_ON_SPR "anchorOnBtn_001.png"_spr
#define ANCHOR_OFF_SPR "anchorOffBtn_001.png"_spr
#define FREEROT_ON_SPR "freeRotOnBtn_001.png"_spr
#define FREEROT_OFF_SPR "freeRotOffBtn_001.png"_spr
#define GRID_SNAP_ON_SPR "gridSnapOnBtn_001.png"_spr
#define GRID_SNAP_OFF_SPR "gridSnapOffBtn_001.png"_spr


struct {
	ccColor4B m_interfaceCol;
	int m_showInterface; // 1 - never, 2 - always, 3 - on change
	float m_buttonScale;
	bool m_freeRotAlways;
	void update() {
		m_interfaceCol = Mod::get()->getSettingValue<cocos2d::ccColor4B>("interface-color");
		m_showInterface = std::clamp(std::atoi(Mod::get()->getSettingValue<std::string>("show-interface").c_str()), 1, 3);
		m_buttonScale = Mod::get()->getSettingValue<double>("button-scale");
		m_freeRotAlways = Mod::get()->getSettingValue<bool>("no-freerot");
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
		CCMenu* m_menu;
		CCSprite* m_freeRotSprite;
		CCMenuItemSpriteExtra* m_gridSnapBtn;
		CCMenuItemSpriteExtra* m_anchorBtn;
		CCMenuItemSpriteExtra* m_freeRotBtn;

		bool m_enableAnchor = false;
		bool m_gridSnap = false;

		bool m_isInFreeRot = false;
		
		GJTransformControlInterface* m_interface;

		bool isGridSnap() {
			return m_gridSnap || CCKeyboardDispatcher::get()->getShiftKeyPressed();
		}
	};


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
			CCSprite::createWithSpriteFrameName(ANCHOR_OFF_SPR), 
			this, menu_selector(MyGJTransformControl::onToggleAnchor)
		);
		m_fields->m_gridSnapBtn = CCMenuItemSpriteExtra::create(
			CCSprite::createWithSpriteFrameName(GRID_SNAP_OFF_SPR), 
			this, menu_selector(MyGJTransformControl::onSnapGridBtn)
		);
		m_fields->m_freeRotBtn = CCMenuItemSpriteExtra::create(
			CCSprite::createWithSpriteFrameName(FREEROT_OFF_SPR), 
			this, menu_selector(MyGJTransformControl::onFreeRotBtn)
		);
		
		m_fields->m_menu->addChild(m_fields->m_anchorBtn);
		m_fields->m_menu->addChild(m_fields->m_gridSnapBtn);
		m_fields->m_menu->addChild(m_fields->m_freeRotBtn);

		m_fields->m_anchorBtn->setPosition(ccp(0, 20));
		m_fields->m_gridSnapBtn->setPosition(ccp(30, 20));
		m_fields->m_freeRotBtn->setPosition(ccp(60, 20));
		
		// add labels to the buttons
		auto labelPos = CCLabelBMFont::create("ScaleXY", "bigFont.fnt");
		auto labelAnchor = CCLabelBMFont::create("Anchor", "bigFont.fnt");
		auto labelSnap = CCLabelBMFont::create("Snap", "bigFont.fnt");
		auto labelFreeRot = CCLabelBMFont::create("FreeRot", "bigFont.fnt");

		m_fields->m_anchorBtn->addChildAtPosition(labelAnchor, Anchor::Bottom);
		m_fields->m_gridSnapBtn->addChildAtPosition(labelSnap, Anchor::Bottom);
		m_fields->m_freeRotBtn->addChildAtPosition(labelFreeRot, Anchor::Bottom);
		m_warpLockButton->addChildAtPosition(labelPos, Anchor::Bottom);

		labelAnchor->setScale(.2f);
		labelSnap->setScale(.2f);
		labelFreeRot->setScale(.2f);
		labelPos->setScale(.2f);

		// add interface node
		m_fields->m_interface = GJTransformControlInterface::create(this);
		m_mainNode->addChild(m_fields->m_interface);
		
		// show interface: 1 - never, 2 - always, 3 - on change
		m_fields->m_interface->setInterfaceVisibility(SETTINGS.m_showInterface == 2);

		// toggle off anchor
		m_fields->m_enableAnchor = true;
		onToggleAnchor(nullptr);

		// add freeRot sprite
		m_fields->m_freeRotSprite = CCSprite::createWithSpriteFrameName("freeRotSpr.png"_spr);
		addChild(m_fields->m_freeRotSprite, 100);
		m_fields->m_freeRotSprite->setID("free-rot"_spr);
		m_fields->m_freeRotSprite->setVisible(false);

		// freerot option
		if (SETTINGS.m_freeRotAlways) {
			m_fields->m_freeRotBtn->setVisible(false);
			m_fields->m_freeRotSprite->setVisible(true);
		}

		return true;
	}


	void resetAllColoredSprites() {
		for(auto *spr : CCArrayExt<CCSprite*>(m_warpSprites)) {
			spr->setColor(WHITE_COL);
		}
		m_fields->m_freeRotSprite->setColor(WHITE_COL);
	}


	void moveAnchorToPos(CCPoint positionInLevelCoords) {
		auto editor = EditorUI::get();
		auto level = LevelEditorLayer::get();

		auto undo = editor->createUndoObject(UndoCommand::Transform, /* don't add to undo list */ true);
		undo->m_transformState.m_transformPosition = positionInLevelCoords;

		level->m_undoObjects->addObject(undo);
		level->undoLastAction();
		level->m_redoObjects->removeLastObject();
		// editor->updateButtons(); <--- bug with rotation mode
	}


	void moveAnchorToSprite(int sprIdx, bool colorSprite = true) {
		auto spr = spriteByTag(sprIdx);
		auto worldPoint = spr->convertToWorldSpace(spr->getContentSize() / 2);
		auto levelPoint = LevelEditorLayer::get()->m_objectLayer->convertToNodeSpace(worldPoint);
		moveAnchorToPos(levelPoint);
		if (colorSprite) {
			spr->setColor(ANCHOR_COL);
		}
	}


	bool tryToBeginFreeRot(CCTouch* touch, CCEvent* event) {
		if (!m_fields->m_freeRotSprite->isVisible()) return false;

		auto nodePoint = convertToNodeSpace(touch->getLocation());
		auto box = m_fields->m_freeRotSprite->boundingBox();
		if (!box.containsPoint(nodePoint)) return false;

		m_fields->m_isInFreeRot = true;
		m_transformButtonType = 12;
		m_cursorDifference = box.origin + box.size / 2 - nodePoint;
		return true;
	}


	$override
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
				if (!tryToBeginFreeRot(p0, p1)) return false; // check freeRot node
			}

			// interface (1 - never, 2 - always, 3 - on change)
			if (SETTINGS.m_showInterface == 3) {
				m_fields->m_interface->setInterfaceVisibility(true);
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

		// interface (1 - never, 2 - always, 3 - on change)
		if (SETTINGS.m_showInterface == 3) {
			m_fields->m_interface->setInterfaceVisibility(true);
		}

		return true;
	}

	
	bool snapTouchToGrid(CCTouch* touch) {
		if (m_transformButtonType < 2 || m_transformButtonType > 9) return false;

		auto grid = LevelEditorLayer::get()->m_drawGridLayer;
		float gridSz = grid->m_gridSize;

		// find node coords in editor (direct conversion)
		auto nodeExpectedEditorPos = grid->convertToNodeSpace(
			m_mainNode->convertToWorldSpace(
				m_mainNode->convertToNodeSpace(touch->getLocation()) + m_cursorDifference
			)
		);

		// find nearest X and Y grid line
		float xLine = roundf(nodeExpectedEditorPos.x / gridSz) * gridSz;
		float yLine = roundf(nodeExpectedEditorPos.y / gridSz) * gridSz;

		// find deltas
		float dx = xLine - nodeExpectedEditorPos.x;
		float dy = yLine - nodeExpectedEditorPos.y;
		bool snapX = fabsf(dx) < gridSz * 0.2;
		bool snapY = fabsf(dy) < gridSz * 0.2;

		if (snapX || snapY) {
			// reverse conversion
			auto nodePos = m_mainNode->convertToNodeSpace(grid->convertToWorldSpace(ccp(xLine, yLine))) - m_cursorDifference;
			auto point = CCDirector::get()->convertToUI(m_mainNode->convertToWorldSpace(nodePos));

			// set new touch coords
			if (snapX) touch->m_point.x = point.x;
			if (snapY) touch->m_point.y = point.y;

			return true;
		}
		return false;
	}

	
	$override 
	void ccTouchMoved(CCTouch* p0, CCEvent* p1) {

		// snap position
		if (m_fields->isGridSnap() && fmod(m_mainNode->getRotation(), 90.f) == 0) {
			bool snapped = snapTouchToGrid(p0);
			spriteByTag(m_transformButtonType)->setColor(snapped ? SNAP_COL : WHITE_COL);
		} else {
			spriteByTag(m_transformButtonType)->setColor(WHITE_COL);
		}
		
		if (m_fields->m_isInFreeRot) {
			auto location = convertToNodeSpace(p0->getLocation());
			float fVar15 = atan2f(location.y + m_cursorDifference.y, location.x + m_cursorDifference.x);
			float newRot = -fVar15 * 180 / M_PI - m_rotation;

			if (m_fields->isGridSnap()) { // rotSnap + freeRot
				float targetRot = roundf(newRot / 90) * 90;
				if (fabsf(targetRot - newRot) < 3) {
					newRot = targetRot;
					m_fields->m_freeRotSprite->setColor(SNAP_COL);
				} else {
					m_fields->m_freeRotSprite->setColor(WHITE_COL);
				}
			}

			m_mainNode->setRotation(newRot);
            GJTransformControl::updateButtons(false, false);

		} else {
			// rotSnap 
			bool rotationSnapped = false;
			if (m_transformButtonType == 12 && m_fields->isGridSnap()) { 
				// decompiled code of GJTransformControl::ccTouchMoved for rotation
				auto location = convertToNodeSpace(p0->getLocation());
				float fVar15 = atan2f(location.y + m_cursorDifference.y, location.x + m_cursorDifference.x);
				float newRot = -fVar15 * 180 / M_PI - m_rotation;

				float targetRot = roundf(newRot / 90) * 90;
				if (fabsf(targetRot - newRot) < 3) {
					newRot = targetRot;
					// if (newRot != m_rotationY) {
					if (true) {
						m_rotationY = newRot;
						m_mainNode->setRotation(newRot);
						m_delegate->transformRotationChanged(newRot);
						rotationSnapped = true;
						GJTransformControl::updateButtons(false, false);
					}
				}
			}

			if (rotationSnapped) {
				spriteByTag(12)->setColor(SNAP_COL);
			} else {
				GJTransformControl::ccTouchMoved(p0, p1);
				spriteByTag(12)->setColor(WHITE_COL);
			}
		}
	}

	$override 
	void ccTouchEnded(CCTouch* p0, CCEvent* p1) {

		if (m_fields->m_isInFreeRot) {
			auto editor = reinterpret_cast<MyEditorUI*>(EditorUI::get());
			auto rot = m_mainNode->getRotation();
			editor->deactivateTransformControl();
			editor->activateTransformControlWithAngle(rot);
			m_fields->m_isInFreeRot = false;
			m_transformButtonType = 0;
		} else {
			GJTransformControl::ccTouchEnded(p0, p1);
		}
		
		resetAllColoredSprites();

		// interface (1 - never, 2 - always, 3 - on change)
		if (SETTINGS.m_showInterface == 3) {
			m_fields->m_interface->setInterfaceVisibility(false);
		}
	}


	// $override
	// void ccTouchCancelled(CCTouch* p0, CCEvent* p1) { <--- crash bc this == EditorUI
	// 	GJTransformControl::ccTouchCancelled(p0, p1);
	// 	// interface (1 - never, 2 - always, 3 - on change)
	// 	if (SETTINGS.m_showInterface == 3) {
	// 		m_fields->m_interface->setInterfaceVisibility(false);
	// 	}
	// 	m_fields->m_freeRot = false;
	// 	m_transformButtonType = 0;
	// 	resetAllColoredSprites();
	// }


	$override 
	void scaleButtons(float scale) {
		GJTransformControl::scaleButtons(scale);

		// fix bug when scaled sprite doesn't match button touch box (scale not btn but menu)
		if (!m_fields->m_menu) return;
		m_fields->m_menu->setScale(scale * SETTINGS.m_buttonScale);
		m_warpLockButton->getChildByTag(1)->setScale(1.f);		

		// freeRot button
		m_fields->m_freeRotSprite->setScale(spriteByTag(12)->getScale());
	}


	$override
	void updateButtons(bool p0, bool p1) {
		GJTransformControl::updateButtons(p0, p1);
		// keep buttons vertical
		const float angle = m_mainNode->getRotation();
		m_fields->m_gridSnapBtn->setRotation(-angle);
		m_fields->m_anchorBtn->setRotation(-angle);
		m_fields->m_freeRotBtn->setRotation(-angle);
		m_warpLockButton->setRotation(-angle);

		// update freeRot sprite pos
		const float dist = 50;
		auto d = ccp(dist * std::cos(angle / 180 * M_PI), -dist * std::sin(angle / 180 * M_PI));
		auto rotSprPos = spriteByTag(12)->getPosition();
		m_fields->m_freeRotSprite->setPosition(rotSprPos + d);
	}


	void onToggleAnchor(CCObject*) {
		auto anchor = spriteByTag(1);
		auto editor = EditorUI::get();

		if (m_fields->m_enableAnchor = !m_fields->m_enableAnchor) {
			anchor->setVisible(true);
			m_fields->m_anchorBtn->setSprite(CCSprite::createWithSpriteFrameName(ANCHOR_ON_SPR));

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
			m_fields->m_anchorBtn->setSprite(CCSprite::createWithSpriteFrameName(ANCHOR_OFF_SPR));
		}
	}


	void onSnapGridBtn(CCObject* sender) {
		if (m_fields->m_gridSnap = !m_fields->m_gridSnap) {
			m_fields->m_gridSnapBtn->setSprite(CCSprite::createWithSpriteFrameName(GRID_SNAP_ON_SPR));
		} else {
			m_fields->m_gridSnapBtn->setSprite(CCSprite::createWithSpriteFrameName(GRID_SNAP_OFF_SPR));
		}
	}


	void onFreeRotBtn(CCObject* sender) {
		if (m_fields->m_freeRotSprite->isVisible()) {
			m_fields->m_freeRotSprite->setVisible(false);
			m_fields->m_freeRotBtn->setSprite(CCSprite::createWithSpriteFrameName(FREEROT_OFF_SPR));
		} else {
			m_fields->m_freeRotSprite->setVisible(true);
			m_fields->m_freeRotBtn->setSprite(CCSprite::createWithSpriteFrameName(FREEROT_ON_SPR));

			auto effect = CCCircleWave::create(0, 45, 1, false, true);
			effect->m_circleMode = CircleMode::Outline;
			m_fields->m_freeRotSprite->addChildAtPosition(effect, Anchor::Center);
		}
	}
};

