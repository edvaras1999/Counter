// Options.cpp - ver Options.h.
// Mesmo padrao do Options.cpp do FreeClimbLE (funcoes nativas do Papyrus e ModEvent), mas o .ini tem varias
// seccoes e e lido/escrito com as funcoes do Windows (GetPrivateProfile*/WritePrivateProfile*), que mantem os
// comentarios do ficheiro.
#include "skse/PapyrusVM.h"
#include "skse/PapyrusNativeFunctions.h"
#include "skse/PapyrusEvents.h"
#include "skse/PluginAPI.h"
#include "skse/GameData.h"
#include "skse/GameForms.h"
#include "Log.h"
#include "Options.h"
#include "Counter.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

GCConfig g_cfg;

namespace
{
	const char * const kIniName = "GuardCounters.ini";

	struct Opt { const char * section; const char * key; char type; void * ptr; float def, lo, hi; };

	Opt g_opts[] =
	{
		{ "Counter",	"WindowSeconds",		'f', &g_cfg.windowSeconds,		2.5f,	0.1f,	5.0f },
		{ "Counter",	"CooldownSeconds",		'f', &g_cfg.cooldownSeconds,	0.0f,	0.0f,	30.0f },

		{ "Combat",		"StaminaCostEnabled",	'i', &g_cfg.staminaOn,			1.0f,	0.0f,	1.0f },
		{ "Combat",		"StaminaCost",			'f', &g_cfg.staminaCost,		20.0f,	0.0f,	500.0f },
		{ "Combat",		"Invulnerability",		'i', &g_cfg.immunityOn,			1.0f,	0.0f,	1.0f },
		{ "Combat",		"SuperArmor",			'i', &g_cfg.superArmorOn,		1.0f,	0.0f,	1.0f },
		{ "Combat",		"ForcedStagger",		'i', &g_cfg.staggerOn,			1.0f,	0.0f,	1.0f },
		{ "Combat",		"StaggerMagnitude",		'f', &g_cfg.staggerMagnitude,	1.0f,	0.2f,	1.0f },
		{ "Combat",		"CounterDamagePercent",	'i', &g_cfg.damagePercent,		100.0f,	0.0f,	300.0f },

		{ "Feedback",	"GlowEnabled",			'i', &g_cfg.markOn,				1.0f,	0.0f,	1.0f },
		{ "Feedback",	"ReadySoundEnabled",	'i', &g_cfg.readySoundOn,		1.0f,	0.0f,	1.0f },
		{ "Feedback",	"ReadySoundVolume",		'i', &g_cfg.readySoundVolume,	100.0f,	0.0f,	200.0f },
		{ "Feedback",	"SwingSoundEnabled",	'i', &g_cfg.swingSoundOn,		1.0f,	0.0f,	1.0f },
		{ "Feedback",	"SwingSoundVolume",		'i', &g_cfg.swingSoundVolume,	100.0f,	0.0f,	200.0f },

		{ "LE",			"ArmedHoldSeconds",		'f', &g_cfg.armedHoldSeconds,	3.0f,	0.2f,	5.0f },
		{ "LE",			"BlockGraceSeconds",	'f', &g_cfg.blockGraceSeconds,	1.5f,	0.1f,	5.0f },
		{ "LE",			"RequireBlocking",		'i', &g_cfg.requireBlocking,	1.0f,	0.0f,	1.0f },
		{ "LE",			"RequireWeaponDrawn",	'i', &g_cfg.requireWeaponDrawn,	1.0f,	0.0f,	1.0f },
		{ "LE",			"Verbose",				'i', &g_cfg.verbose,			1.0f,	0.0f,	1.0f },
		{ "LE",			"SignalEnabled",		'i', &g_cfg.signalEnabled,		1.0f,	0.0f,	1.0f },
		{ "LE",			"SignalActorValue",		'i', &g_cfg.signalActorValue,	77.0f,	0.0f,	200.0f },
		{ "LE",			"Language",				'i', &g_cfg.language,			0.0f,	0.0f,	1.0f },
	};
	const int kNumOpts = sizeof(g_opts) / sizeof(g_opts[0]);

	char	g_iniPath[MAX_PATH] = { 0 };
	bool	g_inited = false;

	bool FileExists(const char * path)
	{
		const DWORD attrs = GetFileAttributesA(path);
		return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
	}

	float Clamp(const Opt * o, float v)
	{
		if(!(v == v)) v = o->def;
		if(v < o->lo) v = o->lo;
		if(v > o->hi) v = o->hi;
		return v;
	}

	void Store(const Opt * o, float v)
	{
		v = Clamp(o, v);
		if(o->type == 'i') *(int *)o->ptr = (int)(v + (v < 0 ? -0.5f : 0.5f));
		else			   *(float *)o->ptr = v;
	}

	float Value(const Opt * o)
	{
		return o->type == 'i' ? (float)*(int *)o->ptr : *(float *)o->ptr;
	}

	const Opt * Find(const char * key)
	{
		if(!key) return NULL;
		for(int i = 0; i < kNumOpts; ++i)
			if(_stricmp(g_opts[i].key, key) == 0) return &g_opts[i];
		return NULL;
	}

	void Defaults(bool keepLanguage)
	{
		for(int i = 0; i < kNumOpts; ++i)
		{
			if(keepLanguage && _stricmp(g_opts[i].key, "Language") == 0) continue;
			Store(&g_opts[i], g_opts[i].def);
		}
		_snprintf_s(g_cfg.deflectVariable, sizeof(g_cfg.deflectVariable), _TRUNCATE, "bNoStagger");
	}

	void FormatValue(const Opt * o, char * out, size_t n)
	{
		if(o->type == 'i') _snprintf_s(out, n, _TRUNCATE, "%d", *(int *)o->ptr);
		else			   _snprintf_s(out, n, _TRUNCATE, "%.4g", (double)*(float *)o->ptr);
	}

	// O .ini fica ao lado da DLL (funciona com o MO2 e o Vortex), como no FreeClimbLE.
	void ResolveIniPath()
	{
		g_iniPath[0] = 0;

		HMODULE self = NULL;
		if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&ResolveIniPath, &self))
			return;

		char dll[MAX_PATH] = { 0 };
		const DWORD n = GetModuleFileNameA(self, dll, MAX_PATH);
		if(n == 0 || n >= MAX_PATH) return;

		char * slash = strrchr(dll, '\\');
		if(!slash) return;
		slash[1] = 0;
		_snprintf_s(g_iniPath, sizeof(g_iniPath), _TRUNCATE, "%s%s", dll, kIniName);
	}

	// Le todas as chaves de um .ini (as que faltam ficam como estao). Devolve quantas encontrou.
	int ReadFrom(const char * path)
	{
		int known = 0;
		for(int i = 0; i < kNumOpts; ++i)
		{
			char buffer[64] = { 0 };
			GetPrivateProfileStringA(g_opts[i].section, g_opts[i].key, "", buffer, sizeof(buffer), path);
			if(!buffer[0]) continue;
			Store(&g_opts[i], (float)atof(buffer));
			++known;
		}

		char text[64] = { 0 };
		GetPrivateProfileStringA("LE", "DeflectVariable", "", text, sizeof(text), path);
		if(text[0]) _snprintf_s(g_cfg.deflectVariable, sizeof(g_cfg.deflectVariable), _TRUNCATE, "%s", text);
		return known;
	}

	bool WriteKey(const char * path, const Opt * o)
	{
		char text[64];
		FormatValue(o, text, sizeof(text));
		return WritePrivateProfileStringA(o->section, o->key, text, path) != 0;
	}

	void WriteAllTo(const char * path)
	{
		for(int i = 0; i < kNumOpts; ++i) WriteKey(path, &g_opts[i]);
		WritePrivateProfileStringA("LE", "DeflectVariable", g_cfg.deflectVariable, path);
	}

	bool UserIniPath(char * out, size_t n)
	{
		if(!g_iniPath[0]) return false;
		_snprintf_s(out, n, _TRUNCATE, "%s", g_iniPath);
		char * dot = strrchr(out, '.');
		if(!dot) return false;
		_snprintf_s(dot, n - (size_t)(dot - out), _TRUNCATE, "_User.ini");
		return true;
	}

	// ------------------------------------------------------------------ funcoes nativas (menu)
	void NoteMenuCall()
	{
		static bool noted = false;
		if(noted) return;
		noted = true;
		_MESSAGE("menu: the menu script called the DLL for the first time - Papyrus is running it");
	}

	void SetOpt(const char * key, float value)
	{
		const Opt * o = Find(key);
		if(!o) { _MESSAGE("menu: unknown option '%s'", key ? key : "(null)"); return; }
		Store(o, value);
		_MESSAGE("menu: %s = %g", o->key, (double)Value(o));
		if(g_iniPath[0]) WriteKey(g_iniPath, o);
	}

	bool	N_GetBool(StaticFunctionTag *, BSFixedString key)				{ NoteMenuCall(); const Opt * o = Find(key.data); return o ? Value(o) != 0.0f : false; }
	SInt32	N_GetInt(StaticFunctionTag *, BSFixedString key)				{ NoteMenuCall(); const Opt * o = Find(key.data); return o ? (SInt32)Value(o) : 0; }
	float	N_GetFloat(StaticFunctionTag *, BSFixedString key)				{ NoteMenuCall(); const Opt * o = Find(key.data); return o ? Value(o) : 0.0f; }
	void	N_SetBool(StaticFunctionTag *, BSFixedString key, bool value)	{ SetOpt(key.data, value ? 1.0f : 0.0f); }
	void	N_SetInt(StaticFunctionTag *, BSFixedString key, SInt32 value)	{ SetOpt(key.data, (float)value); }
	void	N_SetFloat(StaticFunctionTag *, BSFixedString key, float value)	{ SetOpt(key.data, value); }
	void	N_Reload(StaticFunctionTag *)									{ Options::Load(); }
	void	N_SaveAll(StaticFunctionTag *)									{ Options::SaveAll(); _MESSAGE("menu: all options saved to the ini"); }

	void N_ResetDefaults(StaticFunctionTag *)
	{
		Defaults(true);
		Options::SaveAll();
		_MESSAGE("menu: defaults restored");
	}

	void N_SaveProfile(StaticFunctionTag *)
	{
		char path[MAX_PATH];
		if(!UserIniPath(path, sizeof(path))) { _MESSAGE("menu: cannot save the user profile (no ini path)"); return; }
		WriteAllTo(path);
		_MESSAGE("menu: user profile saved to %s", path);
	}

	SInt32 N_LoadProfile(StaticFunctionTag *)
	{
		char path[MAX_PATH];
		if(!UserIniPath(path, sizeof(path)) || !FileExists(path)) { _MESSAGE("menu: no saved user profile"); return 0; }
		const int known = ReadFrom(path);
		Options::SaveAll();
		_MESSAGE("menu: user profile loaded (%d options)", known);
		return known > 0 ? 1 : 0;
	}

	// 0 = o sinal por valor de atributo funciona (1/0), 1 = estado do counter (0 parado, 1 pronto, 2 em curso)
	SInt32 N_Status(StaticFunctionTag *, SInt32 what)
	{
		NoteMenuCall();
		return CounterStatus(what);
	}

	// ------------------------------------------------------------------ eventos para o script do menu
	SKSEMessagingInterface *				g_messaging = NULL;
	EventDispatcher<SKSEModCallbackEvent> *	g_modEvents = NULL;
}

namespace Options
{
	void Load()
	{
		Defaults(true);
		const bool exists = g_iniPath[0] && FileExists(g_iniPath);
		const int known = exists ? ReadFrom(g_iniPath) : 0;
		_MESSAGE("options: ini %s (%s), %d options read", g_iniPath[0] ? g_iniPath : "(not found)", exists ? "found" : "missing - using defaults", known);
		_MESSAGE("options: window %.2f s, cooldown %.2f s, stamina %s %.1f, armed hold %.2f s, block grace %.2f s",
				 (double)g_cfg.windowSeconds, (double)g_cfg.cooldownSeconds, g_cfg.staminaOn ? "on" : "off", (double)g_cfg.staminaCost,
				 (double)g_cfg.armedHoldSeconds, (double)g_cfg.blockGraceSeconds);
		_MESSAGE("options: immunity %d, super armor %d, forced stagger %d (%.2f), counter damage %d%%, mark %d, ready sound %d (%d%%), swing sound %d (%d%%)",
				 g_cfg.immunityOn, g_cfg.superArmorOn, g_cfg.staggerOn, (double)g_cfg.staggerMagnitude, g_cfg.damagePercent,
				 g_cfg.markOn, g_cfg.readySoundOn, g_cfg.readySoundVolume, g_cfg.swingSoundOn, g_cfg.swingSoundVolume);
		_MESSAGE("options: deflect variable '%s', require blocking %d, require weapon drawn %d, verbose %d, signal %s (actor value %d)",
				 g_cfg.deflectVariable, g_cfg.requireBlocking, g_cfg.requireWeaponDrawn, g_cfg.verbose, g_cfg.signalEnabled ? "on" : "off", g_cfg.signalActorValue);
	}

	void Init()
	{
		if(g_inited) return;
		g_inited = true;
		ResolveIniPath();
		Load();
	}

	void SaveAll()
	{
		if(g_iniPath[0]) WriteAllTo(g_iniPath);
	}

	bool RegisterPapyrus(VMClassRegistry * registry)
	{
		registry->RegisterFunction(new NativeFunction1 <StaticFunctionTag, bool, BSFixedString>("GetBool", "GuardCountersNative", N_GetBool, registry));
		registry->RegisterFunction(new NativeFunction1 <StaticFunctionTag, SInt32, BSFixedString>("GetInt", "GuardCountersNative", N_GetInt, registry));
		registry->RegisterFunction(new NativeFunction1 <StaticFunctionTag, float, BSFixedString>("GetFloat", "GuardCountersNative", N_GetFloat, registry));
		registry->RegisterFunction(new NativeFunction2 <StaticFunctionTag, void, BSFixedString, bool>("SetBool", "GuardCountersNative", N_SetBool, registry));
		registry->RegisterFunction(new NativeFunction2 <StaticFunctionTag, void, BSFixedString, SInt32>("SetInt", "GuardCountersNative", N_SetInt, registry));
		registry->RegisterFunction(new NativeFunction2 <StaticFunctionTag, void, BSFixedString, float>("SetFloat", "GuardCountersNative", N_SetFloat, registry));
		registry->RegisterFunction(new NativeFunction0 <StaticFunctionTag, void>("Reload", "GuardCountersNative", N_Reload, registry));
		registry->RegisterFunction(new NativeFunction0 <StaticFunctionTag, void>("SaveAll", "GuardCountersNative", N_SaveAll, registry));
		registry->RegisterFunction(new NativeFunction0 <StaticFunctionTag, void>("SaveProfile", "GuardCountersNative", N_SaveProfile, registry));
		registry->RegisterFunction(new NativeFunction0 <StaticFunctionTag, SInt32>("LoadProfile", "GuardCountersNative", N_LoadProfile, registry));
		registry->RegisterFunction(new NativeFunction0 <StaticFunctionTag, void>("ResetDefaults", "GuardCountersNative", N_ResetDefaults, registry));
		registry->RegisterFunction(new NativeFunction1 <StaticFunctionTag, SInt32, SInt32>("Status", "GuardCountersNative", N_Status, registry));
		_MESSAGE("papyrus functions registered (GuardCountersNative)");
		return true;
	}

	void SetMessaging(SKSEMessagingInterface * messaging) { g_messaging = messaging; }

	// Evento "GuardCounters_Event": 'what' = ready | swing | expire | end | hit. O script do menu trata-o.
	void SendEvent(const char * what, float value, TESForm * sender)
	{
		if(!g_modEvents && g_messaging)
			g_modEvents = (EventDispatcher<SKSEModCallbackEvent> *)g_messaging->GetEventDispatcher(SKSEMessagingInterface::kDispatcher_ModEvent);
		if(!g_modEvents) return;

		SKSEModCallbackEvent evn(BSFixedString("GuardCounters_Event"), BSFixedString(what), value, sender);
		g_modEvents->SendEvent(&evn);

		static int seen = 0;
		if(seen < 3) { ++seen; _MESSAGE("event: '%s' sent to the menu script (if nothing happens, check the Papyrus log for 'GuardCountersMCM')", what); }
		else if(g_cfg.verbose) _MESSAGE("event: %s", what);
	}
}
