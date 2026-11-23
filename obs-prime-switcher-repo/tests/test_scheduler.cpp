#include "../src/core/PrimeScheduler.hpp"
#include "../src/core/Primes.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace prime;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); std::exit(1);} } while (0)

static std::vector<Camera> cams(std::initializer_list<int> primes)
{
	std::vector<Camera> v; int i = 1;
	for (int p : primes) v.push_back({"cam" + std::to_string(i++), p * 1000LL, true});
	return v;
}

int main()
{
	// --- prime helpers
	CHECK(nextPrime(13) == 17 && prevPrime(13) == 11 && nearestPrime(15) == 13);
	CHECK(nearestPrime(14) == 13 && prevPrime(2) == 2);

	// --- 1 hour simulation, 3 cams 11/13/17 s, 1 ms resolution
	PrimeScheduler s;
	s.setCameras(cams({11, 13, 17}));
	s.setMinDwell(1500);
	s.start(0, 12345, "cam1");
	std::vector<Ms> cutTimes; std::string prev = "cam1"; Ms minGap = 1 << 30;
	for (Ms t = 0; t <= 3600000; ++t) {
		auto id = s.tick(t);
		if (!id) continue;
		CHECK(*id != prev);                       // never cut to what's live
		minGap = std::min(minGap, cutTimes.empty() ? t : t - cutTimes.back());
		cutTimes.push_back(t); prev = *id;
	}
	double avg = 3600.0 / cutTimes.size();
	std::printf("cuts/hour: %zu  avg gap: %.2fs  min gap: %lldms\n", cutTimes.size(), avg, (long long)minGap);
	CHECK(minGap >= 1500);                            // min-dwell honoured
	CHECK(avg > 3.0 && avg < 10.0);

	// --- nextWake-driven stepping gives identical result to 1 ms stepping
	PrimeScheduler a, b;
	a.setCameras(cams({11, 13, 17})); b.setCameras(cams({11, 13, 17}));
	a.start(0, 99, "cam1"); b.start(0, 99, "cam1");
	std::vector<std::pair<Ms, std::string>> ra, rb;
	for (Ms t = 0; t <= 600000; ++t) if (auto id = a.tick(t)) ra.push_back({t, *id});
	for (;;) { auto w = b.nextWake(); if (!w || *w > 600000) break; if (auto id = b.tick(*w)) rb.push_back({*w, *id}); }
	CHECK(ra == rb);

	// --- stop == reset
	s.stop();
	CHECK(s.state() == PrimeScheduler::State::Stopped);
	CHECK(!s.tick(5000000).has_value());
	CHECK(!s.nextWake().has_value());
	CHECK(s.remaining("cam1", 0) == -1);

	// --- restart re-seeds phases (different seeds -> different first fires)
	s.start(0, 1, "cam1"); Ms r1 = s.remaining("cam2", 0);
	s.stop();
	s.start(0, 2, "cam1"); Ms r2 = s.remaining("cam2", 0);
	CHECK(r1 != r2);
	CHECK(r1 >= 1500 && r2 >= 1500);                  // first cut never before min-dwell

	// --- disabled cams never fire
	PrimeScheduler d; auto c = cams({11, 13}); c[1].enabled = false;
	d.setCameras(c); d.start(0, 7, "");
	for (Ms t = 0; t <= 600000; ++t) if (auto id = d.tick(t)) CHECK(*id == "cam1");

	std::puts("ALL TESTS PASSED");
	return 0;
}
