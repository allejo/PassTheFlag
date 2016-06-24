/* PassTheFlag
 * Copyright (c) 2010 squad.firing@gmail.com
 *
 * Published under version 3 of the GNU LESSER GENERAL PUBLIC LICENSE
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named lgpl.txt that should have accompanied this file.
 * Also available at http://www.gnu.org/licenses/lgpl.txt at the time this was created
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

// PassTheFlag.cpp : a bzfs plugin to enable passing of flags (actually you throw them)
//
// PassTheFlag.cpp written by FiringSquad based loosely on the HTF plugin code by bullet_catcher that came with the BZFlag sources
// Special thanks to mrapple for his help


#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string>
#include <bitset>

#include "bzfsAPI.h"

// Note: including the following headers will mean that this plugin will not compile under Windows.
// The only other option was modifying the server code to achieve the functionality I needed and that was not really an option.
// Using these headers exposes internal workings of the server that you need to be careful with, so it should not be done lightly.
// It does allow you to do some pretty cool stuff though. :-D
#include "../../src/bzfs/bzfs.h"
#include "BZDBCache.h"
#include "../../src/bzfs/DropGeometry.h"

using namespace std;

#define PASSTHEFLAG_VER "1.00.07"

#define DbgLevelAlways  1
#define DbgLevelErr 1
#define DbgLevelWarn    2
#define DbgLevelInfo    3
#define DbgLevelDbgInfo 4
//#define DbgLevelDbgInfo   1

#define kWorkStrLen 255
#define kInvalidPlayerID -1

enum tFlagType {
    kFlagType_Error = -1,
    kFlagType_HighSpeed,
    kFlagType_QuickTurn,
    kFlagType_OscillationOverdrive,
    kFlagType_RapidFire,
    kFlagType_MachineGun,
    kFlagType_GuidedMissile,
    kFlagType_Laser,
    kFlagType_Ricochet,
    kFlagType_SuperBullet,
    kFlagType_InvisibleBullet,
    kFlagType_Stealth,
    kFlagType_Tiny,
    kFlagType_Narrow,
    kFlagType_Shield,
    kFlagType_SteamRoller,
    kFlagType_ShockWave,
    kFlagType_PhantonZone,
    kFlagType_Genocide,
    kFlagType_Jumping,
    kFlagType_Identify,
    kFlagType_Cloaking,
    kFlagType_Useless,
    kFlagType_Masquerade,
    kFlagType_Seer,
    kFlagType_Thief,
    kFlagType_Burrow,
    kFlagType_Wings,
    kFlagType_ColourBlind,
    kFlagType_LeftTurnOnly,
    kFlagType_RightTurnOnly,
    kFlagType_ForwardOnly,
    kFlagType_ReverseOnly,
    kFlagType_Momentum,
    kFlagType_Blindness,
    kFlagType_Jamming,
    kFlagType_WideAngle,
    kFlagType_NoJumping,
    kFlagType_TriggerHappy,
    kFlagType_Bouncy,
    kFlagType_ReverseControls,
    kFlagType_Agility,
    kFlagType_Red,
    kFlagType_Green,
    kFlagType_Blue,
    kFlagType_Purple,
    kFlagType_Count
};


const char  *   FlagAbbr[kFlagType_Count] = {
    "V", "QT", "OO", "F", "MG", "GM", "L", "R", "SB",
    "IB", "ST", "T", "N", "SH", "SR", "SW", "PZ", "G",
    "JP", "ID", "CL", "US", "MQ", "SE", "TH", "BU", "WG",
    "CB", "LT", "RT", "FO", "RO", "M", "B", "JM", "WA", "NJ",
    "TR", "BY", "RC", "A", "R*", "G*", "B*", "P*"
};



// long long not supported so I will need to use a bitset (makes code more complex unfortunately)

#define kBitString_NoFlags      "000000000000000000000000000000000000000000000"
#define kBitString_GoodFlags    "000010000000000000111111111111111111111111111"
#define kBitString_BadFlags "000001111111111111000000000000000000000000000"
#define kBitString_TeamFlags    "111100000000000000000000000000000000000000000"
#define kBitString_AllFlags "111111111111111111111111111111111111111111111"

typedef std::bitset<kFlagType_Count> tAllowedFlagGroups;

const tAllowedFlagGroups kAllowedFlagGroups_NoFlags(string(kBitString_NoFlags));
const tAllowedFlagGroups kAllowedFlagGroups_GoodFlags(string(kBitString_GoodFlags));
const tAllowedFlagGroups kAllowedFlagGroups_BadFlags(string(kBitString_BadFlags));
const tAllowedFlagGroups kAllowedFlagGroups_TeamFlags(string(kBitString_TeamFlags));
const tAllowedFlagGroups kAllowedFlagGroups_AllFlags(string(kBitString_AllFlags));



tFlagType GetFlagTypeFromAbbr(const char *GivenFlagAbbr)
{
    for (int i = kFlagType_HighSpeed; i < kFlagType_Count; i++)
        if (0 == strcasecmp (FlagAbbr[i], GivenFlagAbbr))
            return (tFlagType)i;
    return kFlagType_Error;
}


//======================================= FPassHandler ================================================
// Standard interface for communicating with the server


int PlayerWithDebugAccess = kInvalidPlayerID;
char DbgAccessPassword[kWorkStrLen] = "";
char CopyOfPluginParams[kWorkStrLen] = "";

// Since I do not support Windows I made life easy for myself and defined a variadic macro
// It will create a warning in GCC if -Wno-variadic-macros is not specified

#define DebugMessage(...) {if (kInvalidPlayerID != PlayerWithDebugAccess)   {bz_sendTextMessagef (BZ_SERVER, PlayerWithDebugAccess, __VA_ARGS__);}}



class FPassHandler : public bz_Plugin, public bz_CustomSlashCommandHandler
{
public:
    virtual const char* Name () {return "PTF 1.0.0";}
    virtual void Init (const char* config);
    virtual void Event (bz_EventData *eventData);
    virtual bool SlashCommand ( int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params);
    virtual void Cleanup (void);
    
protected:
    
    bool SetMaxWaitVal(const char *FloatStr, int *CharsUsed);
    bool parseCommandLine (const char *cmdLine);
    
private:
};

FPassHandler FlagPassHandler;

//======================================= PlayerStats ================================================
// Contains details I need to remember about active players that are not available elsewhere
// It is filled during the bz_ePlayerUpdateEvent, bz_ePlayerSpawnEvent and bz_ePlayerDieEvent

class PlayerStats   {
    
public:
    
    int thisPlayerID;
    bz_eTeamType playerTeam;
    bz_ApiString callsign;
    
    // Stuff I need to remember
    float velocity[3];
    bz_eTeamType KillerTeam;
    int KilledByPlayerID;
    
    void SetStats(int playerID, const float *velocity) {
        if (velocity && (this->thisPlayerID == playerID))
        {
            memcpy(this->velocity, velocity, sizeof(float[3]));
        }
    }
    
    void SetKiller(int KillerID, bz_eTeamType KillerTeam) {
        this->KilledByPlayerID = KillerID;
        this->KillerTeam = KillerTeam;
    }
    
    void InitialiseData(int playerID = kInvalidPlayerID, bz_eTeamType PlayerTeam = eNoTeam, const float *velocity = NULL, const bz_ApiString *callsign = NULL, int KillPlayerID = kInvalidPlayerID, bz_eTeamType KillerTeam = eNoTeam) {
        this->thisPlayerID = playerID;
        this->playerTeam = PlayerTeam;
        this->KillerTeam = KillerTeam;
        this->KilledByPlayerID = KillPlayerID;
        this->velocity[0] = velocity ? velocity[0] : 0.0;
        this->velocity[1] = velocity ? velocity[1] : 0.0;
        this->velocity[2] = velocity ? velocity[2] : 0.0;
        this->callsign = callsign ? *callsign : bz_ApiString();
    }
    
    PlayerStats(int playerID, bz_eTeamType PlayerTeam, const float *velocity, const bz_ApiString *callsign) { InitialiseData(playerID, PlayerTeam, velocity, callsign);   };
    PlayerStats(const PlayerStats& inData) {
        if (this != &inData)
        {
            InitialiseData(inData.thisPlayerID, inData.playerTeam, inData.velocity, &inData.callsign, inData.KilledByPlayerID, inData.KillerTeam);
        }
    };
    PlayerStats() { InitialiseData(kInvalidPlayerID, eNoTeam, NULL, NULL, kInvalidPlayerID, eNoTeam); };
    
    PlayerStats& operator=(const PlayerStats& other);
    
    bool operator==(const PlayerStats& other) const;
    bool operator!=(const PlayerStats& other) const { return !(*this == other); }
    
};



// operator ==
inline bool PlayerStats::operator==(const PlayerStats& inData) const
{
    return (thisPlayerID == inData.thisPlayerID);
}



PlayerStats& PlayerStats::operator=(const PlayerStats& other)
{
    if (this != &other)
    {
        InitialiseData(other.thisPlayerID, other.playerTeam, other.velocity, &other.callsign, other.KilledByPlayerID, other.KillerTeam);
    }
    return *this;
}


// List of active players along with their recorded information
// Players are added during the bz_ePlayerJoinEvent
// and removed after a bz_ePlayerPartEvent

std::vector <PlayerStats> gActivePlayers;

//======================================= PassDetails ================================================
// Contains all the information we need in order to calculate the pass
// There are some times when a flag is dropped that we want to ignore
// The problem is that the client first sends the flagdrop message and later sends the reason
// We therefore need to remember that the flag was dropped and check later if we need to send it flying anywhere

#define kDefault_MaxWaitForReason   0.1f

class DropEvent {
    
public:
    
    float TimeEventOccurred;
    int PlayerThatDroppedTheFlag;
    int DroppedFlagID;
    float DropPos[3];
    float PlayerPosAtThisTime[3];
    float PlayerVelocityAtThisTime[3];
    
    void InitialiseData(int playerID = kInvalidPlayerID, int flagID = -1, const float *DropPos = NULL, const float *PlayerPosAtThisTime = NULL, const float *PlayerVelocityAtThisTime = NULL,  float EventTime = 0.0)
    {
        this->PlayerThatDroppedTheFlag = playerID;
        this->DroppedFlagID = flagID;
        this->TimeEventOccurred = EventTime;
        this->DropPos[0] = DropPos ? DropPos[0] : 0.0;
        this->DropPos[1] = DropPos ? DropPos[1] : 0.0;
        this->DropPos[2] = DropPos ? DropPos[2] : 0.0;
        this->PlayerPosAtThisTime[0] = PlayerPosAtThisTime ? PlayerPosAtThisTime[0] : 0.0;
        this->PlayerPosAtThisTime[1] = PlayerPosAtThisTime ? PlayerPosAtThisTime[1] : 0.0;
        this->PlayerPosAtThisTime[2] = PlayerPosAtThisTime ? PlayerPosAtThisTime[2] : 0.0;
        this->PlayerVelocityAtThisTime[0] = PlayerVelocityAtThisTime ? PlayerVelocityAtThisTime[0] : 0.0;
        this->PlayerVelocityAtThisTime[1] = PlayerVelocityAtThisTime ? PlayerVelocityAtThisTime[1] : 0.0;
        this->PlayerVelocityAtThisTime[2] = PlayerVelocityAtThisTime ? PlayerVelocityAtThisTime[2] : 0.0;
    }
    
    DropEvent(int playerID, int flagID, const float *DropPos, const float *PlayerPosAtThisTime, const float *PlayerVelocityAtThisTime) { InitialiseData(playerID, flagID, DropPos, PlayerPosAtThisTime, PlayerVelocityAtThisTime, TimeKeeper::getCurrent().getSeconds());  };
    DropEvent(const DropEvent& inData) {
        if (this != &inData)
        {
            InitialiseData(inData.PlayerThatDroppedTheFlag, inData.DroppedFlagID, inData.DropPos, inData.PlayerPosAtThisTime, inData.PlayerVelocityAtThisTime, inData.TimeEventOccurred);
        }
    };
    DropEvent() { InitialiseData();   };
    
    DropEvent& operator=(const DropEvent& other);
    
    bool operator==(const DropEvent& other) const;
    bool operator!=(const DropEvent& other) const { return !(*this == other); }
    
};



// operator ==
inline bool DropEvent::operator==(const DropEvent& inData) const
{
    return (PlayerThatDroppedTheFlag == inData.PlayerThatDroppedTheFlag);
}



DropEvent& DropEvent::operator=(const DropEvent& other)
{
    if (this != &other)
    {
        InitialiseData(other.PlayerThatDroppedTheFlag, other.DroppedFlagID, other.DropPos, other.PlayerPosAtThisTime, other.PlayerVelocityAtThisTime, other.TimeEventOccurred);
    }
    return *this;
}


// List of pending drop events

std::vector <DropEvent> gPendingDropEvents;



//=======================================================================================

enum tFumbleMsgOption {
    kFMO_NoMessages,
    kFMO_TellEveryone,
    kFMO_TellPlayer,
    kFMO_MaxVal       // Never used as an option, just as a counter
};


const char *kFumbleMsgSettingDesc[kFMO_MaxVal]  = {
    "No Fumble Messages",
    "Tell everybody about fumbles",
    "only inform the player that fumbled"
};


enum tPassOnDeathOption {
    kPOD_No,
    kPOD_Yes,
    kPOD_Hurts,
    kPOD_MaxVal       // Never used as an option, just as a counter
};

const char *kPassOnDeathSettingDesc[kPOD_MaxVal]  = {
    "Flag drops without being passed",
    "Flag flies in direction tank was moving",
    "Flag holder gets punished in accordance with \"hurt\""
};


enum tPassOption {
    kPassing_On,
    kPassing_Off,
    kPassing_Immediate,
    kPassing_MaxVal       // Never used as an option, just as a counter
};

enum tPassableFlagGroupOption {
    kPassFlgGrp_Everything,
    kPassFlgGrp_TeamFlags,
    kPassFlgGrp_Custom,
    kPassFlgGrp_MaxVal        // Never used as an option, just as a counter
};

const char *kPassableFlagGroupOptionDesc[kPassFlgGrp_MaxVal]  = {
    "All Flags",
    "Only Team Flags",
    "Custom Set"
};


const char *kPassOptionDesc[kPassing_MaxVal]  = {
    "Activated",
    "Disabled",
    "Immediate (No fancy stuff - let the flag fly)"
};


enum tHurtingPassOption {
    kHPO_ToKiller,
    kHPO_ToNonTeamKiller,
    kHPO_MaxVal       // Never used as an option, just as a counter
};

const char *kHurtSettingDesc[kHPO_MaxVal]  = {
    "Flag flies in direction of killer",
    "Flag flies in direction of killer unless it was a TK"
};


#define kMaxFumbledPassMessages     5
const char *kFumbledPassMessages[kMaxFumbledPassMessages]  = {
    "Woops! You need to be more careful passing",
    "You fumbled that pass!",
    "Watch where you're throwing that flag! Fumble!",
    "You tried to send the flag where it can't go. Silly!",
    "I hope you shoot better than you pass."
};


//====================================== Configurable Options =================================================

tPassOption FPassEnabled = kPassing_On;
tFumbleMsgOption FumbleMsg = kFMO_TellPlayer;
tPassOnDeathOption PassOnDeath = kPOD_No;
tHurtingPassOption HurtOption = kHPO_ToNonTeamKiller;
tAllowedFlagGroups FlagsAllowed = kAllowedFlagGroups_AllFlags;


float FPassThrowDistance = 6.0;
float JumpBoostFactor = 2.0;
int MaxSafeZoneTests = 5;

float MaxWaitForReason = kDefault_MaxWaitForReason;

// Reset Values
#define AssignResetVal(ConfigVal) ResetVal_##ConfigVal = ConfigVal
#define ResetTheValue(ConfigVal) ConfigVal = ResetVal_##ConfigVal

tPassOption AssignResetVal(FPassEnabled);
tFumbleMsgOption AssignResetVal(FumbleMsg);
tPassOnDeathOption AssignResetVal(PassOnDeath);
tHurtingPassOption AssignResetVal(HurtOption);
tAllowedFlagGroups AssignResetVal(FlagsAllowed);
float AssignResetVal(FPassThrowDistance);
float AssignResetVal(JumpBoostFactor);
int AssignResetVal(MaxSafeZoneTests);

float AssignResetVal(MaxWaitForReason);

bool RequirePermissions = true;
bool AllowMaxWaitMod = false;

//====================================== PlayerStats Utils =================================================


// Return the PlayerStats information for player PlayerID
PlayerStats *GetActivePlayerStatsByID(int PlayerID)
{
    static PlayerStats UnKnown;
    UnKnown.InitialiseData();
    
    for (unsigned int i = 0; i < gActivePlayers.size(); i++)
    {
        if (gActivePlayers[i].thisPlayerID == PlayerID)
        {
            return &gActivePlayers[i];
        }
    }
    return &UnKnown;
}


// Locate PlayerStats information for player PlayerID in the gActivePlayers list and remove it
bool RemovePlayerStatsForPlayerID(int PlayerID)
{
    std::vector<PlayerStats>::iterator iter = gActivePlayers.begin();
    while ( iter != gActivePlayers.end() )
    {
        if (iter->thisPlayerID == PlayerID)
        {
            iter = gActivePlayers.erase( iter );
            return true;
        }
        else
            ++iter;
    }
    return false;
}


//================================= fpass command utils ======================================================


bool ThisFlagIsPassible(tFlagType FlagToTest)
{
    if ((0 > FlagToTest) || (kFlagType_Count <= FlagToTest))
        return false;
    return FlagsAllowed.test(FlagToTest);
}




// Get the current value for velocity I have for this player
bool getPlayerVelocity(int playerID, float vel[3])
{
    PlayerStats *StatsForThisPlayer = GetActivePlayerStatsByID(playerID);
    memcpy(vel, StatsForThisPlayer->velocity, sizeof(float[3]));
    return (StatsForThisPlayer->thisPlayerID == playerID);
}



// Get the current value for velocity I have for this player
bool getPlayerKiller(int playerID, bz_eTeamType &playerTeam, int &KillerID, bz_eTeamType &KillerTeam)
{
    PlayerStats *StatsForThisPlayer = GetActivePlayerStatsByID(playerID);
    playerTeam = StatsForThisPlayer->playerTeam;
    KillerID = StatsForThisPlayer->KilledByPlayerID;
    KillerTeam = StatsForThisPlayer->KillerTeam;
    return (StatsForThisPlayer->thisPlayerID == playerID);
}



// Ask the server for the callsign for this player
bz_ApiString &getPlayerCallsign(int playerID)
{
    static bz_ApiString UnknownPlayer("some unknown player");
    unique_ptr<bz_BasePlayerRecord> player(bz_getPlayerByIndex(playerID));
    if (player) {
        return player->callsign;
    }
    return UnknownPlayer;
}



// Ask the server for the location for this player
bool getPlayerPosition(int playerID, float PlayerPos[3])
{
    unique_ptr<bz_BasePlayerRecord> player(bz_getPlayerByIndex(playerID));
    if (!player) {
        return false;
    }
    memcpy(PlayerPos, player->lastKnownState.pos, sizeof(float[3]));
    return true;
}



// Ask the server if this player is an Admin
bool playerIsAdmin(int playerID)
{
    unique_ptr<bz_BasePlayerRecord> player(bz_getPlayerByIndex(playerID));
    if (!player) {
        return false;
    }
    return player->admin;
}


// Check if this player has the appropriate permission to perform the command
bool checkPerms (int playerID, const char *FPassCmd, const char *permName)
{
    if (!RequirePermissions)
        return true;
    bool HasPerm = false;
    if ('\0' == *permName)    // Needs Admin
        HasPerm = playerIsAdmin (playerID);
    else
        HasPerm = bz_hasPerm (playerID, permName);
    if (!HasPerm)
        bz_sendTextMessagef (BZ_SERVER, BZ_ALLUSERS, "you need \"%s\" permission to do /FPass %s", *permName ? permName : "Admin", FPassCmd);
    return HasPerm;
}


//================================= fpass command responses ======================================================


// Send command format
void sendHelp (int who)
{
    bz_sendTextMessage(BZ_SERVER, who, "FPass commands: [help|stat|off|on|immediate|reset|allflags|teamflags|[customflags|toggleflags]={V,QT,...}|...");
    bz_sendTextMessage(BZ_SERVER, who, "   ...|dist=< 0.0 .. 20.0>|steps=<0 .. 20>|fmsg=[off|all|player]...");
    bz_sendTextMessage(BZ_SERVER, who, "   ...|passondeath=[on|off|hurts]|hurt=[killr|nontker]|jboost=< 0.2 .. 5.0>");
}


// Send Details about setting MaxWaitForReason
void sendMaxWaitMsg (int who)
{
    bz_sendTextMessagef(BZ_SERVER, who, "FPass Maxwait Variable: e.g. \"/fpass maxwait=0.1\" : Valid Range (0.0 .. 3.0) : Current value is %f editing is %s", MaxWaitForReason, ((who == PlayerWithDebugAccess) || AllowMaxWaitMod) ? "allowed" : "disabled");
}




// Reset All Configurable Variable back to their values at launchplugin time
void ResetAllVariables (int who)
{
    char msg[kWorkStrLen];
    const char *PlayerName;
    
    if (kInvalidPlayerID == who)
        return;
    
    ResetTheValue(FPassEnabled);
    ResetTheValue(FumbleMsg);
    ResetTheValue(PassOnDeath);
    ResetTheValue(HurtOption);
    ResetTheValue(FlagsAllowed);
    ResetTheValue(FPassThrowDistance);
    ResetTheValue(JumpBoostFactor);
    ResetTheValue(MaxSafeZoneTests);
    ResetTheValue(MaxWaitForReason);
    
    PlayerStats *StatsForThisPlayer = GetActivePlayerStatsByID(who);
    if (StatsForThisPlayer->thisPlayerID == who)
        PlayerName = StatsForThisPlayer->callsign.c_str();
    else
        PlayerName = getPlayerCallsign(who).c_str();
    snprintf (msg, kWorkStrLen, "*** Flag Passing variables reset by %s", PlayerName);
    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, msg);
}


#define Flg(Idx) FlagList.test(Idx)?FlagAbbr[Idx]:"",FlagList.test(Idx)?" ":""

void ShowFlagList(int who, const tAllowedFlagGroups &FlagList)
{
    bz_sendTextMessagef(BZ_SERVER, who,  "  %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
                        Flg(kFlagType_HighSpeed), Flg(kFlagType_QuickTurn), Flg(kFlagType_OscillationOverdrive), Flg(kFlagType_RapidFire), Flg(kFlagType_MachineGun), Flg(kFlagType_GuidedMissile),
                        Flg(kFlagType_Laser), Flg(kFlagType_Ricochet), Flg(kFlagType_SuperBullet), Flg(kFlagType_InvisibleBullet), Flg(kFlagType_Stealth), Flg(kFlagType_Tiny), Flg(kFlagType_Narrow),
                        Flg(kFlagType_Shield), Flg(kFlagType_SteamRoller), Flg(kFlagType_ShockWave), Flg(kFlagType_PhantonZone), Flg(kFlagType_Genocide), Flg(kFlagType_Jumping), Flg(kFlagType_Identify),
                        Flg(kFlagType_Cloaking), Flg(kFlagType_Useless), Flg(kFlagType_Masquerade), Flg(kFlagType_Seer), Flg(kFlagType_Thief), Flg(kFlagType_Burrow), Flg(kFlagType_Wings),
                        Flg(kFlagType_ColourBlind), Flg(kFlagType_LeftTurnOnly), Flg(kFlagType_RightTurnOnly), Flg(kFlagType_ForwardOnly), Flg(kFlagType_ReverseOnly), Flg(kFlagType_Momentum),
                        Flg(kFlagType_Blindness), Flg(kFlagType_Jamming), Flg(kFlagType_WideAngle), Flg(kFlagType_NoJumping), Flg(kFlagType_TriggerHappy), Flg(kFlagType_Bouncy), Flg(kFlagType_ReverseControls),
                        Flg(kFlagType_Agility), Flg(kFlagType_Red), Flg(kFlagType_Green), Flg(kFlagType_Blue), Flg(kFlagType_Purple)
                        );
}


void ShowPassableFlags(int who)
{
    bz_sendTextMessagef(BZ_SERVER, who,  "  Flags that can be passed: %s" , (kAllowedFlagGroups_TeamFlags == FlagsAllowed) ? kPassableFlagGroupOptionDesc[kPassFlgGrp_TeamFlags] : (kAllowedFlagGroups_AllFlags == FlagsAllowed) ? kPassableFlagGroupOptionDesc[kPassFlgGrp_Everything] : "");
    if ((kAllowedFlagGroups_AllFlags != FlagsAllowed) && (kAllowedFlagGroups_TeamFlags != FlagsAllowed))
        ShowFlagList(who, FlagsAllowed);
}


// Show current settings
void FPassStats (int who)
{
    bz_sendTextMessagef(BZ_SERVER, who, "FPass plugin version %s", PASSTHEFLAG_VER);
    bz_sendTextMessagef(BZ_SERVER, who,  "  Flag Passing is %s" , kPassOptionDesc[(int) FPassEnabled]);
    bz_sendTextMessagef(BZ_SERVER, who,  "  Flag Throw Distance: %f" , FPassThrowDistance);
    bz_sendTextMessagef(BZ_SERVER, who,  "  Jump Boost Factor: %f" , JumpBoostFactor);
    bz_sendTextMessagef(BZ_SERVER, who,  "  Max Attempts to land flag: %d" , MaxSafeZoneTests);
    bz_sendTextMessagef(BZ_SERVER, who,  "  Fumble Messages are set to \"%s\"" , kFumbleMsgSettingDesc[(int) FumbleMsg]);
    bz_sendTextMessagef(BZ_SERVER, who,  "  Passing on Death set to \"%s\"" , (kPOD_Hurts == PassOnDeath) ? kHurtSettingDesc[(int) HurtOption] : kPassOnDeathSettingDesc[(int) PassOnDeath]);
    ShowPassableFlags(who);
}


// Send details about the PassTheFlag plugin
void FPassSendDesc (int who)
{
    bz_sendTextMessage(BZ_SERVER, who, "PassTheFlag plugin allows the user to throw the flag they are currently holding.");
    bz_sendTextMessage(BZ_SERVER, who, "Essentially the flag flies in the direction you are traveling and the faster you");
    bz_sendTextMessage(BZ_SERVER, who, "go the further it flies");
    bz_sendTextMessage(BZ_SERVER, who, "Jumping effects the distance too. If you are going up it travels twice as far,");
    bz_sendTextMessage(BZ_SERVER, who, "and if you are falling it travels half as far.");
    bz_sendTextMessage(BZ_SERVER, who, "A fumble occurs when you try to pass a flag to an unsafe location.");
    bz_sendTextMessage(BZ_SERVER, who, "For further information, check out the README.txt file accompanying the source.");
    bz_sendTextMessage(BZ_SERVER, who, "You need COUNTDOWN privileges to turn it ON/OFF.");
    bz_sendTextMessage(BZ_SERVER, who, "You need to be an Admin to change other settings.");
    bz_sendTextMessage(BZ_SERVER, who, "\"/fpass help\" and \"/fpass stat\" are available to all.");
    bz_sendTextMessage(BZ_SERVER, who, "Current Settings:");
    FPassStats(who);
    bz_sendTextMessage(BZ_SERVER, who, "Command options:");
    sendHelp(who);
}



// Check if the flag can be dropped at dropPos[] and return true if safe
// landing[] will contain the eventual location where the flag would end up if dropped
bool do_checkFlagDropAtPoint ( int flagID, float dropPos[3], float landing[3] )
{
    assert(world != NULL);
    
    const float size = BZDBCache::worldSize * 0.5f;
    float pos[3];
    pos[0] = ((dropPos[0] < -size) || (dropPos[0] > size)) ? 0.0f : dropPos[0];
    pos[1] = ((dropPos[1] < -size) || (dropPos[1] > size)) ? 0.0f : dropPos[1];
    pos[2] = dropPos[2];      // maxWorldHeight should not be a problem since the flag can not be sent above the "passer"
    
    FlagInfo& thisFlag = *FlagInfo::get(flagID);
    int flagTeam = thisFlag.flag.type->flagTeam;
    
    const float waterLevel = world->getWaterLevel();
    float minZ = 0.0f;
    if (waterLevel > minZ) {
        minZ = waterLevel;
    }
    const float maxZ = MAXFLOAT;
    
    landing[0] = pos[0];
    landing[1] = pos[1];
    landing[2] = pos[2];
    bool safelyDropped = DropGeometry::dropTeamFlag(landing, minZ, maxZ, flagTeam);       // Pretend we are dropping the team flag
    
    return safelyDropped;
}




// Turn passing on/off
void FPassEnable (tPassOption PassingOption, int who)
{
    char msg[kWorkStrLen];
    const char *PlayerName;
    if (PassingOption == FPassEnabled) {
        bz_sendTextMessage(BZ_SERVER, who, "Flag Passing is already that way.");
        return;
    }
    FPassEnabled = PassingOption;
    if (kInvalidPlayerID == who)
        return;
    PlayerStats *StatsForThisPlayer = GetActivePlayerStatsByID(who);
    if (StatsForThisPlayer->thisPlayerID == who)
        PlayerName = StatsForThisPlayer->callsign.c_str();
    else
        PlayerName = getPlayerCallsign(who).c_str();
    snprintf (msg, kWorkStrLen, "*** Flag Passing set to %s by %s",  kPassOptionDesc[(int) FPassEnabled], PlayerName);
    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, msg);
}


// Set the PassingOnDeath option
void SetPassingOnDeathOption (tPassOnDeathOption PassingOption, int who)
{
    char msg[kWorkStrLen];
    const char *PlayerName;
    if (PassingOption == PassOnDeath) {
        bz_sendTextMessage(BZ_SERVER, who, "Flag Passing on death is already that way.");
        return;
    }
    PassOnDeath = PassingOption;
    if (kInvalidPlayerID == who)
        return;
    PlayerStats *StatsForThisPlayer = GetActivePlayerStatsByID(who);
    if (StatsForThisPlayer->thisPlayerID == who)
        PlayerName = StatsForThisPlayer->callsign.c_str();
    else
        PlayerName = getPlayerCallsign(who).c_str();
    snprintf (msg, kWorkStrLen, "*** Passing on death set to %s by %s", kPassOnDeathSettingDesc[(int) PassingOption], PlayerName);
    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, msg);
}



// Set the Hurt option
void SetHurtingOption (tHurtingPassOption HurtingOption, int who)
{
    char msg[kWorkStrLen];
    const char *PlayerName;
    if (HurtingOption == HurtOption) {
        bz_sendTextMessage(BZ_SERVER, who, "Hurting option is already that way.");
        return;
    }
    HurtOption = HurtingOption;
    if (kInvalidPlayerID == who)
        return;
    PlayerStats *StatsForThisPlayer = GetActivePlayerStatsByID(who);
    if (StatsForThisPlayer->thisPlayerID == who)
        PlayerName = StatsForThisPlayer->callsign.c_str();
    else
        PlayerName = getPlayerCallsign(who).c_str();
    snprintf (msg, kWorkStrLen, "*** Hurting option set to %s by %s", kHurtSettingDesc[(int) HurtingOption], PlayerName);
    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, msg);
}





// Set the Fumble Message option
void FumbleMsgEnable (tFumbleMsgOption MsgOption, int who)
{
    char msg[kWorkStrLen];
    const char *PlayerName;
    if (MsgOption == FumbleMsg) {
        bz_sendTextMessage(BZ_SERVER, who, "Fumble Messaging is already that way.");
        return;
    }
    FumbleMsg = MsgOption;
    if (kInvalidPlayerID == who)
        return;
    PlayerStats *StatsForThisPlayer = GetActivePlayerStatsByID(who);
    if (StatsForThisPlayer->thisPlayerID == who)
        PlayerName = StatsForThisPlayer->callsign.c_str();
    else
        PlayerName = getPlayerCallsign(who).c_str();
    snprintf (msg, kWorkStrLen, "*** Fumble Messages changed to \"%s\" by %s", kFumbleMsgSettingDesc[(int) MsgOption], PlayerName);
    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, msg);
}





// Set the option to pass only Team flags or all flags
void FlagPassingEnableFor (const tAllowedFlagGroups DesiredFlagGroup, int who)
{
    char msg[kWorkStrLen];
    const char *PlayerName;
    if (DesiredFlagGroup == FlagsAllowed) {
        bz_sendTextMessage(BZ_SERVER, who, "Flags effected are already that way.");
        return;
    }
    FlagsAllowed = DesiredFlagGroup;
    if (kInvalidPlayerID == who)
        return;
    PlayerStats *StatsForThisPlayer = GetActivePlayerStatsByID(who);
    if (StatsForThisPlayer->thisPlayerID == who)
        PlayerName = StatsForThisPlayer->callsign.c_str();
    else
        PlayerName = getPlayerCallsign(who).c_str();
    
    snprintf (msg, kWorkStrLen, "*** Passable Flags set to \"%s\" by %s", (kAllowedFlagGroups_AllFlags == FlagsAllowed) ? kPassableFlagGroupOptionDesc[kPassFlgGrp_Everything] : (kAllowedFlagGroups_TeamFlags == FlagsAllowed) ? kPassableFlagGroupOptionDesc[kPassFlgGrp_TeamFlags] : kPassableFlagGroupOptionDesc[kPassFlgGrp_Custom], PlayerName);
    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, msg);
}



// Set the "dist=" value
bool SetThrowDistance(const char *FloatStr, int *CharsUsed)
{
    float FloatVal = 0.0;
    char *EndOfNum = (char *) FloatStr;
    FloatVal = strtof (FloatStr, &EndOfNum);
    if (EndOfNum == FloatStr)
        return false;
    if (CharsUsed)
        *CharsUsed = (EndOfNum - FloatStr);
    if ((0.0 > FloatVal) || (20.0 < FloatVal))
        return false;
    bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "PassTheFlag: Flag Throw Distance: Changed from %f to %f", FPassThrowDistance, FloatVal); fflush (stdout);
    FPassThrowDistance = FloatVal;
    return true;
}



// Set the "dist=" value
bool SetJumpBoost(const char *FloatStr, int *CharsUsed)
{
    float FloatVal = 0.0;
    char *EndOfNum = (char *) FloatStr;
    FloatVal = strtof (FloatStr, &EndOfNum);
    if (EndOfNum == FloatStr)
        return false;
    if (CharsUsed)
        *CharsUsed = (EndOfNum - FloatStr);
    if ((0.2 > FloatVal) || (5.0 < FloatVal))
        return false;
    bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "PassTheFlag: Jump Boost Factor: Changed from %f to %f", JumpBoostFactor, FloatVal); fflush (stdout);
    JumpBoostFactor = FloatVal;
    return true;
}





// Set the maxwait value
bool FPassHandler::SetMaxWaitVal(const char *FloatStr, int *CharsUsed)
{
    float FloatVal = 0.0;
    char *EndOfNum = (char *) FloatStr;
    FloatVal = strtof (FloatStr, &EndOfNum);
    if (EndOfNum == FloatStr)
        return false;
    if (CharsUsed)
        *CharsUsed = (EndOfNum - FloatStr);
    if ((0.0 > FloatVal) || (3.0 < FloatVal))
        return false;
    MaxWaitForReason = FloatVal;
    MaxWaitTime = MaxWaitForReason;
    return true;
}


// Set the "steps=" value
bool SetMaxIterations(const char *IntStr, int *CharsUsed)
{
    int IntVal = 0;
    char *EndOfNum = (char *) IntStr;
    IntVal = strtol (IntStr, &EndOfNum, 10);
    if (EndOfNum == IntStr)
        return false;
    if (CharsUsed)
        *CharsUsed = (EndOfNum - IntStr);
    if ((0 > IntVal) || (20 < IntVal))
        return false;
    bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "PassTheFlag: Max Attempts to land flag: Changed from %d to %d", MaxSafeZoneTests, IntVal);
    MaxSafeZoneTests = IntVal;
    return true;
}



bool SetFlagList(const char *FlagListStr, tAllowedFlagGroups &DesiredValue, int *CharsUsed, bool LoadPluginCmd)
{
    bz_debugMessagef(DbgLevelDbgInfo, "++++++ SetFlagList()  FlagListStr = \"%s\"",  FlagListStr); fflush (stdout);
    
    DesiredValue = kAllowedFlagGroups_NoFlags;
    
    char FlagEntry[3];
    char TerminationChar = LoadPluginCmd ? ']' : '}';
    const char *CurrentFlag = FlagListStr;
    
    while ((*CurrentFlag) && (TerminationChar != *CurrentFlag))
    {
        bz_debugMessagef(DbgLevelDbgInfo, "++++++ SetFlagList()  CurrentFlag = \"%s\"",  CurrentFlag); fflush (stdout);
        FlagEntry[0] = *CurrentFlag;
        CurrentFlag++;
        if ((*CurrentFlag) && (',' != *CurrentFlag) && (TerminationChar != *CurrentFlag))
        {
            FlagEntry[1] = *CurrentFlag;
            FlagEntry[2] = 0;
            CurrentFlag++;
        }
        else
            FlagEntry[1] = 0;
        
        if ((*CurrentFlag) && (',' == *CurrentFlag))
            CurrentFlag++;
        else if (TerminationChar != *CurrentFlag)
            return false;     // expected a delimeter or terminator
        
        tFlagType ThisFlag = GetFlagTypeFromAbbr(FlagEntry);
        bz_debugMessagef(DbgLevelDbgInfo, "++++++ SetFlagList()  FlagEntry = \"%s\" ThisFlag=%d",  FlagEntry, ThisFlag); fflush (stdout);
        if (kFlagType_Error == ThisFlag)
            return false;
        if (DesiredValue.test(ThisFlag))
            return false;       // duplicate entry
        DesiredValue.set(ThisFlag);
    }
    
    if (TerminationChar != *CurrentFlag)
        return false;
    if (!LoadPluginCmd)
    {
        // Check for extra characters after TerminationChar character
        if (1 != strlen(CurrentFlag))
            return false;
    }
    CurrentFlag++;
    if (CharsUsed)
        *CharsUsed = CurrentFlag - FlagListStr;
    return true;
}



//================================= Process this drop event ======================================================

void sendFlagUpdateUsingLocalBuffer(FlagInfo &flag)
{
    static char LocalBuff[MaxPacketLen];
    void *buf, *bufStart = &LocalBuff[2 * sizeof(uint16_t)];      // Leave room at the beginning for stuff
    buf = nboPackUShort(bufStart, 1);
    bool hide = (flag.flag.type->flagTeam == ::NoTeam) && (flag.player == -1);
    buf = flag.pack(buf, hide);
    broadcastMessage(MsgFlagUpdate, (char*)buf - (char*)bufStart, bufStart);
}




void ProcessDropEvent(DropEvent &PendingDropEvent)
{
    bool NeedToCalculateLandingPos = true;
    bool ValidFlagThrow = false;;
    float FlagLandingPos[3];
    GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(PendingDropEvent.PlayerThatDroppedTheFlag);
    
    bz_debugMessagef(DbgLevelDbgInfo, "ProcessDropEvent (!playerData)"); fflush (stdout);
    if (!playerData)
        return;     // player not known
    DebugMessage("PDE (playerData)");
    bz_debugMessagef(DbgLevelDbgInfo, "ProcessDropEvent (playerData->isParting)"); fflush (stdout);
    if (playerData->isParting)
        return;     // Player is being kicked - Just handle as a normal drop
    DebugMessage("PDE (!isParting)");
    bz_debugMessagef(DbgLevelDbgInfo, "ProcessDropEvent (!playerData->player.isHuman())"); fflush (stdout);
    if (!playerData->player.isHuman())
        return;     // Not for this plugin to handle
    bz_debugMessagef(DbgLevelDbgInfo, "ProcessDropEvent (playerData->player.isPaused())"); fflush (stdout);
    if (playerData->player.isPaused())
        return;     // Dropped due to pause
    DebugMessage("PDE (!isPaused)");
    bz_debugMessagef(DbgLevelDbgInfo, "ProcessDropEvent (!playerData->player.isAlive())"); fflush (stdout);
    if (!playerData->player.isAlive())
    {
        DebugMessage("PDE (!isAlive)");
        // Player Died - so there might be some special things to do
        bz_debugMessagef(DbgLevelDbgInfo, "ProcessDropEvent (kPOD_No == PassOnDeath)"); fflush (stdout);
        if (kPOD_No == PassOnDeath)
            return;
        bz_debugMessagef(DbgLevelDbgInfo, "ProcessDropEvent (kPOD_Hurts == PassOnDeath)"); fflush (stdout);
        if (kPOD_Hurts == PassOnDeath)
        {
            // Find out how player died
            int               KillerID;
            bz_eTeamType  KillerTeam;
            bz_eTeamType  playerTeam;
            bz_debugMessagef(DbgLevelDbgInfo, "ProcessDropEvent (getPlayerKiller)"); fflush (stdout);
            if (!getPlayerKiller(PendingDropEvent.PlayerThatDroppedTheFlag, playerTeam, KillerID, KillerTeam))
                return;     // Not a standard kill, so just drop the flag
            bz_debugMessagef(DbgLevelDbgInfo, "ProcessDropEvent ((eNoTeam == KillerTeam) || (kInvalidPlayerID == KillerID) || (eNoTeam == playerTeam) || (kInvalidPlayerID == PendingDropEvent.PlayerThatDroppedTheFlag))"); fflush (stdout);
            if ((eNoTeam == KillerTeam) || (kInvalidPlayerID == KillerID) || (eNoTeam == playerTeam) || (kInvalidPlayerID == PendingDropEvent.PlayerThatDroppedTheFlag))
                return;     // Not a standard kill, so just drop the flag
            bz_debugMessagef(DbgLevelDbgInfo, "ProcessDropEvent (kHPO_ToNonTeamKiller == HurtOption)"); fflush (stdout);
            if (kHPO_ToNonTeamKiller == HurtOption)
                if ((playerTeam == KillerTeam) && ((eRedTeam == playerTeam) || (eGreenTeam == playerTeam) || (eBlueTeam == playerTeam) || (ePurpleTeam == playerTeam)))
                    return;       // Just drop it, it was a TK
            float FlagDropPos[3];
            if (getPlayerPosition(KillerID, FlagDropPos))
            {
                bz_debugMessagef(DbgLevelDbgInfo, "             KillerPos(HurtOption): %f, %f, %f", FlagDropPos[0], FlagDropPos[1], FlagDropPos[2]); fflush (stdout);
            }
            else
            {
                bz_debugMessagef(DbgLevelErr, "++++++ FPassHandler: KillerPos(HurtOption): ERR"); fflush (stdout);
                return;
            }
            ValidFlagThrow = do_checkFlagDropAtPoint(PendingDropEvent.DroppedFlagID, FlagDropPos, FlagLandingPos);
            if (!ValidFlagThrow)
                return;     // Killer is not in a safe location for a flag
            NeedToCalculateLandingPos = false;
        }
        // kPOD_Yes is active so just let things continue
    }
    
    if (NeedToCalculateLandingPos)
    {
        float FlagDropPos[3];
        
        float JumpBoost = 1.0;
        if (JumpBoostFactor != 0.0)
            if (PendingDropEvent.PlayerVelocityAtThisTime[2] > 0.0)
                JumpBoost = JumpBoostFactor;
            else if (PendingDropEvent.PlayerVelocityAtThisTime[2] < 0.0)
                JumpBoost = 1.0 / JumpBoostFactor;
        FlagDropPos[0] = PendingDropEvent.DropPos[0] + FPassThrowDistance * PendingDropEvent.PlayerVelocityAtThisTime[0] * JumpBoost;
        FlagDropPos[1] = PendingDropEvent.DropPos[1] + FPassThrowDistance * PendingDropEvent.PlayerVelocityAtThisTime[1] * JumpBoost;
        FlagDropPos[2] = PendingDropEvent.DropPos[2];
        bool PassWasFumbled = false;
        int TriesLeft = MaxSafeZoneTests;
        float DeltaX, DeltaY ;
        DeltaX = DeltaY = 0.0;
        if (0 < MaxSafeZoneTests)
        {
            DeltaX = (PendingDropEvent.DropPos[0] - FlagDropPos[0]) / MaxSafeZoneTests;
            DeltaY = (PendingDropEvent.DropPos[1] - FlagDropPos[1]) / MaxSafeZoneTests;
        }
        do
        {
            ValidFlagThrow = do_checkFlagDropAtPoint(PendingDropEvent.DroppedFlagID, FlagDropPos, FlagLandingPos);
            DebugMessage("PDE %d Flg=%d D=(%.4f,%.4f) L=(%.4f,%.4f) %s", TriesLeft, PendingDropEvent.DroppedFlagID, FlagDropPos[0], FlagDropPos[1], FlagLandingPos[0], FlagLandingPos[1], ValidFlagThrow ? "Y" : "N");
            // Check for flags that were left up high
            if (ValidFlagThrow)
                ValidFlagThrow = (FlagLandingPos[2] <= FlagDropPos[2]) || (0.0 >= FlagLandingPos[2]);       // Remember to allow for burrowed tanks
            // Check for flags that need to be moved
            if (ValidFlagThrow)
                ValidFlagThrow = (FlagLandingPos[0] == FlagDropPos[0]) && (FlagLandingPos[1] == FlagDropPos[1]);        // Perhaps we should allow a tolerance here
            if (!PassWasFumbled)
                PassWasFumbled = !ValidFlagThrow;
            TriesLeft--;
            if ((!ValidFlagThrow) && (TriesLeft >= 0))
            {
                FlagDropPos[0] += DeltaX;
                FlagDropPos[1] += DeltaY;
            }
        }
        while ((!ValidFlagThrow) && (TriesLeft >= 0));
        if (PassWasFumbled && ValidFlagThrow)
        {
            if (kFMO_TellEveryone == FumbleMsg)
            {
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s fumbled the pass.", GetActivePlayerStatsByID(PendingDropEvent.PlayerThatDroppedTheFlag)->callsign.c_str());
            }
            else if (kFMO_TellPlayer == FumbleMsg)
            {
                int randomMsg = rand() % kMaxFumbledPassMessages;
                bz_sendTextMessagef(BZ_SERVER, PendingDropEvent.PlayerThatDroppedTheFlag, kFumbledPassMessages[randomMsg]);
            }
        }
    }
    // All testing etc done So now dropkick the flag already
    if (ValidFlagThrow)
    {
        FlagInfo& flag = *FlagInfo::get(PendingDropEvent.DroppedFlagID);
        
        flag.dropFlag(PendingDropEvent.DropPos, FlagLandingPos, false);
        sendFlagUpdateUsingLocalBuffer(flag);     // Do not send message through normal channels as it will interfere with the DirectMessageBuffer contents being generated by the original event
        bz_debugMessagef(DbgLevelDbgInfo, "             FlagLandingPos:  %f, %f, %f", FlagLandingPos[0], FlagLandingPos[1], FlagLandingPos[2]); fflush (stdout);
        DebugMessage("Sent Flag");
    }
}


// Locate any pending drop events for player PlayerID and process them
// Written to handle more than one, although since we call this on bz_eFlagGrabbedEvent it should not be possible
bool ProcessPendingEventsForPlayer(int PlayerID, float *WaitingSinceThisTime, bz_eEventType Reason)
{
    bool SomethingProcessed = false;
    std::vector<DropEvent>::iterator iter = gPendingDropEvents.begin();
    while ( iter != gPendingDropEvents.end() )
    {
        if (iter->PlayerThatDroppedTheFlag == PlayerID)
        {
            bool ProcessThisEvent = true;
            if (WaitingSinceThisTime)
                ProcessThisEvent = (iter->TimeEventOccurred <= *WaitingSinceThisTime);
            if (ProcessThisEvent)
            {
                DebugMessage("ProcessEvt: Evt=%d Player=%d EvtTime=%f MaxWait=%f since=%f", Reason, iter->PlayerThatDroppedTheFlag, iter->TimeEventOccurred, MaxWaitForReason, WaitingSinceThisTime ? *WaitingSinceThisTime : 0.0);
                bz_debugMessagef(DbgLevelDbgInfo, "ProcessPendingEventsForPlayer: found PlayerThatDroppedTheFlag=%d", iter->PlayerThatDroppedTheFlag); fflush (stdout);
                ProcessDropEvent( *iter );
                iter = gPendingDropEvents.erase( iter );
                SomethingProcessed = true;
            }
            else
                ++iter;
        }
        else
            ++iter;
    }
    return SomethingProcessed;
}



// If the user has a bad connection or is NR then we want to get rid of any drop events they have pending
void RemoveOldEvents(float CurrentTime)
{
    std::vector<DropEvent>::iterator iter = gPendingDropEvents.begin();
    while ( iter != gPendingDropEvents.end() )
    {
        if ((CurrentTime - iter->TimeEventOccurred) > MaxWaitForReason)
        {
            DebugMessage("OldEvt: Player=%d EvtTime=%f CurTime=%f diff=%f MaxWait=%f", iter->PlayerThatDroppedTheFlag, iter->TimeEventOccurred, CurrentTime, CurrentTime - iter->TimeEventOccurred, MaxWaitForReason);
            bz_debugMessagef(DbgLevelDbgInfo, "RemoveOldEvents: PlayerThatDroppedTheFlag=%d EvtTime=%f CurTime=%f diff=%f MaxWaitForReason=%f", iter->PlayerThatDroppedTheFlag, iter->TimeEventOccurred, CurrentTime, CurrentTime - iter->TimeEventOccurred, MaxWaitForReason); fflush (stdout);
            if (AllowMaxWaitMod)
            {
                // Give some feedback to allow some tweeking
                bz_sendTextMessagef(BZ_SERVER, iter->PlayerThatDroppedTheFlag, "Pass assumed! Your client took too long to respond. I waited %.3f seconds.", CurrentTime - iter->TimeEventOccurred);
            }
            ProcessDropEvent( *iter );
            iter = gPendingDropEvents.erase( iter );
        }
        else
            ++iter;
    }
}





//================================= Communication from the server ======================================================


// handle events
void FPassHandler::Event (bz_EventData *eventData)
{
    if (AllowMaxWaitMod)
    {
        // Give some feedback to allow some tweeking
        bz_debugMessagef(DbgLevelInfo, "FPassHandler::process eventType=%d seconds= %f", eventData->eventType, TimeKeeper::getCurrent().getSeconds()); fflush (stdout);
    }
    // ***************************************
    //   Flag Dropped (bz_eFlagDroppedEvent)
    // ***************************************
    if (bz_eFlagDroppedEvent == eventData->eventType)
    {
        bz_FlagDroppedEventData_V1 *dropData = (bz_FlagDroppedEventData_V1*)eventData;
        FlagInfo& thisFlag = *FlagInfo::get(dropData->flagID);
        bool FlagIsPassible = (kPassing_Off != FPassEnabled) && (ThisFlagIsPassible(GetFlagTypeFromAbbr(thisFlag.flag.type->flagAbbv.c_str())));
        
        if (FlagIsPassible)
        {
            // Find out why the flag is being dropped
            GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(dropData->playerID);
            bz_debugMessagef(DbgLevelDbgInfo, "bz_eFlagDroppedEvent (!playerData)"); fflush (stdout);
            if (!playerData)
                return;     // player not known
            bz_debugMessagef(DbgLevelDbgInfo, "bz_eFlagDroppedEvent (playerData->isParting)"); fflush (stdout);
            if (playerData->isParting)
                return;     // Player is being kicked - Just handle as a normal drop
            bz_debugMessagef(DbgLevelDbgInfo, "bz_eFlagDroppedEvent (!playerData->player.isHuman())"); fflush (stdout);
            if (!playerData->player.isHuman())
                return;     // Not for this plugin to handle
            
            // We will not know exactly why the flag was dropped until the client tells us
            // So remember this event and check later when we have better data to work with
            
            float PlayerPos[3];
            float PlayerVelocity[3];
            if (getPlayerVelocity(dropData->playerID, PlayerVelocity))
            {
                bz_debugMessagef(DbgLevelDbgInfo, "             velocity: %f, %f, %f", PlayerVelocity[0], PlayerVelocity[1], PlayerVelocity[2]); fflush (stdout);
            }
            else
            {
                bz_debugMessagef(DbgLevelErr, "++++++ FPassHandler: velocity: ERR"); fflush (stdout);
                return;
            }
            if (getPlayerPosition(dropData->playerID, PlayerPos))
            {
                bz_debugMessagef(DbgLevelDbgInfo, "             PlayerPos: %f, %f, %f", PlayerPos[0], PlayerPos[1], PlayerPos[2]); fflush (stdout);
            }
            else
            {
                bz_debugMessagef(DbgLevelErr, "++++++ FPassHandler: PlayerPos: ERR"); fflush (stdout);
                return;
            }
            DropEvent ThisFlagDrop(dropData->playerID, dropData->flagID, dropData->pos, PlayerPos, PlayerVelocity);
            if (kPassing_Immediate == FPassEnabled)
            {
                // We do not care about the fancy options we just want an immediate response
                ProcessDropEvent(ThisFlagDrop);
            }
            else
            {
                if ((0.0 == PlayerVelocity[0]) && (0.0 == PlayerVelocity[1]))
                    if (kPOD_No == PassOnDeath)
                        return;   // Nothing to do here - It will just drop anyway
                
                // Fancy options come at a price. There can be a delay before flag starts to fly.
                // It just means there is more to learn for your flag-passing skills
                gPendingDropEvents.push_back(ThisFlagDrop);
                bz_debugMessagef(DbgLevelDbgInfo, "gPendingDropEvents added (player=%d Flag=%d)", dropData->playerID, dropData->flagID); fflush (stdout);
            }
        }
    }
    // ***************************************
    //   Player Update (bz_ePlayerUpdateEvent)
    // ***************************************
    else if (bz_ePlayerUpdateEvent == eventData->eventType)
    {
        bz_PlayerUpdateEventData_V1* playerupdatedata = (bz_PlayerUpdateEventData_V1*)eventData;
        //bz_debugMessagef(DbgLevelDbgInfo, "++++++ FPassHandler: PlayerUpdateEvent for player %d velocity = %f, %f, %f", playerupdatedata->playerID, playerupdatedata->velocity[0], playerupdatedata->velocity[1], playerupdatedata->velocity[2]); fflush (stdout);
        PlayerStats *StatsForThisPlayer = GetActivePlayerStatsByID(playerupdatedata->playerID);
        if (StatsForThisPlayer->thisPlayerID == playerupdatedata->playerID)
        {
            float EventTime = playerupdatedata->stateTime;
            StatsForThisPlayer->SetStats(playerupdatedata->playerID, playerupdatedata->lastState.velocity);
            if (0.0 != EventTime)
            {
                EventTime -= 0.04;        // This event might not have come from the client. Any bz_ePlayerUpdateEvent received in less than 1/25 of a second must have come from another plugin
                ProcessPendingEventsForPlayer(playerupdatedata->playerID, &EventTime, eventData->eventType);
            }
            //bz_debugMessagef(DbgLevelDbgInfo, "++++++ FPassHandler: PlayerUpdateEvent for player %d", playerupdatedata->playerID); fflush (stdout);
        }
        else
        {
            bz_debugMessagef(DbgLevelErr, "++++++ FPassHandler: PlayerUpdate: ERR"); fflush (stdout);
        }
    }
    // ***************************************
    //   Player Spawned (bz_ePlayerSpawnEvent)
    // ***************************************
    else if (bz_ePlayerSpawnEvent == eventData->eventType)
    {
        bz_PlayerSpawnEventData_V1* birthDetails = (bz_PlayerSpawnEventData_V1*)eventData;
        PlayerStats *StatsForThisPlayer = GetActivePlayerStatsByID(birthDetails->playerID);
        if (StatsForThisPlayer->thisPlayerID == birthDetails->playerID)
        {
            StatsForThisPlayer->SetKiller(kInvalidPlayerID, eNoTeam);
        }
        else
        {
            bz_debugMessagef(DbgLevelErr, "++++++ FPassHandler: PlayerUpdate(bz_ePlayerSpawnEvent): ERR"); fflush (stdout);
        }
    }
    // ***************************************
    //   Player Died (bz_ePlayerDieEvent)
    // ***************************************
    else if (bz_ePlayerDieEvent == eventData->eventType)
    {
        bz_PlayerDieEventData_V1* deathDetails = ( bz_PlayerDieEventData_V1*)eventData;
        PlayerStats *StatsForThisPlayer = GetActivePlayerStatsByID(deathDetails->playerID);
        if (StatsForThisPlayer->thisPlayerID == deathDetails->playerID)
        {
            bz_debugMessagef(DbgLevelDbgInfo, "bz_ePlayerDieEvent (%d killed by %d of team %d)", deathDetails->playerID, deathDetails->killerID, deathDetails->killerTeam); fflush (stdout);
            StatsForThisPlayer->SetKiller(deathDetails->killerID, deathDetails->killerTeam);
            ProcessPendingEventsForPlayer(deathDetails->playerID, NULL, eventData->eventType);
        }
        else
        {
            bz_debugMessagef(DbgLevelErr, "++++++ FPassHandler: PlayerUpdate(bz_ePlayerDieEvent): ERR"); fflush (stdout);
        }
    }
    // ***************************************
    //   Player Joined (bz_ePlayerJoinEvent)
    // ***************************************
    else if (bz_ePlayerJoinEvent == eventData->eventType)
    {
        bz_PlayerJoinPartEventData_V1 *joinData = (bz_PlayerJoinPartEventData_V1*)eventData;
        if (joinData->record->team != eObservers)
        {
            float NoVelocity[3];
            NoVelocity[0] = NoVelocity[1] = NoVelocity[2] = 0.0;
            PlayerStats NewPlayer(joinData->playerID, joinData->record->team, NoVelocity, &joinData->record->callsign);
            gActivePlayers.push_back(NewPlayer);
            bz_debugMessagef(DbgLevelDbgInfo, "++++++ FPassHandler: created PlayerStats for %d", joinData->playerID); fflush (stdout);
        }
    }
    // ***************************************
    //   Player Left (bz_ePlayerPartEvent)
    // ***************************************
    else if (bz_ePlayerPartEvent == eventData->eventType)
    {
        bz_PlayerJoinPartEventData_V1 *partingData = (bz_PlayerJoinPartEventData_V1*)eventData;
        if (PlayerWithDebugAccess == partingData->playerID)
            PlayerWithDebugAccess = kInvalidPlayerID;
        if (partingData->record->team != eObservers)
        {
            if (RemovePlayerStatsForPlayerID(partingData->playerID))
            {
                bz_debugMessagef(DbgLevelDbgInfo, "++++++ FPassHandler: removed PlayerStats for %d", partingData->playerID); fflush (stdout);
            }
            else
            {
                bz_debugMessagef(DbgLevelErr, "++++++ FPassHandler: ERR no PlayerStats for player %d", partingData->playerID); fflush (stdout);
            }
        }
    }
    // ***************************************
    //   Player Left (bz_eFlagGrabbedEvent)
    // ***************************************
    else if (bz_eFlagGrabbedEvent == eventData->eventType)
    {
        bz_FlagGrabbedEventData_V1 *msgData = (bz_FlagGrabbedEventData_V1*)eventData;
        // If the client has told us about grabbing a flag we know that we have enough information to process any pending drop event
        if (ProcessPendingEventsForPlayer(msgData->playerID, NULL, eventData->eventType))
        {
            // Make sure the player still has this flag
            // This is why you should stay within the bounds of the API - you just confuse the server otherwise
            GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(msgData->playerID);
            if (playerData)
            {
                playerData->player.setFlag(msgData->flagID);
            }
        }
    }
    // ***************************************
    //   Flag Captured (bz_eCaptureEvent)
    // ***************************************
    else if (bz_eCaptureEvent == eventData->eventType)
    {
        bz_CTFCaptureEventData_V1 *capData = (bz_CTFCaptureEventData_V1*)eventData;
        
        for (unsigned int i = 0, totalFlags = bz_getNumFlags(); i < totalFlags; i++)
        {
            std::string flagName = bz_getName(i);
            
            if ((capData->teamCapped == eRedTeam    && flagName == "R*") ||
                (capData->teamCapped == eGreenTeam  && flagName == "G*") ||
                (capData->teamCapped == eBlueTeam   && flagName == "B*") ||
                (capData->teamCapped == ePurpleTeam && flagName == "P*"))
            {
                bz_resetFlag(i);
                break;
            }
            else if (flagName != "R*" && flagName != "G*" && flagName == "B*" && flagName != "P*")
            {
                break;
            }
        }
    }
    // ***************************************
    //   Player Left (bz_eTickEvent)
    // ***************************************
    else if (bz_eTickEvent == eventData->eventType)
    {
        RemoveOldEvents (bz_getCurrentTime());
    }
}

#define kMaxCmdParamSize                                kWorkStrLen

#define kAdminPermission                                ""
#define kCountdownPermission                            "COUNTDOWN"

#define kCmdLine_Cmd                                        "fpass"
#define kOption_noPermissionRequired                "nopermsneeded"
#define kOption_PassWhileDying                      "passwhiledying"
#define kOption_PassToNonTeamKiller                 "pass2nontkiller"
#define kOption_PassToAnyKiller                     "pass2anykiller"
#define kOption_PreventMaxWaitMofification      "mwmodify"
#define kOption_maxwait                                 "maxwait"
#define kCmdLineParam_maxwait                           kOption_maxwait"="
#define kOption_dist                                        "dist"
#define kCmdLineParam_distance                      kOption_dist"="
#define kOption_jumpboost                               "jboost"
#define kCmdLineParam_jumpboost                     kOption_jumpboost"="
#define kOption_steps                                   "steps"
#define kCmdLineParam_SafeLandingAttempts           kOption_steps"="
#define kOption_FumbleMsg                               "fmsg"
#define kCmdLineParam_FumbleMsg                     kOption_FumbleMsg"="
#define kOption_AllFlags                                "allflags"
#define kOption_TeamFlags                               "teamflags"
#define kOption_CustomFlags                         "customflags"
#define kCmdLineParam_CustomFlags                   kOption_CustomFlags"={"
#define kCmdLineParam_PluginCustomFlags         kOption_CustomFlags"=["
#define kOption_ToggleFlags                         "toggleflags"
#define kCmdLineParam_ToggleFlags                   kOption_ToggleFlags"={"
#define kOption_PassOnDeath                         "passondeath"
#define kCmdLineParam_PassOnDeath                   kOption_PassOnDeath"="
#define kOption_Hurt                                        "hurt"
#define kCmdLineParam_Hurt                              kOption_Hurt"="
#define kOption_DebugAccess                         "debugaccess"
#define kCmdLineParam_DebugAccess                   kOption_DebugAccess"="



// handle /FPass command
bool FPassHandler::SlashCommand ( int playerID, bz_ApiString cmd, bz_ApiString, bz_APIStringList* cmdParams )
{
    char subCmd[kWorkStrLen];
    if (strcasecmp (cmd.c_str(), kCmdLine_Cmd))   // is it for me ?
        return false;
    if (cmdParams->get(0).c_str()[0] == '\0') {
        sendHelp (playerID);
        return true;
    }
    
    
    bz_debugMessagef(DbgLevelDbgInfo, "++++++ FPassHandler::handle:  cmdParams->get(0).c_str() = \"%s\"",  cmdParams->get(0).c_str()); fflush (stdout);
    strncpy (subCmd, cmdParams->get(0).c_str(), kWorkStrLen - 1);
    subCmd[kWorkStrLen - 1] = '\0';
    bz_debugMessagef(DbgLevelDbgInfo, "++++++ FPassHandler::handle:  subCmd = \"%s\"",  subCmd); fflush (stdout);
    // kCmdLineParam_distance
    if (strncasecmp (subCmd, kCmdLineParam_distance, strlen(kCmdLineParam_distance)) == 0)
    {
        if (checkPerms (playerID, kOption_dist, kAdminPermission))
        {
            if (!SetThrowDistance(cmdParams->get(0).c_str() + strlen(kCmdLineParam_distance), NULL))
            {
                sendHelp(playerID);
            }
        }
    }
    // kCmdLineParam_jumpboost
    else if (strncasecmp (subCmd, kCmdLineParam_jumpboost, strlen(kCmdLineParam_jumpboost)) == 0)
    {
        if (checkPerms (playerID, kOption_jumpboost, kAdminPermission))
        {
            if (!SetJumpBoost(cmdParams->get(0).c_str() + strlen(kCmdLineParam_jumpboost), NULL))
            {
                sendHelp(playerID);
            }
        }
    }
    // kCmdLineParam_maxwait
    else if (strncasecmp (subCmd, kOption_maxwait, strlen(kOption_maxwait)) == 0)
    {
        if (checkPerms (playerID, kOption_maxwait, kAdminPermission))
        {
            if (((playerID == PlayerWithDebugAccess) || AllowMaxWaitMod) && ('=' == cmdParams->get(0).c_str()[strlen(kOption_maxwait)]))
                SetMaxWaitVal(cmdParams->get(0).c_str() + strlen(kCmdLineParam_maxwait), NULL);
            sendMaxWaitMsg(playerID);
        }
    }
    // kCmdLineParam_FumbleMsg
    else if (strncasecmp (subCmd, kCmdLineParam_FumbleMsg, strlen(kCmdLineParam_FumbleMsg)) == 0)
    {
        if (checkPerms (playerID, kOption_FumbleMsg, kAdminPermission))
        {
            char CmdParam[kMaxCmdParamSize];
            strncpy (CmdParam, cmdParams->get(0).c_str() + strlen(kCmdLineParam_FumbleMsg), kMaxCmdParamSize - 1);
            if (strcasecmp (CmdParam, "off") == 0)
                FumbleMsgEnable (kFMO_NoMessages, playerID);
            else if (strcasecmp (CmdParam, "all") == 0)
                FumbleMsgEnable (kFMO_TellEveryone, playerID);
            else if (strcasecmp (CmdParam, "player") == 0)
                FumbleMsgEnable (kFMO_TellPlayer, playerID);
            else
            {
                sendHelp(playerID);
            }
        }
    }
    // kCmdLineParam_ToggleFlags
    else if (strncasecmp (subCmd, kCmdLineParam_ToggleFlags, strlen(kCmdLineParam_ToggleFlags)) == 0)
    {
        if (checkPerms (playerID, kOption_ToggleFlags, kAdminPermission))
        {
            tAllowedFlagGroups FlagsToSwitch = kAllowedFlagGroups_NoFlags;
            if (SetFlagList(cmdParams->get(0).c_str() + strlen(kCmdLineParam_ToggleFlags), FlagsToSwitch, NULL, false))
            {
                if (FlagsToSwitch.any())
                {
                    FlagsToSwitch ^= FlagsAllowed;
                    FlagPassingEnableFor (FlagsToSwitch, playerID);
                }
            }
            else
            {
                sendHelp(playerID);
            }
        }
    }
    // kCmdLineParam_CustomFlags
    else if (strncasecmp (subCmd, kCmdLineParam_CustomFlags, strlen(kCmdLineParam_CustomFlags)) == 0)
    {
        if (checkPerms (playerID, kOption_CustomFlags, kAdminPermission))
        {
            tAllowedFlagGroups DesiredValue = kAllowedFlagGroups_NoFlags;
            if (SetFlagList(cmdParams->get(0).c_str() + strlen(kCmdLineParam_CustomFlags), DesiredValue, NULL, false))
            {
                FlagPassingEnableFor (DesiredValue, playerID);
            }
            else
            {
                sendHelp(playerID);
            }
        }
    }
    // kCmdLineParam_SafeLandingAttempts
    else if (strncasecmp (subCmd, kCmdLineParam_SafeLandingAttempts, strlen(kCmdLineParam_SafeLandingAttempts)) == 0)
    {
        if (checkPerms (playerID, kOption_steps, kAdminPermission))
        {
            if (!SetMaxIterations(cmdParams->get(0).c_str() + strlen(kCmdLineParam_SafeLandingAttempts), NULL))
            {
                sendHelp(playerID);
            }
        }
    }
    // kCmdLineParam_PassOnDeath
    else if (strncasecmp (subCmd, kCmdLineParam_PassOnDeath, strlen(kCmdLineParam_PassOnDeath)) == 0)
    {
        if (checkPerms (playerID, kOption_PassOnDeath, kAdminPermission))
        {
            char CmdParam[kMaxCmdParamSize];
            strncpy (CmdParam, cmdParams->get(0).c_str() + strlen(kCmdLineParam_PassOnDeath), kMaxCmdParamSize - 1);
            bz_debugMessagef(DbgLevelDbgInfo, "++++++ FPassHandler::handle:  subCmd = \"%s\" CmdParam=\"%s\"",  kCmdLineParam_PassOnDeath, CmdParam); fflush (stdout);
            if (strcasecmp (CmdParam, "off") == 0)
                SetPassingOnDeathOption (kPOD_No, playerID);
            else if (strcasecmp (CmdParam, "on") == 0)
                SetPassingOnDeathOption (kPOD_Yes, playerID);
            else if (strcasecmp (CmdParam, "hurts") == 0)
                SetPassingOnDeathOption (kPOD_Hurts, playerID);
            else
            {
                sendHelp(playerID);
            }
        }
    }
    // hurt
    else if (strncasecmp (subCmd, kCmdLineParam_Hurt, strlen(kCmdLineParam_Hurt)) == 0)
    {
        if (checkPerms (playerID, kOption_Hurt, kAdminPermission))
        {
            char CmdParam[kMaxCmdParamSize];
            strncpy (CmdParam, cmdParams->get(0).c_str() + strlen(kCmdLineParam_Hurt), kMaxCmdParamSize - 1);
            bz_debugMessagef(DbgLevelDbgInfo, "++++++ FPassHandler::handle:  subCmd = \"%s\" CmdParam=\"%s\"",  kCmdLineParam_Hurt, CmdParam); fflush (stdout);
            if (strcasecmp (CmdParam, "killr") == 0)
                SetHurtingOption (kHPO_ToKiller, playerID);
            else if (strcasecmp (CmdParam, "nontker") == 0)
                SetHurtingOption (kHPO_ToNonTeamKiller, playerID);
            else
            {
                sendHelp(playerID);
            }
        }
    }
    else if (strncasecmp (subCmd, kCmdLineParam_DebugAccess, strlen(kCmdLineParam_DebugAccess)) == 0)
    {
        if (checkPerms (playerID, kOption_DebugAccess, kAdminPermission))
        {
            bool PlayerGotDbgAccess = false;
            char CmdParam[kMaxCmdParamSize];
            strncpy (CmdParam, cmdParams->get(0).c_str() + strlen(kCmdLineParam_DebugAccess), kMaxCmdParamSize - 1);
            if (0 != DbgAccessPassword[0])
                if (0 == strcmp(CmdParam, DbgAccessPassword))
                {
                    if (PlayerWithDebugAccess != playerID)
                    {
                        DebugMessage("You have lost Debug Access!!");
                        PlayerWithDebugAccess = playerID;
                        PlayerGotDbgAccess = true;
                        DebugMessage("You have Debug Access!!");
                        DebugMessage("%s", CopyOfPluginParams)
                    }
                    else
                    {
                        PlayerGotDbgAccess = true;
                        DebugMessage("You already have Debug Access!!");
                    }
                }
            if (!PlayerGotDbgAccess)
            {
                DebugMessage( "WARNING!!! Player \"%d\" Attempted to get debug access with password \"%s\"", playerID, CmdParam);
                bz_debugMessagef(DbgLevelDbgInfo, "++++++ FPassHandler:: WARNING!!! Player \"%d\" Attempted to get debug access with password \"%s\"",  playerID, CmdParam); fflush (stdout);
                bz_sendTextMessage(BZ_SERVER, playerID, "Warning!!! Attempting to hack this plugin can get you in trouble. This attempt has been logged.");
            }
        }
    }
    // allflags
    else if (strcasecmp (subCmd, kOption_AllFlags) == 0)
    {
        if (checkPerms (playerID, kOption_AllFlags, kAdminPermission))
            FlagPassingEnableFor (kAllowedFlagGroups_AllFlags, playerID);
    }
    // teamflags
    else if (strcasecmp (subCmd, kOption_TeamFlags) == 0)
    {
        if (checkPerms (playerID, kOption_TeamFlags, kAdminPermission))
            FlagPassingEnableFor (kAllowedFlagGroups_TeamFlags, playerID);
    }
    // off
    else if (strcasecmp (subCmd, "off") == 0)
    {
        if (checkPerms (playerID, "off", kCountdownPermission))
            FPassEnable (kPassing_Off, playerID);
    }
    // on
    else if (strcasecmp (subCmd, "on") == 0)
    {
        if (checkPerms (playerID, "on", kCountdownPermission))
            FPassEnable (kPassing_On, playerID);
    }
    // immediate
    else if (strcasecmp (subCmd, "immediate") == 0)
    {
        if (checkPerms (playerID, "immediate", kCountdownPermission))
            FPassEnable (kPassing_Immediate, playerID);
    }
    // reset
    else if (strcasecmp (subCmd, "reset") == 0)
    {
        if (checkPerms (playerID, "reset", kAdminPermission))
            ResetAllVariables (playerID);
    }
    // stat
    else if (strcasecmp (subCmd, "stat") == 0)
        FPassStats (playerID);
    // help
    else if (strcasecmp (subCmd, "help") == 0)
        FPassSendDesc (playerID);
    else
        sendHelp (playerID);
    return true;
}


//================================= Examine which options were set at loadplugin time ======================================================


bool commandLineHelp (void) {
    const char *help[] = {
        "Command line args:  -loadplugin PLUGINNAME[,nopermsneeded][,teamflags|customflags=[V,QT,...]][,passwhiledying|pass2nontkiller|pass2anykiller][,maxwait=<0.0 .. 3.0>][,mwmodify][,dist=<0.0 .. 20.0>][,jboost=<0.0 .. 5.0>][,steps=<0 .. 20>],[debugaccess=<password>]",
        "  The options are available to set things other than the default values.",
        "  nopermsneeded - gives everybody permission to adjust config - primarily used for testing"
        "  Specifying \"teamflags\" will pass only team flags, \"customflags\" lets you specicify specific flags, default is all flags. You need to enclose the selection in '[' and ']' characters",
        "  passwhiledying is equivalent to /fpass passondeath=on",
        "  pass2nontkiller is equivalent to /fpass passondeath=hurt",
        "  pass2anykiller is equivalent to /fpass passondeath=hurt and /fpass hurt=killr",
        "  maxwait specifies how long we will wait to find out why the user dropped the flag",
        "  mwmodify will enable the \"/fpass maxwait\" command to modify the value",
        "",
        "  The options do not have to be added at loadplugin time, but if they are added then they need to added in the order shown above.",
        "",
        "  Example:   -loadplugin PassTheFlag,teamflags,dist=6.0,steps=5                is fine",
        "  but        -loadplugin PassTheFlag,dist=6.0,teamflagsonly,steps=5                will result in an error.",
        NULL
    };
    bz_debugMessage (0, "+++ PassTheFlag plugin command-line error");
    for (int x = 0; help[x] != NULL; x++)
        bz_debugMessage (0, help[x]);
    return true;
}




bool FPassHandler::parseCommandLine (const char *cmdLine)
{
    int CharOffset = 0;
    if (cmdLine == NULL || *cmdLine == '\0')
        return false;
    strncpy (CopyOfPluginParams, cmdLine, kWorkStrLen - 1);
    if (strncasecmp (cmdLine + CharOffset, kOption_noPermissionRequired, strlen(kOption_noPermissionRequired)) == 0)
    {
        RequirePermissions = false;
        CharOffset += strlen(kOption_noPermissionRequired) + 1;
    }
    if (strncasecmp (cmdLine + CharOffset, kOption_TeamFlags, strlen(kOption_TeamFlags)) == 0)
    {
        FlagPassingEnableFor(kAllowedFlagGroups_TeamFlags, kInvalidPlayerID);
        CharOffset += strlen(kOption_TeamFlags) + 1;
        AssignResetVal(FlagsAllowed);
    }
    if (strncasecmp (cmdLine + CharOffset, kCmdLineParam_PluginCustomFlags, strlen(kCmdLineParam_PluginCustomFlags)) == 0)
    {
        int CharsUsed = 0;
        CharOffset += strlen(kCmdLineParam_PluginCustomFlags);
        if (!SetFlagList(cmdLine + CharOffset, FlagsAllowed, &CharsUsed, true))
            return commandLineHelp ();
        else
            CharOffset += 1 + CharsUsed;
        AssignResetVal(FlagsAllowed);
    }
    if (strncasecmp (cmdLine + CharOffset, kOption_PassWhileDying, strlen(kOption_PassWhileDying)) == 0)
    {
        SetPassingOnDeathOption(kPOD_Yes, kInvalidPlayerID);
        CharOffset += strlen(kOption_PassWhileDying) + 1;
        AssignResetVal(PassOnDeath);
    }
    if (strncasecmp (cmdLine + CharOffset, kOption_PassToNonTeamKiller, strlen(kOption_PassToNonTeamKiller)) == 0)
    {
        SetPassingOnDeathOption(kPOD_Hurts, kInvalidPlayerID);
        SetHurtingOption(kHPO_ToNonTeamKiller, kInvalidPlayerID);     // Is the default option, but set it anyway
        CharOffset += strlen(kOption_PassToNonTeamKiller) + 1;
        AssignResetVal(PassOnDeath);
        AssignResetVal(HurtOption);
    }
    if (strncasecmp (cmdLine + CharOffset, kOption_PassToAnyKiller, strlen(kOption_PassToAnyKiller)) == 0)
    {
        SetPassingOnDeathOption(kPOD_Hurts, kInvalidPlayerID);
        SetHurtingOption(kHPO_ToKiller, kInvalidPlayerID);
        CharOffset += strlen(kOption_PassToAnyKiller) + 1;
        AssignResetVal(PassOnDeath);
        AssignResetVal(HurtOption);
    }
    if (strncasecmp (cmdLine + CharOffset, kCmdLineParam_maxwait, strlen(kCmdLineParam_maxwait)) == 0)
    {
        int CharsUsed = 0;
        if (!SetMaxWaitVal(cmdLine + CharOffset + strlen(kCmdLineParam_maxwait), &CharsUsed))
            return commandLineHelp ();
        else
            CharOffset += strlen(kCmdLineParam_maxwait) + 1 + CharsUsed;
        AssignResetVal(MaxWaitForReason);
    }
    if (strncasecmp (cmdLine + CharOffset, kOption_PreventMaxWaitMofification, strlen(kOption_PreventMaxWaitMofification)) == 0)
    {
        AllowMaxWaitMod = true;
        CharOffset += strlen(kOption_PreventMaxWaitMofification) + 1;
    }
    if (strncasecmp (cmdLine + CharOffset, kCmdLineParam_distance, strlen(kCmdLineParam_distance)) == 0)
    {
        int CharsUsed = 0;
        if (!SetThrowDistance(cmdLine + CharOffset + strlen(kCmdLineParam_distance), &CharsUsed))
            return commandLineHelp ();
        else
            CharOffset += strlen(kCmdLineParam_distance) + 1 + CharsUsed;
        AssignResetVal(FPassThrowDistance);
    }
    if (strncasecmp (cmdLine + CharOffset, kCmdLineParam_jumpboost, strlen(kCmdLineParam_jumpboost)) == 0)
    {
        int CharsUsed = 0;
        if (!SetJumpBoost(cmdLine + CharOffset + strlen(kCmdLineParam_jumpboost), &CharsUsed))
            return commandLineHelp ();
        else
            CharOffset += strlen(kCmdLineParam_jumpboost) + 1 + CharsUsed;
        AssignResetVal(JumpBoostFactor);
    }
    if (strncasecmp (cmdLine + CharOffset, kCmdLineParam_SafeLandingAttempts, strlen(kCmdLineParam_SafeLandingAttempts)) == 0)
    {
        int CharsUsed = 0;
        if (!SetMaxIterations(cmdLine + CharOffset + strlen(kCmdLineParam_SafeLandingAttempts), &CharsUsed))
            return commandLineHelp ();
        else
            CharOffset += strlen(kCmdLineParam_SafeLandingAttempts) + 1 + CharsUsed;
        AssignResetVal(MaxSafeZoneTests);
    }
    if (strncasecmp (cmdLine + CharOffset, kCmdLineParam_DebugAccess, strlen(kCmdLineParam_DebugAccess)) == 0)
    {
        strncpy (DbgAccessPassword, cmdLine + CharOffset + strlen(kCmdLineParam_DebugAccess), kWorkStrLen - 1);
        CharOffset += strlen(kCmdLineParam_DebugAccess) + 1 + strlen(DbgAccessPassword);
    }
    if ('\0' != cmdLine[CharOffset - 1])
    {
        // We did not process all the parameters
        bz_debugMessage (0, "+++ PassTheFlag plugin command-line error: Some parameters were not processed.");
        bz_debugMessage (0, &cmdLine[CharOffset]);
        return commandLineHelp ();
    }
    return false;
}


//================================= Required entry points called by the server  ======================================================


// Tells the server which version of the bzfsAPI.h this plugin was generated with
BZ_PLUGIN(FPassHandler)


// Called by the server when the plugin is loaded
void FPassHandler::Init (const char* cmdLine)
{
    // Check what options were set and complain if there was a problem
    if (parseCommandLine (cmdLine))
        return;
    
    /* initialize random seed: */
    srand ( time(NULL) );
    
    // Set up how we want to commmunicate with the server
    bz_registerCustomSlashCommand (kCmdLine_Cmd, &FlagPassHandler);
    
    Register(bz_eCaptureEvent);
    Register(bz_ePlayerDieEvent);
    Register(bz_ePlayerSpawnEvent);
    Register(bz_eFlagDroppedEvent);
    Register(bz_ePlayerJoinEvent);
    Register(bz_ePlayerPartEvent);
    Register(bz_ePlayerUpdateEvent);
    
    Register(bz_eFlagGrabbedEvent);
    Register(bz_eTickEvent);
    
    MaxWaitTime = MaxWaitForReason;
    
    // Show everything loaded without a problem
    bz_debugMessagef(DbgLevelAlways, "PassTheFlag initialized - v%s ApiVersion=v%d%s", PASSTHEFLAG_VER, BZ_API_VERSION, AllowMaxWaitMod ? " maxwait modification enabled" : " maxwait modification disabled");
}

// Called by the server when the plugin is unloaded
void FPassHandler::Cleanup (void)
{
    // Stop communications with the server
    bz_removeCustomSlashCommand (kCmdLine_Cmd);
    
    Flush(); // Clean up all the events
}


