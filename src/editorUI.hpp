

class $modify(ITCEditorUI, EditorUI) {
	struct Fields {
		float m_initialAngle = 0;
		bool m_useAngle = false;
		std::unordered_map<UndoObject*, float> m_anglesForUndoObjects;
	};

	void addAngleToUndoObject(UndoObject* undo, float angle) {
		m_fields->m_anglesForUndoObjects[undo] = angle;
	}

	std::optional<float> getAngleForUndoObject(UndoObject* undo) {
		auto f = m_fields.self();

		// clear the dangling pointers (sometimes)
		int undoRedoAmount = m_editorLayer->m_undoObjects->count() + m_editorLayer->m_redoObjects->count();
		if (f->m_anglesForUndoObjects.size() > undoRedoAmount + 100) {
			std::unordered_set<void*> undoRedoArrays;
			undoRedoArrays.insert(undo);
			for (int i = 0; i < m_editorLayer->m_undoObjects->count(); i++) {
				undoRedoArrays.insert(m_editorLayer->m_undoObjects->objectAtIndex(i));
			}
			for (int i = 0; i < m_editorLayer->m_redoObjects->count(); i++) {
				undoRedoArrays.insert(m_editorLayer->m_redoObjects->objectAtIndex(i));
			}
			for (auto it = f->m_anglesForUndoObjects.begin(); it != f->m_anglesForUndoObjects.end(); ) {
				if (!undoRedoArrays.contains(it->first)) {
					it = f->m_anglesForUndoObjects.erase(it); // dangling ptr
				} else it++;
			}
		} // todo: maybe test it

		auto it = f->m_anglesForUndoObjects.find(undo);
		if (it == f->m_anglesForUndoObjects.end()) {
			return std::nullopt;
		}
		return it->second;
	}


	UndoObject* createTransfromNoScaleUndoObject(bool addToUndoList = true) {
		
		auto objects = getSelectedObjects();
		auto objectCopies = CCArray::create();
		for (int i = 0; i < objects->count(); i++) {
			auto obj = static_cast<GameObject*>(objects->objectAtIndex(i));
			auto objCopy = GameObjectCopy::create(obj);
			objectCopies->addObject(objCopy);
		}

		UndoObject* undo = new UndoObject();
		undo->autorelease();

		undo->m_redo = false;
		undo->m_objects = nullptr;
		undo->m_objectCopy = nullptr;
		undo->m_undoTransform = false;
		undo->m_command = UndoCommand::Transform;

		if (objectCopies->count() == 1) {
			undo->m_objectCopy = static_cast<GameObjectCopy*>(objectCopies->firstObject());
			undo->m_objectCopy->retain();
		} else {
			undo->m_objects = objectCopies;
			undo->m_objects->retain();
		}

		if (addToUndoList) {
			m_editorLayer->m_redoObjects->removeAllObjects();
			int maxUndo = m_editorLayer->m_increaseMaxUndoRedo ? 1000 : 200;
			if (m_editorLayer->m_undoObjects->count() >= maxUndo) {
				m_editorLayer->m_undoObjects->removeObjectAtIndex(0);
			}
			m_editorLayer->m_undoObjects->addObject(undo);
		}

		return undo;
	}

	// $override
	// void moveObject(GameObject* p0, CCPoint p1) {
	// 	if (std::isnan(p1.x) || std::isnan(p1.y)) return;
	// 	EditorUI::moveObject(p0, p1);
	// }

	// void onBtn1(CCObject*) {
	// 	log::debug("last undo obj--------------------------------");
	// 	auto obj = static_cast<UndoObject*>(m_editorLayer->m_undoObjects->lastObject());
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
		log::debug("activate w angle {}", angle);
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
			static_cast<ButtonSprite*>(m_rotateBtn->getNormalImage())->updateBGImage("GJ_button_01.png");
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
	// 	auto undo = m_editorLayer->m_undoObjects;
	// 	if (undo && undo->count()) {
	// 		auto rot1 = m_transformControl->m_mainNode->getRotation();
	// 		addAngleToUndoObject(static_cast<UndoObject*>(undo->lastObject()), rot1);
	// 	}
	// }


	$override
	UndoObject* createUndoObject(UndoCommand p0, bool p1) { // p1 - don't add to undo list
		UndoObject* ret = EditorUI::createUndoObject(p0, p1);
		if (ret && ret->m_command == UndoCommand::Transform && ret->m_undoTransform) {
			addAngleToUndoObject(ret, m_transformControl->m_mainNode->getRotation());
			log::info("angle added");
		}
		return ret;
	}


	// void anchorPointMoved(CCPoint newWorldCoords) {
	// 	// log::debug("anchor moved {}", newWorldCoords);
	// 	EditorUI::anchorPointMoved(newWorldCoords);
	// }


	// util: return true on success
	void universalUndoRedoHook(bool isUndo, std::function<void()> original) {
		CCArray* from = isUndo ? m_editorLayer->m_undoObjects : m_editorLayer->m_redoObjects;
		CCArray* to = isUndo ? m_editorLayer->m_redoObjects : m_editorLayer->m_undoObjects;
		bool originalWasCalled = false;

		// do we have something to undo?
		if (auto undoObj = static_cast<UndoObject*>(from->lastObject())) {

			// is that buggy transform command?
			if (undoObj->m_command == UndoCommand::Transform && undoObj->m_undoTransform) {
				
				// remember current redo head to check if it will be changed
				auto oldLastRedo = static_cast<UndoObject*>(to->lastObject());

				std::optional<float> setFreeRotValue;

				// is this my FreeRot undo action?
				if (auto maybeRot = getAngleForUndoObject(undoObj)) {

					float currentRot = m_transformControl->m_mainNode->getRotation();
					float targetRot = *maybeRot;

					if (m_transformControl->isVisible() && currentRot != targetRot) {
						deactivateTransformControl();
						original();
						originalWasCalled = true;
						activateTransformControlWithAngle(*maybeRot);

						// undo obj had the angle, so the next redo obj also must have it
						setFreeRotValue = currentRot;
					}
				} 

				if (!originalWasCalled) {
					original();
					originalWasCalled = true;
				}

				// now fix newly created redo object
	
				auto newLastRedo = static_cast<UndoObject*>(to->lastObject());

				if (newLastRedo && newLastRedo != oldLastRedo) {
					// RobTop's bug thai it sometimes is not set
					newLastRedo->m_undoTransform = true;
					newLastRedo->m_command = UndoCommand::Transform;

					// set FreeRot value if it was FreeRot mode
					if (setFreeRotValue) {
						addAngleToUndoObject(newLastRedo, *setFreeRotValue);
					}
				}
			}
		}
		
		if (!originalWasCalled) {
			original();
		}
	}


	// prevent undo/redo bugs
	$override
	void undoLastAction(CCObject* p0) {
		universalUndoRedoHook(true, [this, p0] {
			EditorUI::undoLastAction(p0);
		});
		// log::info("undo {} {}", m_editorLayer->m_undoObjects, m_editorLayer->m_redoObjects);
			
	}


	$override
	void redoLastAction(CCObject* p0) {
		universalUndoRedoHook(false, [this, p0] {
			EditorUI::redoLastAction(p0);
		});
		// log::info("redo {} {}", m_editorLayer->m_undoObjects, m_editorLayer->m_redoObjects);

	}


	#ifdef GEODE_IS_MACOS

	$override
	void keyDown(enumKeyCodes p0) {
		auto dispatcher = CCKeyboardDispatcher::get();
		if (m_editorLayer->m_playbackMode == PlaybackMode::Playing || p0 != KEY_Z || dispatcher->getControlKeyPressed()) {
			return EditorUI::keyDown(p0);
		}

		bool isUndo = !dispatcher->getShiftKeyPressed();
		universalUndoRedoHook(isUndo, [this, p0] {
			EditorUI::keyDown(p0);
		});
	}

	#endif

};
