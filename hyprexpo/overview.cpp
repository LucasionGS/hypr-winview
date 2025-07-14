#include "overview.hpp"
#include <any>
#define private public
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/AnimationManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#undef private
#include "OverviewPassElement.hpp"

static void damageMonitor(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) {
    g_pOverview->damage();
}

static void removeOverview(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) {
    g_pOverview.reset();
}

COverview::~COverview() {
    g_pHyprRenderer->makeEGLCurrent();
    images.clear(); // otherwise we get a vram leak
    g_pInputManager->unsetCursorImage();
    g_pHyprOpenGL->markBlurDirtyForMonitor(pMonitor.lock());
}

COverview::COverview(PHLWINDOW startedWindow, bool swipe_) : focusedWindow(startedWindow), swipe(swipe_), pWindow(startedWindow) {
    const auto PMONITOR = g_pCompositor->m_lastMonitor.lock();
    pMonitor            = PMONITOR;

    static auto* const* PCOLUMNS        = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:columns")->getDataStaticPtr();
    static auto* const* PGAPS           = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:gap_size")->getDataStaticPtr();
    static auto* const* PCOL            = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:bg_col")->getDataStaticPtr();
    static auto* const* PSKIP           = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:skip_empty")->getDataStaticPtr();
    static auto* const* PINCLUDESPECIAL = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:include_special")->getDataStaticPtr();
    static auto const*  PMETHOD         = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:workspace_method")->getDataStaticPtr();

    SIDE_LENGTH = **PCOLUMNS;
    GAP_WIDTH   = **PGAPS;
    BG_COLOR    = **PCOL;

    // Collect all windows from all workspaces on the current monitor
    std::vector<PHLWINDOW> allWindows;
    for (auto const& w : g_pCompositor->m_windows) {
        if (!w->m_isMapped || w->isHidden())
            continue;
            
        // Filter by monitor - only show windows on the current monitor
        if (w->m_monitor != pMonitor)
            continue;
            
        // Skip windows that don't want focus
        if (w->m_windowData.noFocus.valueOrDefault())
            continue;
            
        // Include special workspace windows based on config
        bool includeSpecial = **PINCLUDESPECIAL;
        if (w->onSpecialWorkspace() && !includeSpecial)
            continue;
            
        // Include all windows from all workspaces (no workspace filtering)
        allWindows.push_back(w);
    }
    
    // Calculate grid dimensions based on window count
    int windowCount = allWindows.size();
    int gridCols = **PCOLUMNS;
    int gridRows = (windowCount + gridCols - 1) / gridCols; // Ceiling division
    
    // Resize images vector to accommodate all windows
    images.resize(windowCount);
    
    // Create window tiles
    for (size_t i = 0; i < allWindows.size(); ++i) {
        auto& image = images[i];
        image.pWindow = allWindows[i];
        
        // Calculate grid position
        int col = i % gridCols;
        int row = i / gridCols;
        
        // Store position for rendering
        image.position = Vector2D(col, row);
    }
    
    g_pHyprRenderer->makeEGLCurrent();

    // Calculate tile size based on dynamic grid
    Vector2D tileSize = pMonitor->m_size / std::max(gridCols, gridRows);
    Vector2D tileRenderSize = (pMonitor->m_size - Vector2D{GAP_WIDTH * pMonitor->m_scale, GAP_WIDTH * pMonitor->m_scale} * (std::max(gridCols, gridRows) - 1)) / std::max(gridCols, gridRows);
    CBox     monbox{0, 0, tileSize.x * 2, tileSize.y * 2};

    if (!ENABLE_LOWRES)
        monbox = {{0, 0}, pMonitor->m_pixelSize};

    g_pHyprRenderer->m_bBlockSurfaceFeedback = true;

    // Render each window to its framebuffer
    for (size_t i = 0; i < images.size(); ++i) {
        auto& image = images[i];
        
        if (!image.pWindow)
            continue;

        // Create or get framebuffer for each window
    for (auto& image : images) {
        image.fb.alloc(monbox.w, monbox.h, pMonitor->m_drmFormat);

        CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};
        g_pHyprRenderer->beginRender(pMonitor, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &image.fb);

        g_pHyprOpenGL->clear(CHyprColor(0, 0, 0, 0)); // Clear to transparent

        // Render the window
        if (image.pWindow) {
            g_pHyprRenderer->renderWindow(image.pWindow, pMonitor, Time::steadyNow(), true, RENDER_PASS_ALL, false, true);
        }

        // Calculate tile position in the grid
        image.box = {image.position.x * tileRenderSize.x + image.position.x * GAP_WIDTH, 
                     image.position.y * tileRenderSize.y + image.position.y * GAP_WIDTH, 
                     tileRenderSize.x, tileRenderSize.y};

        g_pHyprOpenGL->m_renderData.blockScreenShader = true;
        g_pHyprRenderer->endRender();
    }

    g_pHyprRenderer->m_bBlockSurfaceFeedback = false;

    // Find index of the currently focused window
    int currentid = 0;
    for (size_t i = 0; i < images.size(); ++i) {
        if (images[i].pWindow == pWindow) {
            currentid = i;
            break;
        }
    }

    // Setup animations for the overview
    g_pAnimationManager->createAnimation(Vector2D{pMonitor->m_size} * pMonitor->m_size / tileSize, size, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_NONE);
    g_pAnimationManager->createAnimation((-((pMonitor->m_size / (double)gridCols) * Vector2D{currentid % gridCols, currentid / gridCols}) * pMonitor->m_scale) *
                                             (pMonitor->m_size / tileSize),
                                         pos, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_NONE);

    size->setUpdateCallback(damageMonitor);
    pos->setUpdateCallback(damageMonitor);

    if (!swipe) {
        *size = pMonitor->m_size;
        *pos  = {0, 0};

        size->setCallbackOnEnd([this](auto) { redrawAll(true); });
    }

    openedID = currentid;

    g_pInputManager->setCursorImageUntilUnset("left_ptr");

    lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;

    auto onCursorMove = [this](void* self, SCallbackInfo& info, std::any param) {
        if (closing)
            return;

        info.cancelled    = true;
        lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;
    };

    auto onCursorSelect = [this](void* self, SCallbackInfo& info, std::any param) {
        if (closing)
            return;

        info.cancelled = true;

        // get tile x,y based on dynamic grid
        int gridCols = **PCOLUMNS;
        int gridRows = (images.size() + gridCols - 1) / gridCols;
        
        int x = lastMousePosLocal.x / pMonitor->m_size.x * gridCols;
        int y = lastMousePosLocal.y / pMonitor->m_size.y * gridRows;

        closeOnID = x + y * gridCols;
        
        // Ensure we don't go out of bounds
        if (closeOnID >= (int)images.size())
            closeOnID = images.size() - 1;

        close();
    };

    mouseMoveHook = g_pHookSystem->hookDynamic("mouseMove", onCursorMove);
    touchMoveHook = g_pHookSystem->hookDynamic("touchMove", onCursorMove);

    mouseButtonHook = g_pHookSystem->hookDynamic("mouseButton", onCursorSelect);
    touchDownHook   = g_pHookSystem->hookDynamic("touchDown", onCursorSelect);
}

void COverview::selectHoveredWindow() {
    if (closing)
        return;

    // get tile x,y based on the dynamic grid layout
    int gridCols = **PCOLUMNS;
    int gridRows = (images.size() + gridCols - 1) / gridCols;
    
    int x = lastMousePosLocal.x / pMonitor->m_size.x * gridCols;
    int y = lastMousePosLocal.y / pMonitor->m_size.y * gridRows;
    
    closeOnID = x + y * gridCols;
    
    // Ensure we don't go out of bounds
    if (closeOnID >= (int)images.size())
        closeOnID = images.size() - 1;
}

void COverview::redrawID(int id, bool forcelowres) {
    blockOverviewRendering = true;

    g_pHyprRenderer->makeEGLCurrent();

    id = std::clamp(id, 0, (int)images.size() - 1);

    if (id >= (int)images.size())
        return;

    int gridCols = **PCOLUMNS;
    int gridRows = (images.size() + gridCols - 1) / gridCols;
    Vector2D tileSize = pMonitor->m_size / std::max(gridCols, gridRows);
    Vector2D tileRenderSize = (pMonitor->m_size - Vector2D{GAP_WIDTH, GAP_WIDTH} * (std::max(gridCols, gridRows) - 1)) / std::max(gridCols, gridRows);
    CBox     monbox{0, 0, tileSize.x * 2, tileSize.y * 2};

    if (!forcelowres && (size->value() != pMonitor->m_size || closing))
        monbox = {{0, 0}, pMonitor->m_pixelSize};

    if (!ENABLE_LOWRES)
        monbox = {{0, 0}, pMonitor->m_pixelSize};

    auto& image = images[id];

    if (!image.pWindow)
        return;

    if (image.fb.m_size != monbox.size()) {
        image.fb.release();
        image.fb.alloc(monbox.w, monbox.h, pMonitor->m_output->state->state().drmFormat);
    }

    CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};
    g_pHyprRenderer->beginRender(pMonitor, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &image.fb);

    g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 1.0});

    // Render the window
    g_pHyprRenderer->renderWindow(image.pWindow, pMonitor, Time::steadyNow(), true, RENDER_PASS_ALL, false, true);

    g_pHyprOpenGL->m_renderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();

    blockOverviewRendering = false;
}

void COverview::redrawAll(bool forcelowres) {
    for (size_t i = 0; i < images.size(); ++i) {
        redrawID(i, forcelowres);
    }
}

void COverview::damage() {
    blockDamageReporting = true;
    g_pHyprRenderer->damageMonitor(pMonitor);
    blockDamageReporting = false;
}

void COverview::onDamageReported() {
    damageDirty = true;

    Vector2D SIZE = size->value();
    int gridCols = **PCOLUMNS;
    int gridRows = (images.size() + gridCols - 1) / gridCols;

    Vector2D tileSize = (SIZE / std::max(gridCols, gridRows));
    Vector2D tileRenderSize = (SIZE - Vector2D{GAP_WIDTH, GAP_WIDTH} * (std::max(gridCols, gridRows) - 1)) / std::max(gridCols, gridRows);
    
    if (openedID >= 0 && openedID < (int)images.size()) {
        CBox texbox = CBox{(openedID % gridCols) * tileRenderSize.x + (openedID % gridCols) * GAP_WIDTH,
                          (openedID / gridCols) * tileRenderSize.y + (openedID / gridCols) * GAP_WIDTH, 
                          tileRenderSize.x, tileRenderSize.y}
                          .translate(pMonitor->m_position);

        damage();

        blockDamageReporting = true;
        g_pHyprRenderer->damageBox(texbox);
        blockDamageReporting = false;
        g_pCompositor->scheduleFrameForMonitor(pMonitor);
    }
}

void COverview::close() {
    if (closing)
        return;

    const int ID = closeOnID == -1 ? openedID : closeOnID;
    
    // Ensure ID is within bounds
    if (ID >= 0 && ID < (int)images.size()) {
        const auto& TILE = images[ID];
        
        // Focus the selected window
        if (TILE.pWindow && validMapped(TILE.pWindow)) {
            g_pCompositor->focusWindow(TILE.pWindow);
            g_pCompositor->warpCursorTo(TILE.pWindow->m_vRealPosition.value() + TILE.pWindow->m_vRealSize.value() / 2.0);
        }
    }

    // Calculate animation properties for the selected tile
    int gridCols = **PCOLUMNS;
    Vector2D tileSize = (pMonitor->m_size / gridCols);

    *size = pMonitor->m_size * pMonitor->m_size / tileSize;
    *pos  = (-((pMonitor->m_size / (double)gridCols) * Vector2D{ID % gridCols, ID / gridCols}) * pMonitor->m_scale) * (pMonitor->m_size / tileSize);

    size->setCallbackOnEnd(removeOverview);

    closing = true;

    redrawAll();
}

void COverview::onPreRender() {
    if (damageDirty) {
        damageDirty = false;
        redrawID(closing ? (closeOnID == -1 ? openedID : closeOnID) : openedID);
    }
}

void COverview::onWindowChange() {
    // Update the focused window in our list if needed
    auto focusedWindow = g_pCompositor->m_pLastWindow.lock();
    if (focusedWindow && focusedWindow->m_pMonitor == pMonitor) {
        for (size_t i = 0; i < images.size(); ++i) {
            if (images[i].pWindow == focusedWindow) {
                openedID = i;
                break;
            }
        }
    }
    
    closeOnID = openedID;
    close();
}

void COverview::render() {
    g_pHyprRenderer->m_renderPass.add(makeUnique<COverviewPassElement>());
}

void COverview::fullRender() {
    const auto GAPSIZE = (closing ? (1.0 - size->getPercent()) : size->getPercent()) * GAP_WIDTH;

    Vector2D SIZE = size->value();

    int gridCols = **PCOLUMNS;
    int gridRows = (images.size() + gridCols - 1) / gridCols;
    Vector2D tileRenderSize = (SIZE - Vector2D{GAPSIZE, GAPSIZE} * (std::max(gridCols, gridRows) - 1)) / std::max(gridCols, gridRows);

    g_pHyprOpenGL->clear(BG_COLOR.stripA());

    for (size_t i = 0; i < images.size(); ++i) {
        int x = i % gridCols;
        int y = i / gridCols;
        
        CBox texbox = {x * tileRenderSize.x + x * GAPSIZE, y * tileRenderSize.y + y * GAPSIZE, tileRenderSize.x, tileRenderSize.y};
        texbox.scale(pMonitor->m_scale).translate(pos->value());
        texbox.round();
        CRegion damage{0, 0, INT16_MAX, INT16_MAX};
        g_pHyprOpenGL->renderTextureInternalWithDamage(images[i].fb.getTexture(), texbox, 1.0, damage);
    }
}

static float lerp(const float& from, const float& to, const float perc) {
    return (to - from) * perc + from;
}

static Vector2D lerp(const Vector2D& from, const Vector2D& to, const float perc) {
    return Vector2D{lerp(from.x, to.x, perc), lerp(from.y, to.y, perc)};
}

void COverview::onSwipeUpdate(double delta) {
    if (swipeWasCommenced)
        return;

    static auto* const* PDISTANCE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:gesture_distance")->getDataStaticPtr();

    const float         PERC = 1.0 - std::clamp(delta / (double)**PDISTANCE, 0.0, 1.0);

    int gridCols = **PCOLUMNS;
    Vector2D tileSize = (pMonitor->m_size / gridCols);

    const auto SIZEMAX = pMonitor->m_size * pMonitor->m_size / tileSize;
    const auto POSMAX =
        (-((pMonitor->m_size / (double)gridCols) * Vector2D{openedID % gridCols, openedID / gridCols}) * pMonitor->m_scale) * (pMonitor->m_size / tileSize);

    const auto SIZEMIN = pMonitor->m_size;
    const auto POSMIN  = Vector2D{0, 0};

    size->setValueAndWarp(lerp(SIZEMIN, SIZEMAX, PERC));
    pos->setValueAndWarp(lerp(POSMIN, POSMAX, PERC));
}

void COverview::onSwipeEnd() {
    const auto SIZEMIN = pMonitor->m_size;
    int gridCols = **PCOLUMNS;
    const auto SIZEMAX = pMonitor->m_size * pMonitor->m_size / (pMonitor->m_size / gridCols);
    const auto PERC    = (size->value() - SIZEMIN).x / (SIZEMAX - SIZEMIN).x;
    if (PERC > 0.5) {
        close();
        return;
    }
    *size = pMonitor->m_size;
    *pos  = {0, 0};

    size->setCallbackOnEnd([this](WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) { redrawAll(true); });

    swipeWasCommenced = true;
}
