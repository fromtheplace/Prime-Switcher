#pragma once
// Small prime helpers used by the UI (snap/step to primes).
namespace prime {

inline bool isPrime(int n)
{
	if (n < 2) return false;
	for (int i = 2; i * i <= n; ++i)
		if (n % i == 0) return false;
	return true;
}

// Smallest prime strictly greater than n.
inline int nextPrime(int n)
{
	do { ++n; } while (!isPrime(n));
	return n;
}

// Largest prime strictly less than n (returns 2 if none).
inline int prevPrime(int n)
{
	do { --n; } while (n > 2 && !isPrime(n));
	return n < 2 ? 2 : n;
}

// Closest prime to n (ties go down).
inline int nearestPrime(int n)
{
	if (isPrime(n)) return n;
	int lo = prevPrime(n), hi = nextPrime(n);
	return (n - lo) <= (hi - n) ? lo : hi;
}

} // namespace prime
