#pragma once
#include "../core/ISwitcher.hpp"

namespace prime {

// Switches OBS scenes through the frontend API. Must be used from the UI thread.
// Studio Mode: sets the preview scene and triggers the active transition.
class ObsSwitcher : public ISwitcher {
public:
	std::vector<std::string> sources() const override;
	std::string currentLive() const override;
	bool cutTo(const std::string &id) override;
	void acknowledgeCurrent() override;
	bool isExternalChange() const override;

private:
	std::string lastCommanded_;
};

} // namespace prime
