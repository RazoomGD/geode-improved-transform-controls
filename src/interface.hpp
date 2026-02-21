// bit flags
enum ITCExtraUIElement { 
	ITC_UI_LineTop      = 1,
	ITC_UI_LineBottom   = 2,
	ITC_UI_LineLeft     = 4,
	ITC_UI_LineRight    = 8,
	ITC_UI_LineVertical = 16,
	ITC_UI_LineHorizon  = 32,
	ITC_UI_Circle       = 64,
	ITC_UI_LineCenterX  = 128,
	ITC_UI_LineCenterY  = 256,
};


class GJTransformControlInterface : public CCNode {
private:
	GJTransformControl* m_transformControl;
	bool m_visibleRect = false;
	uint32_t m_extraUIElements = 0;
	ccColor4B m_color;
	ccColor4B m_colorExtra;

public:
	static GJTransformControlInterface* create(GJTransformControl* transformControl, ccColor4B color, ccColor4B colorExtra) {
		auto ret = new GJTransformControlInterface();
		if (ret && ret->init(transformControl, color, colorExtra)) {
			ret->autorelease();
			return ret;
		}
		CC_SAFE_DELETE(ret);
		return nullptr;
	}

	bool init(GJTransformControl* transformControl, ccColor4B color, ccColor4B colorExtra) {
		m_transformControl = transformControl;
		m_color = color;
		m_colorExtra = colorExtra;
		setID("interface"_spr);
		setZOrder(-1);
		return true;
	}

	void setInterfaceVisibility(bool isVisible) {
		m_visibleRect = isVisible;
	}

	void setExtraUIElementsForActiveSprite(int sprite) {
		switch (sprite) {
			case 1: m_extraUIElements = 0; break;
			case 2: m_extraUIElements = ITC_UI_LineLeft | ITC_UI_LineHorizon; break;
			case 3: m_extraUIElements = ITC_UI_LineRight | ITC_UI_LineHorizon; break;
			case 4: m_extraUIElements = ITC_UI_LineTop | ITC_UI_LineVertical; break;
			case 5: m_extraUIElements = ITC_UI_LineBottom | ITC_UI_LineVertical; break;
			case 6: m_extraUIElements = ITC_UI_LineTop | ITC_UI_LineLeft; break;
			case 7: m_extraUIElements = ITC_UI_LineTop | ITC_UI_LineRight; break;
			case 8: m_extraUIElements = ITC_UI_LineBottom | ITC_UI_LineLeft; break;
			case 9: m_extraUIElements = ITC_UI_LineBottom | ITC_UI_LineRight; break;
			case 10: m_extraUIElements = ITC_UI_LineLeft | ITC_UI_LineRight; break;
			case 11: m_extraUIElements = ITC_UI_LineBottom | ITC_UI_LineTop; break;
			case 12: m_extraUIElements = ITC_UI_Circle | ITC_UI_LineCenterX | ITC_UI_LineCenterY; break;
			default: m_extraUIElements = 0; break;
		}
	}

	void drawLongLine(CCPoint a, CCPoint b) {
		float minLen = 1000 / LevelEditorLayer::get()->m_objectLayer->getScale();
		auto vec = b - a;
		auto len = std::sqrtf(vec.x * vec.x + vec.y * vec.y);
		if (!len) return;
		float mul = minLen / len / 2;
		ccDrawLine(a - vec * mul, b + vec * mul);
	}

	void draw() override {
		// if (m_transformControl->m_objects && m_transformControl->m_objects->count()) {
		// 	auto offsets = getBoxOffsets(m_transformControl->m_objects);
		// 	ccDrawColor4B(ccc4(234, 53, 12, 255));
		// 	float sc = LevelEditorLayer::get()->m_objectLayer->getScale();

		// 	auto maxY = offsets.top - m_transformControl->getPositionY();
		// 	auto minY = offsets.bottom - m_transformControl->getPositionY();
		// 	auto maxX = offsets.right - m_transformControl->getPositionX();
		// 	auto minX = offsets.left - m_transformControl->getPositionX();

		// 	// log::info("{}", offsets.top - offsets.bottom);
		// 	ccDrawLine(ccp(minX, minY), ccp(minX, maxY));
		// 	ccDrawLine(ccp(minX, maxY), ccp(maxX, maxY));
		// 	ccDrawLine(ccp(maxX, maxY), ccp(maxX, minY));
		// 	ccDrawLine(ccp(maxX, minY), ccp(minX, minY));
		// 	ccDrawLine(ccp(maxX, maxY), ccp(minX, minY));
		// }

		if (!m_visibleRect && !m_extraUIElements) return;

		auto br = m_transformControl->spriteByTag(9);
		auto bl = m_transformControl->spriteByTag(8);
		auto tr = m_transformControl->spriteByTag(7);
		auto tl = m_transformControl->spriteByTag(6);
		auto b = m_transformControl->spriteByTag(5);
		auto t = m_transformControl->spriteByTag(4);
		auto r = m_transformControl->spriteByTag(3);
		auto l = m_transformControl->spriteByTag(2);

		if (m_extraUIElements) {
			glLineWidth(1.f);
			ccDrawColor4B(m_colorExtra);
			
			if (m_extraUIElements & ITC_UI_LineTop) drawLongLine(tl->getPosition(), tr->getPosition());
			if (m_extraUIElements & ITC_UI_LineBottom) drawLongLine(bl->getPosition(), br->getPosition());
			if (m_extraUIElements & ITC_UI_LineLeft) drawLongLine(tl->getPosition(), bl->getPosition());
			if (m_extraUIElements & ITC_UI_LineRight) drawLongLine(tr->getPosition(), br->getPosition());
			if (m_extraUIElements & ITC_UI_LineVertical) drawLongLine(t->getPosition(), b->getPosition());
			if (m_extraUIElements & ITC_UI_LineHorizon) drawLongLine(l->getPosition(), r->getPosition());
			if (m_extraUIElements & ITC_UI_LineCenterX) drawLongLine(ccp(0, -10), ccp(0, 10));
			if (m_extraUIElements & ITC_UI_LineCenterY) drawLongLine(ccp(-10, 0), ccp(10, 0));
	
			if (m_extraUIElements & ITC_UI_Circle) {
				auto vec = m_transformControl->spriteByTag(12)->getPosition();
				auto radius = std::sqrtf(vec.x * vec.x + vec.y * vec.y);
				float tcAngle = m_transformControl->m_mainNode->getRotation() / 180.f * M_PI;
				ccDrawCircle(ccp(0,0), radius, tcAngle, 60, false);
			}
		}

		if (m_visibleRect) {
			glLineWidth(1.f);
			ccDrawColor4B(m_color);

			ccDrawRect(tl->getPosition(), br->getPosition());
			ccDrawLine(t->getPosition(),b->getPosition());
			ccDrawLine(l->getPosition(), r->getPosition());
		}
	}
};

