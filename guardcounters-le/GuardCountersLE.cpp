// Guard Counters LE - backport do mod "Guard Counters for BFCO and MCO" (SE) para o Skyrim LE.
//
// A DLL original (SE) nao tem codigo-fonte publicado: este plugin e uma reescrita do COMPORTAMENTO descrito
// na pagina do mod, para o SKSE 1.7.3 (Skyrim LE 1.9.32). As animacoes e os sons sao de black364 e dTry.
//
// Esta versao: so MCO, com o Inpa Sekiro como mod de bloqueio. Ver Counter.cpp.

#include "skse/PluginAPI.h"
#include "skse/skse_version.h"
#include "Log.h"
#include "Counter.h"
#include "Options.h"

#include <shlobj.h>

#define PLUGIN_NAME				"GuardCountersLE"
#define PLUGIN_VERSION_STRING	"0.4.0 (protecao desde o deflect, stagger forcado, marca propria, abrandar o tempo)"
#define PLUGIN_LOG_FILE			"\\My Games\\Skyrim\\SKSE\\GuardCountersLE.log"

static PluginHandle				g_pluginHandle	= kPluginHandle_Invalid;
static SKSEMessagingInterface *	g_messaging		= NULL;

namespace
{
	void OnMessage(SKSEMessagingInterface::Message * message)
	{
		if(!message) return;

		switch(message->type)
		{
			case SKSEMessagingInterface::kMessage_DataLoaded:
				_MESSAGE("message: DataLoaded - the game's data is ready");
				CounterInit();
				break;

			case SKSEMessagingInterface::kMessage_InputLoaded:
				_MESSAGE("message: InputLoaded");
				CounterRegisterInput();
				break;

			case SKSEMessagingInterface::kMessage_PostLoadGame:
				_MESSAGE("message: a save was loaded");
				CounterReset();
				break;

			case SKSEMessagingInterface::kMessage_NewGame:
				_MESSAGE("message: new game");
				CounterReset();
				break;

			default:
				break;
		}
	}
}

extern "C"
{

bool SKSEPlugin_Query(const SKSEInterface * skse, PluginInfo * info)
{
	gLog.OpenRelative(CSIDL_MYDOCUMENTS, PLUGIN_LOG_FILE);

	_MESSAGE("Guard Counters LE " PLUGIN_VERSION_STRING " (built " __DATE__ " " __TIME__ ")");

	info->infoVersion	= PluginInfo::kInfoVersion;
	info->name			= PLUGIN_NAME;
	info->version		= 1;

	g_pluginHandle = skse->GetPluginHandle();

	if(skse->isEditor)
	{
		_MESSAGE("ABORTING: loaded in the editor");
		return false;
	}

	_MESSAGE("game runtime %08X (1.9.32.0 is %08X)", skse->runtimeVersion, RUNTIME_VERSION_1_9_32_0);

	// Os enderecos do motor que o Engine.h usa foram lidos do executavel 1.9.32.0 (como no FreeClimbLE).
	if(skse->runtimeVersion != RUNTIME_VERSION_1_9_32_0)
	{
		_MESSAGE("ABORTING: unsupported runtime");
		return false;
	}

	return true;
}

bool SKSEPlugin_Load(const SKSEInterface * skse)
{
	g_messaging = (SKSEMessagingInterface *)skse->QueryInterface(kInterface_Messaging);
	if(!g_messaging || !g_messaging->RegisterListener(g_pluginHandle, "SKSE", OnMessage))
	{
		_MESSAGE("ABORTING: the messaging interface is not available");
		return false;
	}
	Options::SetMessaging(g_messaging);

	// funcoes nativas do menu (GuardCountersNative)
	SKSEPapyrusInterface * papyrus = (SKSEPapyrusInterface *)skse->QueryInterface(kInterface_Papyrus);
	if(!papyrus || !papyrus->Register(Options::RegisterPapyrus))
		_MESSAGE("WARNING: could not register the Papyrus functions - the in-game menu will not work");

	_MESSAGE("running");
	return true;
}

};
