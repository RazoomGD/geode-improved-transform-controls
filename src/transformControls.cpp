#include <cmath>
#include <Geode/Geode.hpp>
#include <Geode/modify/GJTransformControl.hpp>
#include <Geode/modify/EditorUI.hpp>

using namespace geode::prelude;

#include "editorUI.cpp"
#include "interface.cpp"

#define ANCHOR_COL ccc3(255, 135, 0)
#define WHITE_COL ccc3(255, 255, 255)
#define SNAP_COL ccc3(255, 135, 0)
#define LOCK_COL ccc3(100, 100, 100)

#define MAX_FP_ERROR 0.01f

#define SNAP_ON_SPR "snapOnBtn_001.png"_spr
#define SNAP_OFF_SPR "snapOffBtn_001.png"_spr
#define ANCHOR_ON_SPR "anchorOnBtn_001.png"_spr
#define ANCHOR_OFF_SPR "anchorOffBtn_001.png"_spr
#define FREEROT_ON_SPR "freeRotOnBtn_001.png"_spr
#define FREEROT_OFF_SPR "freeRotOffBtn_001.png"_spr
#define GRID_SNAP_ON_SPR "gridSnapOnBtn_001.png"_spr
#define GRID_SNAP_OFF_SPR "gridSnapOffBtn_001.png"_spr


enum class InterfaceMode {
	Hidden = 1, Visible = 2, OnChange = 3
};


struct {
	ccColor4B m_interfaceCol;
	InterfaceMode m_showInterface;
	float m_buttonScale;
	bool m_freeRotAlways;
	void update() {
		m_interfaceCol = Mod::get()->getSettingValue<cocos2d::ccColor4B>("interface-color");
		m_showInterface = (InterfaceMode) std::clamp(std::atoi(Mod::get()->getSettingValue<std::string>("show-interface").c_str()), 1, 3);
		m_buttonScale = Mod::get()->getSettingValue<double>("button-scale");
		m_freeRotAlways = Mod::get()->getSettingValue<bool>("no-freerot");
	}
} SETTINGS;


struct {
	bool m_lockXY;
	bool m_enableAnchor;
	bool m_snap;         // also Ctrl + spr1
	bool m_gridSnap;     // also Shift + spr 1-9,12
	bool m_freeRot;      // also Ctrl + spr 12

	void save() {
		Mod::get()->setSavedValue("lock-xy", m_lockXY);
		Mod::get()->setSavedValue("anchor", m_enableAnchor);
		Mod::get()->setSavedValue("snap", m_snap);
		Mod::get()->setSavedValue("grid-snap", m_gridSnap);
		Mod::get()->setSavedValue("free-rot", m_freeRot);
	}

	void load() {
		m_lockXY = Mod::get()->getSavedValue<bool>("lock-xy", false);
		m_enableAnchor = Mod::get()->getSavedValue<bool>("anchor", false);
		m_snap = Mod::get()->getSavedValue<bool>("snap", false);
		m_gridSnap = Mod::get()->getSavedValue<bool>("grid-snap", false);
		m_freeRot = Mod::get()->getSavedValue<bool>("free-rot", false);
	}

	bool snap() {return m_snap || CCKeyboardDispatcher::get()->getControlKeyPressed();}
	bool gridSnap() {return m_gridSnap || CCKeyboardDispatcher::get()->getShiftKeyPressed();}
	bool freeRot() {return m_freeRot || CCKeyboardDispatcher::get()->getControlKeyPressed();}

} STATE;


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

Info:
m_mainNode - first child of GJTransformControl, pos always (0,0), may have rotation
m_mainNodeParent - actually main node child, parent for 11 sprites, pos and rot always 0, 

Therefore positions on both nodes are equivalent

*/

class $modify(MyGJTransformControl, GJTransformControl) {
	struct Fields {
		CCMenu* m_menu;
		CCMenuItemSpriteExtra* m_gridSnapBtn;
		CCMenuItemSpriteExtra* m_snapBtn;
		CCMenuItemSpriteExtra* m_anchorBtn;
		CCMenuItemSpriteExtra* m_freeRotBtn;

		uint16_t m_blockedSprites = 0; // bit array

		bool m_isInFreeRot = false;
		
		GJTransformControlInterface* m_interface;

		~Fields() {STATE.save();}
	};


	inline void addLabel(CCNode* button, const char* txt) {
		auto label = CCLabelBMFont::create(txt, "bigFont.fnt");
		button->addChildAtPosition(label, Anchor::Bottom);
		label->setScale(.2f);
	}


	inline void loadState() {
		if (STATE.m_lockXY) {STATE.m_lockXY = false; onToggleLockScale(m_warpLockButton);}
		if (STATE.m_enableAnchor) {STATE.m_enableAnchor = false; onToggleAnchorBtn(nullptr);}
		if (STATE.m_snap) {STATE.m_snap = false; onSnapBtn(nullptr);}
		if (STATE.m_gridSnap) {STATE.m_gridSnap = false; onSnapGridBtn(nullptr);}
		if (STATE.m_freeRot) {STATE.m_freeRot = false; onFreeRotBtn(nullptr);}
	}


	inline void arrangeMenuButtons() {
		float x = -60;
		float y = 20;
		for (auto* ch : CCArrayExt<CCNode*>(m_fields->m_menu->getChildren())) {
			if (ch->isVisible()) {
				ch->setPosition({x, y});
				x += 30;
			}
		}
	}


	$override 
	bool init() {
		if (!GJTransformControl::init()) return false;
		SETTINGS.update();
		STATE.load();

		// fix menu sprite 10 and button overlapping 
		m_fields->m_menu = static_cast<CCMenu*>(m_warpLockButton->getParent());
		m_fields->m_menu->setAnchorPoint(ccp(0,0));

		// add new buttons to the menu
		m_fields->m_anchorBtn = CCMenuItemSpriteExtra::create(
			CCSprite::createWithSpriteFrameName(ANCHOR_OFF_SPR), 
			this, menu_selector(MyGJTransformControl::onToggleAnchorBtn)
		);
		m_fields->m_snapBtn = CCMenuItemSpriteExtra::create(
			CCSprite::createWithSpriteFrameName(SNAP_OFF_SPR), 
			this, menu_selector(MyGJTransformControl::onSnapBtn)
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
		m_fields->m_menu->addChild(m_fields->m_snapBtn);
		m_fields->m_menu->addChild(m_fields->m_gridSnapBtn);
		m_fields->m_menu->addChild(m_fields->m_freeRotBtn);
		
		// add labels to the buttons
		addLabel(m_fields->m_anchorBtn, "Anchor");
		addLabel(m_fields->m_snapBtn, "Snap");
		addLabel(m_fields->m_gridSnapBtn, "GridSnap");
		addLabel(m_fields->m_freeRotBtn, "FreeRot");
		addLabel(m_warpLockButton, "ScaleXY");

		arrangeMenuButtons();

		// add interface node
		m_fields->m_interface = GJTransformControlInterface::create(this, SETTINGS.m_interfaceCol);
		m_mainNode->addChild(m_fields->m_interface);
		
		// show interface: 1 - never, 2 - always, 3 - on change
		m_fields->m_interface->setInterfaceVisibility(SETTINGS.m_showInterface == InterfaceMode::Visible);

		// anchor
		spriteByTag(1)->setVisible(STATE.m_enableAnchor);

		queueInMainThread([this](){loadState();}); // when editorUI is loaded

		return true;
	}


	void resetAllColoredSprites() {
		for(auto *spr : CCArrayExt<CCSprite*>(m_warpSprites)) {
			spr->setColor(WHITE_COL);
		}
	}


	// it takes into account no-anchor mode
	void updateBlockedSprites() {
		auto aPos = spriteByTag(1)->getPosition();
		uint16_t blocked = 0;
		
		if (STATE.m_enableAnchor) {
			uint8_t snapNodeIndx = 0;
			checkAnchorIsOnEdge(MAX_FP_ERROR, aPos, &snapNodeIndx);
			switch (snapNodeIndx) {
				case 2: blocked = 0b010001010000; break; // 2,6,8
				case 3: blocked = 0b001000101000; break; // 3,7,9
				case 4: blocked = 0b000101100000; break; // 4,6,7
				case 5: blocked = 0b000010011000; break; // 5,8,9
				case 6: blocked = 0b010101110000; break; // 2,4,6,7,8
				case 7: blocked = 0b001101101000; break; // 3,4,6,7,9
				case 8: blocked = 0b010011011000; break; // 2,5,6,8,9
				case 9: blocked = 0b001010111000; break; // 3,5,7,8,9
				default: blocked = 0; break;
			}
		}

		// color sprites
		uint16_t mask = 0b100000000000;
		for(int i = 1; i < 13; i++) {
			auto spr = spriteByTag(i);
			spr->setColor((mask & blocked) ? LOCK_COL : WHITE_COL);
			mask = mask >> 1;
		}

		m_fields->m_blockedSprites = blocked;
	}


	bool isSpriteBlocked(int sprIdx) {
		return ((uint16_t)0b1000000000000 >> sprIdx) & m_fields->m_blockedSprites;
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


	void moveAnchorToSprite(int sprIdx1, int sprIdx2, bool colorSprites = true) {
		auto spr1 = spriteByTag(sprIdx1);
		auto spr2 = spriteByTag(sprIdx2);
		auto worldPoint1 = spr1->convertToWorldSpace(spr1->getContentSize() / 2);
		auto worldPoint2 = spr2->convertToWorldSpace(spr2->getContentSize() / 2);
		auto middle = (worldPoint1 + worldPoint2) * 0.5;
		auto levelPoint = LevelEditorLayer::get()->m_objectLayer->convertToNodeSpace(middle);
		moveAnchorToPos(levelPoint);
		if (colorSprites) {
			spr1->setColor(ANCHOR_COL);
			spr2->setColor(ANCHOR_COL);
		}
	}


	bool tryToBeginFreeRot(CCTouch* touch, CCEvent* event) {
		auto nodePoint = convertToNodeSpace(touch->getLocation());
		auto box = spriteByTag(12)->boundingBox();
		if (!box.containsPoint(nodePoint)) return false;

		m_fields->m_isInFreeRot = true;
		m_transformButtonType = 12;
		m_cursorDifference = box.origin + box.size / 2 - nodePoint;
		return true;
	}


	inline bool touchBeginSuccess() {
		if (SETTINGS.m_showInterface == InterfaceMode::OnChange) {
			m_fields->m_interface->setInterfaceVisibility(true);
		}
		return true;
	}


	$override
	bool ccTouchBegan(CCTouch* p0, CCEvent* p1) {

		// find touched button 
		auto mainNodePoint = m_mainNode->convertToNodeSpace(p0->getLocation());
		if (!spriteByTag(1)->boundingBox().containsPoint(mainNodePoint)) {
			for (int i = 2; i < 10; i++) { // early check if this sprite blocked
				if (spriteByTag(i)->boundingBox().containsPoint(mainNodePoint)) {
					if (isSpriteBlocked(i)) return false;
					break;
				}
			}
		}

		auto editor = EditorUI::get();

		// fix no objects crash
		if (editor->getSelectedObjects()->count() == 0) {
			editor->deactivateTransformControl();
			return false;
		}

		// when anchor enabled
		if (STATE.m_enableAnchor) {
			if (STATE.freeRot() && tryToBeginFreeRot(p0, p1)) { // try to begin freeRot
				return touchBeginSuccess();
			} 
			return GJTransformControl::ccTouchBegan(p0, p1) ? touchBeginSuccess() : false;
		}

		// when anchor disabled

		bool res = false;
		if (STATE.freeRot()) { // try to begin freeRot
			res = tryToBeginFreeRot(p0, p1);
		}

		if (!res) {
			auto anchor = spriteByTag(1);
			auto tmp = anchor->getPosition();
			anchor->setPosition(ccp(-999999, -999999));
			res = GJTransformControl::ccTouchBegan(p0, p1);
			anchor->setPosition(tmp);
		}

		if (!res) return false;

		if (CCKeyboardDispatcher::get()->getControlKeyPressed()) {
			switch (m_transformButtonType) {
				case 2:
				case 3: moveAnchorToSprite(4, 5); break;
				case 4:
				case 5: moveAnchorToSprite(2, 3); break;
				case 6:
				case 9: moveAnchorToSprite(7, 8); break;
				case 7:
				case 8: moveAnchorToSprite(6, 9); break;
				// case 10: moveAnchorToSprite(5); break; <--- broken
				// case 11: moveAnchorToSprite(2); break;
				case 12: moveAnchorToPos(editor->getGroupCenter(m_objects, true)); break;
				default: break;
			}
		} else {
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
		}

		return touchBeginSuccess();
	}


	bool snapTouchToGrid(CCTouch* touch) {
		if (fmod(m_mainNode->getRotation(), 90.f) != 0 && m_transformButtonType != 1) return false;
		if (m_transformButtonType < 1 || m_transformButtonType > 9) return false;

		auto baseNode = (m_transformButtonType == 1) ? this : m_mainNode;

		auto grid = LevelEditorLayer::get()->m_drawGridLayer;
		float gridSz = grid->m_gridSize;

		// find node coords in editor (direct conversion)
		auto nodeExpectedEditorPos = grid->convertToNodeSpace(
			baseNode->convertToWorldSpace(
				baseNode->convertToNodeSpace(touch->getLocation()) + m_cursorDifference
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
			auto nodePos = baseNode->convertToNodeSpace(grid->convertToWorldSpace(ccp(xLine, yLine))) - m_cursorDifference;
			auto point = CCDirector::get()->convertToUI(baseNode->convertToWorldSpace(nodePos));

			// set new touch coords
			if (snapX) touch->m_point.x = point.x;
			if (snapY) touch->m_point.y = point.y;

			return true;
		}
		return false;
	}


	bool snapTouchToClosestButton(CCTouch* touch) {
		if (m_transformButtonType != 1) return false;

		const float limit = spriteByTag(1)->getScale() * 18;
		const auto aPos = convertToNodeSpace(touch->getLocation()) + m_cursorDifference;

		const double sin = std::sin(m_mainNode->getRotation()*M_PI/180.0);
		const double cos = std::cos(m_mainNode->getRotation()*M_PI/180.0);
		const auto anchorRelPos = ccp(cos * aPos.x - sin * aPos.y, sin * aPos.x + cos * aPos.y);

		// check distance between the anchor and other sprites
		for (int i = 2; i < 10; i++) {
			CCPoint nodePos = spriteByTag(i)->getPosition();
			CCPoint distVec = anchorRelPos - nodePos;
			auto distSq = distVec.x * distVec.x + distVec.y * distVec.y;

			if (distSq < limit * limit) {
				auto snapCoords = ccp(cos * nodePos.x + sin * nodePos.y, -sin * nodePos.x + cos * nodePos.y);
				touch->m_point = CCDirector::get()->convertToUI(
					convertToWorldSpace(snapCoords - m_cursorDifference)
				);
				return true;
			}
		}
		return false;
	}

	
	$override 
	void ccTouchMoved(CCTouch* p0, CCEvent* p1) {

		bool snapped = false;

		// try to snap anchor
		if ((STATE.snap() || CCKeyboardDispatcher::get()->getControlKeyPressed()) && !snapped) {
			snapped = snapTouchToClosestButton(p0);
		}

		// try to snap grid
		if (STATE.gridSnap() && !snapped) {
			snapped = snapTouchToGrid(p0);
		}

		spriteByTag(m_transformButtonType)->setColor(snapped ? SNAP_COL : WHITE_COL);

		// free rotation
		if (m_fields->m_isInFreeRot) {
			auto location = convertToNodeSpace(p0->getLocation());
			float fVar15 = atan2f(location.y + m_cursorDifference.y, location.x + m_cursorDifference.x);
			float newRot = -fVar15 * 180 / M_PI - m_rotation;

			if (STATE.gridSnap()) { // rotSnap + freeRot
				float targetRot = roundf(newRot / 90) * 90;
				if (fabsf(targetRot - newRot) < 3) {
					newRot = targetRot;
					spriteByTag(12)->setColor(SNAP_COL);
				} else {
					spriteByTag(12)->setColor(WHITE_COL);
				}
			}

			m_mainNode->setRotation(newRot);
            GJTransformControl::updateButtons(false, false);
			return;
		} 
		
		// rotation + gridSnap
		if (m_transformButtonType == 12 && STATE.gridSnap()) {
			bool rotationSnapped = false;
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
			spriteByTag(12)->setColor(rotationSnapped ? SNAP_COL : WHITE_COL);
			if (!rotationSnapped) {
				GJTransformControl::ccTouchMoved(p0, p1);
			}
			return;
		}
		
		return GJTransformControl::ccTouchMoved(p0, p1);
	}

	$override 
	void ccTouchEnded(CCTouch* p0, CCEvent* p1) {

		resetAllColoredSprites();

		if (m_fields->m_isInFreeRot) { // exit freeRot
			auto editor = reinterpret_cast<MyEditorUI*>(EditorUI::get());
			auto rot = m_mainNode->getRotation();
			editor->deactivateTransformControl();
			editor->activateTransformControlWithAngle(rot);
			m_fields->m_isInFreeRot = false;
			m_transformButtonType = 0;
		} else {
			GJTransformControl::ccTouchEnded(p0, p1);
		}
		
		if (SETTINGS.m_showInterface == InterfaceMode::OnChange) {
			m_fields->m_interface->setInterfaceVisibility(false);
		}

		// updateBlockedSprites();
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


	// check if the anchor is aligned with the edges of rectangle or their extensions. 
	// returns the result and sets the spriteIndex
	bool checkAnchorIsOnEdge(const float limit, const CCPoint anchor, uint8_t* const spriteIndex) {
		// math code alert! - convert anchor pos to mainNode coords
		const double sin = std::sin(m_mainNode->getRotation()*M_PI/180.0);
		const double cos = std::cos(m_mainNode->getRotation()*M_PI/180.0);
		const auto anchorRelPos = ccp(
			cos * anchor.x - sin * anchor.y, sin * anchor.x + cos * anchor.y);
		// vertices cw
		CCPoint v[] = {spriteByTag(6)->getPosition(), spriteByTag(7)->getPosition(), 
						spriteByTag(9)->getPosition(), spriteByTag(8)->getPosition()};
		uint8_t alignedEdges = 0;
		// check all rect edges
		for (int A = 3, B = 0; B < 4; A = B++) {
			float ABx = v[B].x - v[A].x;
			float ABy = v[B].y - v[A].y;
			auto C = anchorRelPos;
			if (std::abs(ABx) > std::abs(ABy)) {
				float ACx = C.x - v[A].x;
				float ACy = C.y - v[A].y;
				float tg = ABy / ABx;
				float y = tg * ACx;
				if (std::abs(ACy - y) < limit) {
					alignedEdges |= 0b1000 >> B;
				}
			} else {
				float BCx = v[B].x - C.x;
				float BCy = v[B].y - C.y;
				float tg = ABx / ABy;
				float x = tg * BCy;
				if (std::abs(BCx - x) < limit) {
					alignedEdges |= 0b1000 >> B;
				}
			}
		}
		switch (alignedEdges) {
			case 0b1000: *spriteIndex = 2; break;
			case 0b0100: *spriteIndex = 4; break;
			case 0b0010: *spriteIndex = 3; break;
			case 0b0001: *spriteIndex = 5; break;
			case 0b1100: *spriteIndex = 6; break;
			case 0b0110: *spriteIndex = 7; break;
			case 0b0011: *spriteIndex = 9; break;
			case 0b1001: *spriteIndex = 8; break;
			default: return false;
		}
		return true;
	}


	$override 
	void scaleButtons(float scale) {
		GJTransformControl::scaleButtons(scale);

		// fix bug when scaled sprite doesn't match button touch box (scale not btn but menu)
		if (!m_fields->m_menu) return;
		m_fields->m_menu->setScale(scale * SETTINGS.m_buttonScale);
		m_warpLockButton->getChildByTag(1)->setScale(1.f);		
	}


	$override
	void updateButtons(bool p0, bool p1) {
		GJTransformControl::updateButtons(p0, p1);
		// keep buttons vertical
		const float angle = m_mainNode->getRotation();
		m_fields->m_snapBtn->setRotation(-angle);
		m_fields->m_gridSnapBtn->setRotation(-angle);
		m_fields->m_anchorBtn->setRotation(-angle);
		m_fields->m_freeRotBtn->setRotation(-angle);
		m_warpLockButton->setRotation(-angle);

		if (m_transformButtonType == 0) { // not in touch move
			updateBlockedSprites();
		}
	}


	// void refreshControl() {
	// 	GJTransformControl::refreshControl();
	// 	updateBlockedSprites();
	// }


	// ----------------------- ALL BUTTON HANDLERS -----------------------


	$override
	void onToggleLockScale(CCObject* sender) {
		GJTransformControl::onToggleLockScale(sender);
		STATE.m_lockXY = m_warpLocked;
	}


	void onToggleAnchorBtn(CCObject*) {
		auto anchor = spriteByTag(1);
		auto editor = EditorUI::get();

		if (STATE.m_enableAnchor = !STATE.m_enableAnchor) {
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

		updateBlockedSprites();
	}


	void onSnapBtn(CCObject*) {
		if (STATE.m_snap = !STATE.m_snap) {
			m_fields->m_snapBtn->setSprite(CCSprite::createWithSpriteFrameName(SNAP_ON_SPR));
		} else {
			m_fields->m_snapBtn->setSprite(CCSprite::createWithSpriteFrameName(SNAP_OFF_SPR));
		}
	}


	void onSnapGridBtn(CCObject*) {
		if (STATE.m_gridSnap = !STATE.m_gridSnap) {
			m_fields->m_gridSnapBtn->setSprite(CCSprite::createWithSpriteFrameName(GRID_SNAP_ON_SPR));
		} else {
			m_fields->m_gridSnapBtn->setSprite(CCSprite::createWithSpriteFrameName(GRID_SNAP_OFF_SPR));
		}
	}


	void onFreeRotBtn(CCObject*) {
		if (STATE.m_freeRot = !STATE.m_freeRot) {
			m_fields->m_freeRotBtn->setSprite(CCSprite::createWithSpriteFrameName(FREEROT_ON_SPR));
		} else {
			m_fields->m_freeRotBtn->setSprite(CCSprite::createWithSpriteFrameName(FREEROT_OFF_SPR));
		}
	}
};

