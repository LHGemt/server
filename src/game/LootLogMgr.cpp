/*
 * Copyright (C) 2017 Light's Hope <https://github.com/lightshope>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Policies/Singleton.h"
#include "Database/DatabaseEnv.h"
#include "Policies/SingletonImp.h"
#include "Player.h"
#include "SpellEntry.h"
#include "ProgressBar.h"
#include "LootLogMgr.h"
#include "Chat.h"
#include "Language.h"
#include "ObjectMgr.h"

#include <ctime>

INSTANTIATE_SINGLETON_1(LootLogManager);

/*
    When a boss is killed we get hook into the Unit::Kill function and store the following information:
    loot_creature_death: creature-entry, a timestamp and a key 
    loot_items:          maps each item dropped by the creature to the loot_creature_death table through the same key
    loot_candidates:     maps each eligible player to the same creature through the same key.

                                      
    loot_creature_death   key         creatureGuid    creatureEntry    timestamp    instanceID
    loot_items            key         itemEntry       looterGuid       
    loot_candidates       key         playerGuid

    //.lootlogg ID # <Name>
    //Confirms if player is saved to that ID
    //Output: <Name> Is/Isnot currently saved to ID

    .lootlogg <name> ITEMID
    Returns if player has been awarded that item ID - Could combine with .hasitem ?
    Output: <Name> was awarded ID for raid ID # - Has/hasnot got item.

    .lootlog items #
    Returns all items dropped in the loot id and who it was awarded
    Output: Listing players who recieved loot in ID #
    <Name> awarded Item #(edited)
    Could try that as a starter
*/

void LootLogManager::LogGroupKill(Creature * pCreature, Group * pGroup)
{
	if (!pCreature || !pGroup)
		return;
	Loot* loot = &pCreature->loot;
	if (!loot || loot->empty())
		return;

	// Only loot-log boss loot
	if (!(pCreature->GetCreatureInfo()->type_flags & CREATURE_TYPEFLAGS_BOSS) && !pCreature->IsWorldBoss())
		return;
	
    uint32 instanceId = pCreature->GetMap()->GetInstanceId();

	// Finding the players in range for the loot
	std::map<Player*, std::vector<LootItem>> lootLog;

    //std::lock_guard<std::mutex> guard(_mutex);
    uint64 currentKey = ++highestKey;

    pCreature->SetLastDeathTime(time(NULL));

    CharacterDatabase.PExecute(
        "INSERT INTO `loot_creature_death` (`key`, `creatureGuid`, `creatureEntry`, `timestamp`, `instanceId`) VALUES (%u, %llu, %u, %lld, %u)", 
        currentKey, pCreature->GetGUID(), pCreature->GetEntry(), pCreature->GetLastDeathTime(), instanceId);

    for (auto it = loot->items.begin(); it != loot->items.end(); it++)
    {
        CharacterDatabase.PExecute(
            "INSERT INTO `loot_items` (`key`, `itemEntry`) VALUES (%u, %u)",
            currentKey, it->itemid);
    }

    auto it = pGroup->GetMemberSlots().begin();
    for (it; it != pGroup->GetMemberSlots().end(); ++it)
    {
        if (Player* pl = pCreature->GetMap()->GetPlayer(it->guid))
        {
            if (pl->IsAtGroupRewardDistance(pCreature))
            {
                CharacterDatabase.PExecute(
                    "INSERT INTO `loot_candidates` (`key`, `playerGuid`) VALUES (%u, %u)",
                    currentKey, pl->GetGUIDLow());
            }
        }
    }
}

void LootLogManager::LogLootReceived(Creature * pCreature, Player * pPlayer, Item* item)
{
    if (!pCreature || !pPlayer || !item)
        return;

    // Only loot-log boss loot
    if (!(pCreature->GetCreatureInfo()->type_flags & CREATURE_TYPEFLAGS_BOSS) && !pCreature->IsWorldBoss())
        return;
    
    uint32 instanceId = pCreature->GetMap()->GetInstanceId();
        
    CharacterDatabase.PExecute(
        "UPDATE `loot_items` set `looterGuid`=%u WHERE `itemEntry` = %u AND `key` = "
        "(select `key` from loot_creature_death where creatureGuid=%llu AND instanceID=%u AND `timestamp`=%lld)",
        pPlayer->GetGUIDLow(), item->GetEntry(), pCreature->GetGUID(), instanceId, pCreature->GetLastDeathTime());
}

void LootLogManager::Load()
{
    QueryResult* res = CharacterDatabase.PQuery("SELECT MAX(`key`) from loot_creature_death");
    if (res) {
        Field* f = res->Fetch();
        highestKey = f[0].GetUInt64();
    }
    else {
        sLog.outError("Failed retreiving LAST_INSERT_ID for an item in loot_log");
    }

    sLog.outString("> Finished loading Loot Log variables");
}

/*
.lootlogg <name> <instanceId> <itemid>
Returns if player has been awarded that item ID
*/
bool ChatHandler::HandleCanReceiveItem(char* args)
{
    if (!*args)
        return false;

    char* nameStr = ExtractOptNotLastArg(&args);
    ObjectGuid target_guid;
    std::string player_name;
    Player* plTarget;
    if (!ExtractPlayerTarget(&nameStr, &plTarget, &target_guid, &player_name))
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }
    
    uint32 instanceId = 0;
    if (!ExtractUInt32(&args, instanceId))
    {
        PSendSysMessage("Unable to parse instance ID");
        SetSentErrorMessage(true);
        return false;
    }

    uint32 itemId = 0;
    if (!ExtractUInt32(&args, itemId))
    {
        SendSysMessage(LANG_ITEM_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }
    
    ItemPrototype const* pItem = ObjectMgr::GetItemPrototype(itemId);
    if (!pItem)
    {
        PSendSysMessage("Unknown item %u", itemId);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 itemCount = 0;
    if (plTarget)
    {
        itemCount = plTarget->GetItemCount(itemId, true);
    }
    else
    {
        std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
            "SELECT SUM(count) AS item_count FROM item_instance ii WHERE itemEntry = %u and owner_guid = %u",
            itemId, target_guid.GetCounter()
        ));

        if (result)
        {
            auto fields = result->Fetch();
            itemCount = fields[0].GetUInt32();
        }
    }
    std::string itemLink = GetItemLink(pItem).c_str();

    PSendSysMessage("Loot Log:");
    PSendSysMessage("Player: %s", player_name.c_str());
    PSendSysMessage("Item: %s", itemLink.c_str());
    PSendSysMessage("Inventory count: %u", itemCount);
    PSendSysMessage("Instance ID: %u", instanceId);

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT DISTINCT loot_candidates.key, loot_items.itemEntry, loot_items.looterGuid, loot_creature_death.timestamp, loot_creature_death.creatureEntry, loot_creature_death.creatureGuid "
        "FROM loot_candidates "
        "JOIN loot_items ON loot_candidates.key = loot_items.key "
        "JOIN loot_creature_death ON loot_candidates.key = loot_creature_death.key "
        "WHERE loot_candidates.playerGuid = %u AND loot_items.itemEntry = %u AND loot_creature_death.instanceId = %u",
        target_guid.GetCounter(), itemId, instanceId));
    
    if (!result)
    {
        PSendSysMessage(">No recorded loot-log entries");
        SetSentErrorMessage(true);
        return false;
    }
    else 
    {
        BarGoLink bar((int)result->GetRowCount());
        do
        {
            bar.step();

            Field* fields = result->Fetch();

            uint32 key           = fields[0].GetUInt32();
            uint32 itemEntry     = fields[1].GetUInt32();
            uint32 looterGuid    = fields[2].GetUInt32();
            time_t timestamp     = (time_t)fields[3].GetUInt64();
            uint32 creatureEntry = fields[4].GetUInt32();
            uint32 creatureE2 = ObjectGuid(fields[5].GetUInt64()).GetCounter();
            std::string creature_name;
            if (CreatureInfo const* pCI = sObjectMgr.GetCreatureTemplate(creatureEntry))
                creature_name = std::string(pCI->Name);
            else
                creature_name = std::to_string(creatureEntry);

            std::string ts = TimeToTimestampStr(timestamp);
            std::string looterName;
            bool looterExist = false;
            if (Player* pPlayer = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, looterGuid))) {
                looterName = std::string(pPlayer->GetName());
                looterExist = true;
            }

            if (looterExist)
                PSendSysMessage("> %s: Dropped by %s, looted by player: %s.", ts.c_str(), creature_name.c_str(), looterName.c_str());
            else
                PSendSysMessage("> %s: Dropped by %s, can be looted by %s.", ts.c_str(), creature_name.c_str(), player_name.c_str());

        } while (result->NextRow());
    }
    

    return true;
}
