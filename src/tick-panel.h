#pragma once
#include <vgui/IClientPanel.h>
#include <vgui/IPanel.h>
#include <vgui/IVGui.h>

// An invisible VGUI receiver keeps engine calls on the client thread.
// Based on CastingEssentialsNext's StubPanel (tick registration and cleanup):
// https://github.com/drunderscore/CastingEssentialsNext/blob/f14d10f5dc5065f7798c081433a276e46b6c38c6/CastingEssentials/Controls/StubPanel.h#L7-L57
class TickPanel final : public vgui::IClientPanel {
public:
    bool Start(CreateInterfaceFn factory, void (*callback)()) {
        gui_ = static_cast<vgui::IVGui*>(factory(VGUI_IVGUI_INTERFACE_VERSION, nullptr));
        auto panels = static_cast<vgui::IPanel*>(factory(VGUI_PANEL_INTERFACE_VERSION, nullptr));
        if (!gui_ || !panels) return false;

        panel_ = gui_->AllocPanel();
        if (!panel_) return false;

        callback_ = callback;
        panels->Init(panel_, this);
        panels->SetVisible(panel_, false);
        gui_->AddTickSignal(panel_);

        return true;
    }

    void Stop() {
        if (!panel_) return;

        gui_->RemoveTickSignal(panel_);
        gui_->FreePanel(panel_);

        panel_ = 0;
        callback_ = nullptr;
    }

    vgui::VPANEL GetVPanel() override { return panel_; }
    void OnTick() override { if (callback_) callback_(); }
    void OnMessage(const KeyValues*, vgui::VPANEL) override {}

    void Think() override {}
    void PerformApplySchemeSettings() override {}
    void PaintTraverse(bool, bool) override {}
    void Repaint() override {}
    vgui::VPANEL IsWithinTraverse(int, int, bool) override { return 0; }
    void GetInset(int& a, int& b, int& c, int& d) override { a = b = c = d = 0; }
    void GetClipRect(int& a, int& b, int& c, int& d) override { a = b = c = d = 0; }
    void OnChildAdded(vgui::VPANEL) override {}
    void OnSizeChanged(int, int) override {}

    void InternalFocusChanged(bool) override {}
    bool RequestInfo(KeyValues*) override { return false; }
    void RequestFocus(int) override {}
    bool RequestFocusPrev(vgui::VPANEL) override { return false; }
    bool RequestFocusNext(vgui::VPANEL) override { return false; }
    vgui::VPANEL GetCurrentKeyFocus() override { return 0; }
    int GetTabPosition() override { return 0; }

    const char* GetName() override { return "JF Spec IPC"; }
    const char* GetClassName() override { return "JFSpecTick"; }

    vgui::HScheme GetScheme() override { return 0; }
    bool IsProportional() override { return false; }
    bool IsAutoDeleteSet() override { return false; }
    void DeletePanel() override {}

    void* QueryInterface(vgui::EInterfaceID) override { return nullptr; }
    vgui::Panel* GetPanel() override { return nullptr; }
    const char* GetModuleName() override { return "jf_spec"; }

private:
    vgui::IVGui* gui_ = nullptr;
    vgui::VPANEL panel_ = 0;
    void (*callback_)() = nullptr;
};
