// Counter.cpp - Guard Counters LE, versao 0.3 (primeira fase de efeitos).
//
// O que faz (so MCO, so deflect do Inpa Sekiro):
//   1. Deteta um deflect bem-sucedido do Inpa Sekiro: a variavel de animacao "bNoStagger" do jogador sobe a TRUE
//      (script zzzSekiro_player_parry3), com o jogador a bloquear ("isBlocking") no ultimo instante.
//   2. Abre a janela de counter (WindowSeconds) e SINALIZA-A:
//        - variavel de grafo GuardCounters_Trigger = 1 (BDI; fica por compatibilidade);
//        - valor de atributo do jogador (SignalActorValue, por omissao Variable10) a 1: o Themis (OAR/DAR do LE)
//          nao avalia variaveis de grafo mas avalia atributos, e as animacoes DAR de counter pedem
//          IsActorValueEqualTo(<atributo>, 1).
//      Envia o evento "ready" ao script do menu (som de pronto e marca perilous no jogador).
//   3. Se o jogador atacar dentro da janela ("Right Attack/Block"): consome o counter, cobra a estamina e envia
//      "swing" (som do golpe, imunidade total com SetGhost). Liga a super armor (sem stagger no jogador).
//   4. Enquanto o golpe dura, cada inimigo que o jogador atinge leva o stagger forcado (animacao) e, se
//      CounterDamagePercent != 100, o script aplica o dano extra ("hit").
//   5. Fecha tudo ao acabar o golpe, ao expirar a janela ou por timeout ("end" / "expire").
//
// Tudo corre a partir do receptor de teclas do SKSE (como no FreeClimbLE) e do receptor de hits (TESHitEvent).

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "Engine.h"
#include "Counter.h"
#include "Options.h"
#include "Log.h"

#include "skse/GameInput.h"
#include "skse/GameEvents.h"
#include "skse/GameReferences.h"

namespace
{
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
	int		g_signalMod = -1;				// modificador do atributo que funcionou no auto-teste (-1 = ainda nao testado / falhou)
	bool	g_signalOn = false;				// o sinal esta a 1 por nossa causa
	bool	g_signalTested = false;
	bool	g_armorSet = false;				// mexemos em bNoStagger (super armor)
	bool	g_armorPrev = false;			// valor que bNoStagger tinha antes
	UInt32	g_hitIds[8];					// inimigos ja atingidos por este counter
	int		g_hitCount = 0;

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

	// ---- sinal por valor de atributo (o que o Themis avalia)
	// ActorValueOwner no actor + 0x60, slot 6 = modificador (como o DamageActorValue do Engine.h):
	// modifier 0 = permanente, 1 = temporario, 2 = dano.
	void ApplyModifier(Actor * actor, UInt32 modifier, UInt32 av, float delta)
	{
		if(!Engine::Readable(actor, 0x64)) return;
		void * owner = (UInt8 *)actor + 0x60;
		void ** vtable = *(void ***)owner;
		if(!Engine::Readable(vtable, 7 * sizeof(void *))) return;

		void (__thiscall * fn)(void *, UInt32, UInt32, float) = (void (__thiscall *)(void *, UInt32, UInt32, float))vtable[6];
		fn(owner, modifier, av, delta);
	}

	// Testa que o atributo e escrevivel e descobre que modificador funciona (temporario, dano ou permanente).
	// Se encontrar o atributo a 1 (um sinal esquecido num save), limpa-o.
	void SelfTestSignal(Actor * player)
	{
		g_signalMod = -1;
		g_signalOn = false;
		if(!g_cfg.signalEnabled) return;

		const UInt32 av = (UInt32)g_cfg.signalActorValue;
		static const UInt32 order[3] = { 1, 2, 0 };
		float base = Engine::GetActorValue(player, av);
		_MESSAGE("counter: signal self-test - actor value %u reads %.2f", (unsigned)av, (double)base);

		if(fabsf(base - 1.0f) < 0.01f)
		{
			for(int i = 0; i < 3 && fabsf(base) > 0.01f; ++i)
			{
				ApplyModifier(player, order[i], av, -1.0f);
				base = Engine::GetActorValue(player, av);
				_MESSAGE("counter: signal self-test - clearing a stale signal with modifier %u -> reads %.2f", (unsigned)order[i], (double)base);
			}
		}

		if(fabsf(base) > 0.01f)
		{
			_MESSAGE("counter: signal DISABLED - actor value %u is %.2f, not 0 (another mod may use it). Pick another SignalActorValue in the ini and regenerate the DAR conditions.", (unsigned)av, (double)base);
			return;
		}

		for(int i = 0; i < 3; ++i)
		{
			const UInt32 m = order[i];
			ApplyModifier(player, m, av, 1.0f);
			const float up = Engine::GetActorValue(player, av);
			ApplyModifier(player, m, av, -1.0f);
			float down = Engine::GetActorValue(player, av);

			int guard = 0;
			while(down > base + 0.01f && guard++ < 3)
			{
				ApplyModifier(player, m, av, -1.0f);
				down = Engine::GetActorValue(player, av);
			}

			_MESSAGE("counter: signal self-test - modifier %u: %.2f -> %.2f -> %.2f", (unsigned)m, (double)base, (double)up, (double)down);

			if(fabsf(up - (base + 1.0f)) < 0.01f && fabsf(down - base) < 0.01f)
			{
				g_signalMod = (int)m;
				_MESSAGE("counter: signal OK - using modifier %u on actor value %u", (unsigned)m, (unsigned)av);
				return;
			}
		}

		_MESSAGE("counter: signal DISABLED - actor value %u could not be written with any modifier; the Themis DAR counters will not trigger", (unsigned)av);
	}

	void SetSignal(Actor * player, bool on, const char * why)
	{
		if(!g_cfg.signalEnabled || g_signalMod < 0 || on == g_signalOn) return;

		const UInt32 av = (UInt32)g_cfg.signalActorValue;
		ApplyModifier(player, (UInt32)g_signalMod, av, on ? 1.0f : -1.0f);
		g_signalOn = on;
		_MESSAGE("counter: signal actor value %u -> %d (%s), reads %.2f", (unsigned)av, on ? 1 : 0, why, (double)Engine::GetActorValue(player, av));
	}

	// Abre/fecha a janela para o jogo: variavel de grafo (BDI) + valor de atributo (Themis DAR).
	void SetTrigger(Actor * player, int value, const char * why)
	{
		const bool ok = Engine::SetGraphVariableInt((TESObjectREFR *)player, BSFixedString("GuardCounters_Trigger"), value);
		if(g_cfg.verbose)
			_MESSAGE("counter: GuardCounters_Trigger (graph variable) = %d (%s) -> %s", value, why,
					 ok ? "ok" : "not found (harmless: the Themis DAR counters use the actor-value signal)");
		SetSignal(player, value != 0, why);
	}

	// ---- super armor: o jogador nao leva stagger durante o golpe (bNoStagger a TRUE, como o Inpa no deflect)
	bool DeflectVarIsNoStagger() { return _stricmp(g_cfg.deflectVariable, "bNoStagger") == 0; }

	void ArmorOn(Actor * player)
	{
		if(!g_cfg.superArmorOn || g_armorSet) return;

		bool current = false;
		ReadGraphBool(player, "bNoStagger", current);
		g_armorPrev = current;
		g_armorSet = true;
		if(!current)
		{
			Engine::SetGraphVariableBool((TESObjectREFR *)player, BSFixedString("bNoStagger"), true);
			if(DeflectVarIsNoStagger()) g_prevDeflect = true;		// nao e um deflect novo
			if(g_cfg.verbose) _MESSAGE("counter: super armor on (bNoStagger = TRUE)");
		}
	}

	void ArmorOff(Actor * player)
	{
		if(!g_armorSet) return;
		g_armorSet = false;
		if(!g_armorPrev)
		{
			Engine::SetGraphVariableBool((TESObjectREFR *)player, BSFixedString("bNoStagger"), false);
			if(DeflectVarIsNoStagger()) g_prevDeflect = false;
			if(g_cfg.verbose) _MESSAGE("counter: super armor off (bNoStagger = false)");
		}
	}

	// ---- janela
	void Open(Actor * player, DWORD now, DWORD sinceBlocking)
	{
		SetTrigger(player, 1, "window opened");
		g_state = kReady;
		g_stateSince = now;
		g_attackSeen = false;
		_MESSAGE("counter: deflect detected (%u ms after blocking) - counter ready for %.2f s", (unsigned)sinceBlocking, (double)g_cfg.windowSeconds);
		Options::SendEvent("ready", 0.0f, NULL);
	}

	// Fecha: a janela expirou sem ataque ('swung' = false) ou o golpe acabou ('swung' = true).
	void Close(Actor * player, const char * why, bool swung)
	{
		SetTrigger(player, 0, why);
		if(swung) ArmorOff(player);
		g_state = kIdle;
		g_attackSeen = false;
		Options::SendEvent(swung ? "end" : "expire", 0.0f, NULL);
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
				Close(player, "window expired without an attack", false);
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
				Close(player, "the counter attack finished", true);
			else if(now - g_stateSince > (DWORD)(g_cfg.armedHoldSeconds * 1000.0f))
				Close(player, g_attackSeen ? "hold timeout (attack still running)" : "hold timeout (the attack never started)", true);
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
		g_hitCount = 0;

		g_cooldownStart = now;
		g_cooldownMs = (DWORD)(g_cfg.cooldownSeconds * 1000.0f);

		if(g_cfg.staminaOn && g_cfg.staminaCost > 0.0f)
		{
			const float current = Engine::GetActorValue(player, Engine::kActorValue_Stamina);
			const float cost = g_cfg.staminaCost < current ? g_cfg.staminaCost : current;
			if(cost > 0.0f) Engine::DamageActorValue(player, Engine::kActorValue_Stamina, cost);
			_MESSAGE("counter: stamina %.1f -> %.1f (cost %.1f)", (double)current, (double)(current - cost), (double)cost);
		}

		ArmorOn(player);
		Options::SendEvent("swing", 0.0f, NULL);		// o script toca o som do golpe e liga a imunidade
	}

	void OnInput(bool attackPressed)
	{
		Actor * player = ThePlayer();

		if(!player || !Engine::ActorIsBuilt(player) || Engine::LifeState(player) != Engine::kLifeState_Alive)
		{
			// o jogador ainda esta a ser construido, ou morreu: nada a perguntar ao jogo
			if(g_state != kIdle) Options::SendEvent("end", 0.0f, NULL);
			g_state = kIdle;
			g_armorSet = false;
			g_primed = false;
			return;
		}

		if(!g_signalTested)
		{
			g_signalTested = true;
			SelfTestSignal(player);
		}

		Poll(player);
		if(attackPressed) OnAttackPressed(player);
	}

	// ------------------------------------------------------------------ hits do jogador durante o counter
	void ApplyStagger(Actor * victim)
	{
		TESObjectREFR * ref = (TESObjectREFR *)victim;
		const bool a = Engine::SetGraphVariableFloat(ref, BSFixedString("staggerDirection"), 0.0f);
		const bool b = Engine::SetGraphVariableFloat(ref, BSFixedString("staggerMagnitude"), g_cfg.staggerMagnitude);
		const bool c = Engine::NotifyAnimationGraph(ref, BSFixedString("staggerStart"));
		_MESSAGE("counter: forced stagger on %08X (magnitude %.2f): direction %s, magnitude %s, event %s",
				 (unsigned)victim->formID, (double)g_cfg.staggerMagnitude, a ? "ok" : "FAILED", b ? "ok" : "FAILED", c ? "sent" : "FAILED");
	}

	void OnCounterHit(TESHitEvent * evn)
	{
		Actor * player = ThePlayer();
		if(!player || !evn->caster || evn->caster != (TESObjectREFR *)player) return;
		if(!evn->target || evn->target == (TESObjectREFR *)player) return;
		if(evn->target->formType != kFormType_Character) return;
		if(evn->flags & TESHitEvent::kFlag_Bash) return;

		Actor * victim = (Actor *)evn->target;
		if(Engine::LifeState(victim) != Engine::kLifeState_Alive) return;

		for(int i = 0; i < g_hitCount; ++i)
			if(g_hitIds[i] == victim->formID) return;			// um so efeito por inimigo e por counter
		if(g_hitCount < 8) g_hitIds[g_hitCount++] = victim->formID;

		_MESSAGE("counter: the counter hit %08X (flags %u)", (unsigned)victim->formID, (unsigned)evn->flags);

		if(g_cfg.staggerOn) ApplyStagger(victim);
		if(g_cfg.damagePercent != 100) Options::SendEvent("hit", (float)g_cfg.damagePercent, (TESForm *)victim);
	}

	class HitSink : public BSTEventSink<TESHitEvent>
	{
	public:
		virtual EventResult ReceiveEvent(TESHitEvent * evn, EventDispatcher<TESHitEvent> * dispatcher)
		{
			if(evn && g_state == kConsumed) OnCounterHit(evn);
			return kEvent_Continue;
		}
	};
	HitSink g_hitSink;

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
	Options::Init();
}

void CounterRegisterInput()
{
	InputEventDispatcher * dispatcher = *g_inputEventDispatcher;
	if(!dispatcher) { _MESSAGE("counter: input dispatcher not available - the counter will not work"); return; }
	dispatcher->AddEventSink(&g_sink);
	_MESSAGE("counter: input receiver registered - block with Inpa Sekiro, then attack");

	if(g_hitEventDispatcher)
	{
		g_hitEventDispatcher->AddEventSink(&g_hitSink);
		_MESSAGE("counter: hit receiver registered (forced stagger and counter damage)");
	}
	else _MESSAGE("counter: hit dispatcher not available - no forced stagger / counter damage");
}

void CounterReset()
{
	Engine::ForgetMemoryLayout();

	Actor * player = ThePlayer();
	if(g_state != kIdle)
	{
		if(player && Engine::ActorIsBuilt(player)) { SetTrigger(player, 0, "reset (load / new game)"); ArmorOff(player); }
		Options::SendEvent("end", 0.0f, NULL);
	}

	g_state = kIdle;
	g_primed = false;
	g_prevDeflect = false;
	g_lastBlocking = 0;
	g_attackSeen = false;
	g_cooldownMs = 0;
	g_signalOn = false;
	g_signalTested = false;			// repetir o auto-teste no proximo evento de input (limpa um sinal esquecido no save)
	g_armorSet = false;
	g_hitCount = 0;
	_MESSAGE("counter: state reset");
}

// 0 = o sinal por valor de atributo funciona (1/0), 1 = estado do counter (0 parado, 1 pronto, 2 em curso)
int CounterStatus(int what)
{
	if(what == 0) return (g_cfg.signalEnabled && g_signalMod >= 0) ? 1 : 0;
	if(what == 1) return (int)g_state;
	return 0;
}
