#include <Windows.h>

#include <fmt/core.h>
#include <nowide/args.hpp>

int main() {
	UINT acp = GetACP();
	fmt::print("ACP {}", acp);
	return 0;
}