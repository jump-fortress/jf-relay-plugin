#include "ipc.h"
#include "tick-panel.h"
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <string>

#include <cdll_int.h>
#include <engine/iserverplugin.h>
#include <icvar.h>
#include <tier1/convar.h>
#include <tier1/tier1.h>
#include <tier0/icommandline.h>

namespace {
IVEngineClient* engineClient = nullptr;
Ranking ranking;
Ipc ipc(ranking);
TickPanel tickPanel;
int lastUserid = 0;
int lastRank = 0;
bool paused = false;

bool Current(Snapshot& snapshot, bool requireRanks = true) {
    if (paused || !engineClient || !engineClient->IsInGame()) {
        Msg("[JF Spec] Not connected or plugin paused.\n");
        return false;
    }
    if (!engineClient->IsHLTV()) {
        Msg("[JF Spec] This version supports live SourceTV spectating only.\n");
        return false;
    }

    snapshot = ranking.Read();
    const auto age = std::chrono::steady_clock::now() - snapshot.received;
    if (snapshot.map.empty() || age >= std::chrono::seconds(15)) {
        Msg("[JF Spec] Progress feed unavailable or stale.\n");
        return false;
    }

    const std::string map = std::filesystem::path(engineClient->GetLevelName()).stem().string();
    if (map != snapshot.map) {
        Msg("[JF Spec] Feed map does not match current map.\n");
        return false;
    }
    if (requireRanks && snapshot.userids.empty()) {
        Msg("[JF Spec] No active runners.\n");
        return false;
    }
    return true;
}

void SelectUserid(int userid, int rank) {
    bool found = false;
    for (int index = 1; index <= engineClient->GetMaxClients(); ++index) {
        player_info_t player{};
        if (engineClient->GetPlayerInfo(index, &player) && player.userID == userid) {
            found = true;
            break;
        }
    }
    if (!found) {
        Msg("[JF Spec] Target userid is not present in the client player list.\n");
        return;
    }

    char command[64];
    std::snprintf(command, sizeof(command), "spec_player #%d", userid);
    engineClient->ClientCmd_Unrestricted(command);
    lastUserid = userid;
    lastRank = rank;
    if (rank > 0) {
        Msg("[JF Spec] Selected userid %d (progress position %d).\n", userid, rank);
    } else {
        Msg("[JF Spec] Selected userid %d (timer stopped).\n", userid);
    }
}

void Select(const Snapshot& snapshot, int rank) {
    if (rank < 1 || rank > static_cast<int>(snapshot.userids.size())) {
        Msg("[JF Spec] Progress position not available.\n");
        return;
    }

    SelectUserid(snapshot.userids[rank - 1], rank);
}

void RankCommand(const CCommand& args) {
    int rank = 0;
    if (args.ArgC() != 2) {
        Msg("Usage: jf_spec_rank <progress position>\n");
        return;
    }

    const std::string value = args.Arg(1);
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), rank);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || rank < 1) {
        Msg("Usage: jf_spec_rank <progress position>\n");
        return;
    }

    Snapshot snapshot;
    if (Current(snapshot)) Select(snapshot, rank);
}

void NextCommand() {
    Snapshot snapshot;
    if (Current(snapshot)) Select(snapshot, RelativeRank(snapshot.userids, lastUserid, lastRank, 1));
}

void PrevCommand() {
    Snapshot snapshot;
    if (Current(snapshot)) Select(snapshot, RelativeRank(snapshot.userids, lastUserid, lastRank, -1));
}

void LastCommand() {
    Snapshot snapshot;
    if (Current(snapshot)) Select(snapshot, static_cast<int>(snapshot.userids.size()));
}

void StatusCommand() {
    const auto snapshot = ranking.Read();
    const double age = snapshot.map.empty() ? -1.0
        : std::chrono::duration<double>(std::chrono::steady_clock::now() - snapshot.received).count();
    Msg("[JF Spec] map=%s runners=%d age=%.1fs last-userid=%d last-progress-position=%d paused=%d\n",
        snapshot.map.c_str(), static_cast<int>(snapshot.userids.size()), age, lastUserid, lastRank, paused);
}

void ConsumeSelection() {
    const int userid = ranking.TakeSelection();
    if (!userid) return;

    Snapshot snapshot;
    if (!Current(snapshot, false)) return;

    if (std::find(snapshot.selectableUserids.begin(), snapshot.selectableUserids.end(), userid)
        == snapshot.selectableUserids.end()) return;

    const auto found = std::find(snapshot.userids.begin(), snapshot.userids.end(), userid);
    const int rank = found == snapshot.userids.end() ? 0 : static_cast<int>(found - snapshot.userids.begin()) + 1;
    SelectUserid(userid, rank);
}

ConCommand rankCommand("jf_spec_rank", RankCommand, "Spectate current map progress position N");
ConCommand nextCommand("jf_spec_next_rank", NextCommand, "Spectate next map progress position");
ConCommand prevCommand("jf_spec_prev_rank", PrevCommand, "Spectate previous map progress position");
ConCommand lastCommand("jf_spec_last", LastCommand, "Spectate last map progress position");
ConCommand statusCommand("jf_spec_status", StatusCommand, "Show competition spectator feed status");
}

class JfSpecPlugin final : public IServerPluginCallbacks {
public:
    bool Load(CreateInterfaceFn factory, CreateInterfaceFn) override {
        if (!factory || engineClient) return false;

        engineClient = static_cast<IVEngineClient*>(factory(VENGINE_CLIENT_INTERFACE_VERSION, nullptr));
        if (!engineClient) {
            Warning("[JF Spec] Client engine interface unavailable.\n");
            return false;
        }

        ConnectTier1Libraries(&factory, 1);
        const char* instance = CommandLine()->ParmValue("-jf_spec_instance", "A");

        if (!g_pCVar || !instance[0] || instance[1] || !ipc.Start(instance[0]) || !tickPanel.Start(factory, ConsumeSelection)) {
            tickPanel.Stop();
            ipc.Stop();

            Warning("[JF Spec] Console interface or local IPC listener unavailable (another instance or stale socket?).\n");
            DisconnectTier1Libraries();
            engineClient = nullptr;
            return false;
        }

        paused = false;
        lastUserid = lastRank = 0;
        ConVar_Register();

        Msg("     ██╗███████╗\n"
            "     ██║██╔════╝\n"
            "     ██║█████╗  \n"
            "██   ██║██╔══╝  \n"
            "╚█████╔╝██║     \n"
            " ╚════╝ ╚═╝     \n"
            "JF Spec Extension v2 - Successfully Loaded (Instance %s)\n", instance);

        return true;
    }

    void Unload() override {
        tickPanel.Stop();
        ConVar_Unregister();
        ipc.Stop();

        DisconnectTier1Libraries();
        engineClient = nullptr;
        lastUserid = lastRank = 0;
    }

    void Pause() override { paused = true; }
    void UnPause() override { paused = false; }
    const char* GetPluginDescription() override { return "Jump Fortress spectator rank commands 0.1"; }
    void LevelInit(const char*) override {}
    void ServerActivate(edict_t*, int, int) override {}
    void GameFrame(bool) override {}
    void LevelShutdown() override {
        ranking.Clear();
        lastUserid = lastRank = 0;
    }
    void ClientActive(edict_t*) override {}
    void ClientDisconnect(edict_t*) override {}
    void ClientPutInServer(edict_t*, const char*) override {}
    void SetCommandClient(int) override {}
    void ClientSettingsChanged(edict_t*) override {}
    PLUGIN_RESULT ClientConnect(bool*, edict_t*, const char*, const char*, char*, int) override { return PLUGIN_CONTINUE; }
    PLUGIN_RESULT ClientCommand(edict_t*, const CCommand&) override { return PLUGIN_CONTINUE; }
    PLUGIN_RESULT NetworkIDValidated(const char*, const char*) override { return PLUGIN_CONTINUE; }
    void OnQueryCvarValueFinished(QueryCvarCookie_t, edict_t*, EQueryCvarValueStatus, const char*, const char*) override {}
    void OnEdictAllocated(edict_t*) override {}
    void OnEdictFreed(const edict_t*) override {}
};

static JfSpecPlugin plugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(JfSpecPlugin, IServerPluginCallbacks,
    INTERFACEVERSION_ISERVERPLUGINCALLBACKS, plugin);
