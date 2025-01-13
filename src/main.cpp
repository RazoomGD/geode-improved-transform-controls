#include <cmath>
#include <Geode/Geode.hpp>
#include <Geode/modify/GJTransformControl.hpp>
#include <Geode/modify/EditorUI.hpp>
using namespace geode::prelude;

#define SNAP_COL ccc3(255, 135, 0)
#define LOCK_COL ccc3(155,155,155)
#define WHITE_COL ccc3(255, 255, 255)

struct MyGJTransformControl;

struct {
	bool m_isSnap = false;
	bool m_isFreeRot = false;
	bool m_isRotDirty = false;
	float m_freeRotFinalAngle = 0;
	MyGJTransformControl* m_transformControls = nullptr;
} GLOBAL;

/*
Transform controls scheme: (each sprite has a unique index)

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
		float m_lockedRotation = 0; // last value of rotation before it's been locked
		CCMenu* m_menu;
		CCMenuItemSpriteExtra* m_rotBtn;
		CCMenuItemSpriteExtra* m_snapBtn;
		uint16_t m_disabledSpritesRot = 0;  // sprites disabled because of free rotation or snap
		uint16_t m_disabledSpritesSnap = 0; // both arebit arrays (lowest 12 bits used - one for each sprite)

		~Fields() {GLOBAL.m_transformControls = nullptr;}
	};

	inline uint16_t getDisabledSprites() {
		return m_fields->m_disabledSpritesSnap | m_fields->m_disabledSpritesRot;
	}

	$override bool init() {
		if (!GJTransformControl::init()) return false;
		GLOBAL.m_transformControls = this;

		// fix menu sprite 10 and button overlapping 
		m_fields->m_menu = static_cast<CCMenu*>(this->m_warpLockButton->getParent());
		m_fields->m_menu->setAnchorPoint(ccp(0,0));
		m_warpLockButton->setPosition(ccp(0, 20));

		// add new buttons to the menu
		m_fields->m_snapBtn = CCMenuItemSpriteExtra::create(
			CCSprite::createWithSpriteFrameName("warpLockOffBtn_001.png"), 
			this, menu_selector(MyGJTransformControl::onSnapBtn));
		m_fields->m_rotBtn = CCMenuItemSpriteExtra::create(
			CCSprite::createWithSpriteFrameName("freeRotOffBtn_001.png"_spr), 
			this, menu_selector(MyGJTransformControl::onRotBtn));
		
		m_fields->m_menu->addChild(m_fields->m_snapBtn);
		m_fields->m_menu->addChild(m_fields->m_rotBtn);

		m_fields->m_snapBtn->setPosition(ccp(30, 20));
		m_fields->m_rotBtn->setPosition(ccp(60, 20));
		
		// add labels to the buttons
		auto labelSnap = CCLabelBMFont::create("Snap", "bigFont.fnt");
		auto labelPos = CCLabelBMFont::create("ScaleXY", "bigFont.fnt");
		auto labelRot = CCLabelBMFont::create("FreeRot", "bigFont.fnt");

		m_fields->m_snapBtn->addChildAtPosition(labelSnap, Anchor::Bottom);
		m_fields->m_rotBtn->addChildAtPosition(labelRot, Anchor::Bottom);
		m_warpLockButton->addChildAtPosition(labelPos, Anchor::Bottom);

		labelSnap->setScale(.2f);
		labelRot->setScale(.2f);
		labelPos->setScale(.2f);

		// reset global state
		GLOBAL.m_isFreeRot = false;
		GLOBAL.m_isRotDirty = false;
		GLOBAL.m_isSnap = false;

		return true;
	}

	// I call this before EditorUI::activateTransformControls()
	void prepareToActivate() {
		m_fields->m_disabledSpritesSnap = 0;
		m_fields->m_disabledSpritesRot = 0;
		if (GLOBAL.m_isFreeRot) {
			m_fields->m_rotBtn->setSprite(CCSprite::createWithSpriteFrameName("freeRotOffBtn_001.png"_spr));
			GLOBAL.m_isFreeRot = false;
		}
	}

	void updateDisabledSprites() {
		uint16_t mask = 0b100000000000;
		const uint16_t disabled = getDisabledSprites();
		for(int i = 1; i < 13; i++) {
			auto spr = spriteByTag(i);
			// check if the sprite is disabled
			spr->setColor((mask & disabled) ? LOCK_COL : WHITE_COL);
			mask = mask >> 1;
		}
	}

	void setDisabledSpritesByNodeIndex(short indx) {
		switch (indx) {
			case 2: m_fields->m_disabledSpritesSnap = 0b010001010000; break; // 2,6,8
			case 3: m_fields->m_disabledSpritesSnap = 0b001000101000; break; // 3,7,9
			case 4: m_fields->m_disabledSpritesSnap = 0b000101100000; break; // 4,6,7
			case 5: m_fields->m_disabledSpritesSnap = 0b000010011000; break; // 5,8,9
			case 6: m_fields->m_disabledSpritesSnap = 0b010101110000; break; // 2,4,6,7,8
			case 7: m_fields->m_disabledSpritesSnap = 0b001101101000; break; // 3,4,6,7,9
			case 8: m_fields->m_disabledSpritesSnap = 0b010011011000; break; // 2,5,6,8,9
			case 9: m_fields->m_disabledSpritesSnap = 0b001010111000; break; // 3,5,7,8,9
			default: m_fields->m_disabledSpritesSnap = 0; break;
		}
	}

	void checkAndUpdateDisabledSpritesForCurrentAnchorPosition() {
		const auto anchor = spriteByTag(1);
		auto aPos = anchor->getPosition();
		uint8_t snapNodeIndx = 0;

		checkAnchorIsOnEdge(0.05, aPos, &snapNodeIndx);

		setDisabledSpritesByNodeIndex(snapNodeIndx);

		updateDisabledSprites();
	}

	// return true and set snapCoords and snapSpriteIndex if anchor snaps
	bool checkAnchorSnaps(const float limit, const CCPoint anchor, CCPoint* const snapCoords, 
							uint8_t* const spriteIndex) {
		// math code alert! - convert anchor pos to mainNode coords
		const double sin = std::sin(m_mainNode->getRotation()*M_PI/180.0);
		const double cos = std::cos(m_mainNode->getRotation()*M_PI/180.0);
		const auto anchorRelPos = ccp(cos * anchor.x - sin * anchor.y, sin * anchor.x + cos * anchor.y);
		// check distance between the anchor and the sprite
		for (int i = 2; i < 10; i++) {
			auto node = spriteByTag(i);
			auto distVec = anchorRelPos - node->getPosition();
			auto distSq = distVec.x * distVec.x + distVec.y * distVec.y;
			if (distSq < limit * limit) {
				auto nodePos = node->getPosition();
				*snapCoords = ccp(cos * nodePos.x + sin * nodePos.y, -sin * nodePos.x + cos * nodePos.y);
				*spriteIndex = i;
				return true;
			}
		}
		return false;
	}

	// check if the anchor is aligned with the edges of rectangle or their extensions. 
	// returns the result and sets the spriteIndex
	bool checkAnchorIsOnEdge(const float limit, const CCPoint anchor, uint8_t* const spriteIndex) {
		// math code alert! - convert anchor pos to mainNode coords
		const double sin = std::sin(m_mainNode->getRotation()*M_PI/180.0);
		const double cos = std::cos(m_mainNode->getRotation()*M_PI/180.0);
		const auto anchorRelPos = ccp(cos * anchor.x - sin * anchor.y, sin * anchor.x + cos * anchor.y);
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
	
	$override void ccTouchMoved(CCTouch* p0, CCEvent* p1) {
		if (m_touchID != p0->m_nId) return;

		// check if the current button is disabled, don't allow to use it
		if (((uint16_t)0b1000000000000 >> m_transformButtonType) & getDisabledSprites()) {
			return;
		}

		GJTransformControl::ccTouchMoved(p0, p1);

		// check anchor snaps
		if (m_transformButtonType == 1) { // anchor
			if (GLOBAL.m_isSnap) {
				// check anchor snap
				const auto anchor = spriteByTag(1);
				auto aPos = anchor->getPosition();
				// min dist after which the anchor snaps to the node
				const float limit = anchor->getScale() * 18;
				uint8_t snapNodeIndx;
				if (checkAnchorSnaps(limit, aPos, &aPos, &snapNodeIndx)) {
					// anchor was moved and we've just attached to the node
					anchor->setColor(SNAP_COL);
					anchor->setPosition(aPos);
				} else {
					anchor->setColor(WHITE_COL);
				}
			}
		
		} else if (m_transformButtonType == 12) { // rot
			if (GLOBAL.m_isSnap) {
				// check rotation snap
				const auto rotator = spriteByTag(12);
				const float rot = m_mainNode->getRotation();
				const int rotDiff = (int)(std::abs(rot) + 0.5) % 90;
				const int deadzone = 2;
				if (rotDiff <= deadzone || rotDiff >= 90 - deadzone) {
					// make obj rot multiple of 90
					int newRot = ((int)rot + 10 * (rot > 0 ? 1 : -1)) / 90 * 90;
					m_mainNode->setRotation(newRot);
					if (!GLOBAL.m_isFreeRot) {
						EditorUI::get()->transformRotationChanged(newRot);
					}
					rotator->setColor(SNAP_COL);
				} else {
					rotator->setColor(WHITE_COL);
				}
			}
			if (GLOBAL.m_isFreeRot) {
				GLOBAL.m_isRotDirty = true;
				EditorUI::get()->transformRotationChanged(m_fields->m_lockedRotation);
			}
			
		}
	}

	$override void ccTouchEnded(CCTouch* p0, CCEvent* p1) {
		// check what sprites should be disabled depending on where the anchor snaps
		// (we have to "disable" sprites that are aligned with the anchor because
		// otherwise we will get the infinite scale when try to use them. In worst case
		// this will cause the zero-division crash in RobTop's code)
		const auto anchor = spriteByTag(1);
		auto aPos = anchor->getPosition();
		uint8_t snapNodeIndx = 0;

		if (m_transformButtonType == 1 && GLOBAL.m_isSnap) { 
			float limit = anchor->getScale() * 18; // min dist after which the anchor snaps to the node
			checkAnchorSnaps(limit, aPos, &aPos, &snapNodeIndx);
		} else {
			// some sprites may still stay aligned with the anchor even after transform
			checkAnchorIsOnEdge(0.05, aPos, &snapNodeIndx);
		}

		setDisabledSpritesByNodeIndex(snapNodeIndx);

		updateDisabledSprites();
		
		GJTransformControl::ccTouchEnded(p0, p1);
	}

	$override void scaleButtons(float scale) {
		GJTransformControl::scaleButtons(scale);
		// fix bug when scaled sprite doesn't match button touch box (scale not btn but menu)
		if (!m_fields->m_menu) return;
		m_fields->m_menu->setScale(scale);
		auto btn = m_warpLockButton->getChildByTag(1);
		btn->setScale(1.f);		
	}

	void onSnapBtn(CCObject* sender) {
		GLOBAL.m_isSnap = !GLOBAL.m_isSnap;
		auto spr = GLOBAL.m_isSnap ? "warpLockOnBtn_001.png" : "warpLockOffBtn_001.png";
		m_fields->m_snapBtn->setSprite(CCSprite::createWithSpriteFrameName(spr));
	}

	void onRotBtn(CCObject* sender) {
		GLOBAL.m_isFreeRot = !GLOBAL.m_isFreeRot;
		if (GLOBAL.m_isFreeRot) {
			m_fields->m_rotBtn->setSprite(CCSprite::createWithSpriteFrameName("freeRotOnBtn_001.png"_spr));
			m_fields->m_disabledSpritesRot = 0b011111111110; // 1...11
			m_fields->m_lockedRotation = m_mainNode->getRotation();
		} else {
			m_fields->m_rotBtn->setSprite(CCSprite::createWithSpriteFrameName("freeRotOffBtn_001.png"_spr));
			m_fields->m_disabledSpritesRot = 0;
			if (GLOBAL.m_isRotDirty) {
				GLOBAL.m_freeRotFinalAngle = m_mainNode->getRotation();
				EditorUI::get()->deactivateTransformControl();
				EditorUI::get()->activateTransformControl(nullptr);
				return;
			}
		}
		updateDisabledSprites();
	}
};


class $modify(MyEditorUI, EditorUI) {
	// The purpose here is to implement free rotation for transform 
	// controls. This means that you would be able to rotate the interface 
	// while the selected objects stay in place. 
	// The rotation value is set as soon as transform mode is activated inside the
	// updateTransformControl() function. And it is equal to the rotation of the first 
	// object in selection. 
	// So the way the code below works is it puts an object with desired rotation in the first 
	// position in selected objects array. Then this value is used to set the rotation for
	// the transform controls, then this object is removed from array.

	// P.S. I tried many other things to get this work, but it seems impossible to do 
	// without affecting other parts of transform system (which either break everything or 
	// don't let anything change). 
	// And only this trick worked fine.

	struct Fields {
		bool m_isActivate = false; // is activateTransformControl func on the call stack
		bool m_isSneaky = false; // is the fake main object used
		Ref<GameObject> m_sneakyObj;
		Fields() {
			GLOBAL.m_isSnap = false;
			GLOBAL.m_isFreeRot = false;
			GLOBAL.m_isRotDirty = false;
			m_sneakyObj = GameObject::createWithKey(929);
			m_sneakyObj->commonSetup();
			m_sneakyObj->m_outerSectionIndex = -1;
		}
	};

	$override 
	void transformObjects(CCArray* objs, CCPoint anchor, float scaleX, float scaleY, 
							float rotX, float rotY, float warpX, float warpY) {
		// we don't need this object anymore
		if (m_fields->m_isSneaky) {
			popFakeMainObject();
			m_fields->m_isSneaky = false;
		}
		EditorUI::transformObjects(objs, anchor, scaleX, scaleY, rotX, rotY, warpX, warpY);
		return;
	}

	// adds a new object with given rotation to the selection
	bool pushFakeMainObjectWithRotation(float rot) {
		if (m_selectedObject != nullptr) {
			// selected 1 obj
			if (m_selectedObjects == nullptr) {
				m_selectedObjects = CCArray::create();
				m_selectedObjects->retain();
			}
			m_selectedObjects->addObject(m_fields->m_sneakyObj);
			m_selectedObjects->addObject(m_selectedObject);
			m_fields->m_sneakyObj->setPosition(m_selectedObject->getPosition());
			m_fields->m_sneakyObj->setRotation(rot);
			m_selectedObject = nullptr;

		} else if (m_selectedObjects != nullptr && m_selectedObjects->count() > 0) {
			// selected 2+ objects
			auto first = static_cast<GameObject*>(m_selectedObjects->firstObject());
			m_selectedObjects->addObject(first);
			m_selectedObjects->replaceObjectAtIndex(0, m_fields->m_sneakyObj);
			m_fields->m_sneakyObj->setPosition(first->getPosition());
			m_fields->m_sneakyObj->setRotation(rot);
		} else {
			// nothing is selected
			return false;
		}
		return true;
	}

	// removes the first object from selection
	void popFakeMainObject() {
		// object stays at the array less than a frame, so assume it can't be removed
		m_selectedObjects->fastRemoveObjectAtIndex(0); // remove sneakyObj
		if (m_selectedObjects->count() == 1) {
			auto obj = m_selectedObjects->objectAtIndex(0);
			m_selectedObjects->removeObjectAtIndex(0);
			m_selectedObject = static_cast<GameObject*>(obj);
		}
	}

	$override 
	void updateTransformControl() {
		// if the function is called from activateTransformControl()
		if (m_fields->m_isActivate && GLOBAL.m_isRotDirty) {
			float rot = GLOBAL.m_freeRotFinalAngle;
			if (pushFakeMainObjectWithRotation(rot)) {
				m_fields->m_isSneaky = true;
			}
			GLOBAL.m_isRotDirty = false;
		}

		EditorUI::updateTransformControl();

		// in case transformObjects() was not reached
		if (m_fields->m_isSneaky) {
			popFakeMainObject();
			m_fields->m_isSneaky = false;
		}
	}

	// prevent undo/redo bugs
	$override
	void undoLastAction(CCObject* p0) {
		EditorUI::undoLastAction(p0);
		if (auto controls = GLOBAL.m_transformControls) {
			if (controls->isVisible()) {
				controls->checkAndUpdateDisabledSpritesForCurrentAnchorPosition();
			}
		}
	}

	$override
	void redoLastAction(CCObject* p0) {
		EditorUI::redoLastAction(p0);
		if (auto controls = GLOBAL.m_transformControls) {
			if (controls->isVisible()) {
				controls->checkAndUpdateDisabledSpritesForCurrentAnchorPosition();
			}
		}
	}

	$override 
	void activateTransformControl(CCObject* p0) {
		if (auto controls = GLOBAL.m_transformControls) {
			controls->prepareToActivate();
		}

		m_fields->m_isActivate = true;
		EditorUI::activateTransformControl(p0);
		m_fields->m_isActivate = false;

		GLOBAL.m_isRotDirty = false;

		if (auto controls = GLOBAL.m_transformControls) {
			if (controls->isVisible()) {
				controls->updateDisabledSprites();
			}
		}
	}

};
