#include "ObsSwitcher.hpp"
#include <obs-frontend-api.h>
#include <obs.h>

namespace prime {

std::vector<std::string> ObsSwitcher::sources() const
{
	std::vector<std::string> out;
	char **names = obs_frontend_get_scene_names();
	if (names) {
		for (char **n = names; *n; ++n) out.emplace_back(*n);
		bfree(names);
	}
	return out;
}

std::string ObsSwitcher::currentLive() const
{
	std::string name;
	obs_source_t *src = obs_frontend_get_current_scene();
	if (src) {
		name = obs_source_get_name(src);
		obs_source_release(src);
	}
	return name;
}

bool ObsSwitcher::cutTo(const std::string &id)
{
	obs_source_t *src = obs_get_source_by_name(id.c_str());
	if (!src) return false;

	// Record first: the SCENE_CHANGED event may arrive after a transition.
	lastCommanded_ = id;

	if (obs_frontend_preview_program_mode_active()) {
		obs_frontend_set_current_preview_scene(src);
		obs_frontend_preview_program_trigger_transition();
	} else {
		obs_frontend_set_current_scene(src);
	}
	obs_source_release(src);
	return true;
}

void ObsSwitcher::acknowledgeCurrent()
{
	lastCommanded_ = currentLive();
}

bool ObsSwitcher::isExternalChange() const
{
	return currentLive() != lastCommanded_;
}

} // namespace prime
