// Counter.cpp - Guard Counters LE, versao 0.1.
//
// O que faz (so MCO, so deflect do Inpa Sekiro):
//   1. Deteta um deflect bem-sucedido do Inpa Sekiro. O Inpa (script zzzSekiro_player_parry3) poe a
//      variavel de animacao "bNoStagger" do jogador a TRUE quando o deflect resulta, e volta a poe-la
//      a FALSE passado 1,5 a 4,2 s. Esse flanco (false -> true) e o nosso gatilho. Para nao confundir
//      com outros mods que usam bNoStagger, exige-se que o jogador estivesse a bloquear ("isBlocking")
//      no ultimo segundo.
//   2. Abre a janela de counter (WindowSeconds): poe a variavel de grafo GuardCounters_Trigger a 1.
//      E ela que o Behavior Data Injector (GuardCounters_BDI.json) cria e o Open Animation Replacer
//      usa nas condicoes das animacoes de counter.
//   3. Se o jogador atacar dentro da janela ("Right Attack/Block"), consome o counter: cobra a estamina
//      e mantem a variavel a 1 ate o golpe comecar (e acabar), para o OAR escolher a animacao de counter.
//   4. Fecha a janela (variavel a 0) ao expirar, ou quando o golpe termina.
//
// Tudo corre a partir do receptor de teclas do SKSE (o mesmo metodo do FreeClimbLE), por isso a leitura
// do estado acontece sempre que chega um evento de input (rato, teclas, comando).
//
// Fica para versoes seguintes (precisam de hooks de dano/golpe que nao se podem escrever sem o jogo):
// invencibilidade, stagger forcado do inimigo, brilho, sons e menu proprio. Super armor: ja a da o
// proprio Inpa, que mantem bNoStagger a TRUE durante 1,5 a 4,2 s apos o deflect.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "Engine.h"
#include "Counter.h"
#include "Log.h"

#include "skse/GameInput.h"
#include "skse/GameReferences.h"

namespace
{
	// ------------------------------------------------------------------ afinacoes (GuardCounters.ini)
	struct Config
	{
		float	windowSeconds;			// [Counter] WindowSeconds - quanto tempo o counter fica disponivel apos o deflect
		float	cooldownSeconds;		// [Counter] CooldownSeconds - pausa apos usar um counter
		int		staminaOn;				// [Combat] StaminaCostEnabled
		float	staminaCost;			// [Combat] StaminaCost - pontos de estamina
		float	armedHoldSeconds;		// [LE] ArmedHoldSeconds - maximo que a variavel fica a 1 depois de atacar
		float	blockGraceSeconds;		// [LE] BlockGraceSeconds - o jogador tinha de estar a bloquear ha menos de isto
		int		requireBlocking;		// [LE] RequireBlocking
		int		requireWeaponDrawn;		// [LE] RequireWeaponDrawn
		int		verbose;				// [LE] Verbose - regista cada flanco/estado (ligado nesta versao de teste)
		char	deflectVariable[64];	// [LE] DeflectVariable - variavel de grafo que o Inpa poe a TRUE no deflect
	};

	Config	g_cfg;
	char	g_iniPath[MAX_PATH];

	bool FileExists(const char * path)
	{
		const DWORD attrs = GetFileAttributesA(path);
		return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
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
		_snprintf_s(g_iniPath, sizeof(g_iniPath), _TRUNCATE, "%sGuardCounters.ini", dll);
	}

	float ReadFloat(const char * section, const char * key, float fallback, float lo, float hi)
	{
		char text[64];
		_snprintf_s(text, sizeof(text), _TRUNCATE, "%g", (double)fallback);

		char buffer[64] = { 0 };
		GetPrivateProfileStringA(section, key, text, buffer, sizeof(buffer), g_iniPath);

		float value = (float)atof(buffer);
		if(!(value >= lo)) value = lo;		// tambem apanha NaN
		if(value > hi) value = hi;
		return value;
	}

	int ReadInt(const char * section, const char * key, int fallback)
	{
		return (int)GetPrivateProfileIntA(section, key, fallback, g_iniPath);
	}

	void LoadConfig()
	{
		ResolveIniPath();

		g_cfg.windowSeconds = ReadFloat("Counter", "WindowSeconds", 1.5f, 0.1f, 5.0f);
		g_cfg.cooldownSeconds = ReadFloat("Counter", "CooldownSeconds", 0.0f, 0.0f, 30.0f);
		g_cfg.staminaOn = ReadInt("Combat", "StaminaCostEnabled", 1);
		g_cfg.staminaCost = ReadFloat("Combat", "StaminaCost", 20.0f, 0.0f, 500.0f);
		g_cfg.armedHoldSeconds = ReadFloat("LE", "ArmedHoldSeconds", 1.5f, 0.2f, 5.0f);
		g_cfg.blockGraceSeconds = ReadFloat("LE", "BlockGraceSeconds", 1.0f, 0.1f, 5.0f);
		g_cfg.requireBlocking = ReadInt("LE", "RequireBlocking", 1);
		g_cfg.requireWeaponDrawn = ReadInt("LE", "RequireWeaponDrawn", 1);
		g_cfg.verbose = ReadInt("LE", "Verbose", 1);

		GetPrivateProfileStringA("LE", "DeflectVariable", "bNoStagger", g_cfg.deflectVariable, sizeof(g_cfg.deflectVariable), g_iniPath);
		if(!g_cfg.deflectVariable[0]) _snprintf_s(g_cfg.deflectVariable, sizeof(g_cfg.deflectVariable), _TRUNCATE, "bNoStagger");

		_MESSAGE("counter: ini %s (%s)", g_iniPath[0] ? g_iniPath : "(not found)", g_iniPath[0] && FileExists(g_iniPath) ? "found" : "missing - using defaults");
		_MESSAGE("counter: window %.2f s, cooldown %.2f s, stamina %s %.1f, armed hold %.2f s, block grace %.2f s",
				 (double)g_cfg.windowSeconds, (double)g_cfg.cooldownSeconds, g_cfg.staminaOn ? "on" : "off", (double)g_cfg.staminaCost,
				 (double)g_cfg.armedHoldSeconds, (double)g_cfg.blockGraceSeconds);
		_MESSAGE("counter: deflect variable '%s', require blocking %d, require weapon drawn %d, verbose %d",
				 g_cfg.deflectVariable, g_cfg.requireBlocking, g_cfg.requireWeaponDrawn, g_cfg.verbose);
	}

	// ------------------------------------------------------------------ estado
	enum State { kIdle = 0, kReady = 1, kConsumed = 2 };

	State	g_state = kIdle;
	DWORD	g_stateSince = 0;
	DWORD	g_cooldownStart = 0;
	DWORD	g_cooldownMs = 0;
	DWORD	g_lastBlocking = 0;				// ultimo instante com isBlocking a TRUE (0 = nunca)
	bool	g_primed = false;				// ja se leu a variavel do deflect uma vez (evita um falso flanco ao carregar)
	bool	g_prevDeflect = false;
	bool	g_attackSeen = false;			// depois de consumir: o golpe ja comecou
	bool	g_blockReadable = false;
	bool	g_firstPollLogged = false;
	bool	g_warnedDeflect = false;
	bool	g_warnedBlocking = false;

	const char * StateName(int s)
	{
		return s == kReady ? "ready" : s == kConsumed ? "consumed" : "idle";
	}

	bool CooldownActive()
	{
		return g_cooldownMs != 0 && (GetTickCount() - g_cooldownStart) < g_cooldownMs;
	}

	Actor * ThePlayer() { return (Actor *)Engine::Player(); }

	bool ReadGraphBool(Actor * player, const char * name, bool & out)
	{
		return Engine::GetGraphVariableBool((TESObjectREFR *)player, BSFixedString(name), out);
	}

	// GuardCounters_Trigger: a variavel que o BDI cria no grafo do jogador e o OAR le nas condicoes.
	void SetTrigger(Actor * player, int value, const char * why)
	{
		const bool ok = Engine::SetGraphVariableInt((TESObjectREFR *)player, BSFixedString("GuardCounters_Trigger"), value);
		_MESSAGE("counter: GuardCounters_Trigger = %d (%s) -> %s", value, why,
				 ok ? "ok" : "FAILED (is GuardCounters_BDI.json in SKSE\\Plugins\\BehaviorDataInjector, and the BDI for LE installed?)");
	}

	void Open(Actor * player, DWORD now, DWORD sinceBlocking)
	{
		SetTrigger(player, 1, "window opened");
		g_state = kReady;
		g_stateSince = now;
		g_attackSeen = false;
		_MESSAGE("counter: deflect detected (%u ms after blocking) - counter ready for %.2f s", (unsigned)sinceBlocking, (double)g_cfg.windowSeconds);
	}

	void Close(Actor * player, const char * why)
	{
		SetTrigger(player, 0, why);
		g_state = kIdle;
		g_attackSeen = false;
	}

	// Chamado a cada evento de input: le o estado do jogo e avanca a maquina de estados.
	void Poll(Actor * player)
	{
		const DWORD now = GetTickCount();

		// 1. o jogador esta a bloquear? (so conta como deflect se acabou de bloquear)
		bool blocking = false;
		if(ReadGraphBool(player, "isBlocking", blocking))
		{
			g_blockReadable = true;
			if(blocking) g_lastBlocking = now;
		}
		else if(g_cfg.requireBlocking && !g_warnedBlocking)
		{
			g_warnedBlocking = true;
			_MESSAGE("counter: WARNING - could not read the graph variable 'isBlocking'; the 'was blocking' requirement is ignored");
		}

		// 2. a variavel que o Inpa poe a TRUE no deflect
		bool deflect = false;
		const bool haveDeflect = ReadGraphBool(player, g_cfg.deflectVariable, deflect);
		if(!haveDeflect && !g_warnedDeflect)
		{
			g_warnedDeflect = true;
			_MESSAGE("counter: WARNING - could not read the graph variable '%s'; no deflect can be detected until it can", g_cfg.deflectVariable);
		}

		if(!g_firstPollLogged && (haveDeflect || g_blockReadable))
		{
			g_firstPollLogged = true;
			_MESSAGE("counter: first read - '%s' %s (value %d), 'isBlocking' %s (value %d)",
					 g_cfg.deflectVariable, haveDeflect ? "readable" : "NOT readable", (int)deflect,
					 g_blockReadable ? "readable" : "NOT readable", (int)blocking);
		}

		const bool edge = haveDeflect && g_primed && deflect && !g_prevDeflect;
		if(haveDeflect)
		{
			if(g_cfg.verbose && g_primed && deflect != g_prevDeflect)
				_MESSAGE("counter: '%s' -> %s (state %s, blocking %d)", g_cfg.deflectVariable, deflect ? "TRUE" : "false", StateName(g_state), (int)blocking);
			g_prevDeflect = deflect;
			g_primed = true;
		}

		// 3. um deflect novo
		if(edge)
		{
			const DWORD sinceBlocking = g_lastBlocking ? (now - g_lastBlocking) : 0xFFFFFFFF;

			bool blockOk = true;
			if(g_cfg.requireBlocking && g_blockReadable)
				blockOk = g_lastBlocking != 0 && sinceBlocking <= (DWORD)(g_cfg.blockGraceSeconds * 1000.0f);

			if(g_state != kIdle)
				_MESSAGE("counter: deflect ignored - a counter is already %s", StateName(g_state));
			else if(CooldownActive())
				_MESSAGE("counter: deflect ignored - cooldown");
			else if(!blockOk)
				_MESSAGE("counter: '%s' went TRUE but the player was not blocking in the last %.2f s - ignored (not an Inpa deflect)",
						 g_cfg.deflectVariable, (double)g_cfg.blockGraceSeconds);
			else if(g_cfg.requireWeaponDrawn && Engine::WeaponState(player) != Engine::kWeaponState_Drawn)
				_MESSAGE("counter: deflect ignored - weapon not drawn");
			else
				Open(player, now, sinceBlocking);
		}

		// 4. a janela / o golpe em curso
		if(g_state == kReady)
		{
			if(now - g_stateSince > (DWORD)(g_cfg.windowSeconds * 1000.0f))
				Close(player, "window expired without an attack");
		}
		else if(g_state == kConsumed)
		{
			const UInt32 attack = Engine::AttackState(player);
			if(attack != 0 && !g_attackSeen)
			{
				g_attackSeen = true;
				if(g_cfg.verbose) _MESSAGE("counter: the attack started (attack state %u) %u ms after the press", (unsigned)attack, (unsigned)(now - g_stateSince));
			}

			if(g_attackSeen && attack == 0)
				Close(player, "the counter attack finished");
			else if(now - g_stateSince > (DWORD)(g_cfg.armedHoldSeconds * 1000.0f))
				Close(player, g_attackSeen ? "hold timeout (attack still running)" : "hold timeout (the attack never started)");
		}
	}

	// O jogador carregou em atacar. Se ha um counter pronto, consome-o.
	void OnAttackPressed(Actor * player)
	{
		if(g_state != kReady) return;

		const DWORD now = GetTickCount();
		_MESSAGE("counter: attack pressed %u ms after the window opened - counter consumed", (unsigned)(now - g_stateSince));

		g_state = kConsumed;
		g_stateSince = now;
		g_attackSeen = false;

		g_cooldownStart = now;
		g_cooldownMs = (DWORD)(g_cfg.cooldownSeconds * 1000.0f);

		if(g_cfg.staminaOn && g_cfg.staminaCost > 0.0f)
		{
			const float current = Engine::GetActorValue(player, Engine::kActorValue_Stamina);
			const float cost = g_cfg.staminaCost < current ? g_cfg.staminaCost : current;
			if(cost > 0.0f) Engine::DamageActorValue(player, Engine::kActorValue_Stamina, cost);
			_MESSAGE("counter: stamina %.1f -> %.1f (cost %.1f)", (double)current, (double)(current - cost), (double)cost);
		}
	}

	void OnInput(bool attackPressed)
	{
		Actor * player = ThePlayer();

		if(!player || !Engine::ActorIsBuilt(player) || Engine::LifeState(player) != Engine::kLifeState_Alive)
		{
			// o jogador ainda esta a ser construido, ou morreu: nada a perguntar ao jogo
			g_state = kIdle;
			g_primed = false;
			return;
		}

		Poll(player);
		if(attackPressed) OnAttackPressed(player);
	}

	// ------------------------------------------------------------------ teclas
	class InputSink : public BSTEventSink<InputEvent>
	{
	public:
		virtual EventResult ReceiveEvent(InputEvent ** events, InputEventDispatcher * dispatcher)
		{
			bool attack = false;

			if(events && *events)
			{
				for(InputEvent * e = *events; e; e = e->next)
				{
					if(e->eventType != InputEvent::kEventType_Button) continue;

					ButtonEvent * button = (ButtonEvent *)e;
					const BSFixedString * control = button->GetControlID();
					if(!control || !control->data) continue;

					const float value = *(float *)&button->flags;
					const bool pressedNow = (value != 0.0f) && (button->timer == 0.0f);
					if(pressedNow && strcmp(control->data, "Right Attack/Block") == 0) attack = true;
				}
			}

			OnInput(attack);
			return kEvent_Continue;
		}
	};
	InputSink g_sink;
}

void CounterInit()
{
	LoadConfig();
}

void CounterRegisterInput()
{
	InputEventDispatcher * dispatcher = *g_inputEventDispatcher;
	if(!dispatcher) { _MESSAGE("counter: input dispatcher not available - the counter will not work"); return; }
	dispatcher->AddEventSink(&g_sink);
	_MESSAGE("counter: input receiver registered - block with Inpa Sekiro, then attack");
}

void CounterReset()
{
	Engine::ForgetMemoryLayout();

	Actor * player = ThePlayer();
	if(g_state != kIdle && player && Engine::ActorIsBuilt(player))
		SetTrigger(player, 0, "reset (load / new game)");

	g_state = kIdle;
	g_primed = false;
	g_prevDeflect = false;
	g_lastBlocking = 0;
	g_attackSeen = false;
	g_cooldownMs = 0;
	_MESSAGE("counter: state reset");
}
