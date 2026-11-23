#pragma once
#include <string>
#include <vector>

namespace prime {

// Output backend. The scheduler never touches OBS or ATEM directly; it only
// asks a backend to put "source id" live. Add AtemSwitcher by implementing this.
class ISwitcher {
public:
	virtual ~ISwitcher() = default;

	// Names/ids of everything that can be switched to.
	virtual std::vector<std::string> sources() const = 0;
	// What is live (program) right now.
	virtual std::string currentLive() const = 0;
	// Put `id` live. Returns false if it doesn't exist / failed.
	virtual bool cutTo(const std::string &id) = 0;

	// Remember the current live source as "ours" (call when starting).
	virtual void acknowledgeCurrent() = 0;
	// True if the live source differs from the last one WE commanded,
	// i.e. somebody switched manually (click, Stream Deck, ...).
	virtual bool isExternalChange() const = 0;
};

} // namespace prime
