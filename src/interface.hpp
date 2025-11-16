class GJTransformControlInterface : public CCNode {
private:
	GJTransformControl* m_transformControl;
	bool m_visibleRect = false;
	ccColor4B m_color;
public:
	static GJTransformControlInterface* create(GJTransformControl* transformControl, ccColor4B color) {
		auto ret = new GJTransformControlInterface();
		if (ret && ret->init(transformControl, color)) {
			ret->autorelease();
			return ret;
		}
		CC_SAFE_DELETE(ret);
		return nullptr;
	}

	bool init(GJTransformControl* transformControl, ccColor4B color) {
		m_transformControl = transformControl;
		m_color = color;
		setID("interface"_spr);
		return true;
	}

	void setInterfaceVisibility(bool rect) {
		m_visibleRect = rect;
	}

	void draw() override {
		if (m_visibleRect) {
			ccDrawColor4B(m_color);
			auto tl = m_transformControl->spriteByTag(6);
			auto br = m_transformControl->spriteByTag(9);
			auto t = m_transformControl->spriteByTag(4);
			auto b = m_transformControl->spriteByTag(5);
			auto l = m_transformControl->spriteByTag(2);
			auto r = m_transformControl->spriteByTag(3);
			ccDrawRect(tl->getPosition(), br->getPosition());
			ccDrawLine(t->getPosition(),b->getPosition());
			ccDrawLine(l->getPosition(), r->getPosition());
		}
	}
};

