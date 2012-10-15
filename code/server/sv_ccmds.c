/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "stdio.h"
#include "stdlib.h"
#include "server.h"
#include "time.h"
#include "ctype.h"
#include "math.h"
#include "stdbool.h"
#include "../sqlite3/sqlite3.h"

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/

/*
Reusable version of SV_GetPlayerByHandle() that doesn't
print any silly messages.
*/
client_t *SV_BetterGetPlayerByHandle(const char *handle)
{
client_t *cl;
int i;
char cleanName[64];

// make sure server is running
if ( !com_sv_running->integer ) {
return NULL;
}

// Check whether this is a numeric player handle
for(i = 0; handle[i] >= '0' && handle[i] <= '9'; i++);

if(!handle[i])
{
int plid = atoi(handle);

// Check for numeric playerid match
if(plid >= 0 && plid < sv_maxclients->integer)
{
cl = &svs.clients[plid];

if(cl->state)
return cl;
}
}

// check for a name match
for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
if ( !cl->state ) {
continue;
}
if ( !Q_stricmp( cl->name, handle ) ) {
return cl;
}

Q_strncpyz( cleanName, cl->name, sizeof(cleanName) );
Q_CleanStr( cleanName );
if ( !Q_stricmp( cleanName, handle ) ) {
return cl;
}
}

return NULL;
}

/*
==================
SV_GetPlayerByHandle

Returns the player with player id or name from Cmd_Argv(1)
==================
*/
static client_t *SV_GetPlayerByHandle( void ) {
	client_t	*cl;
	int			i;
	char		*s;
	char		cleanName[64];

	char		*comp = NULL;
	char		*name;
	int		n = 0;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		return NULL;
	}

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "No player specified.\n" );
		return NULL;
	}

	s = Cmd_Argv(1);

	// Check whether this is a numeric player handle
	for(i = 0; s[i] >= '0' && s[i] <= '9'; i++);
	
	if(!s[i])
	{
		int plid = atoi(s);

		// Check for numeric playerid match
		if(plid >= 0 && plid < sv_maxclients->integer)
		{
			cl = &svs.clients[plid];
			
			if(cl->state)
				return cl;
		}
	}

	// check for a name part 
	for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {

		if ( !cl->state ) {
			continue;
		}
		
		Q_strncpyz( cleanName, cl->name, sizeof(cleanName) );
		Q_CleanStr( cleanName );

		comp = strstr (cleanName, s);
		if (comp != NULL) {
			n++;
			name = cl->name;
		}
	}

	if ( n == 1 )
	{
		s = name; 
	}

	// check for a name match
	for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
		if ( !cl->state ) {
			continue;
		}
		if ( !Q_stricmp( cl->name, s ) ) {
			return cl;
		}

		Q_strncpyz( cleanName, cl->name, sizeof(cleanName) );
		Q_CleanStr( cleanName );
		if ( !Q_stricmp( cleanName, s ) ) {
			return cl;
		}
	}

	Com_Printf( "Player %s is not on the server\n", s );

	return NULL;
}

/*
==================
SV_GetPlayerByNum

Returns the player with idnum from Cmd_Argv(1)
==================
*/
static client_t *SV_GetPlayerByNum( void ) {
	client_t	*cl;
	int			i;
	int			idnum;
	char		*s;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		return NULL;
	}

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "No player specified.\n" );
		return NULL;
	}

	s = Cmd_Argv(1);

	for (i = 0; s[i]; i++) {
		if (s[i] < '0' || s[i] > '9') {
			Com_Printf( "Bad slot number: %s\n", s);
			return NULL;
		}
	}
	idnum = atoi( s );
	if ( idnum < 0 || idnum >= sv_maxclients->integer ) {
		Com_Printf( "Bad client slot: %i\n", idnum );
		return NULL;
	}

	cl = &svs.clients[idnum];
	if ( !cl->state ) {
		Com_Printf( "Client %i is not active\n", idnum );
		return NULL;
	}
	return cl;
}

//=========================================================


/*
==================
SV_Map_f

Restart the server on a different map
==================
*/
static void SV_Map_f( void ) {
	char		*cmd;
	char		*map;
	qboolean	killBots, cheat;
	char		expanded[MAX_QPATH];
	char		mapname[MAX_QPATH];

	map = Cmd_Argv(1);
	if ( !map ) {
		return;
	}

	// make sure the level exists before trying to change, so that
	// a typo at the server console won't end the game
	Com_sprintf (expanded, sizeof(expanded), "maps/%s.bsp", map);
	if ( FS_ReadFile (expanded, NULL) == -1 ) {
		Com_Printf ("Can't find map %s\n", expanded);
		return;
	}

	// force latched values to get set
	Cvar_Get ("g_gametype", "0", CVAR_SERVERINFO | CVAR_USERINFO | CVAR_LATCH );

	cmd = Cmd_Argv(0);
	if( Q_stricmpn( cmd, "sp", 2 ) == 0 ) {
		Cvar_SetValue( "g_gametype", GT_SINGLE_PLAYER );
		Cvar_SetValue( "g_doWarmup", 0 );
		// may not set sv_maxclients directly, always set latched
		Cvar_SetLatched( "sv_maxclients", "8" );
		cmd += 2;
		cheat = qfalse;
		killBots = qtrue;
	}
	else {
		if ( !Q_stricmp( cmd, "devmap" ) || !Q_stricmp( cmd, "spdevmap" ) ) {
			cheat = qtrue;
			killBots = qtrue;
		} else {
			cheat = qfalse;
			killBots = qfalse;
		}
		if( sv_gametype->integer == GT_SINGLE_PLAYER ) {
			Cvar_SetValue( "g_gametype", GT_FFA );
		}
	}

	// save the map name here cause on a map restart we reload the q3config.cfg
	// and thus nuke the arguments of the map command
	Q_strncpyz(mapname, map, sizeof(mapname));

	// start up the map
	SV_SpawnServer( mapname, killBots );

	// set the cheat value
	// if the level was started with "map <levelname>", then
	// cheats will not be allowed.  If started with "devmap <levelname>"
	// then cheats will be allowed
	if ( cheat ) {
		Cvar_Set( "sv_cheats", "1" );
	} else {
		Cvar_Set( "sv_cheats", "0" );
	}
}

/*
================
SV_MapRestart_f

Completely restarts a level, but doesn't send a new gamestate to the clients.
This allows fair starts with variable load times.
================
*/
static void SV_MapRestart_f( void ) {
	int			i;
	client_t	*client;
	char		*denied;
	qboolean	isBot;
	int			delay;

	// make sure we aren't restarting twice in the same frame
	if ( com_frameTime == sv.serverId ) {
		return;
	}

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( sv.restartTime ) {
		return;
	}

	if (Cmd_Argc() > 1 ) {
		delay = atoi( Cmd_Argv(1) );
	}
	else {
		delay = 5;
	}
	if( delay && !Cvar_VariableValue("g_doWarmup") ) {
		sv.restartTime = sv.time + delay * 1000;
		SV_SetConfigstring( CS_WARMUP, va("%i", sv.restartTime) );
		return;
	}

	// check for changes in variables that can't just be restarted
	// check for maxclients change
	if ( sv_maxclients->modified || sv_gametype->modified ) {
		char	mapname[MAX_QPATH];

		Com_Printf( "variable change -- restarting.\n" );
		// restart the map the slow way
		Q_strncpyz( mapname, Cvar_VariableString( "mapname" ), sizeof( mapname ) );

		SV_SpawnServer( mapname, qfalse );
		return;
	}

	// toggle the server bit so clients can detect that a
	// map_restart has happened
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	// generate a new serverid	
	// TTimo - don't update restartedserverId there, otherwise we won't deal correctly with multiple map_restart
	sv.serverId = com_frameTime;
	Cvar_Set( "sv_serverid", va("%i", sv.serverId ) );

	// reset all the vm data in place without changing memory allocation
	// note that we do NOT set sv.state = SS_LOADING, so configstrings that
	// had been changed from their default values will generate broadcast updates
	sv.state = SS_LOADING;
	sv.restarting = qtrue;

	SV_RestartGameProgs();

	// run a few frames to allow everything to settle
	for (i = 0; i < 3; i++)
	{
		VM_Call (gvm, GAME_RUN_FRAME, sv.time);
		sv.time += 100;
		svs.time += 100;
	}

	sv.state = SS_GAME;
	sv.restarting = qfalse;

	// connect and begin all the clients
	for (i=0 ; i<sv_maxclients->integer ; i++) {
		client = &svs.clients[i];

		// send the new gamestate to all connected clients
		if ( client->state < CS_CONNECTED) {
			continue;
		}

		if ( client->netchan.remoteAddress.type == NA_BOT ) {
			isBot = qtrue;
		} else {
			isBot = qfalse;
		}

		// add the map_restart command
		SV_AddServerCommand( client, "map_restart\n" );

		// connect the client again, without the firstTime flag
		denied = VM_ExplicitArgPtr( gvm, VM_Call( gvm, GAME_CLIENT_CONNECT, i, qfalse, isBot ) );
		if ( denied ) {
			// this generally shouldn't happen, because the client
			// was connected before the level change
			SV_DropClient( client, denied );
			Com_Printf( "SV_MapRestart_f(%d): dropped client %i - denied!\n", delay, i );
			continue;
		}

		client->state = CS_ACTIVE;

		SV_ClientEnterWorld( client, &client->lastUsercmd );
	}	

	// run another frame to allow things to look at all the players
	VM_Call (gvm, GAME_RUN_FRAME, sv.time);
	sv.time += 100;
	svs.time += 100;
}

//===============================================================

/*
==================
SV_Kick_f

Kick a user off of the server  FIXME: move to game
==================
*/
static void SV_Kick_f( void ) {
	client_t	*cl;
	int			i;
	char    *reason = "was kicked";

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( (Cmd_Argc() < 2) || (Cmd_Argc() > 3) ) {
		Com_Printf ("Usage: kick <player> <reason>\nkick all = kick everyone\nkick allbots = kick all bots\n");
		return;
	}

	if ( Cmd_Argc() == 3 ) 
		reason = Cmd_Argv(2);

	cl = SV_GetPlayerByHandle();
	if ( !cl ) {
		if ( !Q_stricmp(Cmd_Argv(1), "all") ) {
			for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
				if ( !cl->state ) {
					continue;
				}
				if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
					continue;
				}
				SV_DropClient( cl, reason );
				cl->lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		}
		else if ( !Q_stricmp(Cmd_Argv(1), "allbots") ) {
			for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
				if ( !cl->state ) {
					continue;
				}
				if( cl->netchan.remoteAddress.type != NA_BOT ) {
					continue;
				}
				SV_DropClient( cl, reason );
				cl->lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		}
		return;
	}
	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
		return;
	}

	SV_DropClient( cl, reason );
	cl->lastPacketTime = svs.time;	// in case there is a funny zombie
}

/*
==================
SV_Ban_f

Ban a user from being able to play on this server through the auth
server
==================
*/
static void SV_Ban_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: banUser <player name>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();

	if (!cl) {
		return;
	}

	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
		return;
	}

	// look up the authorize server's IP
	if ( !svs.authorizeAddress.ip[0] && svs.authorizeAddress.type != NA_BAD ) {
		Com_Printf( "Resolving %s\n", AUTHORIZE_SERVER_NAME );
		if ( !NET_StringToAdr( AUTHORIZE_SERVER_NAME, &svs.authorizeAddress ) ) {
			Com_Printf( "Couldn't resolve address\n" );
			return;
		}
		svs.authorizeAddress.port = BigShort( PORT_AUTHORIZE );
		Com_Printf( "%s resolved to %i.%i.%i.%i:%i\n", AUTHORIZE_SERVER_NAME,
			svs.authorizeAddress.ip[0], svs.authorizeAddress.ip[1],
			svs.authorizeAddress.ip[2], svs.authorizeAddress.ip[3],
			BigShort( svs.authorizeAddress.port ) );
	}

	// otherwise send their ip to the authorize server
	if ( svs.authorizeAddress.type != NA_BAD ) {
		NET_OutOfBandPrint( NS_SERVER, svs.authorizeAddress,
			"banUser %i.%i.%i.%i", cl->netchan.remoteAddress.ip[0], cl->netchan.remoteAddress.ip[1], 
								   cl->netchan.remoteAddress.ip[2], cl->netchan.remoteAddress.ip[3] );
		Com_Printf("%s was banned from coming back\n", cl->name);
	}
}

/*
==================
SV_BanNum_f

Ban a user from being able to play on this server through the auth
server
==================
*/
static void SV_BanNum_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: banClient <client number>\n");
		return;
	}

	cl = SV_GetPlayerByNum();
	if ( !cl ) {
		return;
	}
	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
		return;
	}

	// look up the authorize server's IP
	if ( !svs.authorizeAddress.ip[0] && svs.authorizeAddress.type != NA_BAD ) {
		Com_Printf( "Resolving %s\n", AUTHORIZE_SERVER_NAME );
		if ( !NET_StringToAdr( AUTHORIZE_SERVER_NAME, &svs.authorizeAddress ) ) {
			Com_Printf( "Couldn't resolve address\n" );
			return;
		}
		svs.authorizeAddress.port = BigShort( PORT_AUTHORIZE );
		Com_Printf( "%s resolved to %i.%i.%i.%i:%i\n", AUTHORIZE_SERVER_NAME,
			svs.authorizeAddress.ip[0], svs.authorizeAddress.ip[1],
			svs.authorizeAddress.ip[2], svs.authorizeAddress.ip[3],
			BigShort( svs.authorizeAddress.port ) );
	}

	// otherwise send their ip to the authorize server
	if ( svs.authorizeAddress.type != NA_BAD ) {
		NET_OutOfBandPrint( NS_SERVER, svs.authorizeAddress,
			"banUser %i.%i.%i.%i", cl->netchan.remoteAddress.ip[0], cl->netchan.remoteAddress.ip[1], 
								   cl->netchan.remoteAddress.ip[2], cl->netchan.remoteAddress.ip[3] );
		Com_Printf("%s was banned from coming back\n", cl->name);
	}
}

/*
==================
SV_KickNum_f

Kick a user off of the server  FIXME: move to game
==================
*/
static void SV_KickNum_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: kicknum <client number>\n");
		return;
	}

	cl = SV_GetPlayerByNum();
	if ( !cl ) {
		return;
	}
	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
		return;
	}

	SV_DropClient( cl, "was kicked" );
	cl->lastPacketTime = svs.time;	// in case there is a funny zombie
}

/*
================
SV_Status_f
================
*/
static void SV_Status_f( void ) {
	int			i, j, l;
	client_t	*cl;
	playerState_t	*ps;
	const char		*s;
	int			ping;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	Com_Printf ("map: %s\n", sv_mapname->string );

	Com_Printf ("num score ping name            lastmsg address               qport rate\n");
	Com_Printf ("--- ----- ---- --------------- ------- --------------------- ----- -----\n");
	for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++)
	{
		if (!cl->state)
			continue;
		Com_Printf ("%3i ", i);
		ps = SV_GameClientNum( i );
		Com_Printf ("%5i ", ps->persistant[PERS_SCORE]);

		if (cl->state == CS_CONNECTED)
			Com_Printf ("CNCT ");
		else if (cl->state == CS_ZOMBIE)
			Com_Printf ("ZMBI ");
		else
		{
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Com_Printf ("%4i ", ping);
		}

		Com_Printf ("%s", cl->name);
    // TTimo adding a ^7 to reset the color
    // NOTE: colored names in status breaks the padding (WONTFIX)
    Com_Printf ("^7");
		l = 16 - strlen(cl->name);
		for (j=0 ; j<l ; j++)
			Com_Printf (" ");

		Com_Printf ("%7i ", svs.time - cl->lastPacketTime );

		s = NET_AdrToString( cl->netchan.remoteAddress );
		Com_Printf ("%s", s);
		l = 22 - strlen(s);
		for (j=0 ; j<l ; j++)
			Com_Printf (" ");
		
		Com_Printf ("%5i", cl->netchan.qport);

		Com_Printf (" %5i", cl->rate);

		Com_Printf ("\n");
	}
	Com_Printf ("\n");
}

/*
==================
SV_ConSay_f
==================
*/
static void SV_ConSay_f(void) {
	char	*p;
	char	text[1024];

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc () < 2 ) {
		return;
	}

	strcpy (text, sv_sayprefix->string);
	p = Cmd_Args();

	if ( *p == '"' ) {
		p++;
		p[strlen(p)-1] = 0;
	}

	strcat(text, p);

	SV_SendServerCommand(NULL, "chat \"%s\n\"", text);
}


/*
==================
SV_ConTell_f
==================
*/
static void SV_ConTell_f(void) {
	char	*p;
	char	text[1024];
	client_t        *cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() < 3 ) {
		Com_Printf ("Usage: tell <client number> <text>\n");
		return;
	}
	
	cl = SV_GetPlayerByNum();
	if ( !cl ) {
		return;
	}

	strcpy (text, sv_tellprefix->string);
	p = Cmd_ArgsFrom(2);

	if ( *p == '"' ) {
		p++;
		p[strlen(p)-1] = 0;
	}

	strcat(text, p);

	SV_SendServerCommand(cl, "chat \"%s\n\"", text);
}
/*
==================
SV_Heartbeat_f

Also called by SV_DropClient, SV_DirectConnect, and SV_SpawnServer
==================
*/
void SV_Heartbeat_f( void ) {
	svs.nextHeartbeatTime = -9999999;
}


/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void SV_Serverinfo_f( void ) {
	Com_Printf ("Server info settings:\n");
	Info_Print ( Cvar_InfoString( CVAR_SERVERINFO ) );
}


/*
===========
SV_Systeminfo_f

Examine or change the serverinfo string
===========
*/
static void SV_Systeminfo_f( void ) {
	Com_Printf ("System info settings:\n");
	Info_Print ( Cvar_InfoString( CVAR_SYSTEMINFO ) );
}


/*
===========
SV_DumpUser_f

Examine all a users info strings FIXME: move to game
===========
*/
static void SV_DumpUser_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: info <userid>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if ( !cl ) {
		return;
	}

	Com_Printf( "userinfo\n" );
	Com_Printf( "--------\n" );
	Info_Print( cl->userinfo );
}


/*
=================
SV_KillServer
=================
*/
static void SV_KillServer_f( void ) {
	SV_Shutdown( "killserver" );
}

//===========================================================
/*
Start a server-side demo.

This does it all, create the file and adjust the demo-related
stuff in client_t.

This is mostly ripped from sv_client.c/SV_SendClientGameState
and cl_main.c/CL_Record_f.
*/
static void SVD_StartDemoFile(client_t *client, const char *path)
{
	int		i, len;
	entityState_t	*base, nullstate;
	msg_t		msg;
	byte		buffer[MAX_MSGLEN];
	fileHandle_t	file;
#ifdef USE_DEMO_FORMAT_42
	char		*s;
	int			v, size;
#endif

	Com_DPrintf("SVD_StartDemoFile\n");
	assert(!client->demo_recording);

	// create the demo file and write the necessary header
	file = FS_FOpenFileWrite(path);
	assert(file != 0);
	
	/* File_write_header_demo // ADD this fx */
	/* HOLBLIN  entete demo */ 
	#ifdef USE_DEMO_FORMAT_42
		// s = CG_ConfigString( CS_GAME_VERSION ); // is egal to next line
		// s = Cvar_VariableString("g_modversion");	
		s = malloc( 2 );
		s[0] = 'H'; // urg
		s[1] = '\0'; // urg
	
		size = strlen( s );
		len = LittleLong( size );
		FS_Write( &len, 4, file );
		FS_Write( s , size ,  file );
		
		v = LittleLong( PROTOCOL_VERSION );
		FS_Write ( &v, 4 , file );
		
		len = 0;
		len = LittleLong( len );
		FS_Write ( &len, 4 , file );
		FS_Write ( &len, 4 , file );
	#endif
	/* END HOLBLIN  entete demo */ 

	MSG_Init(&msg, buffer, sizeof(buffer));
	MSG_Bitstream(&msg); // XXX server code doesn't do this, client code does

	MSG_WriteLong(&msg, client->lastClientCommand); // TODO: or is it client->reliableSequence?

	MSG_WriteByte(&msg, svc_gamestate);
	MSG_WriteLong(&msg, client->reliableSequence);

	for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
		if (sv.configstrings[i][0]) {
			MSG_WriteByte(&msg, svc_configstring);
			MSG_WriteShort(&msg, i);
			MSG_WriteBigString(&msg, sv.configstrings[i]);
		}
	}

	Com_Memset(&nullstate, 0, sizeof(nullstate));
	for (i = 0 ; i < MAX_GENTITIES; i++) {
		base = &sv.svEntities[i].baseline;
		if (!base->number) {
			continue;
		}
		MSG_WriteByte(&msg, svc_baseline);
		MSG_WriteDeltaEntity(&msg, &nullstate, base, qtrue);
	}

	MSG_WriteByte(&msg, svc_EOF);

	MSG_WriteLong(&msg, client - svs.clients);
	MSG_WriteLong(&msg, sv.checksumFeed);

	MSG_WriteByte(&msg, svc_EOF); // XXX server code doesn't do this, SV_Netchan_Transmit adds it!

	len = LittleLong(client->netchan.outgoingSequence-1);
	FS_Write(&len, 4, file);

	len = LittleLong (msg.cursize);
	FS_Write(&len, 4, file);
	FS_Write(msg.data, msg.cursize, file);

	#ifdef USE_DEMO_FORMAT_42
		// add size of packet in the end for backward play /* holblin */
		FS_Write(&len, 4, file);
	#endif

	FS_Flush(file);

	// adjust client_t to reflect demo started
	client->demo_recording = qtrue;
	client->demo_file = file;
	client->demo_waiting = qtrue;
	client->demo_backoff = 1;
	client->demo_deltas = 0;
}

/*
Write a message to a server-side demo file.
*/
void SVD_WriteDemoFile(const client_t *client, const msg_t *msg)
{
	int len;
	msg_t cmsg;
	byte cbuf[MAX_MSGLEN];
	fileHandle_t file = client->demo_file;

	if (*(int *)msg->data == -1) { // TODO: do we need this?
		Com_DPrintf("Ignored connectionless packet, not written to demo!\n");
		return;
	}

	// TODO: we only copy because we want to add svc_EOF; can we add it and then
	// "back off" from it, thus avoiding the copy?
	MSG_Copy(&cmsg, cbuf, sizeof(cbuf), (msg_t*) msg);
	MSG_WriteByte(&cmsg, svc_EOF); // XXX server code doesn't do this, SV_Netchan_Transmit adds it!

	// TODO: the headerbytes stuff done in the client seems unnecessary
	// here because we get the packet *before* the netchan has it's way
	// with it; just not sure that's really true :-/

	len = LittleLong(client->netchan.outgoingSequence);
	FS_Write(&len, 4, file);

	len = LittleLong(cmsg.cursize);
	FS_Write(&len, 4, file);

	FS_Write(cmsg.data, cmsg.cursize, file); // XXX don't use len!
	
	#ifdef USE_DEMO_FORMAT_42
		// add size of packet in the end for backward play /* holblin */
		FS_Write(&len, 4, file);
	#endif
	
	FS_Flush(file);
}

/*
Stop a server-side demo.

This finishes out the file and clears the demo-related stuff
in client_t again.
*/
static void SVD_StopDemoFile(client_t *client)
{
	int marker = -1;
	fileHandle_t file = client->demo_file;

	Com_DPrintf("SVD_StopDemoFile\n");
	assert(client->demo_recording);

	// write the necessary trailer and close the demo file
	FS_Write(&marker, 4, file);
	FS_Write(&marker, 4, file);
	FS_Flush(file);
	FS_FCloseFile(file);

	// adjust client_t to reflect demo stopped
	client->demo_recording = qfalse;
	client->demo_file = -1;
	client->demo_waiting = qfalse;
	client->demo_backoff = 1;
	client->demo_deltas = 0;
}

/*
Clean up player name to be suitable as path name.
Similar to Q_CleanStr() but tweaked.
*/
static void SVD_CleanPlayerName(char *name)
{
	char *src = name, *dst = name, c;
	while ((c = *src)) {
		// note Q_IsColorString(src++) won't work since it's a macro
		if (Q_IsColorString(src)) {
			src++;
		}
		else if (c == ':' || c == '\\' || c == '/' || c == '*' || c == '?') {
			*dst++ = '%';
		}
		else if (c > ' ' && c < 0x7f) {
			*dst++ = c;
		}
		src++;
	}
	*dst = '\0';

	if (strlen(name) == 0) {
		strcpy(name, "UnnamedPlayer");
	}
}

/*
Generate unique name for a new server demo file.
(We pretend there are no race conditions.)
*/
static void SV_NameServerDemo(char *filename, int length, const client_t *client, char *fn)
{
	qtime_t time;
	char playername[32];
	char demoName[64]; //@Barbatos

	Com_DPrintf("SV_NameServerDemo\n");

	Com_RealTime(&time);
	Q_strncpyz(playername, client->name, sizeof(playername));
	SVD_CleanPlayerName(playername);
	
	if (fn != NULL) {
		Q_strncpyz(demoName, fn, sizeof(demoName));
		
		#ifdef USE_DEMO_FORMAT_42
			Q_snprintf(filename, length-1, "%s/%s.urtdemo", sv_demofolder->string, demoName );
			if (FS_FileExists(filename)) {
				Q_snprintf(filename, length-1, "%s/%s_%d.urtdemo", sv_demofolder->string, demoName, Sys_Milliseconds() );
			}
		#else
			Q_snprintf(filename, length-1, "%s/%s.dm_%d", sv_demofolder->string, demoName , PROTOCOL_VERSION );
			if (FS_FileExists(filename)) {
				Q_snprintf(filename, length-1, "%s/%s_%d.dm_%d", sv_demofolder->string, demoName, Sys_Milliseconds() , PROTOCOL_VERSION );
			}		
		#endif
	} else {
		#ifdef USE_DEMO_FORMAT_42
			Q_snprintf(
				filename, length-1, "%s/%.4d-%.2d-%.2d_%.2d-%.2d-%.2d_%s_%d.urtdemo",
				sv_demofolder->string, time.tm_year+1900, time.tm_mon + 1, time.tm_mday,
				time.tm_hour, time.tm_min, time.tm_sec,
				playername,
				Sys_Milliseconds()
			);
		#else
			Q_snprintf(
				filename, length-1, "%s/%.4d-%.2d-%.2d_%.2d-%.2d-%.2d_%s_%d.dm_%d",
				sv_demofolder->string, time.tm_year+1900, time.tm_mon + 1, time.tm_mday,
				time.tm_hour, time.tm_min, time.tm_sec,
				playername,
				Sys_Milliseconds(),
				PROTOCOL_VERSION
			);
		#endif
		filename[length-1] = '\0';
		
		if (FS_FileExists(filename)) {
			filename[0] = 0;
			return;
		}
	}
}

static void SV_StartRecordOne(client_t *client, char *filename)
{
	char path[MAX_OSPATH];

	Com_DPrintf("SV_StartRecordOne\n");

	if (client->demo_recording) {
		Com_Printf("startserverdemo: %s is already being recorded\n", client->name);
		return;
	}
	if (client->state != CS_ACTIVE) {
		Com_Printf("startserverdemo: %s is not active\n", client->name);
		return;
	}
	if (client->netchan.remoteAddress.type == NA_BOT) {
		Com_Printf("startserverdemo: %s is a bot\n", client->name);
		return;
	}

	SV_NameServerDemo(path, sizeof(path), client, filename);
	SVD_StartDemoFile(client, path);

	if(sv_demonotice->string)
		SV_SendServerCommand(client, "print \"%s\"\n", sv_demonotice->string);

	Com_Printf("startserverdemo: recording %s to %s\n", client->name, path);
}

static void SV_StartRecordAll(void)
{
	int slot;
	client_t *client;

	Com_DPrintf("SV_StartRecordAll\n");

	for (slot=0, client=svs.clients; slot < sv_maxclients->integer; slot++, client++) {
		// filter here to avoid lots of bogus messages from SV_StartRecordOne()
		if (client->netchan.remoteAddress.type == NA_BOT
		    || client->state != CS_ACTIVE
		    || client->demo_recording) {
			continue;
		}
		SV_StartRecordOne(client, NULL);
	}
}

static void SV_StopRecordOne(client_t *client)
{
	Com_DPrintf("SV_StopRecordOne\n");

	if (!client->demo_recording) {
		Com_Printf("stopserverdemo: %s is not being recorded\n", client->name);
		return;
	}
	if (client->state != CS_ACTIVE) { // disconnects are handled elsewhere
		Com_Printf("stopserverdemo: %s is not active\n", client->name);
		return;
	}
	if (client->netchan.remoteAddress.type == NA_BOT) {
		Com_Printf("stopserverdemo: %s is a bot\n", client->name);
		return;
	}

	SVD_StopDemoFile(client);

	Com_Printf("stopserverdemo: stopped recording %s\n", client->name);
}

static void SV_StopRecordAll(void)
{
	int slot;
	client_t *client;

	Com_DPrintf("SV_StopRecordAll\n");

	for (slot=0, client=svs.clients; slot < sv_maxclients->integer; slot++, client++) {
		// filter here to avoid lots of bogus messages from SV_StopRecordOne()
		if (client->netchan.remoteAddress.type == NA_BOT
		    || client->state != CS_ACTIVE // disconnects are handled elsewhere
		    || !client->demo_recording) {
			continue;
		}
		SV_StopRecordOne(client);
	}
}

/*
==================
SV_StartServerDemo_f

Record a server-side demo for given player/slot. The demo
will be called "YYYY-MM-DD_hh-mm-ss_playername_id.urtdemo",
in the "demos" directory under your game directory. Note
that "startserverdemo all" will start demos for all players
currently in the server. Players who join later require a
new "startserverdemo" command. If you are already recording
demos for some players, another "startserverdemo all" will
start new demos only for players not already recording. Note
that bots will never be recorded, not even if "all" is given.
The server-side demos will stop when "stopserverdemo" is issued
or when the server restarts for any reason (such as a new map
loading).
==================
*/
static void SV_StartServerDemo_f(void)
{
	client_t *client;

	Com_DPrintf("SV_StartServerDemo_f\n");

	if (!com_sv_running->integer) {
		Com_Printf("startserverdemo: Server not running\n");
		return;
	}

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: startserverdemo <player-or-all> <optional demo name>\n");
		return;
	}

	client = SV_BetterGetPlayerByHandle(Cmd_Argv(1));
	if (!Q_stricmp(Cmd_Argv(1), "all")) {
		if (client) {
			Com_Printf("startserverdemo: Player 'all' ignored, starting all demos instead\n");
		}
		SV_StartRecordAll();
	}
	else if (client) {
		if (Cmd_Argc() > 2) {
      		SV_StartRecordOne(client, Cmd_ArgsFrom(2));
    	} else {
      		SV_StartRecordOne(client, NULL);
		}
	}
	else {
		Com_Printf("startserverdemo: No player with that handle/in that slot\n");
	}
}

/*
==================
SV_StopServerDemo_f

Stop a server-side demo for given player/slot. Note that
"stopserverdemo all" will stop demos for all players in
the server.
==================
*/
static void SV_StopServerDemo_f(void)
{
	client_t *client;

	Com_DPrintf("SV_StopServerDemo_f\n");

	if (!com_sv_running->integer) {
		Com_Printf("stopserverdemo: Server not running\n");
		return;
	}

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: stopserverdemo <player-or-all>\n");
		return;
	}

	client = SV_BetterGetPlayerByHandle(Cmd_Argv(1));
	if (!Q_stricmp(Cmd_Argv(1), "all")) {
		if (client) {
			Com_Printf("stopserverdemo: Player 'all' ignored, stopping all demos instead\n");
		}
		SV_StopRecordAll();
	}
	else if (client) {
		SV_StopRecordOne(client);
	}
	else {
		Com_Printf("stopserverdemo: No player with that handle/in that slot\n");
	}
}

/*
==========
SV_PrivateBigtext_f
Send a bigtext to private clients
==========
*/

static void SV_PrivateBigtext_f(void)
{
    client_t *cl;
    char *string;
	
    // make sure server is running
    if (!com_sv_running->integer)
    {
        Com_Printf("Server is not running.\n");
        return;
    }
	
    if (Cmd_Argc() < 3 || strlen(Cmd_Argv(2)) == 0)
    {
        Com_Printf("Usage: privatebigtext <player number> <string>\n");
        return;
    }

    

    if (!(cl = SV_GetPlayerByHandle()))

        return;

	string = Cmd_ArgsFromRaw(2);
	Cmd_TokenizeString(string);
    
    Com_Printf("PrivateBigtext %s: \"%s\"\n", cl->name, string);
    SV_SendServerCommand(cl, "cp \"^3[PM]:^7 %s\"", string);

}

/*
=================
SV_Rename_f
=================
*/

static void SV_Rename_f(void)
{   


    client_t *cl;
    char *newname;

    if (!com_sv_running->integer)
    {
        Com_Printf("Server is not running.\n");
        return;
    }
	
    if (Cmd_Argc() < 3 || strlen(Cmd_Argv(2)) == 0)
    {
        Com_Printf("Usage: rename <player number> <new name>\n");
        return;
    }

    if (!(cl = SV_GetPlayerByHandle()))
        return;

    newname = Cmd_Argv(2);

    Info_SetValueForKey(cl->userinfo, "name", newname);
    SV_UserinfoChanged(cl);
    VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);
    
    return;
}

/*
==========
SV_KillPlayer_f
kill client
==========
*/

static void SV_KillPlayer_f(void)
{
    client_t *cl;
    char *cmd;
    int clId;
    playerState_t *ps;
    int	cteam;
	
    // make sure server is running
    if (!com_sv_running->integer)
    {
        Com_Printf("Server is not running.\n");
        return;
    }
	
    if (Cmd_Argc() < 2 )
    {
        Com_Printf("Usage: killplayer <player name>\n");
        return;
    }
	
    if (!(cl = SV_GetPlayerByHandle()))
        return;

    clId = cl - svs.clients;
    ps = SV_GameClientNum( clId );
    cteam = ps->persistant[PERS_TEAM];
           
    if (cteam == 3)
    {
        Com_Printf("%s ^7is in spectator\n", cl->name);
        return;
    }
	
	cmd = "kill";
	Cmd_TokenizeString(cmd);
    
    SV_SendServerCommand(NULL, "cp \"%s ^7killed by Admin\"", cl->name);
    
    VM_Call(gvm, GAME_CLIENT_COMMAND, cl - svs.clients);
}
/*
==========================================================================================
Systeme Administration
==========================================================================================
*/
/*
========================
SV_Nospace
========================
*/
static void SV_Nospace(char *name)
{
	char *src = name, *dst = name, c;
	while ((c = *src)) {
		if (c == '/') {
			*dst++ = '*';
		}
		else if (c > ' ' && c < 0x7f) {
			*dst++ = c;
		}
		src++;
	}
	*dst = '\0';

	if (strlen(name) == 0) {
		strcpy(name, "Newbie");
	}
}

/*
=================
SV_NoSpacesInNicks
=================
*/


void SV_NoSpacesInNicks(client_t *cl, char *name)
{   

    char playername[32];

    Q_strncpyz(playername, name, sizeof(playername));
    SV_Nospace(playername);

    Info_SetValueForKey(cl->userinfo, "name", playername);
    SV_UserinfoChanged(cl);
    VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);
    
    return;

}

/*
==========
SQ_TestDatabase_f

==========
*/
int databasefile ()
{
   char *homePath;
   char *databasepath;
   homePath = Sys_DefaultHomePath();

   databasepath = FS_BuildOSPath( homePath, "q3ut4", "UrTDataBase.db");
   
   return databasepath;
}

int existsfile( char * databasepath )
{

    FILE* fichier = NULL;

    fichier = fopen(databasepath, "r");
   
    if (fichier != NULL){return 1;}
    else {return 0;}
} 

void SQ_TestDatabase_f(void)
{

   int testdatabase;  
   char *database;

   database = databasefile();

   testdatabase = existsfile(database);

   if(testdatabase==0) {
 
   int rc;

   sqlite3 *db;
   rc = sqlite3_open(database,&db);

   char create_table[1064] = "CREATE TABLE IF NOT EXISTS clients (id INTEGER PRIMARY KEY DEFAULT(NULL), name VARCHAR (32) NOT NULL, aka VARCHAR (32), ip VARCHAR (16) NOT NULL, guid VARCHAR (36) NOT NULL UNIQUE, level INT (1) NOT NULL DEFAULT(0), connections INT (11) NOT NULL DEFAULT(0), date INT (11) NOT NULL, ban INT (11) DEFAULT(NULL))";
   
   rc = sqlite3_exec(db,create_table,0,0,0);
 
   sqlite3_close(db);
   }

}

/*
==========
SQ_ClientConnect

==========
*/

int SQ_ClientConnect(char *name, char *guid, char *ip, char *type, int level, char *aka, int ban)
{

    char *database;
    char requete[1024];
    database = databasefile();
    sqlite3 *db;
    int rv;
    sqlite3_stmt* stmt;
    time_t t1;
    char *c;    

    t1 = time(NULL);
   
    database = databasefile();

    rv = sqlite3_open(database,&db);

    if (!Q_stricmp(guid, "")) {return;}

    if (c = strpbrk(ip, ":")!= NULL)
    {
        ip = strtok(ip, ":");
    }

    sprintf(requete, "SELECT * from clients where guid = '%s'", guid);
    rv = sqlite3_prepare_v2(db,requete,-1,&stmt,0);
   
    if (rv == SQLITE_OK){
        int connection;
        
        rv = sqlite3_step(stmt);
       
        if(rv == SQLITE_ROW)
        {
           
           char *tconnections = (char*)sqlite3_column_text(stmt,6);

           connection = atoi(tconnections);
           
           char urequete[1024];
        
           if (!Q_stricmp(type, "Setlevel")){

              if (level == 0){
                 sprintf(urequete, "UPDATE clients SET name = '%s', aka = NULL, ip = '%s', level = '%i' WHERE guid = '%s'",name,ip,level,guid);
              }
              else {
                  if ((!Q_stricmp(name, "Newbie"))||(!Q_stricmp(name, "UnnamedPlayer"))) {return;}
                  sprintf(urequete, "UPDATE clients SET name = '%s', aka = '%s', ip = '%s', level = '%i' WHERE guid = '%s'",name,aka,ip,level,guid);
              }
           }

           else if (!Q_stricmp(type, "Register")){

                  if ((!Q_stricmp(name, "Newbie"))||(!Q_stricmp(name, "UnnamedPlayer"))) {return;}

                  sprintf(urequete, "UPDATE clients SET name = '%s', aka = '%s', ip = '%s', level = '%i' WHERE guid = '%s'",name,aka,ip,level,guid);

           }

           else if (!Q_stricmp(type, "Ban")){

                  sprintf(urequete, "UPDATE clients SET ip = '%s', ban = '%i' WHERE guid = '%s'",ip,ban,guid);

           }

           else if (!Q_stricmp(type, "Unban")){

                  sprintf(urequete, "UPDATE clients SET ban = NULL WHERE guid = '%s'",guid);

           }

           else if (!Q_stricmp(type, "Connect")){
 
                    connection =connection + 1;
                    sprintf(urequete, "UPDATE clients SET name = '%s', ip = '%s', connections = '%i' WHERE guid = '%s'",name,ip,connection,guid);
           }

           else if (!Q_stricmp(type, "UpdateUserinfo")){

                    sprintf(urequete, "UPDATE clients SET name = '%s' WHERE guid = '%s'",name,guid);
           }

           rv = sqlite3_exec(db,urequete,0,0,0);
        }

        else {

           char irequete[1024];
           sprintf(irequete, "INSERT INTO clients (\"name\", \"ip\", \"guid\", \"level\", \"connections\", \"date\", \"ban\") VALUES('%s', '%s', '%s', '0', '1', '%ld', NULL)",name,ip,guid,t1);

           rv = sqlite3_exec(db,irequete,0,0,0);

        }

    } 
  
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

/*
=================
SQ_TestName
=================
*/
int SQ_TestName(client_t *cl)
{

    char *tdatabase;
    char nrequete[1024];
    tdatabase = databasefile();
    sqlite3 *ndb;
    int nrv;
    sqlite3_stmt* nstmt;
    char *taka;
    char caka[64];
    char cname[64];

    tdatabase = databasefile();

    nrv = sqlite3_open(tdatabase,&ndb);

    sprintf(nrequete, "SELECT * from clients");
    
    nrv = sqlite3_prepare_v2(ndb,nrequete,-1,&nstmt,0);

    char *nbguid = Info_ValueForKey(cl->userinfo, "cl_guid");
    char *nbname = cl->name;

    Q_strncpyz( cname, nbname, sizeof(cname) );
    Q_CleanStr( cname );
    
    if (nrv == SQLITE_OK){
         while(1)
        {        

           nrv = sqlite3_step(nstmt);
       
           if(nrv == SQLITE_ROW)
           {
           
               char *tguid = (char*)sqlite3_column_text(nstmt,4);
               
               if ((char*)sqlite3_column_text(nstmt,2) != NULL)
               {
                   taka = strdup((char*)sqlite3_column_text(nstmt,2));
                   Q_strncpyz( caka, taka, sizeof(caka) );
                   Q_CleanStr( caka );

                  if (!strcmp(cname, caka))
                  {
                     if (strcmp(tguid, nbguid) != 0)
                     {
                        SV_SendServerCommand(NULL, "print \"^1Warning: ^3The name '^7%s' ^3belongs to another player\"", cl->name);
                        SV_SendServerCommand(NULL, "print \"^1Warning: ^7%s ^3is renamed ^7Newbie\"", cl->name);

                        Info_SetValueForKey(cl->userinfo, "name", "Newbie");
                        SV_UserinfoChanged(cl);
                        VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);

                     }

                  }
                   
               }   
                 
           }
           
           else if(nrv == SQLITE_DONE)
           {
               break;
           }  

        }
    }
    
    sqlite3_finalize(nstmt);
   
    sqlite3_close(ndb);

}
/*
=================
SQ_TestBan
=================
*/

int SQ_TestBan(client_t *cl, char *guid)
{  

    char *bandatabase;
    char brequete[1024];
    sqlite3 *bandb;
    int banrv;
    sqlite3_stmt* banstmt;
    int cban;
    char cmd[64];
    int clId;

    struct tm * t;
    struct tm * tban;
    time_t timestamp;
    time_t timesban;    
   
    timestamp = time(NULL);
    
    bandatabase = databasefile();

    banrv = sqlite3_open(bandatabase,&bandb);

    sprintf(brequete, "SELECT * from clients where guid = '%s'", guid);
    
    banrv = sqlite3_prepare_v2(bandb,brequete,-1,&banstmt,0);

    if (banrv == SQLITE_OK){
        
        banrv = sqlite3_step(banstmt);

        if(banrv == SQLITE_ROW)
        {

              char *ctban = (char*)sqlite3_column_text(banstmt,8);

              if (ctban != NULL){

                 cban = atoi(ctban);

                 if (cban == 0){ 
                    SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7is banned on this server\"", cl->name);
                    clId = cl - svs.clients;
                    Com_sprintf(cmd, sizeof(cmd), "kick %i\n", clId);
                    Cmd_ExecuteString(cmd);
                 }

                 if (cban > (int) timestamp){
                 
                    timesban= cban;
    
                    tban = localtime(&timesban);

                    SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7is banned on this server\"", cl->name);
                    SV_SendServerCommand(NULL, "chat \"^1Warning: ^7TempBan expire: %02u/%02u/%02u %02u:%02u\"",tban->tm_mon, tban->tm_mday, 1900 + tban->tm_year, tban->tm_hour, tban->tm_min);
                    clId = cl - svs.clients;
                    Com_sprintf(cmd, sizeof(cmd), "kick %i\n", clId);
                    Cmd_ExecuteString(cmd);
                    
             
                 }
              }
         }

    }

    sqlite3_finalize(banstmt);
    sqlite3_close(bandb);

    return;
}
/*
=================
SV_Clientindatabase
=================
*/

void SV_Clientindatabase(client_t *cl, char *type)
{  
        SQ_TestDatabase_f();

        char *guid = Info_ValueForKey(cl->userinfo, "cl_guid");
        char *ip = Info_ValueForKey(cl->userinfo, "ip");

        ip = strtok(ip, ":");

        SQ_ClientConnect(cl->name, guid, ip, type, NULL, NULL, NULL);

        SQ_TestBan(cl, guid);

        if (!Q_stricmp(type, "UpdateUserinfo")){
           SQ_TestName(cl);
        }

        return;    
}
/*
========================
SV_ChangeGear_f
========================
*/
static void SV_ChangeGears_f(char *cgear, char a, char b)
{
	char *src = cgear, *dst = cgear, c;
	while ((c = *src)) {
		if (c == a) {
			*dst++ = b;
		}
                else {
                  *dst++ = c;
                }
		src++;
	}
	*dst = '\0';
}

/*
=================
SV_ChangeGear
=================
*/
void SV_ChangeGear(client_t *cl, char *gear)
{   
    char a;
    char b; 
    char ngear[32];

    Q_strncpyz(ngear, gear, sizeof(ngear));
    
    if (!Q_stricmp(sv_nosmoke->string, "1")) {
        a = 'Q';
        b = 'A';
    
        SV_ChangeGears_f(ngear, a, b);

        Info_SetValueForKey(cl->userinfo, "gear", ngear);
        SV_UserinfoChanged(cl);
        VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);
    }

    if (!Q_stricmp(sv_nogrenade->string, "1")) {
        a = 'O';
        b = 'A';
     
        SV_ChangeGears_f(ngear, a, b);

        Info_SetValueForKey(cl->userinfo, "gear", ngear);
        SV_UserinfoChanged(cl);
        VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);
    }

    if (!Q_stricmp(sv_nog36->string, "1")) {
        a = 'M';
        b = 'L';
     
        SV_ChangeGears_f(ngear, a, b);

        Info_SetValueForKey(cl->userinfo, "gear", ngear);
        SV_UserinfoChanged(cl);
        VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);
    }
  
    if (!Q_stricmp(sv_nopsg->string, "1")) {
        a = 'N';
        b = 'L';
    
        SV_ChangeGears_f(ngear, a, b);

        Info_SetValueForKey(cl->userinfo, "gear", ngear);
        SV_UserinfoChanged(cl);
        VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);
    }
    
    if (!Q_stricmp(sv_nosr8->string, "1")) {
        a = 'Z';
        b = 'L';
     
        SV_ChangeGears_f(ngear, a, b);

        Info_SetValueForKey(cl->userinfo, "gear", ngear);
        SV_UserinfoChanged(cl);
        VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);
    }

    if (!Q_stricmp(sv_nohk69->string, "1")) {
        a = 'K';
        b = 'L';
     
        SV_ChangeGears_f(ngear, a, b);

        Info_SetValueForKey(cl->userinfo, "gear", ngear);
        SV_UserinfoChanged(cl);
        VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);
    }

    if (!Q_stricmp(sv_nonegev->string, "1")) {
        a = 'c';
        b = 'L';
     
        SV_ChangeGears_f(ngear, a, b);

        Info_SetValueForKey(cl->userinfo, "gear", ngear);
        SV_UserinfoChanged(cl);
        VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);
    }
    
    if (!Q_stricmp(sv_notacgoggle->string, "1")) {
        a = 'S';
        b = 'A';
     
        SV_ChangeGears_f(ngear, a, b);

        if (ngear[4] ==  'A'){
          if (ngear[5] != 'A'){
             sprintf( ngear, "%c%c%c%c%c%c%c", ngear[0], ngear[1], ngear[2], ngear[3], ngear[5], 'A', ngear[6]);
          }
          else if (ngear[6] != 'A'){
             sprintf( ngear, "%c%c%c%c%c%c%c", ngear[0], ngear[1], ngear[2], ngear[3], ngear[6], ngear[5], 'A');
          }
          else {
             sprintf( ngear, "%c%c%c%c%c%c%c", ngear[0], ngear[1], ngear[2], ngear[3], 'W', ngear[5], ngear[6]);
          }
          
        }

        Info_SetValueForKey(cl->userinfo, "gear", ngear);
        SV_UserinfoChanged(cl);
       
        VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);
    }

    if (!Q_stricmp(sv_nokevlar->string, "1")) {
        a = 'R';
        b = 'A';

        SV_ChangeGears_f(ngear, a, b);

        if (ngear[4] ==  'A'){
          if (ngear[5] != 'A'){
             sprintf( ngear, "%c%c%c%c%c%c%c", ngear[0], ngear[1], ngear[2], ngear[3], ngear[5], 'A', ngear[6]);
          }
          else if (ngear[6] != 'A'){
             sprintf( ngear, "%c%c%c%c%c%c%c", ngear[0], ngear[1], ngear[2], ngear[3], ngear[6], ngear[5], 'A');
          }
          else {
             sprintf( ngear, "%c%c%c%c%c%c%c", ngear[0], ngear[1], ngear[2], ngear[3], 'W', ngear[5], ngear[6]);
          }
          
        }

        Info_SetValueForKey(cl->userinfo, "gear", ngear);
        SV_UserinfoChanged(cl);

        VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);
    }

    if (!Q_stricmp(sv_nomedkit->string, "1")) {
        a = 'T';
        b = 'A';

        SV_ChangeGears_f(ngear, a, b);

        if (ngear[4] ==  'A'){
          if (ngear[5] != 'A'){
             sprintf( ngear, "%c%c%c%c%c%c%c", ngear[0], ngear[1], ngear[2], ngear[3], ngear[5], 'A', ngear[6]);
          }
          else if (ngear[6] != 'A'){
             sprintf( ngear, "%c%c%c%c%c%c%c", ngear[0], ngear[1], ngear[2], ngear[3], ngear[6], ngear[5], 'A');
          }
          else {
             sprintf( ngear, "%c%c%c%c%c%c%c", ngear[0], ngear[1], ngear[2], ngear[3], 'W', ngear[5], ngear[6]);
          }
          
        }

        Info_SetValueForKey(cl->userinfo, "gear", ngear);
        SV_UserinfoChanged(cl);

        VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);
    }

    if (!Q_stricmp(sv_noxtra->string, "1")) {
        a = 'X';
        b = 'A';

        SV_ChangeGears_f(ngear, a, b);

        if (ngear[4] ==  'A'){
          if (ngear[5] != 'A'){
             sprintf( ngear, "%c%c%c%c%c%c%c", ngear[0], ngear[1], ngear[2], ngear[3], ngear[5], 'A', ngear[6]);
          }
          else if (ngear[6] != 'A'){
             sprintf( ngear, "%c%c%c%c%c%c%c", ngear[0], ngear[1], ngear[2], ngear[3], ngear[6], ngear[5], 'A');
          }
          else {
             sprintf( ngear, "%c%c%c%c%c%c%c", ngear[0], ngear[1], ngear[2], ngear[3], 'W', ngear[5], ngear[6]);
          }
          
        }

        Info_SetValueForKey(cl->userinfo, "gear", ngear);
        SV_UserinfoChanged(cl);

        VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);
    }

    if (!Q_stricmp(sv_nospas->string, "1")) {
        a = 'H';
        b = 'A';

        SV_ChangeGears_f(ngear, a, b);

        if (ngear[1] ==  'A'){

             sprintf( ngear, "%c%c%c%c%c%c%c", ngear[0], 'L', ngear[2], ngear[3], ngear[4], ngear[5], ngear[6]);
        }

        Info_SetValueForKey(cl->userinfo, "gear", ngear);
        SV_UserinfoChanged(cl);

        VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);
    }

    if (!Q_stricmp(sv_nomp5->string, "1")) {
        a = 'I';
        b = 'A';

        SV_ChangeGears_f(ngear, a, b);

        if (ngear[1] ==  'A'){

             sprintf( ngear, "%c%c%c%c%c%c%c", ngear[0], 'L', ngear[2], ngear[3], ngear[4], ngear[5], ngear[6]);
        }

        Info_SetValueForKey(cl->userinfo, "gear", ngear);
        SV_UserinfoChanged(cl);

        VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);
    }

    if (!Q_stricmp(sv_noump45->string, "1")) {
        a = 'J';
        b = 'A';

        SV_ChangeGears_f(ngear, a, b);

        if (ngear[1] ==  'A'){

             sprintf( ngear, "%c%c%c%c%c%c%c", ngear[0], 'L', ngear[2], ngear[3], ngear[4], ngear[5], ngear[6]);
        }

        Info_SetValueForKey(cl->userinfo, "gear", ngear);
        SV_UserinfoChanged(cl);

        VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);
    }
    return;
}
/*
==================
SV_SetLevel_f
==================
*/

void CmdsTest(void)
{

    FILE* fichier = NULL;
    char ligne[128];
    char *test = NULL;
    char *homePath;
    char *q3ut4Path;
    char *commandpath;

    homePath = Sys_DefaultHomePath();

    q3ut4Path = Cvar_VariableString( "fs_game" );

    commandpath = FS_BuildOSPath( homePath, q3ut4Path, "commands.cfg");

    fichier = fopen(commandpath, "r");
   
    if (fichier != NULL)
    {
        while (fgets(ligne, sizeof(ligne), fichier) != NULL)
        {
           Cmd_TokenizeString( ligne );

           if (!Q_stricmp("", Cmd_Argv(0))==0)
           {
              test = "ok"; break;      
           }
           
        }
        fclose(fichier);
     }

     if (test == NULL)
     {        
        fichier = fopen(commandpath, "w");

        if (fichier != NULL)
        {
             fputs("help 0\n", fichier);
             fputs("time 0\n", fichier);
             fputs("register 0\n", fichier);
             fputs("me 1\n", fichier);
             fputs("mapname 1\n", fichier);
             fputs("teams 1\n", fichier);
             fputs("admins 1\n", fichier);
             fputs("warn 3\n", fichier);
             fputs("restart 3\n", fichier);
             fputs("reload 3\n", fichier);
             fputs("cyclemap 3\n", fichier);
             fputs("map 3\n", fichier);
             fputs("setnextmap 3\n", fichier);
             fputs("pause 3\n", fichier);
             fputs("swapteams 3\n", fichier);
             fputs("shuffleteams 3\n", fichier);
             fputs("slap 3\n", fichier);
             fputs("nuke 3\n", fichier);
             fputs("mute 3\n", fichier);
             fputs("kill 3\n", fichier);
             fputs("kick 3\n", fichier);
             fputs("list 3\n", fichier);
             fputs("playerinfo 3\n", fichier);
             fputs("force 3\n", fichier);
             fputs("veto 3\n", fichier);
             fputs("rename 3\n", fichier);
             fputs("bigtext 3\n", fichier);
             fputs("privatebigtext 3\n", fichier);
             fputs("lookup 3\n", fichier);
             fputs("lookupip 3\n", fichier);
             fputs("ban 4\n", fichier);
             fputs("tempban 4\n", fichier);
             fputs("lookupban 4\n", fichier);
             fputs("infoban 4\n", fichier);
             fputs("gametype 4\n", fichier);
             fputs("exec 4\n", fichier);
             fputs("superslap 4\n", fichier);
             fputs("unban 5\n", fichier);
             fputs("moon 5\n", fichier);
             fputs("gravity 5\n", fichier);
             fputs("respawndelay 5\n", fichier);
             fputs("respawngod 5\n", fichier);
             fputs("timelimit 5\n", fichier);
             fputs("fraglimit 5\n", fichier);
             fputs("caplimit 5\n", fichier);
             fputs("matchmode 5\n", fichier);
             fputs("swaproles 5\n", fichier);
             fputs("friendlyfire 5\n", fichier);
             fputs("setlevel 5\n", fichier);
             
             fclose(fichier);
        }
     }
     return;
}

static void SV_SetLevel_f(void)
{   
    
    client_t *cl;
    char    *guid;
    int level;
    char *ip;
    char *type = "Setlevel";
    
    // make sure server is running
    if (!com_sv_running->integer)
    {
        Com_Printf("Server is not running.\n");
        return;
    }
	
    if (Cmd_Argc() < 3 || strlen(Cmd_Argv(2)) == 0)
    {
        Com_Printf("Usage: setlevel <player number> <level>\n");
        return;
    }

    if (!(cl = SV_GetPlayerByHandle()))
    {
        return;
    }

    ip = Info_ValueForKey(cl->userinfo, "ip");
    ip = strtok(ip, ":");

    guid = Info_ValueForKey(cl->userinfo, "cl_guid");
    level = atoi(Cmd_Argv(2));
    

        if (level > 5 || level < 0) {return;}
        if (level == 0){
             if (!Q_stricmp("0", Cmd_Argv(2)) == 0){return;}
                       }

    char *aka = cl->name;

    if ((!Q_stricmp(cl->name, "Newbie"))||(!Q_stricmp(cl->name, "UnnamedPlayer"))) {
        SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^2: ^7You can not change the level of %s^7\"", cl->name);
    }
    else {

        SQ_ClientConnect(cl->name, guid, ip, type, level, aka, NULL);
        Com_Printf("changelevel: %s ^7is put in level(^1%d^7)\n",cl->name, level);
    } 
    CmdsTest();

}

/*
===================================================================================================================
                                                    COMMANDES
===================================================================================================================
*/

int TestLevel(char *command)
{

    FILE* fichier = NULL;
    char ligne[128];
    char *test = NULL;
    int tlevel;
    int tclevel;
    char *homePath;
    char *q3ut4Path;
    char *commandpath;

    homePath = Sys_DefaultHomePath();

    q3ut4Path = Cvar_VariableString( "fs_game" );

    commandpath = FS_BuildOSPath( homePath, q3ut4Path, "commands.cfg");

    fichier = fopen(commandpath, "r+");

    if (fichier != NULL)
    {
        while (fgets(ligne, sizeof(ligne), fichier) != NULL)
        {
           Cmd_TokenizeString( ligne );
           sscanf(Cmd_Argv(1),"%i",&tlevel);

           if (Q_stricmp(command, Cmd_Argv(0))==0)
           {
              tclevel = tlevel; test = "ok"; break;      
           }
           
        }
        fclose(fichier);

    }
   else {return -1;}

   if (test == NULL){return -1;}
   else {return tclevel;}

}
/*
=================
SQ_TestClient
=================
*/
int SQ_TestClient(char *guid, char *type)
{

    char *tdatabase;
    char requete[1024];
    tdatabase = databasefile();
    sqlite3 *tdb;
    int trv;
    sqlite3_stmt* stmtt;
    int elevel;
    int levelt = 0;
    int eid;
    int idt;
    char *tban;
    char *taka;

    tdatabase = databasefile();

    trv = sqlite3_open(tdatabase,&tdb);

    sprintf(requete, "SELECT * from clients where guid = '%s'", guid);
    
    trv = sqlite3_prepare_v2(tdb,requete,-1,&stmtt,0);
    
    if (trv == SQLITE_OK){
        
        trv = sqlite3_step(stmtt);
       
        if(trv == SQLITE_ROW)
        {
           
           //char *tguid = strdup((char*)sqlite3_column_text(stmtt,4));
           
           if (type == "Setlevel")
           {  

              char *tlevel = (char*)sqlite3_column_text(stmtt,5);

              elevel = atoi(tlevel);
                            
              levelt = elevel;             
 
              sqlite3_finalize(stmtt);
              sqlite3_close(tdb);
 
              return levelt;
 
           }

           if (type == "Aka")
           {  
              if ((char*)sqlite3_column_text(stmtt,2) != NULL)
              {
                  taka = strdup((char*)sqlite3_column_text(stmtt,2));
              }   
              else
              {
                  taka = NULL;
              }
              
              sqlite3_finalize(stmtt);
              sqlite3_close(tdb);

              return taka;
 
           }

           if (type == "Connections")
           {  

              char *tconnections = strdup((char*)sqlite3_column_text(stmtt,6));      
 
              sqlite3_finalize(stmtt);
              sqlite3_close(tdb);

              return tconnections;
 
           }

           if (type == "Date")
           {  

              char *tdate = strdup((char*)sqlite3_column_text(stmtt,7));

              sqlite3_finalize(stmtt);
              sqlite3_close(tdb);

              return tdate;
 
           }

           if (type == "ID")
           {  

              char *tid = (char*)sqlite3_column_text(stmtt,0);

              eid = atoi(tid);
                            
              idt = eid; 

              sqlite3_finalize(stmtt);
              sqlite3_close(tdb);

              return idt;
 
           }

           if (type == "Ban")
           {  

              if ((char*)sqlite3_column_text(stmtt,8) != NULL)
              {
                  tban = strdup((char*)sqlite3_column_text(stmtt,8));
                  
              }   
              else
              {
                  tban = NULL;
              }

              sqlite3_finalize(stmtt);
              sqlite3_close(tdb);

              return tban;
 
           }

           if (type == "IP")
           {  

              char *tip = strdup((char*)sqlite3_column_text(stmtt,3));

              sqlite3_finalize(stmtt);
              sqlite3_close(tdb);

              return tip;
 
           }

           if (type == "Name")
           {  

              char *tname = strdup((char*)sqlite3_column_text(stmtt,1));

              sqlite3_finalize(stmtt);
              sqlite3_close(tdb);

              return tname;
 
           }
       
       }
       else {
         sqlite3_finalize(stmtt);
         sqlite3_close(tdb);    
         return NULL;
       }
    } 
    else {
      sqlite3_finalize(stmtt);
      sqlite3_close(tdb);    
      return NULL;

   }

    
    sqlite3_finalize(stmtt);
   
    sqlite3_close(tdb);

}

/*
=================
SQ_TestClientID
=================
*/
int SQ_TestClientID(int id)
{

    char *tdatabase;
    char idrequete[1024];
    tdatabase = databasefile();
    sqlite3 *iddb;
    int idrv;
    sqlite3_stmt* idstmt;


    tdatabase = databasefile();

    idrv = sqlite3_open(tdatabase,&iddb);

    sprintf(idrequete, "SELECT * from clients where id = '%i'", id);
    
    idrv = sqlite3_prepare_v2(iddb,idrequete,-1,&idstmt,0);
    
    if (idrv == SQLITE_OK){
        
        idrv = sqlite3_step(idstmt);
       
        if(idrv == SQLITE_ROW)
        {
           
              char *tguid = strdup((char*)sqlite3_column_text(idstmt,4));

              sqlite3_finalize(idstmt);
              sqlite3_close(iddb);

              return tguid;

       }
    } 
    
    sqlite3_finalize(idstmt);
   
    sqlite3_close(iddb);

}


// !time
void PB_Ctime (client_t *cl) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;

       time_t timestamp;
       struct tm * t;
       timestamp = time(NULL);
       t = localtime(&timestamp);  
       
       clevel = TestLevel("time");

       if (clevel == -1){clevel = 0;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   void PB_Cpause(client_t *cl);
           SV_SendServerCommand(cl, "chat \"^3Time[PM]:^7 %02u:%02u\"", t->tm_hour, t->tm_min);   
       }

       
       return;
}

// !me 
void PB_Cme (client_t *cl) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;

       time_t timestamp;
       struct tm * t;

       clevel = TestLevel("me");

       if (clevel == -1){clevel = 1;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)   
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   

              char      *playeraka;
              char	*playerip;
              char	*playerconnection;
              char	*playerdate;
              int       playerid;

              playerip = Info_ValueForKey(cl->userinfo, "ip");

              playeraka = SQ_TestClient(cguid, "Aka");
              playerip = strtok(playerip, ":");
              playerconnection = SQ_TestClient(cguid, "Connections");
              playerdate = SQ_TestClient(cguid, "Date");
              playerid = SQ_TestClient(cguid, "ID");
       
              timestamp = atoi(playerdate);
              t = localtime(&timestamp);

              if (playeraka == NULL){
                SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^3ID: ^1@%i ^3Level ^7(^5%i^7)\"", playerid, clientlevel);

                }
                else{
                SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^3ID: ^1@%i ^3Level ^7(^5%i^7) ^3AKA:^7 %s^7\"", playerid, clientlevel , playeraka);

                }

              SV_SendServerCommand(cl, "chat \"^3IP: ^7%s ^3Guid: ^7%s\"", playerip, cguid);
              SV_SendServerCommand(cl, "chat \"^3First Visit: ^7%02u/%02u/%02u ^3Connection ^7%s ^3times\"", t->tm_mday, t->tm_mon, 1900 + t->tm_year, playerconnection);
  
       }

       return;
}

// !reload
void PB_Creload (client_t *cl) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       char	cmd[64];
       int	clevel;
       int      clientlevel;
       
       clevel = TestLevel("reload");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   
           Com_sprintf(cmd, sizeof(cmd), "reload\n");
           Cmd_ExecuteString(cmd);    
       }

       
       return;
}
// !restart
void PB_Crestart (client_t *cl) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       char	cmd[64];
       int	clevel;
       int      clientlevel;
       
       clevel = TestLevel("restart");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   
           Com_sprintf(cmd, sizeof(cmd), "restart\n");
           Cmd_ExecuteString(cmd);    
       }

       
       return;
}
// !cyclemap
void PB_Ccyclemap (client_t *cl) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       char	cmd[64];
       int	clevel;
       int      clientlevel;
       
       clevel = TestLevel("cyclemap");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)  
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   
           Com_sprintf(cmd, sizeof(cmd), "cyclemap\n");
           Cmd_ExecuteString(cmd);    
       }

       
       return;
}
// !map
void PB_Cmap (client_t *cl, char *arg) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       char	cmd[64];
       int	clevel;
       int      clientlevel;
       
       clevel = TestLevel("map");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   
           Cmd_TokenizeString( arg );
       
           if (Cmd_Argc() != 2) 
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !map <map>\"");
              return;
           }
           else
           {
              Com_sprintf(cmd, sizeof(cmd), "map %s\n", Cmd_Argv(1));
              Cmd_ExecuteString(cmd);
           }
         }
       
       return;
}

// !help
void PB_Chelp (client_t *cl) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       char     *clientlevel;
       clevel = TestLevel("help");

       if (clevel == -1){clevel = 0;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   

          FILE* fichier = NULL;
          char ligne[128];
          char cmdlevel[64]="";
          char listcmds[1024]="";
          int tclevel;
          char *homePath;
          char *commandpath;

          homePath = Sys_DefaultHomePath();

          commandpath = FS_BuildOSPath( homePath, "q3ut4", "commands.cfg");

          fichier = fopen(commandpath, "r+");

          SV_SendServerCommand(cl, "print \"^3Commands List[PM]: \"");

          if (fichier != NULL)
          {
             while (fgets(ligne, sizeof(ligne), fichier) != NULL)
             {
                 Cmd_TokenizeString( ligne );
                 sscanf(Cmd_Argv(1),"%i",&tclevel);
                 if (!Q_stricmp("", Cmd_Argv(0))==0)
                 {
                     if (clientlevel >= tclevel )
                     {
                        sprintf(cmdlevel,"^7!%s ",Cmd_Argv(0));
                        strcat(listcmds,cmdlevel);     
                     }
                 }
             }

             SV_SendServerCommand(cl, "print \"%s\n\"",listcmds);             
             fclose(fichier);
             
           }

         }
      
       return;
}

// !setnextmap
void PB_Csetnextmap (client_t *cl, char *arg) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       char	cmd[64];
       int	clevel;
       int      clientlevel;
       
       clevel = TestLevel("setnextmap");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   
           Cmd_TokenizeString( arg );
       
           if (Cmd_Argc() != 2) 
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !setnextmap <map>\"");
              return;
           }
           else
           {
              Com_sprintf(cmd, sizeof(cmd), "g_nextmap %s\n", Cmd_Argv(1));
              Cmd_ExecuteString(cmd);
           }
        }
     
       
       return;
}
// !kick
void PB_Ckick (client_t *cl, char *arg) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;
     
       clevel = TestLevel("kick");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   

           int      clId;
           client_t *client;
           char     cmd[64]; 

           Cmd_TokenizeString( arg );
           
           if (Cmd_Argc() != 2) 
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !kick <client>\"");
              return;
           }

           client = SV_GetPlayerByHandle();

                     
           if (client) 
           {
           
               clId = client - svs.clients;
               
               SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7was kicked by Admin\"", client->name);                 
               Com_sprintf(cmd, sizeof(cmd), "kick %i \"you have been kicked by %s\"\n", clId, cl->name);
               Cmd_ExecuteString(cmd);


           }
           else 
           { 
                   SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player %s ^7is not on the server\"", Cmd_Argv(1));
           }

         

       }
       return; 
}
// !slap
void PB_Cslap (client_t *cl, char *arg) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;
       
       clevel = TestLevel("slap");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       { 

           int           clId;
           client_t      *client;
           playerState_t *ps;
           int	         cteam;
           char          cmd[64];

           Cmd_TokenizeString( arg );
  
           if (Cmd_Argc() != 2) 
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !slap <client>\"");
              return;
           }

           client = SV_GetPlayerByHandle();

                     
           if (client) 
           {
                      
              clId = client - svs.clients;
	      ps = SV_GameClientNum( clId );
	      cteam = ps->persistant[PERS_TEAM];
           
              if (cteam == 3)
              {
                  SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7%s ^7is in spectator\"", client->name) ;
                  return;
              }
              else 
              {                 
               Com_sprintf(cmd, sizeof(cmd), "slap %i\n", clId);
               Cmd_ExecuteString(cmd);
               SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7was slapped by Admin\"", client->name);
              }

           }
           else 
           { 
                   SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player %s ^7is not on the server\"", Cmd_Argv(1));
           }
       }
       return; 
}
// !kill
void PB_Ckill (client_t *cl, char *arg) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;

       clevel = TestLevel("kill");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)  
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       { 

           int            clId;
           client_t       *client;
           playerState_t  *ps;
           int	          cteam;
           char	          cmd[64];

           Cmd_TokenizeString( arg );
  
           if (Cmd_Argc() != 2) 
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !kill <client>\"");
              return;
           }

           client = SV_GetPlayerByHandle();

                     
           if (client) 
           {
           
              clId = client - svs.clients;
	      ps = SV_GameClientNum( clId );
	      cteam = ps->persistant[PERS_TEAM];
           
              if (cteam == 3)
              {
                  SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7%s ^7is in spectator\"", client->name) ;
                  return;
              }
              else 
              {                 
               Com_sprintf(cmd, sizeof(cmd), "killplayer %i\n", clId);
               Cmd_ExecuteString(cmd);
               SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7killed by Admin\"", client->name);
              }
           }
           else 
           { 
              SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player %s ^7is not on the server\"", Cmd_Argv(1));
           }
       }
       return; 
}
// !list
void PB_Clist( client_t *cl ) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;

       clevel = TestLevel("list");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   

            char           *guid;
            int		   i;
            int            playerlevel;
            client_t       *clientl;

            for (i=0,clientl=svs.clients ; i < sv_maxclients->integer ; i++,clientl++)
	    {
		if (!clientl->state)
			continue;
            
                playerState_t  *ps;
                char           *lteam = NULL;
                int            nuteam = NULL;
		
                ps = SV_GameClientNum( i );
		nuteam = ps->persistant[PERS_TEAM];

                if (nuteam == 0)
                {
                   lteam = "^3Free";
                }
                if (nuteam == 1)
                {
                   lteam = "^1Red";
                }
                if (nuteam == 2)
                {
                   lteam = "^4Blue";
                }
                if (nuteam == 3)
                {
                   lteam = "^3Spectator";
                }

                guid = Info_ValueForKey(clientl->userinfo, "cl_guid");
                playerlevel = SQ_TestClient(guid, "Setlevel");

                if (playerlevel == NULL ) {playerlevel = 0;}

                SV_SendServerCommand(cl, "chat \"^1PLAYER^3[PM]^1: ^7[^1%i^7]%s^3 Level^7(^2%i^7) ^3Score: ^5%i ^3Team: %s^7\"", i, clientl->name, playerlevel, ps->persistant[PERS_SCORE], lteam);

            }   
       }

       return;      
}

// !playerinfo
void PB_Cplayerinfo (client_t *cl, char *arg) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;

       time_t timestamp;
       struct tm * t;
       
       clevel = TestLevel("playerinfo");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       { 

           client_t *client;
           int      playerlevel;
           char     *playeraka;
           char     *playerip;
           char     *playerguid;
           char     *playerconnection;
           char     *playerdate;
           char     *playername;
           int      playerid;

           Cmd_TokenizeString( arg );
  
           if (Cmd_Argc() != 2) 
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !playerinfo <client or @id>\"");
              return;
           }

           char *c;
           char *p;

           p = Cmd_Argv(1);
           c = *p;
  
           if (c == '@')

           {           
               p++;
               playerid = atoi(p);
               playerguid = SQ_TestClientID(playerid);
               playerip = SQ_TestClient(playerguid, "IP");
               playername = SQ_TestClient(playerguid, "Name");

           }
           else
           {
                client = SV_GetPlayerByHandle();

                if (client) 
                {
                  playerip = Info_ValueForKey(client->userinfo, "ip");
                  playerguid = Info_ValueForKey(client->userinfo, "cl_guid");
                  playerip = strtok(playerip, ":");
                  playername = client->name;
                  playerid = SQ_TestClient(playerguid, "ID");  

                }

                else 
                { 
                  SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player %s ^7is not on the server\"", Cmd_Argv(1));
                  return;
                }
            }
           
           if (playerguid != NULL)  
           {
              playerlevel = SQ_TestClient(playerguid, "Setlevel");

              if ((playerid == NULL)||(playerguid == NULL)) {

                 SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^2: %s is not in the database^7\"", playername);
                 return;
              }

              playeraka = SQ_TestClient(playerguid, "Aka");
              playerconnection = SQ_TestClient(playerguid, "Connections");
              playerdate = SQ_TestClient(playerguid, "Date");

              timestamp = atoi(playerdate);
              t = localtime(&timestamp);

              if (playeraka == NULL){

                    SV_SendServerCommand(cl, "chat \"^2INFO^3[PM]^2: ^3ID: ^5@%i ^7%s ^3Level^7(^1%i^7)\"",playerid, playername, playerlevel);
              
             }
             else{

                    SV_SendServerCommand(cl, "chat \"^2INFO^3[PM]^2: ^3ID: ^5@%i ^7%s ^3Level^7(^1%i^7) ^3aka^7 %s\"",playerid, playername, playerlevel, playeraka);
                    
             }

             SV_SendServerCommand(cl, "chat \"^3IP: ^7%s ^3Guid: ^7%s\"", playerip, playerguid);
             SV_SendServerCommand(cl, "chat \"^3First Visit: ^7%02u/%02u/%02u ^3Connection ^7%s ^3times\"", t->tm_mday, t->tm_mon, 1900 + t->tm_year, playerconnection);

           }
          else 
          { 
              SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player with id ^5@%i ^7does not exist\"", playerid);
          }
       }
       return; 
}
// !register
void PB_Cregister(client_t *cl)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;
       char	*ip;

       ip = Info_ValueForKey(cl->userinfo, "ip");
       ip = strtok(ip, ":");

       clevel = TestLevel("register");

       if (clevel == -1){clevel = 0;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)  
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   
           
              char *aka = cl->name;
               
              char *clientaka = SQ_TestClient(cguid, "Aka");
                           
              if (clientaka == NULL){clientlevel = 1;}

              if ((!Q_stricmp(cl->name, "Newbie"))||(!Q_stricmp(cl->name, "UnnamedPlayer"))) {
                   SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^2: ^7You can not get registered with nickname '%s^7'\"", cl->name);
              }
              else {

                   SQ_ClientConnect(cl->name, cguid, ip, "Register", clientlevel, aka, NULL);

                   SV_SendServerCommand(cl, "chat \"^2Server^3[PM]^2: ^7You are registered with the nickname %s ^7at level (^1%i^7)\"", cl->name, clientlevel);
              }
       }

       return;
}

// !setlevel
void PB_Csetlevel(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;
       char     *type = "Setlevel";

       clevel = TestLevel("setlevel");

       if (clevel == -1){clevel = 5;}

       clientlevel = SQ_TestClient(cguid, type);

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   

           client_t *client;
           char	    *clientip;
           char	    *clientname;
           int      clientid;
           char     *dtest = NULL;       
           int      newlevel;
           char     *guid;

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() != 3)
           {
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !setlevel <client or @id> <level>\"");
              return;
           }

           newlevel = atoi(Cmd_Argv(2));
           
           if (newlevel > 10 || newlevel < 0) {
              SV_SendServerCommand(cl, "chat \"^1ERROR^3[PM]^1:^7 Level 0 or 1 or 2 or 3 or 5\"");
              return;}

           char *c;
           char *p;

           p = Cmd_Argv(1);
           c = *p;
  
           if (c == '@')

           {           
               p++;
               clientid = atoi(p);
               guid = SQ_TestClientID(clientid);
               clientip = SQ_TestClient(guid, "IP");
               clientname = SQ_TestClient(guid, "Name");
               dtest = "id";
              
           }
           else
           {
                client = SV_GetPlayerByHandle();
                dtest = "client";

                if (client) 
                {

                 guid = Info_ValueForKey(client->userinfo, "cl_guid");
                 clientname = client->name;
                 clientip = Info_ValueForKey(client->userinfo, "ip");
                 clientip = strtok(clientip, ":");

                }

                else 
                { 
                  SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player %s ^7is not on the server\"", Cmd_Argv(1));
                  return;
                }
            }
           
           if (guid != NULL) 
           {

              if ((!Q_stricmp(clientname, "Newbie"))||(!Q_stricmp(clientname, "UnnamedPlayer"))) {
                   SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^2: ^7You can not change the level of %s^7\"", clientname);
              }
              else {

                   SQ_ClientConnect(clientname, guid, clientip, type, newlevel, clientname, NULL);
               
                   SV_SendServerCommand(cl, "chat \"^2Server^3[PM]^2: ^7You have put %s ^7at level (^1%i^7)\"", clientname, newlevel);
                   if (dtest == "client"){
                      SV_SendServerCommand(client, "chat \"^2Server^3[PM]^2: ^7%s ^7just put you at level (^1%i^7)\"", cl->name, newlevel);
                   }
              }
           }
           else 
           { 
               SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player with id ^5@%i ^7does not exist\"", clientid);
           }
       }

       return;
}
// !ban
void PB_Cban(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       char	*guid;
       int	clevel;
       int      clientlevel;
       char     *type = "Ban";

       clevel = TestLevel("ban");

       if (clevel == -1){clevel = 4;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)  
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   

           char     *clientip;
           char	    *clientname;
           int      clientid;
           char     *etest = NULL;
           client_t *client;
           char	    cmd[64];
           int      clId;

           Cmd_TokenizeString( arg );
           
           if (Cmd_Argc() != 2)
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !ban <client or @id>\"");
              return;
           }

           char *c;
           char *p;

           p = Cmd_Argv(1);
           c = *p;
  
           if (c == '@')

           {           
               p++;
               clientid = atoi(p);
               guid = SQ_TestClientID(clientid);
               clientip = SQ_TestClient(guid, "IP");
               clientname = SQ_TestClient(guid, "Name");
               etest = "id";
           }
           else
           {
                client = SV_GetPlayerByHandle();
                etest = "client";

                if (client) 
                {

                 guid = Info_ValueForKey(client->userinfo, "cl_guid");
                 clientname = client->name;
                 clientip = Info_ValueForKey(client->userinfo, "ip");
                 clientip = strtok(clientip, ":");

                }

                else 
                { 
                  SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player %s ^7is not on the server\"", Cmd_Argv(1));
                  return;
                }
            }
           
           if (guid != NULL) 
           {

              SQ_ClientConnect("", guid, clientip, type, "", "", 0);
           
              SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7was banned by %s^7\"", clientname, cl->name);

              Com_sprintf(cmd, sizeof(cmd), "addip %s\n", clientip);
              Cmd_ExecuteString(cmd);                  

              if (etest == "client") {
                 clId = client - svs.clients;
                 Com_sprintf(cmd, sizeof(cmd), "kick %i \"You have been banned by %s\"\n", clId, cl->name);
                 Cmd_ExecuteString(cmd);
              }

           }
           else 
           { 
              SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player with id ^5@%i ^7does not exist\"", clientid);
           }
       }

       return;
}

// !tempban
void PB_Ctempban(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;

       clevel = TestLevel("tempban");

       if (clevel == -1){clevel = 4;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)  
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   

           int      duration;
           char     *induration;
           char     *tduration;
           int      ban;
           char	    *clientip;
           char	    *clientname;
           int      clientid;
           client_t *client;
           char	    *test = NULL;
           char     *ctest = NULL;
           char	    cmd[64];
           int      clId;
           char	    *guid;
           char     *type = "Ban";

           Cmd_TokenizeString( arg );
           
           if (Cmd_Argc() != 4)
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !tempban <client or @id> <duration> <m or h or d or w>\"");
              return;
           }

           char *c;
           char *p;

           p = Cmd_Argv(1);
           c = *p;
  
           if (c == '@')

           {           
               p++;
               clientid = atoi(p);
               guid = SQ_TestClientID(clientid);
               clientip = SQ_TestClient(guid, "IP");
               clientname = SQ_TestClient(guid, "Name");
               ctest = "id";
           }
           else
           {
                client = SV_GetPlayerByHandle();
                ctest = "client";

                if (client) 
                {

                 guid = Info_ValueForKey(client->userinfo, "cl_guid");
                 clientname = client->name;
                 clientip = Info_ValueForKey(client->userinfo, "ip");
                 clientip = strtok(clientip, ":");
                }

                else 
                { 
                  SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player %s ^7is not on the server\"", Cmd_Argv(1));
                  return;
                }
            }

           if (guid != NULL) 
           {

              duration = atoi(Cmd_Argv(2));          
              induration = Cmd_Argv(3);

              if (!strcmp("m", induration)) {duration = duration*60; tduration = "minute(s)";}
              else if (!strcmp("h", induration)){duration = duration*3600; tduration = "hour(s)";}
              else if (!strcmp("d", induration)){duration = duration*86400; tduration = "day(s)";}
              else if (!strcmp("w", induration)){duration = duration*604800;tduration = "week(s)";}
              else {
                 SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !tempban <client> <duration> <m or h or d or w>\"");
                 return;
              }

              time_t timesban;
              timesban = time(NULL) + duration;
              ban = (int) timesban;

              SQ_ClientConnect("", guid, clientip, type, "", "", ban);

              SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7banned for %s %s by %s^7\"", clientname, Cmd_Argv(2), tduration, cl->name);
              
              if (ctest == "client"){

                  clId = client - svs.clients;
                 
                  Com_sprintf(cmd, sizeof(cmd), "kick %i \"You have been banned for %s %s\"\n", clId, Cmd_Argv(2), tduration);
                  Cmd_ExecuteString(cmd);
              }
              
           }
           else 
           { 
              SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player with id ^5@%i ^7does not exist\"", clientid);
           }
       }

       return;
}

// !unban
void PB_Cunban(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;

       clevel = TestLevel("unban");

       if (clevel == -1){clevel = 5;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)  
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   

           char   *type = "UnBan";
           char   *guid;
           char	  *clientip;
           char   *clientname;       
           char   cmd[64];
           int    clientid;

           Cmd_TokenizeString( arg );
           
           if (Cmd_Argc() != 2)
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !unban <@id>\"");
              return;
           }

           char *c;
           char *p;

           p = Cmd_Argv(1);
           c = *p;
  
           if (c == '@')

           {           
               p++;
               clientid = atoi(p);
           }
           else
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !unban <@id>\"");
              return;
           }

           guid = SQ_TestClientID(clientid);

           if (guid != NULL) 
           {
                           
              clientip = SQ_TestClient(guid, "IP");
              clientname = SQ_TestClient(guid, "Name");

              SQ_ClientConnect("", guid, "", type, "", "", NULL);
           
              SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^3Unbanned: ^5@%i ^7%s ^7his IP(^1%s^7) has been removed\"", clientid, clientname, clientip);

              Com_sprintf(cmd, sizeof(cmd), "removeip %s\n", clientip);
              Cmd_ExecuteString(cmd);                  


           }
           else 
           { 
              SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player with id ^5@%i ^7does not exist\"", clientid);
           }
       }

       return;
}

// !force
void PB_Cforce(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;

       clevel = TestLevel("force");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }
       else
       {

           client_t *client;
           int	clId;
           char	cmd[64];
           char	*newteam;

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() != 3)
           {
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !force <client> <team>\"");
               return;
           }

           client = SV_GetPlayerByHandle();
           newteam = Cmd_Argv(2);

           if (Q_stricmp(newteam, "r") == 0){newteam = "red";}
           if (Q_stricmp(newteam, "b") == 0){newteam = "blue";}
           if (Q_stricmp(newteam, "s") == 0){newteam = "spectator";}

           if ((Q_stricmp(newteam, "red") == 0) || (Q_stricmp(newteam, "blue") == 0) || (Q_stricmp(newteam, "spectator") == 0))
           {
               if (client) 
               {

                  clId = client - svs.clients;
               
                  Com_sprintf(cmd, sizeof(cmd), "forceteam %i %s\n", clId, newteam);
                  Cmd_ExecuteString(cmd);
                  SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7%s ^7forced to: %s\"", client->name, newteam);
                  SV_SendServerCommand(client, "chat \"^1Warning^3[PM]^1: ^7Your are forced to: %s\"", newteam);

                }
                else 
                { 
                  SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player %s ^7is not on the server\"", Cmd_Argv(1));
                }
           }          
           else
           {
               SV_SendServerCommand(cl, "chat \"^1ERROR^3[PM]^1:^7 Team = r or b or s or red or blue or spectator\"");
               return;
           } 
       }
       return;
}

// !teams
void PB_Cteams( client_t *cl ) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;

       clevel = TestLevel("teams");

       if (clevel == -1){clevel = 1;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }


       else 
       {   

            int            i;
            client_t       *client;
            playerState_t  *ps;
            int            cteam = NULL;
            char           *mteam = NULL;
            int            oteam;
            int            nr = 0;
            int	           nb = 0;
            int	           ns = 0;
            int            compteur = 0;
            int            nplayermove;
            char           cmd[64];
            int            clId;

            for (i=0,client=svs.clients ; i < sv_maxclients->integer ; i++,client++)
	    {
		if (!client->state)
			continue;

		ps = SV_GameClientNum( i );
		cteam = ps->persistant[PERS_TEAM];

                if (cteam == 1)
                {
                   nr++;
                }
                if (cteam == 2)
                {
                   nb++;
                }
                if (cteam == 3)
                {
                   ns++;
                }

            }   
       SV_SendServerCommand(NULL, "chat \"^2INFO: ^7RED: ^1%i ^7- BLUE: ^4%i ^7- SPECTATOR: ^3%i^7\"", nr, nb, ns);

       compteur = fabs(nr - nb);

       if (compteur > 1)
       {
         
         if (nr > nb){oteam = 1; mteam = "b";}
         if (nb > nr){oteam = 2; mteam = "r";}
 
         nplayermove = compteur / 2;

       	    nr = 0;
            nb = 0;
            ns = 0;
       
            for (i=0,client=svs.clients ; i < sv_maxclients->integer ; i++,client++)
	    {
		if (!client->state)
			continue;

		ps = SV_GameClientNum( i );
		cteam = ps->persistant[PERS_TEAM];

                if (cteam == oteam)
                {
                  nplayermove = nplayermove - 1;

                  if (nplayermove >= 0)
                  {
                     clId = client - svs.clients;
                     Com_sprintf(cmd, sizeof(cmd), "forceteam %i %s\n", clId, mteam);
                     Cmd_ExecuteString(cmd);
                  }
                }
            }

            for (i=0,client=svs.clients ; i < sv_maxclients->integer ; i++,client++)
	    {
		if (!client->state)
			continue;

		ps = SV_GameClientNum( i );
		cteam = ps->persistant[PERS_TEAM];

                if (cteam == 1)
                {
                   nr++;
                }
                if (cteam == 2)
                {
                   nb++;
                }
                if (cteam == 3)
                {
                   ns++;
                }

            }            

         SV_SendServerCommand(NULL, "chat \"^2INFO: ^7Teams are now balanced Red: ^1%i ^7Blue: ^4%i^7\"", nr, nb);
         SV_SendServerCommand(NULL, "cp \"Teams are now balanced\"");
       }

       else
       {
         SV_SendServerCommand(NULL, "chat \"^2INFO: ^7Teams are already balanced Red: ^1%i ^7Blue: ^4%i^7\"", nr, nb);
         SV_SendServerCommand(NULL, "cp \"Teams are already balanced\"");
       }

          
     }
     return;      
}

// !mapname
void PB_Cmapname (client_t *cl) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       char	*mapname;
       int	clevel;
       int      clientlevel;
       
       clevel = TestLevel("mapname");

       if (clevel == -1){clevel = 1;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   
           mapname = Cvar_VariableString( "mapname" );
           SV_SendServerCommand(cl, "chat \"^2INFO^3[PM]^2: ^7Map: ^4%s^7\"", mapname);    
       }

       
       return;
}

// !warn
void PB_Cwarn(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       char	*message;
       int      clientlevel;
       client_t *client;

       clevel = TestLevel("warn");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }
       else
       {

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() < 3)
           {
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !warn <client> <message>\"");
               return;
           }

           client = SV_GetPlayerByHandle();
	   message = Cmd_ArgsFromRaw(2);
	   Cmd_TokenizeString(message);
           

           if (client) 
           {

                  SV_SendServerCommand(NULL, "chat \"^1WARNING: ^7%s^7, %s\"", client->name, message);
                  SV_SendServerCommand(client, "cp \"^1Warning^3[PM]^1:^7%s^7, %s\"",client->name, message);

           }
           else 
           { 
                 SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player %s ^7is not on the server\"", Cmd_Argv(1));
           }

       }
       return;
}

// !veto
void PB_Cveto (client_t *cl) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       char	cmd[64];
       int	clevel;
       int      clientlevel;
       
       clevel = TestLevel("veto");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)  
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   
           Com_sprintf(cmd, sizeof(cmd), "veto\n");
           Cmd_ExecuteString(cmd);    
       }

       
       return;
}

// !nuke
void PB_Cnuke (client_t *cl, char *arg) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;
       
       clevel = TestLevel("nuke");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       { 

           int      clId;
           client_t *client;
           playerState_t *ps;
           int	cteam;
           char	cmd[64];

           Cmd_TokenizeString( arg );
  
           if (Cmd_Argc() != 2) 
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !nuke <client>\"");
              return;
           }

           client = SV_GetPlayerByHandle();

                     
           if (client) 
           {
                      
              clId = client - svs.clients;
	      ps = SV_GameClientNum( clId );
	      cteam = ps->persistant[PERS_TEAM];
           
              if (cteam == 3)
              {
                  SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7%s ^7is in spectator\"", client->name) ;
                  return;
              }
              else 
              {                 
               Com_sprintf(cmd, sizeof(cmd), "nuke %i\n", clId);
               Cmd_ExecuteString(cmd);
               SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7was nuked by Admin\"", client->name);
              }

           }
           else 
           { 
                   SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player %s ^7is not on the server\"", Cmd_Argv(1));
           }
       }
       return; 
}

// !rename
void PB_Crename(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       char	*newname;
       int      clientlevel;
       client_t *client;

       clevel = TestLevel("rename");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)  
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }
       else
       {

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() < 3)
           {
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !rename <client> <new name>\"");
               return;
           }

           client = SV_GetPlayerByHandle();
           newname = Cmd_Argv(2);
           

           if (client) 
           {

             
             Info_SetValueForKey(client->userinfo, "name", newname);
             SV_UserinfoChanged(cl);
             VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, client - svs.clients);

           }
           else 
           { 
                 SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player %s ^7is not on the server\"", Cmd_Argv(1));
           }

       }
       return;
}

// !bigtext
void PB_Cbigtext(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int	*message;
       int      clientlevel;

       clevel = TestLevel("bigtext");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }
       else
       {

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() < 2)
           {
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !bigtext <message>\"");
               return;
           }


	   message = Cmd_Args();


            SV_SendServerCommand(NULL, "cp \"%s\"", message);


       }
       return;
}

// !privatebigtext
void PB_Cprivatebigtext(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       char	*message;
       int      clientlevel;
       client_t *client;

       clevel = TestLevel("privatebigtext");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }
       else
       {

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() < 3)
           {
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !pbigtext <client> <message>\"");
               return;
           }

           client = SV_GetPlayerByHandle();
	   message = Cmd_ArgsFromRaw(2);
	   Cmd_TokenizeString(message);
           
           if (client) 
           {

                  SV_SendServerCommand(client, "cp \"^3[PM]:^7 %s\"", message);

           }
           else 
           { 
                 SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player %s ^7is not on the server\"", Cmd_Argv(1));
           }

       }
       return;
}

// !mute
void PB_Cmute (client_t *cl, char *arg) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;
       
       clevel = TestLevel("mute");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       { 

           int      clId;
           client_t *client;
           char	cmd[64];

           Cmd_TokenizeString( arg );
  
           if (Cmd_Argc() != 2) 
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !mute <client>\"");
              return;
           }

           client = SV_GetPlayerByHandle();

                     
           if (client) 
           {
                      
              clId = client - svs.clients;
                
               Com_sprintf(cmd, sizeof(cmd), "mute %i\n", clId);
               Cmd_ExecuteString(cmd);
               SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7was muted by Admin\"", client->name);

           }
           else 
           { 
               SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player %s ^7is not on the server\"", Cmd_Argv(1));
           }
       }
       return; 
}

// !pause
void PB_Cpause (client_t *cl) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       char	cmd[64];
       int	clevel;
       int      clientlevel;
       
       clevel = TestLevel("pause");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)  
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   
           Com_sprintf(cmd, sizeof(cmd), "pause\n");
           Cmd_ExecuteString(cmd);    
       }

       
       return;
}

// !swapteams
void PB_Cswapteams (client_t *cl) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       char	cmd[64];
       int	clevel;
       int      clientlevel;
       
       clevel = TestLevel("swapteams");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   
           Com_sprintf(cmd, sizeof(cmd), "swapteams\n");
           Cmd_ExecuteString(cmd);    
       }

       
       return;
}

// !shuffleteams
void PB_Cshuffleteams (client_t *cl) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       char	cmd[64];
       int	clevel;
       int      clientlevel;
       
       clevel = TestLevel("shuffleteams");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)  
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   
           Com_sprintf(cmd, sizeof(cmd), "shuffleteams\n");
           Cmd_ExecuteString(cmd);    
       }

       
       return;
}

// !moon
void PB_Cmoon(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       char	*onoff;
       int      clientlevel;
       char	cmd[64];
       int	gravity;

       clevel = TestLevel("moon");

       if (clevel == -1){clevel = 5;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }
       else
       {

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() < 2)
           {
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !moon <on / off>\"");
               return;
           }


	   onoff = Cmd_Args();

           if ((Q_stricmp(onoff, "on") == 0) || (Q_stricmp(onoff, "off") == 0))
           {
               if (Q_stricmp(onoff, "on") == 0){
                  gravity = 100;
                  SV_SendServerCommand(NULL, "chat \"^2INFO: ^7Moon mode: ^2ON^7\"");
               }
             
               if (Q_stricmp(onoff, "off") == 0){
                  gravity = 800;
                  SV_SendServerCommand(NULL, "chat \"^2INFO: ^7Moon mode: ^1OFF^7\"");
               }

               Com_sprintf(cmd, sizeof(cmd), "g_gravity %i\n", gravity);
               Cmd_ExecuteString(cmd); 
           }

           else
           {
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !moon <on / off>\"");
               return;
           }

 
       }
       return;
}

// !gravity
void PB_Cgravity(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;
       char	cmd[64];
       int	gravity;

       clevel = TestLevel("moon");

       if (clevel == -1){clevel = 5;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }
       else
       {

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() < 2)
           {
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !gravity <value>\"");
               return;
           }


	   gravity = atoi(Cmd_Args());

           SV_SendServerCommand(NULL, "chat \"^2INFO: ^7Gravity changed to ^4%i^7\"", gravity);

           Com_sprintf(cmd, sizeof(cmd), "g_gravity %i\n", gravity);
           Cmd_ExecuteString(cmd); 
 
       }
       return;
}

// !exec
void PB_Cexec(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       char	*value;
       int      clientlevel;
       char	cmd[64];

       clevel = TestLevel("exec");

       if (clevel == -1){clevel = 4;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)  
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }
       else
       {

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() < 2)
           {
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !exec <value>\"");
               return;
           }


	   value = Cmd_Args();

           Com_sprintf(cmd, sizeof(cmd), "exec %s\n", value);
           Cmd_ExecuteString(cmd); 
 
       }
       return;
}

// !gametype
void PB_Cgametype(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       char	*gametype;
       char     *ngametype;
       int	*vgametype;
       int      clientlevel;
       char	cmd[64];

       clevel = TestLevel("gametype");

       if (clevel == -1){clevel = 4;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }
       else
       {

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() < 2)
           {
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !gametype <ffa, tdm, ts, ftl, cah, ctf, bomb>\"");
               return;
           }


	   gametype = Cmd_Args();

           if (Q_stricmp(gametype, "ffa") == 0){vgametype = 0; ngametype = "FreeForAll";}
           else if (Q_stricmp(gametype, "tdm") == 0){vgametype = 3; ngametype = "TeamDeathMatch";}
           else if (Q_stricmp(gametype, "ts") == 0){vgametype = 4; ngametype = "Team Survivor";}
           else if (Q_stricmp(gametype, "ftl") == 0){vgametype = 5; ngametype = "Follow the Leader";}
           else if (Q_stricmp(gametype, "cah") == 0){vgametype = 6; ngametype = "Capture and Hold";}
           else if (Q_stricmp(gametype, "ctf") == 0){vgametype = 7; ngametype = "Capture The Flag";}
           else if (Q_stricmp(gametype, "bomb") == 0){vgametype = 8; ngametype = "Bomb";}
           else
           {
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !gametype <ffa, tdm, ts, ftl, cah, ctf, bomb>\"");
               return;
           }
           
           SV_SendServerCommand(NULL, "chat \"^2INFO: ^7Game type changed to ^4%s^7\"", ngametype);
           Com_sprintf(cmd, sizeof(cmd), "g_gametype %i\n", vgametype);
           Cmd_ExecuteString(cmd); 
 
       }
       return;
}

// !respawndelay
void PB_Crespawndelay(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;
       char	cmd[64];
       int	value;
       char     *respawndelay;

       clevel = TestLevel("respawndelay");

       if (clevel == -1){clevel = 5;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }
       else
       {

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() < 2)
           {
               respawndelay = Cvar_VariableString( "g_respawnDelay" );
               SV_SendServerCommand(cl, "chat \"^2INFO^3[PM]^3: ^7RespawnDelay: %s^7\"", respawndelay);
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !respawndelay <value>\"");
               return;
           }


	   value = atoi(Cmd_Args());

           SV_SendServerCommand(NULL, "chat \"^2INFO: ^7RespawnDelay changed to ^4%i^7\"", value);

           Com_sprintf(cmd, sizeof(cmd), "g_respawnDelay %i\n", value);
           Cmd_ExecuteString(cmd); 
 
       }
       return;
}

// !respawngod
void PB_Crespawngod(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;
       char	cmd[64];
       int	value;
       char	*respawngod;

       clevel = TestLevel("respawngod");

       if (clevel == -1){clevel = 5;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }
       else
       {

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() < 2)
           {
               respawngod = Cvar_VariableString( "g_respawnProtection" );
               SV_SendServerCommand(cl, "chat \"^2INFO^3[PM]^3: ^7RespawnProtection: %s^7\"", respawngod);
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !respawngod <value>\"");
               return;
           }


	   value = atoi(Cmd_Args());

           SV_SendServerCommand(NULL, "chat \"^2INFO: ^7RespawnProtection changed to ^4%i^7\"", value);

           Com_sprintf(cmd, sizeof(cmd), "g_respawnProtection %i\n", value);
           Cmd_ExecuteString(cmd); 
 
       }
       return;
}

// !timelimit
void PB_Ctimelimit(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;
       char	cmd[64];
       int	value;
       char     *timelimit;

       clevel = TestLevel("timelimit");

       if (clevel == -1){clevel = 5;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }
       else
       {

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() < 2)
           {
               timelimit = Cvar_VariableString( "timelimit" );
               SV_SendServerCommand(cl, "chat \"^2INFO^3[PM]^3: ^7Timelimit: %s^7\"", timelimit);
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !timelimit <value>\"");
               return;
           }


	   value = atoi(Cmd_Args());

           SV_SendServerCommand(NULL, "chat \"^2INFO: ^7Timelimit changed to ^4%i^7\"", value);

           Com_sprintf(cmd, sizeof(cmd), "timelimit %i\n", value);
           Cmd_ExecuteString(cmd); 
 
       }
       return;
}

// !fraglimit
void PB_Cfraglimit(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;
       char	cmd[64];
       int	value;
       char     *fraglimit;

       clevel = TestLevel("fraglimit");

       if (clevel == -1){clevel = 5;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clientlevel == 0)
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }
       else
       {

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() < 2)
           {
               fraglimit = Cvar_VariableString( "fraglimit" );
               SV_SendServerCommand(cl, "chat \"^2INFO^3[PM]^3: ^7Fraglimit: %s^7\"", fraglimit);
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !fraglimit <value>\"");
               return;
           }


	   value = atoi(Cmd_Args());

           SV_SendServerCommand(NULL, "chat \"^2INFO: ^7Fraglimit changed to ^4%i^7\"", value);

           Com_sprintf(cmd, sizeof(cmd), "fraglimit %i\n", value);
           Cmd_ExecuteString(cmd); 
 
       }
       return;
}

// !caplimit
void PB_Ccaplimit(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;
       char	cmd[64];
       int	value;
       char	*caplimit;

       clevel = TestLevel("caplimit");

       if (clevel == -1){clevel = 5;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)  
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }
       else
       {

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() < 2)
           {
               caplimit = Cvar_VariableString( "caplimit" );
               SV_SendServerCommand(cl, "chat \"^2INFO^3[PM]^3: ^7Capturelimit: %s^7\"", caplimit);
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !caplimit <value>\"");
               return;
           }


	   value = atoi(Cmd_Args());

           SV_SendServerCommand(NULL, "chat \"^2INFO: ^7Capturelimit changed to ^4%i^7\"", value);

           Com_sprintf(cmd, sizeof(cmd), "capturelimit %i\n", value);
           Cmd_ExecuteString(cmd); 
 
       }
       return;
}

// !matchmode
void PB_Cmatchmode(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       char	*onoff;
       int      clientlevel;
       char	cmd[64];
       char	*value;
       char	*matchmode;

       clevel = TestLevel("matchmode");

       if (clevel == -1){clevel = 5;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }
       else
       {

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() < 2)
           {
               matchmode = Cvar_VariableString( "g_matchmode" );
               if (Q_stricmp(matchmode, "1") == 0){matchmode = "^2ON";}
               else if (Q_stricmp(matchmode, "0") == 0){matchmode = "^1OFF";}
               SV_SendServerCommand(cl, "chat \"^2INFO^3[PM]^3: ^7Matchmode: %s^7\"", matchmode);
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !matchmode <on / off>\"");
               return;
           }


	   onoff = Cmd_Args();

           if ((Q_stricmp(onoff, "on") == 0) || (Q_stricmp(onoff, "off") == 0))
           {
               if (Q_stricmp(onoff, "on") == 0){
                  value = "1";
                  SV_SendServerCommand(NULL, "chat \"^2INFO: ^7Matchmode: ^2ON^7\"");
               }
             
               if (Q_stricmp(onoff, "off") == 0){
                  value = "0";
                  SV_SendServerCommand(NULL, "chat \"^2INFO: ^7Matchmode: ^1OFF^7\"");
               }

               Com_sprintf(cmd, sizeof(cmd), "g_matchmode %s\n", value);
               Cmd_ExecuteString(cmd); 
           }

           else
           {
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !matchmode <on / off>\"");
               return;
           }

 
       }
       return;
}

// !swaproles
void PB_Cswaproles(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       char	*onoff;
       int      clientlevel;
       char	cmd[64];
       char	*value;
       char	*swaproles;

       clevel = TestLevel("swaproles");

       if (clevel == -1){clevel = 5;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }
       else
       {

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() < 2)
           {   
               swaproles = Cvar_VariableString( "g_swaproles" );
               if (Q_stricmp(swaproles, "1") == 0){swaproles = "^2ON";}
               else if (Q_stricmp(swaproles, "0") == 0){swaproles = "^1OFF";}
               SV_SendServerCommand(cl, "chat \"^2INFO^3[PM]^3: ^7Swaproles: %s^7\"", swaproles);
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !swaproles <on / off>\"");
               return;
           }


	   onoff = Cmd_Args();

           if ((Q_stricmp(onoff, "on") == 0) || (Q_stricmp(onoff, "off") == 0))
           {
               if (Q_stricmp(onoff, "on") == 0){
                  value = "1";
                  SV_SendServerCommand(NULL, "chat \"^2INFO: ^7Swaproles: ^2ON^7\"");
               }
             
               if (Q_stricmp(onoff, "off") == 0){
                  value = "0";
                  SV_SendServerCommand(NULL, "chat \"^2INFO: ^7Swaproles: ^1OFF^7\"");
               }

               Com_sprintf(cmd, sizeof(cmd), "g_swaproles %s\n", value);
               Cmd_ExecuteString(cmd); 
           }

           else
           {
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !swaproles <on / off>\"");
               return;
           }

 
       }
       return;
}

// !friendlyfire
void PB_Cfriendlyfire(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       char	*onoff;
       int      clientlevel;
       char	cmd[64];
       char	*value;
       char	*friendlyfire;

       clevel = TestLevel("friendlyfire");

       if (clevel == -1){clevel = 5;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }
       else
       {

           Cmd_TokenizeString( arg );

           if (Cmd_Argc() < 2)
           {   
               friendlyfire = Cvar_VariableString( "g_friendlyFire" );
               if (Q_stricmp(friendlyfire, "1") == 0){friendlyfire = "^2ON";}
               else if (Q_stricmp(friendlyfire, "0") == 0){friendlyfire = "^1OFF";}
               else if (Q_stricmp(friendlyfire, "2") == 0){friendlyfire = "^2ON NO KICK";}
               SV_SendServerCommand(cl, "chat \"^2INFO^3[PM]^3: ^7Friendlyfire: %s^7\"", friendlyfire);
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !friendlyfire <on / nokick / off>\"");
               return;
           }


	   onoff = Cmd_Args();

           if ((Q_stricmp(onoff, "on") == 0) || (Q_stricmp(onoff, "off") == 0) || (Q_stricmp(onoff, "nokick") == 0))
           {
               if (Q_stricmp(onoff, "on") == 0){
                  value = "1";
                  SV_SendServerCommand(NULL, "chat \"^2INFO: ^7Friendlyfire: ^2ON^7\"");
               }
             
               if (Q_stricmp(onoff, "off") == 0){
                  value = "0";
                  SV_SendServerCommand(NULL, "chat \"^2INFO: ^7Friendlyfire: ^1OFF^7\"");
               }

               if (Q_stricmp(onoff, "nokick") == 0){
                  value = "2";
                  SV_SendServerCommand(NULL, "chat \"^2INFO: ^7Friendlyfire: ^2ON NO KICK^7\"");
               }

               Com_sprintf(cmd, sizeof(cmd), "g_friendlyFire %s\n", value);
               Cmd_ExecuteString(cmd); 
           }

           else
           {
               SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !friendlyfire <on / nokick / off>\"");
               return;
           }

 
       }
       return;
}

// !nextmap
int TestMapcycle(char *mapcycle){
    FILE* fichier = NULL;
    fichier = fopen(mapcycle, "r");

    if (fichier != NULL)
    {
        return 1;
        fclose(fichier);

    }
   else {return 0;}

}

void PB_Cnextmap (client_t *cl) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       char	*nextmapname;
       int	clevel;
       int      clientlevel;
       int      testmc;

       clevel = TestLevel("nextmap");

       if (clevel == -1){clevel = 1;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   

           nextmapname = Cvar_VariableString( "g_nextmap" );
           
           if (!Q_stricmp(nextmapname, "") == 0){
               SV_SendServerCommand(cl, "chat \"^2INFO^3[PM]^2: ^7NextMap: ^5%s^7\"", nextmapname);
           } 
           else{
               FILE* mapsfile = NULL;
               char lmaps[128];       
       
               char *homePath;
               char *basePath;
               char *q3ut4Path;
               char *mapcyclefile;
               char *mapcyclefile2;
               char *filemapcycle = Cvar_VariableString( "g_mapcycle" );
               char *mapname;
               char *testb = NULL;
               int n = 0;
               char firstmap[128];
               char *test = NULL;

               homePath = Sys_DefaultHomePath();
  
               basePath = Cvar_VariableString( "fs_basepath" );

               q3ut4Path = Cvar_VariableString( "fs_game" );

               mapcyclefile = FS_BuildOSPath( homePath, q3ut4Path , filemapcycle);
               mapcyclefile2 = FS_BuildOSPath( basePath, "q3ut4" , filemapcycle);


               mapname = Cvar_VariableString( "mapname" );
           
               if (testmc = TestMapcycle(mapcyclefile) == 1){mapsfile = fopen(mapcyclefile, "r");}
               else {mapsfile = fopen(mapcyclefile2, "r");}
           
               if (mapsfile != NULL)
               {
                  while (fgets(lmaps, sizeof(lmaps), mapsfile) != NULL)
                  { 
                      char *suprl = strchr(lmaps, '\n');
                      if (suprl) {*suprl = 0;}
                      suprl = strchr(lmaps," ");
                      if (suprl) {*suprl = 0;}
                      if ((n == 0)&&(strcmp(lmaps,"") != 0)){
                         n++;
                         strcpy(firstmap,lmaps);
                      }
                                      
                      if (Q_stricmp(lmaps, mapname) == 0){
                          test = "ok";
                          continue;
                      }
                      if (test != NULL){
                     
                            if (testb == NULL){
 
                                if (strpbrk(lmaps, "{") != NULL){testb = "ok"; 
                                    continue;}                      

                                if (strcmp(lmaps, "") == 0){continue;}
                                else{SV_SendServerCommand(cl, "chat \"^2INFO^3[PM]^2: ^7NextMap: ^5%s^7\"", lmaps); 
                                     break;}
                               
                             }
         
                            if (testb != NULL){
                               if (strpbrk(lmaps, "}") != NULL){testb = NULL;
                                   continue;}
                            }
                        }
                  }
                  fclose(mapsfile);

               }

               if ((test == NULL)||(strcmp(lmaps, "") == 0)){SV_SendServerCommand(cl, "chat \"^2INFO^3[PM]^2: ^7NextMap: ^5%s^7\"", firstmap);}
               
             }
       }
       
       return;
}

// !superslap
void PB_Csuperslap (client_t *cl, char *arg) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;

       clevel = TestLevel("superslap");

       if (clevel == -1){clevel = 4;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)  
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       { 

          int      clId;
          client_t *client;
          playerState_t	*ps;
          int	cteam;
          int      nslap;
          int      i;
          char	cmd[64];

           Cmd_TokenizeString( arg );
  
           if (Cmd_Argc() != 3) 
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !superslap <client> <death / big>\"");
              return;
           }

           client = SV_GetPlayerByHandle();
                                
           if (client) 
           {
                      
              clId = client - svs.clients;
	      ps = SV_GameClientNum( clId );
	      cteam = ps->persistant[PERS_TEAM];
           
              if (cteam == 3)
              {
                  SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7%s ^7is in spectator\"", client->name) ;
                  return;
              }
              else 
              {   
                  if ((Q_stricmp("death", Cmd_Argv(2))==0)||(Q_stricmp("d", Cmd_Argv(2))==0))
                  {
                     SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7was slapped to death by Admin\"", client->name);
                     nslap = 25;
                  }

                  else if ((Q_stricmp("big", Cmd_Argv(2))==0)||(Q_stricmp("b", Cmd_Argv(2))==0))         
                  {
                     SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7has took a big slap by Admin\"", client->name);
                     nslap = 10;
                  }

                  else
                  {
                     SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !superslap <client> <death / big>\"");
                     return;
                  }

                  for (i=1; i<nslap; i++) {
 
                     Com_sprintf(cmd, sizeof(cmd), "slap %i\n", clId);
                     Cmd_ExecuteString(cmd);
                     
                  }
                               
              }

           }
           else 
           { 
                   SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player %s ^7is not on the server\"", Cmd_Argv(1));
           }
       }
       return; 
}

// !admins
void PB_Cadmins(client_t *cl) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       int	clevel;
       int      clientlevel;

       clevel = TestLevel("admins");

       if (clevel == -1){clevel = 1;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel) 
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   

          int      i;
          client_t *client;
          char *adminguid;
          char *adminaka;
          int   adminlevel;
 
          SV_SendServerCommand(cl, "chat \"^2Admins online^3[PM]:\"");


                for (i=0,client=svs.clients ; i < sv_maxclients->integer ; i++,client++)
	        {
		    if (!client->state)
		       {continue;}

                     adminguid = Info_ValueForKey(client->userinfo, "cl_guid");
                     
                     adminlevel = SQ_TestClient(adminguid, "Setlevel");
                     adminaka = SQ_TestClient(adminguid, "Aka");

                     if (adminlevel > 2){
                        SV_SendServerCommand(cl, "chat \"^7[^1%i^7]%s Level(^5%i^7) aka %s^7\"", i, client->name, adminlevel, adminaka);
                    }
                }

       }
      
       return;
}

// !lookup
int SQ_TestClientname(client_t *cl, char *name)
{

    char *tdatabase;
    char nrequete[1024];
    tdatabase = databasefile();
    sqlite3 *ndb;
    int nrv;
    sqlite3_stmt* nstmt;
    char *test = NULL;
    char *c;
    char cleanName[64];
    int i = 0;          

    tdatabase = databasefile();

    nrv = sqlite3_open(tdatabase,&ndb);

    sprintf(nrequete, "SELECT * from clients ORDER BY date DESC");
    
    nrv = sqlite3_prepare_v2(ndb,nrequete,-1,&nstmt,0);
    
    if (nrv == SQLITE_OK){

        while(1)
        {        

           nrv = sqlite3_step(nstmt);
    

           if(nrv == SQLITE_ROW)
           {

              char *tname = (char*)sqlite3_column_text(nstmt,1);
              Q_strncpyz( cleanName, tname, sizeof(cleanName) );
	      Q_CleanStr( cleanName );

              if (c = strstr(cleanName, name)!= NULL)
              {
                 test = "ok";
                 i++;
                 char *tid = (char*)sqlite3_column_text(nstmt,0);
                 char *tlevel = (char*)sqlite3_column_text(nstmt,5);
                 char *taka = (char*)sqlite3_column_text(nstmt,2);

                 if (taka == NULL){
                 SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%s^7 %s ^3Level ^7(^5%s^7)\"", tid, tname, tlevel);
                 }
                 else {
                 SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%s^7 %s ^3Level ^7(^5%s^7) ^3Aka:^7 %s^7\"", tid, tname, tlevel, taka);
                 }
                 if (i== 20){break;}  
               
              }

           }

           else if(nrv == SQLITE_DONE)
           {
              break;
           }

        }

    } 
   
    if (test == NULL){
       SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7No player found\"");
    }
   
    sqlite3_finalize(nstmt);
    sqlite3_close(ndb);

}

int SQ_TestClientexactname(client_t *cl, char *name)
{

    char *tdatabase;
    char xrequete[1024];
    tdatabase = databasefile();
    sqlite3 *xdb;
    int xrv;
    sqlite3_stmt* xstmt;
    char *test = NULL;
    char cleanName[64];

    tdatabase = databasefile();

    xrv = sqlite3_open(tdatabase,&xdb);

    sprintf(xrequete, "SELECT * from clients ORDER BY date DESC");
    
    xrv = sqlite3_prepare_v2(xdb,xrequete,-1,&xstmt,0);
    
    if (xrv == SQLITE_OK){

        while(1)
        {        

           xrv = sqlite3_step(xstmt);
    

           if(xrv == SQLITE_ROW)
           {

              char *tname = (char*)sqlite3_column_text(xstmt,1);
              Q_strncpyz( cleanName, tname, sizeof(cleanName) );
	      Q_CleanStr( cleanName );

              if (!strcmp(cleanName, name))
              {
                 test = "ok";
                 char *tid = (char*)sqlite3_column_text(xstmt,0);
                 char *tlevel = (char*)sqlite3_column_text(xstmt,5);
                 char *taka = (char*)sqlite3_column_text(xstmt,2);

                 if (taka == NULL){
                 SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%s^7 %s ^3Level ^7(^5%s^7)\"", tid, tname, tlevel);
                 }
                 else {
                 SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%s^7 %s ^3Level ^7(^5%s^7) ^3Aka:^7 %s^7\"", tid, tname, tlevel, taka);
                 }

              }

           }

           else if(xrv == SQLITE_DONE)
           {
              break;
           }

        }

    } 
   
    sqlite3_finalize(xstmt);
    sqlite3_close(xdb);

    if (test == NULL){
       SQ_TestClientname(cl, name);
    }

}

void PB_Clookup (client_t *cl, char *arg) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");

       int	clevel;
       int      clientlevel;

       clevel = TestLevel("lookup");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)  
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       { 
           Cmd_TokenizeString( arg );
  
           if (Cmd_Argc() != 2) 
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !lookup <name>\"");
              return;
           }

           SQ_TestClientexactname(cl, Cmd_Argv(1)); 

       }

       return;
}

// !lookupip
int SQ_TestClientip(client_t *cl, char *ip)
{

    char *tdatabase;
    char iprequete[1024];
    tdatabase = databasefile();
    sqlite3 *ipdb;
    int iprv;
    sqlite3_stmt* ipstmt;
    char *test = NULL;

    tdatabase = databasefile();

    iprv = sqlite3_open(tdatabase,&ipdb);

    sprintf(iprequete, "SELECT * from clients ORDER BY date DESC");
    
    iprv = sqlite3_prepare_v2(ipdb,iprequete,-1,&ipstmt,0);
    
    if (iprv == SQLITE_OK){

        while(1)
        {        

           iprv = sqlite3_step(ipstmt);
    

           if(iprv == SQLITE_ROW)
           {

              char *tip = (char*)sqlite3_column_text(ipstmt,3);

              if (!strcmp(ip, tip))
              {
                 test = "ok";
                 char *tid = (char*)sqlite3_column_text(ipstmt,0);
                 char *tname = (char*)sqlite3_column_text(ipstmt,1);
                 char *tlevel = (char*)sqlite3_column_text(ipstmt,5);
                 char *taka = (char*)sqlite3_column_text(ipstmt,2);

                 if (taka == NULL){
                 SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%s^7 %s ^3Level ^7(^5%s^7)\"", tid, tname, tlevel);
                 }
                 else {
                 SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%s^7 %s ^3Level ^7(^5%s^7) ^3Aka:^7 %s^7\"", tid, tname, tlevel, taka);
                 }

              }

           }

           else if(iprv == SQLITE_DONE)
           {
              break;
           }

        }

    } 
   
    if (test == NULL){
       SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7No player with IP (^1%s^7) found\"", ip);
    }
   
    sqlite3_finalize(ipstmt);
    sqlite3_close(ipdb);

}

void PB_Clookupip (client_t *cl, char *arg) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");

       int	clevel;
       int      clientlevel;

       clevel = TestLevel("lookupip");

       if (clevel == -1){clevel = 3;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)  
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       { 
           Cmd_TokenizeString( arg );
  
           if (Cmd_Argc() != 2) 
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !lookupip <ip>\"");
              return;
           }

            SQ_TestClientip(cl, Cmd_Argv(1));

       }

       return;
}

//!lookupban
int SQ_TestBanname(client_t *cl, char *name)
{

    char *tdatabase;
    char nrequete[1024];
    tdatabase = databasefile();
    sqlite3 *ndb;
    int nrv;
    sqlite3_stmt* nstmt;
    char *test = NULL;
    char *c;
    char cleanName[64];
    int i = 0;          
    int ban;

    struct tm * ttban;
    time_t timestamp;
    time_t timesban; 

    timestamp = time(NULL);

    tdatabase = databasefile();

    nrv = sqlite3_open(tdatabase,&ndb);

    sprintf(nrequete, "SELECT * from clients ORDER BY date DESC");
    
    nrv = sqlite3_prepare_v2(ndb,nrequete,-1,&nstmt,0);
    
    if (nrv == SQLITE_OK){

        while(1)
        {        

           nrv = sqlite3_step(nstmt);
    

           if(nrv == SQLITE_ROW)
           {

              char *tname = (char*)sqlite3_column_text(nstmt,1);
              Q_strncpyz( cleanName, tname, sizeof(cleanName) );
	      Q_CleanStr( cleanName );

              if (c = strstr(cleanName, name)!= NULL)
              {
                 char *tid = (char*)sqlite3_column_text(nstmt,0);
                 char *taka = (char*)sqlite3_column_text(nstmt,2);
                 char *tban = (char*)sqlite3_column_text(nstmt,8);

                 if (tban != NULL){

                     ban = atoi(tban);

                     if (ban == 0){
                        test = "ok";                        
                        i++;                        

                        if (taka == NULL){
                           SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%s^7 %s ^3Ban: ^1Permanent^7\"", tid, tname);
                        }
                        else {
                           SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%s^7 %s ^3Aka:^7 %s ^3Ban: ^1Permanent^7\"", tid, tname, taka);
                        }
                    }
                    if (ban > (int) timestamp){
                        test = "ok";
                        i++;
                                              
                        timesban= ban;
    
                        ttban = localtime(&timesban);

                        if (taka == NULL){
                           SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%s^7 %s ^3Ban: ^1TempBan^7\"", tid, tname);
                        }
                        else {
                           SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%s^7 %s ^3Aka:^7 %s ^3Ban: ^1TempBan^7\"", tid, tname, taka);
                        }

                        SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^3TempBan expire:^7 %02u/%02u/%02u %02u:%02u\"",ttban->tm_mday, ttban->tm_mon, 1900 + ttban->tm_year, ttban->tm_hour, ttban->tm_min);
                    }
                 }
                 if (i== 20){break;}  
             }

           }

            
           else if(nrv == SQLITE_DONE)
           {
              break;
           }

        }

    } 
   
    if (test == NULL){
       SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7No Ban found\"");
    }
   
    sqlite3_finalize(nstmt);
    sqlite3_close(ndb);

}

int SQ_TestBanexactname(client_t *cl, char *name)
{

    char *tdatabase;
    char xrequete[1024];
    tdatabase = databasefile();
    sqlite3 *xdb;
    int xrv;
    sqlite3_stmt* xstmt;
    char *test = NULL;
    char cleanName[64];
    int ban;

    struct tm * ttban;
    time_t timestamp;
    time_t timesban;    
   
    timestamp = time(NULL);

    tdatabase = databasefile();

    xrv = sqlite3_open(tdatabase,&xdb);

    sprintf(xrequete, "SELECT * from clients ORDER BY date DESC");
    
    xrv = sqlite3_prepare_v2(xdb,xrequete,-1,&xstmt,0);
    
    if (xrv == SQLITE_OK){

        while(1)
        {        

           xrv = sqlite3_step(xstmt);
    

           if(xrv == SQLITE_ROW)
           {

              char *tname = (char*)sqlite3_column_text(xstmt,1);
              Q_strncpyz( cleanName, tname, sizeof(cleanName) );
	      Q_CleanStr( cleanName );

              if (!strcmp(cleanName, name))
              {
                 char *tid = (char*)sqlite3_column_text(xstmt,0);
                 char *taka = (char*)sqlite3_column_text(xstmt,2);
                 char *tban = (char*)sqlite3_column_text(xstmt,8);

                 if (tban != NULL){

                     ban = atoi(tban);

                     if (ban == 0){

                        test = "ok";
                        
                        if (taka == NULL){
                           SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%s^7 %s ^3Ban: ^1Permanent^7\"", tid, tname);
                        }
                        else {
                           SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%s^7 %s ^3Aka:^7 %s ^3Ban: ^1Permanent^7\"", tid, tname, taka);
                        }
                    }
                    if (ban > (int) timestamp){

                        test = "ok";

                        timesban= ban;
    
                        ttban = localtime(&timesban);

                        if (taka == NULL){
                           SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%s^7 %s ^3Ban: ^1TempBan^7\"", tid, tname);
                        }
                        else {
                           SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%s^7 %s ^3Aka:^7 %s ^3Ban: ^1TempBan^7\"", tid, tname, taka);
                        }

                        SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^3TempBan expire:^7 %02u/%02u/%02u %02u:%02u\"",ttban->tm_mday, ttban->tm_mon, 1900 + ttban->tm_year, ttban->tm_hour, ttban->tm_min);
                    }
                 }
              }  
           }

           else if(xrv == SQLITE_DONE)
           {
              break;
           }

        }
    } 
   
    sqlite3_finalize(xstmt);
    sqlite3_close(xdb);

    if (test == NULL){
       SQ_TestBanname(cl, name);
    }

}

void PB_Clookupban (client_t *cl, char *arg) {

    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");

       int	clevel;
       int      clientlevel;

       clevel = TestLevel("lookupban");

       if (clevel == -1){clevel = 4;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)  
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       { 
           Cmd_TokenizeString( arg );
  
           if (Cmd_Argc() != 2) 
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !lookupban <name>\"");
              return;
           }

           SQ_TestBanexactname(cl, Cmd_Argv(1)); 

       }

       return;
}

// !infoban
void PB_Cinfoban(client_t *cl, char *arg)
{   
    if (!Q_stricmp(sv_commands->string, "0")) {
        return;
    }

       char	*cguid = Info_ValueForKey(cl->userinfo, "cl_guid");
       char	*guid;
       int	clevel;
       int      clientlevel;

       struct tm * tban;
       time_t timestamp;
       time_t timesban;    
   
       timestamp = time(NULL);
      
       clevel = TestLevel("infoban");

       if (clevel == -1){clevel = 4;}

       clientlevel = SQ_TestClient(cguid, "Setlevel");

       if (clevel > clientlevel)  
       {
           SV_SendServerCommand(NULL, "chat \"^1Warning: ^7%s ^7trying to use an admin command\"", cl->name);
           return; 
       }

       else 
       {   

           char	*clientaka;
           int      clientban;
           char	*clientname;
           int      clientid;
           int      ban;

           Cmd_TokenizeString( arg );
           
           if (Cmd_Argc() != 2)
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !infoban <@id>\"");
              return;
           }

           char *c;
           char *p;

           p = Cmd_Argv(1);
           c = *p;
  
           if (c == '@')

           {           
               p++;
               clientid = atoi(p);
           }
           else
           {
              SV_SendServerCommand(cl, "chat \"^3Usage[PM]:^7 !infoban <@id>\"");
              return;
           }

           guid = SQ_TestClientID(clientid);

           if (guid != NULL) 
           {
                           
              clientban = SQ_TestClient(guid, "Ban");
              clientname = SQ_TestClient(guid, "Name");
              clientaka = SQ_TestClient(guid, "Aka");

              if (clientban != NULL){

                    ban = atoi(clientban);

                 if (ban == 0){
                     if (clientaka == NULL){
                        SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%i^7 %s ^3Ban: ^1Permament^7\"", clientid, clientname);
                     }
                     else {
                        SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%i^7 %s ^3Aka:^7 %s ^3Ban: ^1Permament^7\"", clientid, clientname, clientaka);
                     }
                }
                else if (ban > (int) timestamp){

                     timesban= ban;
    
                     tban = localtime(&timesban);

                     if (clientaka == NULL){
                        SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%i^7 %s ^3TempBan expire: ^7%02u/%02u/%02u %02u:%02u\"",clientid, clientname,tban->tm_mday, tban->tm_mon, 1900 + tban->tm_year, tban->tm_hour, tban->tm_min);
                     }
                     else {
                        SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%i^7 %s ^3Aka:^7 %s ^3TempBan expire: ^7%02u/%02u/%02u %02u:%02u\"",clientid, clientname, clientaka, tban->tm_mday, tban->tm_mon, 1900 + tban->tm_year, tban->tm_hour, tban->tm_min);
                     }
                }
                else {
                     if (clientaka == NULL){
                        SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%i^7 %s ^3Ban: ^2No Ban Active^7\"", clientid, clientname);
                     }
                     else {
                        SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%i^7 %s ^3Aka:^7 %s ^3Ban: ^2No Ban Active^7\"", clientid, clientname, clientaka);
                     }
                }
              }
              else {

                 if (clientaka == NULL){
                     SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%i^7 %s ^3Ban: ^2No Ban Active^7\"", clientid, clientname);
                 }
                 else {
                      SV_SendServerCommand(cl, "chat \"^2Info^3[PM]^2: ^1@%i^7 %s ^3Aka:^7 %s ^3Ban: ^2No Ban Active^7\"", clientid, clientname, clientaka);
                 }
            
              }

           }
           else 
           { 
              SV_SendServerCommand(cl, "chat \"^1Warning^3[PM]^1: ^7Player with id ^5@%i ^7does not exist\"", clientid);
           }
       }

       return;
}
//===========================================================

/*
==================
SV_AddOperatorCommands
==================
*/
void SV_AddOperatorCommands( void ) {
	static qboolean	initialized;

	if ( initialized ) {
		return;
	}
	initialized = qtrue;

	Cmd_AddCommand ("heartbeat", SV_Heartbeat_f);
	Cmd_AddCommand ("kick", SV_Kick_f);
	Cmd_AddCommand ("banUser", SV_Ban_f);
	Cmd_AddCommand ("banClient", SV_BanNum_f);
	Cmd_AddCommand ("clientkick", SV_KickNum_f);
	Cmd_AddCommand ("status", SV_Status_f);
	Cmd_AddCommand ("serverinfo", SV_Serverinfo_f);
	Cmd_AddCommand ("systeminfo", SV_Systeminfo_f);
	Cmd_AddCommand ("dumpuser", SV_DumpUser_f);
	Cmd_AddCommand ("map_restart", SV_MapRestart_f);
	Cmd_AddCommand ("sectorlist", SV_SectorList_f);
	Cmd_AddCommand ("map", SV_Map_f);

#ifndef PRE_RELEASE_DEMO
	Cmd_AddCommand ("devmap", SV_Map_f);
	Cmd_AddCommand ("spmap", SV_Map_f);
	Cmd_AddCommand ("spdevmap", SV_Map_f);
#endif
	Cmd_AddCommand ("killserver", SV_KillServer_f);
	if( com_dedicated->integer ) {
		Cmd_AddCommand ("say", SV_ConSay_f);
		Cmd_AddCommand ("tell", SV_ConTell_f);
                Cmd_AddCommand("startserverdemo", SV_StartServerDemo_f);
                Cmd_AddCommand("stopserverdemo", SV_StopServerDemo_f);
	}
	Cmd_AddCommand ("privatebigtext", SV_PrivateBigtext_f);
	Cmd_AddCommand ("pbigtext", SV_PrivateBigtext_f);
	Cmd_AddCommand ("rename", SV_Rename_f);
	Cmd_AddCommand ("killplayer", SV_KillPlayer_f);
	Cmd_AddCommand ("killp", SV_KillPlayer_f);
	Cmd_AddCommand ("setlevel", SV_SetLevel_f);
}

/*
==================
SV_RemoveOperatorCommands
==================
*/
void SV_RemoveOperatorCommands( void ) {
#if 0
	// removing these won't let the server start again
	Cmd_RemoveCommand ("heartbeat");
	Cmd_RemoveCommand ("kick");
	Cmd_RemoveCommand ("banUser");
	Cmd_RemoveCommand ("banClient");
	Cmd_RemoveCommand ("status");
	Cmd_RemoveCommand ("serverinfo");
	Cmd_RemoveCommand ("systeminfo");
	Cmd_RemoveCommand ("dumpuser");
	Cmd_RemoveCommand ("map_restart");
	Cmd_RemoveCommand ("sectorlist");
	Cmd_RemoveCommand ("say");
	Cmd_RemoveCommand ("tell");
        Cmd_RemoveCommand ("startserverdemo");
        Cmd_RemoveCommand ("stopserverdemo");
#endif
}

