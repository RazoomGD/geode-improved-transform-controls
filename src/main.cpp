#include <cmath>
#include <Geode/Geode.hpp>
#include <Geode/modify/GJTransformControl.hpp>
#include <Geode/modify/EditorUI.hpp>
using namespace geode::prelude;

struct {
	bool m_isFreeRot = false;
	void reset() {
		m_isFreeRot = false;
	}
} GLOBAL;

class $modify(MyGJTransformControl, GJTransformControl) {
	struct Fields {
		bool m_isSnap = false;
		CCMenu* m_menu;
		CCMenuItemSpriteExtra* m_snapBtn;
		CCMenuItemSpriteExtra* m_rotBtn;
	};

	$override bool init() {
		if (!GJTransformControl::init()) return false;
		// fix menu anchor and button overlapping
		m_fields->m_menu = static_cast<CCMenu*>(this->m_warpLockButton->getParent());
		m_fields->m_menu->setAnchorPoint(ccp(0,0));
		m_warpLockButton->setPosition(ccp(0, 20));

		// add new buttons to the menu
		m_fields->m_snapBtn = CCMenuItemSpriteExtra::create(
			CCSprite::createWithSpriteFrameName("warpLockOffBtn_001.png"), 
			this, menu_selector(MyGJTransformControl::onSnapBtn));
		m_fields->m_rotBtn = CCMenuItemSpriteExtra::create(
			CCSprite::createWithSpriteFrameName("warpLockOffBtn_001.png"), 
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

		GLOBAL.m_isFreeRot = false;

		return true;
	}

	// return true and write snap coords to snapCoords if anchor snaps
	bool checkAnchorSnaps(const float limit, const CCPoint anchor, CCPoint* const snapCoords) {
		// (math code alert!) convert anchor pos to mainNode coords
		const double sin = std::sin(m_mainNode->getRotation()*M_PI/180.0);
		const double cos = std::cos(m_mainNode->getRotation()*M_PI/180.0);
		const auto anchorRelPos = ccp(cos * anchor.x - sin * anchor.y, sin * anchor.x + cos * anchor.y);
		// check snap
		for (int i = 2; i < 10; i++) {
			auto node = spriteByTag(i);
			auto distVec = anchorRelPos - node->getPosition();
			auto distSq = distVec.x * distVec.x + distVec.y * distVec.y;
			if (distSq < limit * limit) {
				auto nodePos = node->getPosition();
				*snapCoords = ccp(cos * nodePos.x + sin * nodePos.y, -sin * nodePos.x + cos * nodePos.y);
				return true;
			}
		}
		return false;
	}
	
	$override void ccTouchMoved(CCTouch* p0, CCEvent* p1) {
		GJTransformControl::ccTouchMoved(p0, p1);
		if (!m_fields->m_isSnap) return;

		auto anchor = spriteByTag(1);
		auto aPos = anchor->getPosition();
		if (aPos.x == 0 && aPos.y == 0) return;
		// anchor was moved
		float limit = anchor->getScale() * 18; // min dist after which the anchor snaps to the node

		if (checkAnchorSnaps(limit, aPos, &aPos)) {
			anchor->setColor(ccc3(255, 135, 0));
			anchor->setPosition(aPos);
			updateButtons(false, false);
		} else {
			anchor->setColor(ccc3(255, 255, 255));
		}
	}

	$override void ccTouchEnded(CCTouch* p0, CCEvent* p1) {
		GJTransformControl::ccTouchEnded(p0, p1);
		spriteByTag(1)->setColor(ccc3(255, 255, 255));
	}

	$override void ccTouchCancelled(CCTouch* p0, CCEvent* p1) {
		GJTransformControl::ccTouchCancelled(p0, p1);
		spriteByTag(1)->setColor(ccc3(255, 255, 255));
	}

	$override void scaleButtons(float scale) {
		GJTransformControl::scaleButtons(scale);
		// fix scaled sprite doesn't match button touch box (scale not btn but menu)
		if (!m_fields->m_menu) return;
		m_fields->m_menu->setScale(scale);
		auto btn = m_warpLockButton->getChildByTag(1);
		btn->setScale(1.f);		
	}

	$override void updateButtons(bool p0, bool p1) {
		log::debug("call buttons update {} {}", p0, p1);
		// GLOBAL.m_forceAllowDefaultTransform = true;
		GJTransformControl::updateButtons(p0, p1);
		log::debug("ret buttons update {} {}", p0, p1);
	// 	// GLOBAL.m_forceAllowDefaultTransform = false;
	// 	// auto editor = EditorUI::get();

	// 	// log::debug("custom updated -");
	// 	// CCArray* selected;
	// 	// if (editor->m_selectedObjects == nullptr || editor->m_selectedObjects->count() == 0) {
	// 	// 	if (editor->m_selectedObject == nullptr) return;
	// 	// 	selected = CCArray::createWithObject(editor->m_selectedObject);
	// 	// } else {
	// 	// 	selected = editor->m_selectedObjects;
	// 	// 	if (selected == nullptr) return;
	// 	// }
	// 	// editor->transformObjects(selected, editor->m_pivotPoint, 
	// 	// 	editor->m_transformState.m_scaleX, editor->m_transformState.m_scaleY, 
	// 	// 	editor->m_transformState.m_angleX - 45, editor->m_transformState.m_angleY - 45,
	// 	// 	editor->m_transformState.m_skewX, editor->m_transformState.m_skewY);
	// 	// // editor->transformObjects(selected, editor->m_pivotPoint, 1, 1, 30, 30, 0, 0);
	// 	// log::debug("custom updated");
	}

	void onSnapBtn(CCObject* sender) {
		m_fields->m_isSnap = !m_fields->m_isSnap;
		auto spr = m_fields->m_isSnap ? "warpLockOnBtn_001.png" : "warpLockOffBtn_001.png";
		m_fields->m_snapBtn->setSprite(CCSprite::createWithSpriteFrameName(spr));

		// auto editor = EditorUI::get();
		// auto scSt = &editor->m_transformState;
		
		// GLOBAL.m_suppressTransform = m_fields->m_isSnap;
		// log::debug("array {}", this->m_unk1);
		// log::debug("array {}", editor->m_selectedObjects);
		// log::debug("equal {}", editor->m_selectedObjects->objectAtIndex(0) == m_unk1->objectAtIndex(0));
		// static GJTransformState trSt;
		// if (GLOBAL.m_suppressTransform) {
		// 	trSt = editor->m_transformState;
			
		// } else {
		// 	editor->m_transformState = trSt;
		// }
		// angleX, angleY - rot
		// unk1 - rot
		// unk4 - prev. rot
		// log::debug("state 1: {} {} {} {} {} {}", scSt->m_scaleX, scSt->m_scaleY, scSt->m_angleX, scSt->m_angleY, scSt->m_skewX, scSt->m_skewY);
		// log::debug("state 2: {} {} {} {} {} {}", scSt->m_unk1, scSt->m_unk2, scSt->m_unk3, scSt->m_unk4, scSt->m_unk8, scSt->m_unk9);
		// log::debug("state 3: {} {} {}", scSt->m_unk5, scSt->m_unk6, scSt->m_unk7);

	}

	void onRotBtn(CCObject* sender) {
		GLOBAL.m_isFreeRot = !GLOBAL.m_isFreeRot;
		auto spr = GLOBAL.m_isFreeRot ? "warpLockOnBtn_001.png" : "warpLockOffBtn_001.png";
		m_fields->m_rotBtn->setSprite(CCSprite::createWithSpriteFrameName(spr));
	}
};


class $modify(MyEditorUI, EditorUI) {

	struct Fields {
		bool m_isActivate = false; // is activateTransformControl func on the call stack
		bool m_isSneaky = false; // is the sneakyObj used
		Ref<GameObject> m_sneakyObj;
		Fields() {
			GLOBAL.reset();
			m_sneakyObj = GameObject::createWithKey(929);
			m_sneakyObj->commonSetup();
			m_sneakyObj->m_outerSectionIndex = -1;
		}
	};

	// todo: debug func
	void toggleSnap(CCObject* p) {
		// auto tmpObj = static_cast<GameObject*>(m_selectedObjects->objectAtIndex(0));
		// if(tmpObj == nullptr) return;
		// log::debug("startpos={}", tmpObj->m_startPosition);
		// log::debug("pos     ={}", tmpObj->getPosition());
		// m_transformControl->m_mainNode->setVisible(!m_transformControl->m_mainNode->isVisible());
		// log::debug("unk220 {} {}", m_unk220->getPosition(), m_unk220->getRotation());
		// log::debug("unk224 {} {}", m_unk224->getPosition(), m_unk224->getRotation());
		// log::debug("id {}", m_selectedObject->m_uniqueID);

	}

	$override void transformObjects(CCArray* objs, CCPoint anchor, float scaleX, float scaleY, float rotX, float rotY, float warpX, float warpY) {
		log::debug("call transform objs {}", rotX);
		if (m_fields->m_isSneaky) {
			m_selectedObjects->fastRemoveObjectAtIndex(0); // remove sneakyObj
			if (m_selectedObjects->count() == 1) {
				auto obj = m_selectedObjects->objectAtIndex(0);
				m_selectedObjects->removeObjectAtIndex(0);
				m_selectedObject = static_cast<GameObject*>(obj);
				objs = CCArray::createWithObject(obj);
			}
			log::debug("-sneaky");
			m_fields->m_isSneaky = false;
		}
		EditorUI::transformObjects(objs, anchor, scaleX, scaleY, rotX, rotY, warpX, warpY);
		log::debug("ret transform objs {}", rotX);
		return;
	}

	$override void updateTransformControl() {
		float rot = 45;
		log::debug("call update controls");
		if (m_fields->m_isActivate) {
			// means this function is called from activateTransformControl()
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
				m_fields->m_isSneaky = true;

			} else if (m_selectedObjects != nullptr && m_selectedObjects->count() > 0) {
				// selected 2+ objects
				auto first = static_cast<GameObject*>(m_selectedObjects->firstObject());
				m_selectedObjects->addObject(first);
				m_selectedObjects->replaceObjectAtIndex(0, m_fields->m_sneakyObj);
				m_fields->m_sneakyObj->setPosition(first->getPosition());
				m_fields->m_sneakyObj->setRotation(rot);
				m_fields->m_isSneaky = true;
			} else {
				// nothing is selected
			}
		}

		EditorUI::updateTransformControl();

		// in case transformObjects() was not reached
		if (m_fields->m_isSneaky) {
			m_selectedObjects->fastRemoveObjectAtIndex(0); // remove sneakyObj
			if (m_selectedObjects->count() == 1) {
				auto obj = m_selectedObjects->objectAtIndex(0);
				m_selectedObjects->removeObjectAtIndex(0);
				m_selectedObject = static_cast<GameObject*>(obj);
			}
			m_fields->m_isSneaky = false;
		}

		log::debug("ret update controls");
	}

	$override void activateTransformControl(CCObject* p0) {
		log::debug("call activate controls");

		m_fields->m_isActivate = true;
		EditorUI::activateTransformControl(p0);
		m_fields->m_isActivate = false;

		log::debug("ret activate controls");
	}

};
