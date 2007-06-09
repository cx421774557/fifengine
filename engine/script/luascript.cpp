/***************************************************************************
 *   Copyright (C) 2005-2007 by the FIFE Team                              *
 *   fife-public@lists.sourceforge.net                                     *
 *   This file is part of FIFE.                                            *
 *                                                                         *
 *   FIFE is free software; you can redistribute it and/or modify          *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA              *
 ***************************************************************************/

// Standard C++ library includes
#include <cassert>
#include <cstring>
#include <list>

// 3rd party library includes
#include "lunar.h"

// FIFE includes
// These includes are split up in two parts, separated by one empty line
// First block: files included from the FIFE root src directory
// Second block: files included from the same folder
#include "gui/lua_gui_actionlistener.h"
#include "map/viewgamestate.h"
#include "util/gamestate/gamestatemanager.h"
#ifdef HAVE_MOVIE
#include "video/movie/video_gamestate.h"
#endif

#include "eventchannel/lua/lua_mouselistener.h"
#include "eventchannel/lua/lua_keylistener.h"
#include "eventchannel/lua/lua_widgetlistener.h"
#include "eventchannel/widget/ec_iwidgetlistener.h"
#include "map/lua/lua_mapcamera.h"
#include "map/lua/lua_mapcontrol.h"
#include "map/lua/lua_object.h"
#include "map/lua/lua_layer.h"
#include "map/lua/lua_elevation.h"
#include "map/lua/lua_map.h"

#include "luascript.h"
#include "scriptcontainer.h"
#include "lua_ref.h"
#include "lua_stackguard.h"
#include "lua_timer.h"

namespace FIFE {

	extern int luaopen_console(lua_State* L);
	extern int luaopen_vfs(lua_State* L);
	extern int luaopen_engine(lua_State* L);
	extern int luaopen_audiomanager(lua_State *L);
	extern int luaopen_inputmanager(lua_State *L);
	extern int luaopen_guimanager(lua_State *L);
	extern int luaopen_gsmanager(lua_State *L);

	

	void LuaScript::_initLua() {
		L = lua_open();
		if (L == NULL)
			throw FIFE::ScriptException("LuaScript - Creating LUA state failed");

		/* Lua 5.0 init calls are commented out; I am surprised this was
		 * working at all with 5.1?
		 * http://www.lua.org/manual/5.1/manual.html#7.3
		 *
		 * I am not sure how that affects our own "luaopen_*" calls below.
		 *
		 * - Do we really need IO? What for?
		 *
		 * - I would like a few of the os.* calls; but enabling things like
		 *   "execute" seems a bit dangerous.
		 */

		//luaopen_base(L);
		lua_pushcfunction(L, luaopen_base);
		lua_call(L, 0, 0);

		//luaopen_table(L);
		lua_pushcfunction(L, luaopen_table);
		lua_call(L, 0, 0);

		//luaopen_io(L);
		lua_pushcfunction(L, luaopen_io);
		lua_call(L, 0, 0);

		//luaopen_string(L);
		lua_pushcfunction(L, luaopen_string);
		lua_call(L, 0, 0);

		//luaopen_math(L);
		lua_pushcfunction(L, luaopen_math);
		lua_call(L, 0, 0);

		lua_pushcfunction(L, luaopen_package);
		lua_call(L, 0, 0);

		lua_pushcfunction(L, set_next_mapfile);
		lua_setglobal(L, "set_next_mapfile");

		lua_pushcfunction(L, set_next_videofile);
		lua_setglobal(L, "set_next_videofile");

		luaopen_gsmanager(L);
		// 		luaopen_log(L);
		luaopen_console(L);
		luaopen_audiomanager(L);
		luaopen_inputmanager(L);
		luaopen_guimanager(L);
		luaopen_engine(L);
		luaopen_vfs(L);

		Lunar<Timer_Lunar>::Register(L);
		Lunar<MapControl_Lunar>::Register(L);
		Lunar<MapCamera_Lunar>::Register(L);
		Lunar<Object_LuaScript>::RegisterTable(L);
		Lunar<Layer_LuaScript>::RegisterTable(L);
		Lunar<Elevation_LuaScript>::RegisterTable(L);
		Lunar<Map_LuaScript>::RegisterTable(L);
		Lunar<LuaMouseListener>::Register(L);
		Lunar<LuaKeyListener>::Register(L);
		Lunar<LuaWidgetListener>::Register(L);
	}

	LuaScript::LuaScript(IWidgetListener& wl) {
		new ActionListener_Lua(wl);
		Log("LuaScript") << "Scripting enabled";
		L = NULL;
	}

	void LuaScript::init() {
		if (L != NULL) {
			Log("LuaScript") << "state already started? Stopping it first!";
			deinit();
		}
		_initLua();
	}
	
	void LuaScript::deinit() {
		if (L != NULL) {
			LuaRef::unrefAll(L);
			lua_close(L);
			L = NULL;
			Log("LuaScript") << "scripting disabled";
		}
	}

	LuaScript::~LuaScript() {
		deinit(); // just in case
	}

	/* now the base calls from c++ to the script layer ... */

	void LuaScript::runFile(const std::string& filename) {
		LuaStackguard guard(L);
		if (filename.size() > 0) {
			Log("LuaScript") << std::string("loading file: ") << filename;
			if (luaL_loadfile(L, filename.c_str()) || lua_pcall(L, 0, 0, 0)) {
				const char* msg = lua_tostring(L,-1);
				lua_pop(L,1);
				throw FIFE::ScriptException(msg);
			}
		}
	}

	void LuaScript::runString(const std::string& s) {
		LuaStackguard guard(L);
		if (s.length() != 0) {
			Log("LuaScript") << std::string("loading string:") << s;
			if (luaL_loadbuffer(L, s.c_str(), s.length(), "cmd") || lua_pcall(L, 0, 0, 0)) {
				const char* msg = lua_tostring(L,-1);
				lua_pop(L,1);
				throw FIFE::ScriptException(msg);
			}
		}
	}

	int lua_follow_key(lua_State *L, const std::string& key) {
		std::string keyremainder= key;
		//Log("DEBUG") << "follow_key: " << key;

		size_t offset= keyremainder.find_first_of(".");
		while (offset != std::string::npos) {
			std::string keyprefix= keyremainder.substr(0, offset);
			keyremainder= keyremainder.substr(offset+1);
			//Log("DEBUG") << "searching for sub-key: " << keyprefix << " remainder: " << keyremainder;

			lua_pushstring(L, keyprefix.c_str());
			lua_gettable(L, -2);
			if (!lua_istable(L, -1)) {
				throw FIFE::ScriptException(std::string("Unable to find key element ") + keyprefix + std::string(" from query ") + key);
			}
			offset = keyremainder.find_first_of(".");
		}
		if (keyremainder.length() != 0) {
			lua_pushstring(L, keyremainder.c_str());
		}
		lua_gettable(L, -2);
		return 0;
	}

	void LuaScript::callFunction(const std::string& name) {
		LuaStackguard guard(L);

		size_t offset= name.find_first_of(".");
		if (offset == std::string::npos) {
			lua_getglobal(L, name.c_str());
		} else {
			std::string buffer= name.substr(0, offset);
			lua_getglobal(L, buffer.c_str());
			if (lua_type(L, -1) != LUA_TTABLE) {
				lua_pop(L, 1);
				throw ScriptException(std::string("Cannot call ")
					+ name + std::string(" as ")
					+ buffer + std::string(" is not a table"));
			}
			lua_follow_key(L, name.substr(offset+1));
		}

		if (lua_type(L, -1) == LUA_TFUNCTION) {
			if (lua_pcall(L, 0, 0, 0) != 0) {
				const char *msg = lua_tostring(L, -1);
				lua_pop(L,1);
				throw ScriptException(std::string("Error running: ") 
					+ name + " " + msg);
			}
		} else {
			lua_pop(L,1);
			Log("LuaScript") 
				<< "Ignoring call to " << name << " - no such lua function";
		}
	}

	void LuaScript::setGlobalInt(const std::string& name, const int val) {
		LuaStackguard guard(L);
		lua_pushnumber(L, val);
		lua_setglobal(L, name.c_str());
	}

	void LuaScript::setGlobalDouble(const std::string& name, const double val) {
		LuaStackguard guard(L);
		lua_pushnumber(L, val);
		lua_setglobal(L, name.c_str());
	}

	void LuaScript::setGlobalString(const std::string& name, const std::string& val) {
		LuaStackguard guard(L);
		lua_pushstring(L, val.c_str());
		lua_setglobal(L, name.c_str());
	}

	int LuaScript::getGlobalInt(const std::string& name) const {
		LuaStackguard guard(L);
		int result = 0;
		lua_getglobal(L, name.c_str());
		if (!lua_isnumber(L, -1)) {
			lua_pop(L, 1);
			throw FIFE::ScriptException("INT value expected in getGlobalInt("+name+")");
		}
		result = int(lua_tonumber(L, -1));
		lua_pop(L, 1);
		return result;
	}

	double LuaScript::getGlobalDouble(const std::string& name) const {
		LuaStackguard guard(L);
		double result = 0.0f;
		lua_getglobal(L, name.c_str());
		if (!lua_isnumber(L, -1)) {
			lua_pop(L, 1);
			throw FIFE::ScriptException("DOUBLE value expected in getGlobalDouble("+name+")");
		}
		result = lua_tonumber(L, -1);
		lua_pop(L, 1);
		return result;
	}

	std::string LuaScript::getGlobalString(const std::string& name) const {
		LuaStackguard guard(L);
		const char* result = NULL;
		lua_getglobal(L, name.c_str());
		if (!lua_isstring(L, -1)) {
			lua_pop(L, 1);
			throw FIFE::ScriptException("STRING value expected in getGlobalString("+name+")");
		}
		result = lua_tostring(L, -1);
		lua_pop(L, 1);
		return result;

	}

	template <typename GS_T> 
		void set_next_T(const std::string & t_name, const std::string & value_string) {
		GameStateManager *gsm = GameStateManager::instance();
		GS_T * gs = NULL;
		assert(gsm);
		if (gsm->gameStateExists(t_name)) {
			gs = static_cast<GS_T*>(gsm->getGameState(t_name));
			assert(gs);
			gs->setFile(value_string);
		}
		else
			throw FIFE::ScriptException("Error: no " + t_name + " GameState registered!");
	}

	int LuaScript::set_next_mapfile(lua_State *L) {
		if (!lua_isstring(L, -1))
			throw FIFE::ScriptException(std::string("String value expected:") + __FUNCTION__);
/*
		map::ViewGameState *mvgs = NULL;
		GameStateManager *gsm = GameStateManager::instance();
		assert(gsm);
		if (gsm->gameStateExists("MapView")) { // not good; hardcoded value!
			mvgs = static_cast<map::ViewGameState*>(gsm->getGameState("MapView"));
			mvgs->setFile(lua_tostring(L, -1));
		} else
			throw FIFE::ScriptException("Error: no MapView GameState registered!");
*/
		std::string v(lua_tostring(L, -1));
		set_next_T<map::ViewGameState>("MapView", v);
		return 0;
	}

	int LuaScript::set_next_videofile(lua_State *L) {
		if (!lua_isstring(L, -1))
			throw FIFE::ScriptException(std::string("String value expected:") + __FUNCTION__);
		std::string v(lua_tostring(L, -1));
#ifdef HAVE_MOVIE
		set_next_T<VideoGameState>("MoviePlayer", v);
#else
#endif
		return 0;
	}

	void LuaScript::execute(const ScriptContainer& sc) {
		if( sc.type == ScriptContainer::ScriptFile ) {
			runFile(sc.value.c_str());
		} else if( sc.type == ScriptContainer::ScriptString ) {
			runString(sc.value.c_str());
		}
	}


}
/* vim: set noexpandtab: set shiftwidth=2: set tabstop=2: */
