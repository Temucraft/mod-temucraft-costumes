/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

// Costumes: look like a creature of the world. Only creatures whose model has every animation a player body needs
// (walk, run, strafe, jump, swim, combat, casting, sit, mount, loot, sleep, kneel...) can be worn, so a costume never
// slides, freezes or T-poses. The allowed displays are listed in `mod_temucraft_costume_display` (made from the
// client's own models by tools/animcheck.py).
//
// - Unlocked per account: defeating a creature (or talking to a friendly one) unlocks its costume, or every costume
//   is open (Temucraft.Costumes.Unlock).
// - .temucraft costume opens the wardrobe: costumes by creature type, pages, a name search, and "Take it off".
// - The look stays through shapeshifts and mounting; it ends with "Take it off", .temucraft costume off, logging out, or
//   entering a battleground or arena (Temucraft.Costumes.InPvP = 0).
// - With the Temucraft interface (TemucraftCostumes.lua, addon prefix TCO) .temucraft costume opens a wardrobe window with a
//   model preview, categories and a search box; without it, the same wardrobe as a gossip menu.
//   Addon -> server: HELLO, LIST, WEAR~display, OFF. Server -> addon: OPEN, BEGIN~all~worn~count, IDS~a,b,c..., END,
//   WORN~display (0 = none), NEW~display.

#include "Chat.h"
#include "CommandScript.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "StringFormat.h"
#include "TemucraftAddonMessage.h"
#include "TemucraftModuleText.h"
#include "WorldSession.h"
#include <algorithm>
#include <cstdlib>
#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace Acore::ChatCommands;

namespace
{
    constexpr char const* MODULE = "mod-temucraft-costumes";
    constexpr uint32 SENDER_MAIN = 0x7C00, SENDER_PAGE = 0x7C10, SENDER_WEAR = 0x7C20;
    constexpr uint32 ACTION_MAIN = 0, ACTION_OFF = 1, ACTION_SEARCH = 2;
    constexpr uint32 PER_PAGE = 18;

    enum Text : uint32
    {
        T_OFF = 1, T_HEAD, T_CATEGORY, T_TAKE_OFF, T_SEARCH, T_SEARCH_BOX, T_NEXT, T_PREV, T_BACK, T_WEARING, T_TOOK_OFF,
        T_UNLOCKED, T_NONE, T_NO_PVP, T_NO_MATCH, T_NOT_WEARING, T_GM_UNLOCKED, T_UNKNOWN
    };
    char const* Say(Player* p, uint32 id, char const* fallback) { return TemucraftModuleText(MODULE, p, id, fallback); }

    struct Settings
    {
        bool enable = true;
        bool all = false;            // every costume open
        bool unlockKill = true, unlockTalk = true;
        bool inPvP = false;
        bool announce = true;
    } cfg;

    struct Costume { std::string name; uint8 type = 0; };
    std::unordered_map<uint32, Costume> catalog;                    // display -> costume (only allowed displays)
    std::unordered_map<uint32, std::vector<uint32>> byCreature;      // creature entry -> its allowed displays
    std::unordered_map<uint32, std::unordered_set<uint32>> owned;    // account -> displays
    std::unordered_map<uint32, uint32> wearing;                      // player guid -> display
    std::unordered_set<uint32> setting;                              // players whose display we are setting
    std::unordered_set<uint32> withAddon;                            // players whose client has the wardrobe window
    constexpr char const* PREFIX = "TCO";

    void SendWorn(Player* p)
    {
        auto it = wearing.find(p->GetGUID().GetCounter());
        TemucraftAddonSend(p, PREFIX, "WORN~" + std::to_string(it == wearing.end() ? 0 : it->second));
    }

    // creature types, in menu order (CreatureType values)
    std::vector<std::pair<uint8, char const*>> const TYPES = {
        { 7, "Humanoids" }, { 1, "Beasts" }, { 2, "Dragonkin" }, { 3, "Demons" }, { 4, "Elementals" },
        { 5, "Giants" }, { 6, "Undead" }, { 9, "Mechanical" }, { 10, "Other" }
    };
    uint8 MenuType(uint8 t) { for (auto const& [k, n] : TYPES) if (k == t) return t; return 10; }

    void LoadSettings()
    {
        cfg.enable = sConfigMgr->GetOption<bool>("Temucraft.Costumes.Enable", true);
        std::string unlock = sConfigMgr->GetOption<std::string>("Temucraft.Costumes.Unlock", "kill,talk");
        cfg.all = unlock.find("all") != std::string::npos;
        cfg.unlockKill = unlock.find("kill") != std::string::npos;
        cfg.unlockTalk = unlock.find("talk") != std::string::npos;
        cfg.inPvP = sConfigMgr->GetOption<bool>("Temucraft.Costumes.InPvP", false);
        cfg.announce = sConfigMgr->GetOption<bool>("Temucraft.Costumes.AnnounceUnlock", true);
    }

    // the catalog: allowed displays that a creature of the world uses, named after that creature
    void LoadCatalog()
    {
        catalog.clear();
        byCreature.clear();
        QueryResult r = WorldDatabase.Query(
            "SELECT m.CreatureDisplayID, t.entry, t.name, t.type, t.`rank` FROM creature_template_model m "
            "JOIN creature_template t ON t.entry = m.CreatureID "
            "JOIN mod_temucraft_costume_display d ON d.display = m.CreatureDisplayID "
            "WHERE t.name <> '' AND t.name NOT LIKE '%Trigger%' AND t.name NOT LIKE '%Bunny%' AND t.name NOT LIKE '%DND%' "
            "ORDER BY t.`rank` DESC, t.entry");
        if (!r)
            return;
        do
        {
            Field* f = r->Fetch();
            uint32 const display = f[0].Get<uint32>(), entry = f[1].Get<uint32>();
            byCreature[entry].push_back(display);
            if (!catalog.count(display))
                catalog[display] = { f[2].Get<std::string>(), MenuType(f[3].Get<uint8>()) };
        } while (r->NextRow());
        LOG_INFO("server.loading", ">> mod-temucraft-costumes: {} costumes from {} creatures", catalog.size(), byCreature.size());
    }

    uint32 Account(Player* p) { return p->GetSession()->GetAccountId(); }

    bool Owns(Player* p, uint32 display)
    {
        if (cfg.all)
            return catalog.count(display);
        auto it = owned.find(Account(p));
        return it != owned.end() && it->second.count(display);
    }

    void Unlock(Player* p, Creature* c)
    {
        auto it = byCreature.find(c->GetEntry());
        if (it == byCreature.end() || cfg.all)
            return;
        auto& mine = owned[Account(p)];
        uint32 const display = c->GetNativeDisplayId();
        uint32 const pick = std::find(it->second.begin(), it->second.end(), display) != it->second.end() ? display : it->second.front();
        if (!mine.insert(pick).second)
            return;
        CharacterDatabase.Execute("INSERT IGNORE INTO mod_temucraft_costume_collection (account, display) VALUES ({}, {})", Account(p), pick);
        if (cfg.announce)
            ChatHandler(p->GetSession()).PSendSysMessage(Say(p, T_UNLOCKED, "|cff66ccffNew costume:|r {} (.temucraft costume)."), catalog[pick].name);
        if (withAddon.count(p->GetGUID().GetCounter()))
            TemucraftAddonSend(p, PREFIX, "NEW~" + std::to_string(pick));
    }

    bool PvPHere(Player* p) { Map* m = p->GetMap(); return m && m->IsBattlegroundOrArena(); }

    void Wear(Player* p, uint32 display)
    {
        uint32 const guid = p->GetGUID().GetCounter();
        wearing[guid] = display;
        setting.insert(guid);
        p->SetDisplayId(display);
        setting.erase(guid);
        if (withAddon.count(guid))
            SendWorn(p);
    }

    void TakeOff(Player* p, bool tell)
    {
        uint32 const guid = p->GetGUID().GetCounter();
        if (!wearing.erase(guid))
        {
            if (tell) ChatHandler(p->GetSession()).SendSysMessage(Say(p, T_NOT_WEARING, "You aren't wearing a costume."));
            return;
        }
        setting.insert(guid);
        p->RestoreDisplayId();
        setting.erase(guid);
        if (withAddon.count(guid))
            SendWorn(p);
        if (tell) ChatHandler(p->GetSession()).SendSysMessage(Say(p, T_TOOK_OFF, "You take the costume off."));
    }

    std::vector<uint32> Mine(Player* p, uint8 type, std::string const& search = "")
    {
        std::vector<uint32> out;
        std::string needle = search;
        std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
        auto consider = [&](uint32 display)
        {
            auto c = catalog.find(display);
            if (c == catalog.end() || (type && c->second.type != type))
                return;
            if (!needle.empty())
            {
                std::string n = c->second.name;
                std::transform(n.begin(), n.end(), n.begin(), ::tolower);
                if (n.find(needle) == std::string::npos)
                    return;
            }
            out.push_back(display);
        };
        if (cfg.all)
            for (auto const& [d, c] : catalog) consider(d);
        else if (auto it = owned.find(Account(p)); it != owned.end())
            for (uint32 d : it->second) consider(d);
        std::sort(out.begin(), out.end(), [](uint32 a, uint32 b) { return catalog[a].name != catalog[b].name ? catalog[a].name < catalog[b].name : a < b; });
        return out;
    }

    // the collection for the window: every costume (the window has the catalog), or the owned display ids
    void SendList(Player* p)
    {
        std::vector<uint32> mine;
        if (!cfg.all)
            if (auto it = owned.find(Account(p)); it != owned.end())
                for (uint32 d : it->second)
                    if (catalog.count(d))
                        mine.push_back(d);
        auto w = wearing.find(p->GetGUID().GetCounter());
        TemucraftAddonSend(p, PREFIX, Acore::StringFormat("BEGIN~{}~{}~{}", cfg.all ? 1 : 0, w == wearing.end() ? 0 : w->second,
            cfg.all ? catalog.size() : mine.size()));
        std::string chunk;
        for (uint32 d : mine)
        {
            std::string const id = std::to_string(d);
            if (chunk.size() + id.size() + 1 > 230)
            {
                TemucraftAddonSend(p, PREFIX, "IDS~" + chunk);
                chunk.clear();
            }
            chunk += (chunk.empty() ? "" : ",") + id;
        }
        if (!chunk.empty())
            TemucraftAddonSend(p, PREFIX, "IDS~" + chunk);
        TemucraftAddonSend(p, PREFIX, "END");
    }

    void TryWear(Player* p, uint32 display);

    void MainMenu(Player* p)
    {
        ClearGossipMenuFor(p);
        if (wearing.count(p->GetGUID().GetCounter()))
            AddGossipItemFor(p, GOSSIP_ICON_CHAT, Say(p, T_TAKE_OFF, "Take the costume off."), SENDER_MAIN, ACTION_OFF);
        AddGossipItemFor(p, GOSSIP_ICON_CHAT, Say(p, T_SEARCH, "Search by name..."), SENDER_MAIN, ACTION_SEARCH,
            Say(p, T_SEARCH_BOX, "Type part of a creature's name."), 0, true);
        for (auto const& [type, name] : TYPES)
        {
            size_t const n = Mine(p, type).size();
            if (n)
                AddGossipItemFor(p, GOSSIP_ICON_TABARD, Acore::StringFormat(Say(p, T_CATEGORY, "{} ({})"), name, n), SENDER_PAGE, uint32(type) * 1000);
        }
        if (Mine(p, 0).empty())
            ChatHandler(p->GetSession()).SendSysMessage(Say(p, T_NONE, "No costumes yet: defeat creatures or talk to people to collect their looks."));
        SendGossipMenuFor(p, DEFAULT_GOSSIP_MESSAGE, p->GetGUID());
    }

    void ListMenu(Player* p, std::vector<uint32> const& list, uint8 type, uint32 page)
    {
        ClearGossipMenuFor(p);
        uint32 const first = page * PER_PAGE;
        for (uint32 i = first; i < list.size() && i < first + PER_PAGE; ++i)
            AddGossipItemFor(p, GOSSIP_ICON_TABARD, catalog[list[i]].name, SENDER_WEAR, list[i]);
        if (type && first + PER_PAGE < list.size())
            AddGossipItemFor(p, GOSSIP_ICON_CHAT, Say(p, T_NEXT, "Next page"), SENDER_PAGE, uint32(type) * 1000 + page + 1);
        if (type && page > 0)
            AddGossipItemFor(p, GOSSIP_ICON_CHAT, Say(p, T_PREV, "Previous page"), SENDER_PAGE, uint32(type) * 1000 + page - 1);
        AddGossipItemFor(p, GOSSIP_ICON_CHAT, Say(p, T_BACK, "Back"), SENDER_MAIN, ACTION_MAIN);
        SendGossipMenuFor(p, DEFAULT_GOSSIP_MESSAGE, p->GetGUID());
    }

    void Select(Player* p, uint32 sender, uint32 action)
    {
        if (sender == SENDER_MAIN)
        {
            if (action == ACTION_OFF) { CloseGossipMenuFor(p); TakeOff(p, true); return; }
            MainMenu(p);
        }
        else if (sender == SENDER_PAGE)
            ListMenu(p, Mine(p, uint8(action / 1000)), uint8(action / 1000), action % 1000);
        else if (sender == SENDER_WEAR)
        {
            CloseGossipMenuFor(p);
            TryWear(p, action);
        }
    }

    void TryWear(Player* p, uint32 display)
    {
        if (!Owns(p, display))
            return;
        if (!cfg.inPvP && PvPHere(p)) { ChatHandler(p->GetSession()).SendSysMessage(Say(p, T_NO_PVP, "Costumes can't be worn in battlegrounds and arenas.")); return; }
        Wear(p, display);
        ChatHandler(p->GetSession()).PSendSysMessage(Say(p, T_WEARING, "You now look like {}."), catalog[display].name);
    }
}

class TemucraftCostumes_World : public WorldScript
{
public:
    TemucraftCostumes_World() : WorldScript("TemucraftCostumes_World", { WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_STARTUP }) { }
    void OnAfterConfigLoad(bool /*reload*/) override { LoadSettings(); }
    void OnStartup() override { LoadCatalog(); }
};

class TemucraftCostumes_Player : public PlayerScript
{
public:
    TemucraftCostumes_Player() : PlayerScript("TemucraftCostumes_Player",
        { PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LOGOUT, PLAYERHOOK_ON_CREATURE_KILL, PLAYERHOOK_ON_GOSSIP_SELECT,
          PLAYERHOOK_ON_GOSSIP_SELECT_CODE, PLAYERHOOK_ON_MAP_CHANGED, PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT }) { }

    using PlayerScript::OnPlayerCanUseChat;
    bool OnPlayerCanUseChat(Player* p, uint32 type, uint32 language, std::string& msg, Player* receiver) override
    {
        std::string body;
        if (!TemucraftAddonParse(p, type, language, msg, receiver, PREFIX, body))
            return true;
        std::vector<std::string> f = TemucraftAddonFields(body);
        if (f.empty() || !cfg.enable)
            return false;
        uint32 const guid = p->GetGUID().GetCounter();
        if (f[0] == "HELLO")
            withAddon.insert(guid);
        else if (f[0] == "LIST")
        {
            withAddon.insert(guid);
            SendList(p);
        }
        else if (f[0] == "WEAR" && f.size() > 1)
            TryWear(p, uint32(std::strtoul(f[1].c_str(), nullptr, 10)));
        else if (f[0] == "OFF")
            TakeOff(p, true);
        return false;
    }

    void OnPlayerLogin(Player* p) override
    {
        uint32 const account = Account(p);
        if (owned.count(account))
            return;
        auto& mine = owned[account];
        if (QueryResult r = CharacterDatabase.Query("SELECT display FROM mod_temucraft_costume_collection WHERE account = {}", account))
            do mine.insert(r->Fetch()[0].Get<uint32>()); while (r->NextRow());
    }
    void OnPlayerLogout(Player* p) override { wearing.erase(p->GetGUID().GetCounter()); withAddon.erase(p->GetGUID().GetCounter()); }
    void OnPlayerCreatureKill(Player* p, Creature* c) override { if (cfg.enable && cfg.unlockKill && c) Unlock(p, c); }
    void OnPlayerMapChanged(Player* p) override
    {
        if (!cfg.inPvP && PvPHere(p) && wearing.count(p->GetGUID().GetCounter()))
            TakeOff(p, true);
    }
    void OnPlayerGossipSelect(Player* p, uint32 menuId, uint32 sender, uint32 action) override
    {
        if (menuId == 0 && sender >= SENDER_MAIN && sender <= SENDER_WEAR && cfg.enable)
            Select(p, sender, action);
    }
    void OnPlayerGossipSelectCode(Player* p, uint32 menuId, uint32 sender, uint32 action, char const* code) override
    {
        if (menuId != 0 || sender != SENDER_MAIN || action != ACTION_SEARCH || !cfg.enable)
            return;
        std::vector<uint32> list = Mine(p, 0, code ? code : "");
        if (list.empty())
        {
            CloseGossipMenuFor(p);
            ChatHandler(p->GetSession()).SendSysMessage(Say(p, T_NO_MATCH, "No costume of yours matches that."));
            return;
        }
        if (list.size() > PER_PAGE)
            list.resize(PER_PAGE);
        ListMenu(p, list, 0, 0);
    }
};

// talking to a friendly creature unlocks its look (never takes over the creature's own menu)
class TemucraftCostumes_Creatures : public AllCreatureScript
{
public:
    TemucraftCostumes_Creatures() : AllCreatureScript("TemucraftCostumes_Creatures") { }
    bool CanCreatureGossipHello(Player* p, Creature* c) override
    {
        if (cfg.enable && cfg.unlockTalk && p && c)
            Unlock(p, c);
        return false;
    }
};

// keep the costume through shapeshifts, mounting and anything else that resets the model
class TemucraftCostumes_Unit : public UnitScript
{
public:
    TemucraftCostumes_Unit() : UnitScript("TemucraftCostumes_Unit", true, { UNITHOOK_ON_DISPLAYID_CHANGE }) { }
    void OnDisplayIdChange(Unit* unit, uint32 displayId) override
    {
        Player* p = unit ? unit->ToPlayer() : nullptr;
        if (!p)
            return;
        uint32 const guid = p->GetGUID().GetCounter();
        auto it = wearing.find(guid);
        if (it == wearing.end() || setting.count(guid) || displayId == it->second)
            return;
        uint32 const display = it->second;
        p->m_Events.AddEventAtOffset([p, display]()
        {
            auto w = wearing.find(p->GetGUID().GetCounter());
            if (w != wearing.end() && w->second == display && p->GetDisplayId() != display)
                Wear(p, display);
        }, 50ms);
    }
};

class TemucraftCostumes_Commands : public CommandScript
{
public:
    TemucraftCostumes_Commands() : CommandScript("TemucraftCostumes_Commands") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable sub =
        {
            { "",       HandleOpen,   101650, Console::No },
            { "off",    HandleOff,    101651, Console::No },
            { "menu",   HandleMenu,   101652, Console::No },
            { "unlock", HandleUnlock, 101601,     Console::No },   // RBAC permission (data/sql/db-auth)
            { "reload", HandleReload, 101602,     Console::Yes },
        };
        static ChatCommandTable root = { { "temucraft costume", sub } };
        return root;
    }

    static bool HandleOpen(ChatHandler* handler)
    {
        Player* p = handler->GetPlayer();
        if (!p) return true;
        if (!cfg.enable) { handler->SendSysMessage(Say(p, T_OFF, "Costumes are turned off.")); return true; }
        if (withAddon.count(p->GetGUID().GetCounter()))
            TemucraftAddonSend(p, PREFIX, "OPEN");
        else
            MainMenu(p);
        return true;
    }
    // the wardrobe as a gossip menu, even with the window
    static bool HandleMenu(ChatHandler* handler)
    {
        if (Player* p = handler->GetPlayer())
            if (cfg.enable)
                MainMenu(p);
        return true;
    }
    static bool HandleOff(ChatHandler* handler)
    {
        if (Player* p = handler->GetPlayer()) TakeOff(p, true);
        return true;
    }
    // .temucraft costume unlock <display>: unlock a costume for the selected player's account
    static bool HandleUnlock(ChatHandler* handler, uint32 display)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target) return true;
        if (!catalog.count(display)) { handler->SendSysMessage(Say(target, T_UNKNOWN, "That display isn't a costume.")); return true; }
        owned[Account(target)].insert(display);
        CharacterDatabase.Execute("INSERT IGNORE INTO mod_temucraft_costume_collection (account, display) VALUES ({}, {})", Account(target), display);
        handler->PSendSysMessage(Say(target, T_GM_UNLOCKED, "Unlocked {} for {}."), catalog[display].name, target->GetName());
        return true;
    }
    static bool HandleReload(ChatHandler* handler)
    {
        LoadCatalog();
        handler->PSendSysMessage("mod-temucraft-costumes: {} costumes.", catalog.size());
        return true;
    }
};

void AddTemucraftCostumesScripts()
{
    new TemucraftCostumes_World();
    new TemucraftCostumes_Player();
    new TemucraftCostumes_Creatures();
    new TemucraftCostumes_Unit();
    new TemucraftCostumes_Commands();
}
