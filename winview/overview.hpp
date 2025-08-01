#pragma once

#define WLR_USE_UNSTABLE

#include "globals.hpp"
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Framebuffer.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <vector>

// saves on resources, but is a bit broken rn with blur.
// hyprland's fault, but cba to fix.
constexpr bool ENABLE_LOWRES = false;

class CMonitor;

class COverview {
  public:
    COverview(PHLWINDOW startedOn_, bool swipe = false);
    ~COverview();

    void render();
    void damage();
    void onDamageReported();
    void onPreRender();

    void onSwipeUpdate(double delta);
    void onSwipeEnd();

    // close without a selection
    void          close();
    void          selectHoveredWindow();

    bool          blockOverviewRendering = false;
    bool          blockDamageReporting   = false;

    PHLMONITORREF pMonitor;

  private:
    void       redrawID(int id, bool forcelowres = false);
    void       redrawAll(bool forcelowres = false);
    void       onWindowChange();
    void       fullRender();

    int        SIDE_LENGTH = 3;
    int        GAP_WIDTH   = 5;
    CHyprColor BG_COLOR    = CHyprColor{0.1, 0.1, 0.1, 1.0};

    bool       damageDirty = false;

    struct SWindowImage {
        CFramebuffer fb;
        PHLWINDOW    pWindow;
        CBox         box;
        Vector2D     position; // Grid position (col, row)
    };

    Vector2D                     lastMousePosLocal = Vector2D{};

    int                          openedID  = -1;
    int                          closeOnID = -1;

    std::vector<SWindowImage> images;

    PHLWINDOW                focusedWindow;
    PHLWINDOW                pWindow;

    PHLANIMVAR<Vector2D>         size;
    PHLANIMVAR<Vector2D>         pos;

    bool                         closing = false;

    SP<HOOK_CALLBACK_FN>         mouseMoveHook;
    SP<HOOK_CALLBACK_FN>         mouseButtonHook;
    SP<HOOK_CALLBACK_FN>         touchMoveHook;
    SP<HOOK_CALLBACK_FN>         touchDownHook;

    bool                         swipe             = false;
    bool                         swipeWasCommenced = false;

    friend class COverviewPassElement;
};

inline std::unique_ptr<COverview> g_pOverview;
