#include "backends/ObsSwitcher.hpp"
#include "ui/PrimeDock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QMetaObject>
#include <QPointer>

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_name(void) { return "Prime Switcher"; }
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Auto-switches scenes using independent prime-number dwell timers.";
}

namespace {

prime::ObsSwitcher g_switcher;
QPointer<PrimeDock> g_dock;

enum Action { ActToggle = 0, ActStart = 1, ActStop = 2 };
obs_hotkey_id g_hk[3] = {OBS_INVALID_HOTKEY_ID, OBS_INVALID_HOTKEY_ID, OBS_INVALID_HOTKEY_ID};
const char *kHkKeys[3] = {"prime_switcher.hk_toggle", "prime_switcher.hk_start", "prime_switcher.hk_stop"};

// Hotkey callbacks run on OBS's hotkey thread -> bounce to the UI thread.
void hotkeyCb(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed || !g_dock) return;
	const int action = static_cast<int>(reinterpret_cast<intptr_t>(data));
	QMetaObject::invokeMethod(
		g_dock.data(),
		[action] {
			if (!g_dock) return;
			switch (action) {
			case ActToggle: g_dock->toggle(); break;
			case ActStart: g_dock->startSwitching(); break;
			case ActStop: g_dock->stopSwitching(); break;
			}
		},
		Qt::QueuedConnection);
}

void onFrontendEvent(enum obs_frontend_event event, void *)
{
	if (!g_dock) return;
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		g_dock->refreshScenes();
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		// Live scene differs from what we last commanded -> manual switch -> stop/reset.
		if (g_dock->running() && g_switcher.isExternalChange()) g_dock->onManualSwitch();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
	case OBS_FRONTEND_EVENT_EXIT:
		g_dock->stopSwitching();
		break;
	default: break;
	}
}

// Dock settings + hotkey bindings live in the scene collection.
void onSaveLoad(obs_data_t *data, bool saving, void *)
{
	if (saving) {
		if (g_dock) g_dock->save(data);
		for (int i = 0; i < 3; ++i) {
			obs_data_array_t *a = obs_hotkey_save(g_hk[i]);
			obs_data_set_array(data, kHkKeys[i], a);
			obs_data_array_release(a);
		}
	} else {
		if (g_dock) g_dock->load(data);
		for (int i = 0; i < 3; ++i) {
			obs_data_array_t *a = obs_data_get_array(data, kHkKeys[i]);
			if (a) {
				obs_hotkey_load(g_hk[i], a);
				obs_data_array_release(a);
			}
		}
	}
}

} // namespace

bool obs_module_load(void)
{
	g_dock = new PrimeDock(&g_switcher);
	// OBS takes ownership of the widget.
	obs_frontend_add_dock_by_id("prime-switcher", "Prime Switcher", g_dock.data());

	g_hk[ActToggle] = obs_hotkey_register_frontend("prime_switcher.toggle", "Prime Switcher: Start/Stop",
						      hotkeyCb, reinterpret_cast<void *>(intptr_t(ActToggle)));
	g_hk[ActStart] = obs_hotkey_register_frontend("prime_switcher.start", "Prime Switcher: Start",
						     hotkeyCb, reinterpret_cast<void *>(intptr_t(ActStart)));
	g_hk[ActStop] = obs_hotkey_register_frontend("prime_switcher.stop", "Prime Switcher: Stop",
						    hotkeyCb, reinterpret_cast<void *>(intptr_t(ActStop)));

	obs_frontend_add_event_callback(onFrontendEvent, nullptr);
	obs_frontend_add_save_callback(onSaveLoad, nullptr);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
	obs_frontend_remove_save_callback(onSaveLoad, nullptr);
	for (auto id : g_hk)
		if (id != OBS_INVALID_HOTKEY_ID) obs_hotkey_unregister(id);
}
