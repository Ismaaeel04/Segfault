// ============================================================================
// ai_app.cpp  -  SegFault AI as an app on the desktop: the same tutor as the
// side panel (ai_panel.cpp), full screen, for questions that aren't about the
// code on some other screen.
// ============================================================================
#include "app/app.h"
#include "app/ui.h"

using namespace ui;

namespace {

class AiScene : public Scene {
public:
    const char* Title() const override { return "SegFault AI"; }
    ai::Screen AiInfo() const override { ai::Screen s; s.label = "SegFault AI"; return s; }
    void Enter(const std::string&) override { app.aiOpen = false; FocusWidget(9102); }
    void Frame(float) override { AiFullScreen({W / 2 - 430, 80, 860, H - 96}); }
};

} // namespace

Scene* CreateAiScene() { return new AiScene(); }
