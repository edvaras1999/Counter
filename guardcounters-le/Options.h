// Options.h - opcoes do Guard Counters LE (GuardCounters.ini), funcoes nativas do menu (GuardCountersNative)
// e envio de eventos (ModEvent "GuardCounters_Event") para o script do menu, que toca os sons e mostra a marca.
#pragma once

struct SKSEMessagingInterface;
class VMClassRegistry;
class TESForm;

struct GCConfig
{
	// [Counter]
	float	windowSeconds;			// tempo para atacar depois do deflect
	float	cooldownSeconds;		// pausa depois de usar um counter
	// [Combat]
	int		staminaOn;
	float	staminaCost;
	int		immunityOn;				// imunidade total ao dano durante o golpe (SetGhost, como o Inpa)
	int		superArmorOn;			// sem stagger no jogador durante o golpe
	int		immunityFromDeflect;	// a imunidade comeca no deflect (1) ou so quando o golpe de counter comeca (0)
	int		staggerOn;				// stagger forcado no inimigo atingido
	int		staggerDeflectedOn;		// stagger forcado tambem no inimigo do deflect, mesmo que o golpe nao o atinja
	int		staggerDelayMs;			// quanto depois do golpe comecar (inimigo do deflect)
	float	staggerMagnitude;		// 0.2 pequeno ... 1.0 maximo
	int		damagePercent;			// dano do counter em % (100 = normal)
	int		slowTimeOn;				// abrandar o tempo durante o golpe de counter (opcional)
	float	slowTimeSeconds;		// quanto tempo dura o abrandamento
	// [Feedback]
	int		markOn;					// marca perilous (kanji) no jogador enquanto o counter esta pronto
	int		readySoundOn;
	int		readySoundVolume;		// 0-200 %
	int		swingSoundOn;
	int		swingSoundVolume;
	// [LE]
	float	armedHoldSeconds;
	float	minSwingSeconds;		// o golpe de counter conta como ativo pelo menos este tempo
	float	blockGraceSeconds;
	int		requireBlocking;
	int		requireWeaponDrawn;
	int		verbose;
	int		signalEnabled;
	int		signalActorValue;
	int		language;				// 0 = English, 1 = Portugues (menu)
	char	deflectVariable[64];
};

extern GCConfig g_cfg;

namespace Options
{
	void Init();									// resolve o .ini e le tudo (chamar quando os dados do jogo carregarem)
	void Load();
	void SaveAll();
	bool RegisterPapyrus(VMClassRegistry * registry);
	void SetMessaging(SKSEMessagingInterface * messaging);
	void SendEvent(const char * what, float value, TESForm * sender);	// ModEvent "GuardCounters_Event"
}
