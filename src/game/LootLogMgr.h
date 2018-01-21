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

#ifndef LOOT_LOG_MGR
#define LOOT_LOG_MGR

#include "Common.h"
#include "Policies/Singleton.h"

#include <mutex>
#include <atomic>
class Player;

// Handles removal of auras from players on map changes, based on
// definitions in world-db table instance_buff_removal
class LootLogManager
{
public:                                                 // Constructors
    LootLogManager() {}

public:                                                 // Accessors
    void LogGroupKill(Creature* pCreature, Group* pGroup);
    void LogLootReceived(Creature* pCreature, Player* pPlayer, Item* item);

    void Load();

private:
	std::mutex _mutex;
    std::atomic<uint64> highestKey;
};

#define sLootLogMgr MaNGOS::Singleton<LootLogManager>::Instance()

#endif