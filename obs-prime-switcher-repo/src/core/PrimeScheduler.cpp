#include "PrimeScheduler.hpp"
#include <algorithm>
#include <random>

namespace prime {

void PrimeScheduler::setCameras(std::vector<Camera> cams)
{
	cameras_ = std::move(cams);
}

void PrimeScheduler::setMinDwell(Ms ms)
{
	minDwell_ = std::max<Ms>(0, ms);
}

void PrimeScheduler::start(Ms now, uint32_t seed, const std::string &liveId)
{
	slots_.clear();
	std::mt19937 rng(seed);
	std::uniform_real_distribution<double> phase(0.25, 1.0);

	for (const Camera &c : cameras_) {
		if (!c.enabled || c.periodMs <= 0) continue;
		Slot s;
		s.cam = c;
		// Random first-fire inside the first period, never sooner than
		// min-dwell, so every start produces a different pattern.
		Ms first = static_cast<Ms>(static_cast<double>(c.periodMs) * phase(rng));
		s.nextFire = now + std::max(first, minDwell_);
		s.lastLive = now;
		slots_.push_back(std::move(s));
	}
	live_ = liveId;
	lastCut_ = now;
	state_ = State::Running;
}

void PrimeScheduler::stop()
{
	slots_.clear();
	live_.clear();
	state_ = State::Stopped;
}

std::optional<std::string> PrimeScheduler::tick(Ms now)
{
	if (state_ != State::Running) return std::nullopt;

	// Collect due cameras. Re-arm as nextFire += P (never now + P) so the
	// prime relationships never drift. Loop covers a late tick.
	std::vector<Slot *> due;
	for (Slot &s : slots_) {
		if (s.nextFire > now) continue;
		due.push_back(&s);
		do { s.nextFire += s.cam.periodMs; } while (s.nextFire <= now);
	}
	if (due.empty()) return std::nullopt;

	// A camera firing for the scene that is already live is a no-op.
	due.erase(std::remove_if(due.begin(), due.end(),
				 [&](Slot *s) { return s->cam.id == live_; }),
		  due.end());
	if (due.empty()) return std::nullopt;

	// Min-dwell: too soon after the last cut -> drop this firing.
	if (now - lastCut_ < minDwell_) return std::nullopt;

	// Tie-break: longest time since it was live; then shortest period.
	Slot *best = *std::min_element(due.begin(), due.end(), [](Slot *a, Slot *b) {
		if (a->lastLive != b->lastLive) return a->lastLive < b->lastLive;
		return a->cam.periodMs < b->cam.periodMs;
	});

	best->lastLive = now;
	lastCut_ = now;
	live_ = best->cam.id;
	return live_;
}

std::optional<Ms> PrimeScheduler::nextWake() const
{
	if (state_ != State::Running || slots_.empty()) return std::nullopt;
	Ms m = slots_.front().nextFire;
	for (const Slot &s : slots_) m = std::min(m, s.nextFire);
	return m;
}

Ms PrimeScheduler::remaining(const std::string &id, Ms now) const
{
	if (state_ != State::Running) return -1;
	for (const Slot &s : slots_)
		if (s.cam.id == id) return std::max<Ms>(0, s.nextFire - now);
	return -1;
}

} // namespace prime
