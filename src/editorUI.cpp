class $modify(MyEditorUI, EditorUI) {
	struct Fields {
		float m_initialAngle = 0;
		bool m_useAngle = false;
		std::unordered_map<UndoObject*, float> m_transformAnglesForUndoObjects;
	};

	void addAngleToUndoObject(UndoObject* undo, float angle) {
		if (angle == 0) return;
		m_fields->m_transformAnglesForUndoObjects[undo] = angle;
	}

	float getAngleForUndoObject(UndoObject* undo) {
		auto it = m_fields->m_transformAnglesForUndoObjects.find(undo);
		return it == m_fields->m_transformAnglesForUndoObjects.end() ? 0.f : it->second;
	}

	// $override
	// void moveObject(GameObject* p0, CCPoint p1) {
	// 	if (std::isnan(p1.x) || std::isnan(p1.y)) return;
	// 	EditorUI::moveObject(p0, p1);
	// }

	// void onBtn1(CCObject*) {
	// 	log::debug("last undo obj--------------------------------");
	// 	auto obj = static_cast<UndoObject*>(LevelEditorLayer::get()->m_undoObjects->lastObject());
	// 	if (obj->m_command == UndoCommand::Transform) {
	// 		auto t = obj->m_transformState;
	// 		log::debug("objects.count()={}; undoTransform={}", obj->m_objects->count(), obj->m_undoTransform);
	// 		log::debug("=======================================");
	// 		log::debug("GameObject copy {}", obj->m_objectCopy);
	// 		if (auto oc = obj->m_objectCopy) {
	// 			log::debug("m_object={}", oc->m_object);
	// 			log::debug("m_position={}", oc->m_position);
	// 			log::debug("m_rotationX={}", oc->m_rotationX);
	// 			log::debug("m_rotationY={}", oc->m_rotationY);
	// 			log::debug("m_isFlipX={}", oc->m_isFlipX);
	// 			log::debug("m_isFlipY={}", oc->m_isFlipY);
	// 			log::debug("m_customScaleX={}", oc->m_customScaleX);
	// 			log::debug("m_customScaleY={}", oc->m_customScaleY);
	// 		}
	// 		log::debug("=======================================");
	// 		log::debug("m_scaleX={}", t.m_scaleX);
	// 		log::debug("m_scaleY={}", t.m_scaleY);
	// 		log::debug("m_angleX={}", t.m_angleX);
	// 		log::debug("m_angleY={}", t.m_angleY);
	// 		log::debug("m_skewX={}", t.m_skewX);
	// 		log::debug("m_skewY={}", t.m_skewY);
	// 		log::debug("m_transformRotation={}", t.m_transformRotation);
	// 		log::debug("m_transformReset={}", t.m_transformReset);
	// 		log::debug("m_transformRotationX={}", t.m_transformRotationX);
	// 		log::debug("m_transformRotationY={}", t.m_transformRotationY);
	// 		log::debug("m_transformPosition={}", t.m_transformPosition);
	// 		log::debug("m_transformSkewX={}", t.m_transformSkewX);
	// 		log::debug("m_transformSkewY={}", t.m_transformSkewY);
	// 		log::debug("m_transformScaleX={}", t.m_transformScaleX);
	// 		log::debug("m_transformScaleY={}", t.m_transformScaleY);
	// 	} else {
	// 		log::debug("wrong undo command {}", (int)obj->m_command);
	// 	}
	// }

	$override 
	void transformObjects(CCArray* objs, CCPoint anchor, float scaleX, float scaleY, 
							float rotX, float rotY, float warpX, float warpY) {
		// fix RobTop's crash with extremely thin objects
		if (warpX == 45 && warpY == 45) {
			transformSkewXChanged(44.9f);
			transformSkewYChanged(44.9f);
		} else if (warpX == -45 && warpY == -45) {
			transformSkewXChanged(-44.9f);
			transformSkewYChanged(-44.9f);
		}
		EditorUI::transformObjects(objs, anchor, scaleX, scaleY, rotX, rotY, warpX, warpY);
		return;
	}


	void activateTransformControlWithAngle(float angle) {
		// log::debug("activate w angle {}", angle);
		m_fields->m_initialAngle = angle;
		m_fields->m_useAngle = true;
		EditorUI::deactivateRotationControl();
		EditorUI::activateTransformControl(nullptr);
		m_fields->m_useAngle = false;
        m_transformControl->refreshControl();
	}


	$override
	void activateTransformControl(CCObject* p0) {
		// force toggle off rotation mode (fix to RobTop's bugs)
		if (GameManager::get()->getGameVariable("0007")) {
			GameManager::get()->setGameVariable("0007", false);
			static_cast<ButtonSprite*>(m_rotateBtn->getChildByTag(1))->updateBGImage("GJ_button_01.png");
			deactivateRotationControl();
		}
		EditorUI::activateTransformControl(p0);
	}


	$override
	void updateTransformControl() {
		auto selected = getSelectedObjects();
		if (!m_fields->m_useAngle || selected->count() == 0) {
			return EditorUI::updateTransformControl();
		}

		auto mainObj = static_cast<GameObject*>(selected->objectAtIndex(0));
		int id = mainObj->m_uniqueID;
		float rot = m_fields->m_initialAngle;

		float realRotX = mainObj->getRotationX();
		float realRotY = mainObj->getRotationY();
		mainObj->setRotation(rot);
		
		EditorUI::updateTransformControl();
		
		mainObj->setRotationX(realRotX);
		mainObj->setRotationY(realRotY);

		if (m_objectEditorStates.contains(id)) {
			auto &st = m_objectEditorStates.at(id);
			st.m_rotationX = realRotX - rot;
			st.m_rotationY = realRotY - rot;
		}
	}


	// ! For some reason just existence of this hook crashes the game on Android
	// $override
	// void transformChangeBegin() {
	// 	EditorUI::transformChangeBegin(); // this function add undo object
	// 	auto undo = LevelEditorLayer::get()->m_undoObjects;
	// 	if (undo && undo->count()) {
	// 		auto rot1 = m_transformControl->m_mainNode->getRotation();
	// 		addAngleToUndoObject(static_cast<UndoObject*>(undo->lastObject()), rot1);
	// 	}
	// }


	$override
	UndoObject* createUndoObject(UndoCommand p0, bool p1) {
		UndoObject* ret = EditorUI::createUndoObject(p0, p1);
		if (p0 == UndoCommand::Transform && p1) {
			auto undo = LevelEditorLayer::get()->m_undoObjects;
			if (undo && undo->count()) {
				auto rot1 = m_transformControl->m_mainNode->getRotation();
				addAngleToUndoObject(static_cast<UndoObject*>(undo->lastObject()), rot1);
			}
		}
		return ret;
	}


	// void anchorPointMoved(CCPoint newWorldCoords) {
	// 	// log::debug("anchor moved {}", newWorldCoords);
	// 	EditorUI::anchorPointMoved(newWorldCoords);
	// }


	// prevent undo/redo bugs
	$override
	void undoLastAction(CCObject* p0) {
		auto undo = LevelEditorLayer::get()->m_undoObjects;
		if (m_transformControl->isVisible() && undo && undo->count()) {
			deactivateTransformControl();
			float rot = getAngleForUndoObject(static_cast<UndoObject*>(undo->lastObject()));
			EditorUI::undoLastAction(p0);
			activateTransformControlWithAngle(rot);
		} else {
			EditorUI::undoLastAction(p0);
		}
	}


	$override
	void redoLastAction(CCObject* p0) {
		auto redo = LevelEditorLayer::get()->m_redoObjects;
		if (m_transformControl->isVisible() && redo && redo->count()) {
			deactivateTransformControl();
			float rot = getAngleForUndoObject(static_cast<UndoObject*>(redo->lastObject()));
			EditorUI::redoLastAction(p0);
			activateTransformControlWithAngle(rot);
		} else {
			EditorUI::redoLastAction(p0);
		}
	}

	#ifdef GEODE_IS_MACOS
	$override
	void keyDown(enumKeyCodes p0) {
		auto dispatcher = CCKeyboardDispatcher::get();
		if (m_editorLayer->m_playbackMode == PlaybackMode::Playing || p0 != KEY_Z || dispatcher->getControlKeyPressed()) {
			return EditorUI::keyDown(p0);
		}

		auto undo = dispatcher->getShiftKeyPressed() ? m_editorLayer->m_redoObjects : m_editorLayer->m_undoObjects;
		if (m_transformControl->isVisible() && undo && undo->count()) {
			deactivateTransformControl();
			float rot = getAngleForUndoObject(static_cast<UndoObject*>(undo->lastObject()));
			EditorUI::keyDown(p0);
			activateTransformControlWithAngle(rot);
		} else {
			EditorUI::keyDown(p0);
		}
	}
	#endif

};
