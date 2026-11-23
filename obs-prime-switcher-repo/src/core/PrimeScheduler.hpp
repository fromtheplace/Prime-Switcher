#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace prime {

using Ms = int64_t; // milliseconds on any monotonic clock (injected by caller)

struct Camera {
	std::string id;     // scene name / ATEM input key
	Ms periodMs = 0;    // prime number of seconds * 1000
	bool enabled = true;
};

// Free-running per-camera metronomes. Each camera fires every `periodMs`;
// the merged stream of firings is the switching pattern.
//
// States: Stopped <-> Running. There is no pause: stop() discards all phase
// state, and start() re-seeds with random phases (stop == reset).
//
// No OBS / Qt dependencies; time is passed in, so it is fully unit-testable.
class PrimeScheduler {
public:
	enum class State { Stopped, Running };

	void setCameras(std::vector<Camera> cams);
	void setMinDwell(Ms ms);

	// Begin running at `now`. `liveId` is what is currently live (never
	// re-cut to). `seed` randomises the starting phases.
	void start(Ms now, uint32_t seed, const std::string &liveId = {});
	// Stop and reset. All schedule state is dropped.
	void stop();

	// Call when the clock reaches nextWake(). Returns the source to cut to,
	// or nullopt if nothing should happen.
	std::optional<std::string> tick(Ms now);

	// Earliest upcoming fire time (to arm a single timer), if running.
	std::optional<Ms> nextWake() const;

	// Time until `id` next fires; -1 if stopped/disabled/unknown.
	Ms remaining(const std::string &id, Ms now) const;

	State state() const { return state_; }
	const std::string &live() const { return live_; }

private:
	struct Slot {
		Camera cam;
		Ms nextFire = 0;
		Ms lastLive = 0;
	};

	std::vector<Camera> cameras_;
	std::vector<Slot> slots_;
	Ms minDwell_ = 1500;
	Ms lastCut_ = 0;
	std::string live_;
	State state_ = State::Stopped;
};

} // namespace prime
