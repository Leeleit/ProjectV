import std;

#include <cstdio>

int main()
{
	std::vector<int> v{1, 2, 3, 4, 5};
	int sum = 0;
	for (int x : v) {
		sum += x;
	}
	std::printf("std module probe: sum=%d (expected 15)\n", sum);
	return sum == 15 ? 0 : 1;
}
