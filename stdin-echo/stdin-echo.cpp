// Program "stdin-echo"
//
// This program should read from stdin and print to stdout

#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <fmt/format.h>
#include <nowide/convert.hpp>

#if !defined(UNICODE)
#error macro UNICODE is not defined
#endif

#if !defined(_UNICODE)
#error macro _UNICODE is not defined
#endif

constexpr const char8_t UTF_8_test_1[] = u8"ü";
static_assert(sizeof(UTF_8_test_1) == 3);
static_assert(UTF_8_test_1[0] == static_cast<char8_t>(0xC3u));
static_assert(UTF_8_test_1[1] == static_cast<char8_t>(0xBCu));
static_assert(UTF_8_test_1[2] == static_cast<char8_t>(0x0u));

constexpr const char UTF_8_test_2[] = "ü";
static_assert(sizeof(UTF_8_test_2) == 3);
static_assert(UTF_8_test_2[0] == static_cast<char>(0xC3u));
static_assert(UTF_8_test_2[1] == static_cast<char>(0xBCu));
static_assert(UTF_8_test_2[2] == static_cast<char>(0x0u));

struct handle_wrapper {
	HANDLE handle{ nullptr };
	handle_wrapper() = default;
	explicit handle_wrapper(HANDLE h) : handle{ h } {}
	handle_wrapper(const handle_wrapper&) = delete;
	handle_wrapper& operator=(const handle_wrapper&) = delete;

	handle_wrapper(handle_wrapper&& other) noexcept
		: handle{ other.handle }
	{
		other.handle = nullptr;
	}
	handle_wrapper& operator=(handle_wrapper&& other) = delete;

	bool try_move(handle_wrapper&& other) {
		if (IsNullOrInvalid()) {
			handle = other.handle;
			other.handle = nullptr;
			return true;
		}
		return false;
	}
	~handle_wrapper() {
		Close();
	}
	bool Close() {
		if (!IsNullOrInvalid()) {
			bool ret = CloseHandle(handle);
			handle = nullptr;
		}
		return true;
	}

	bool IsNullOrInvalid() {
		return handle == nullptr || handle == INVALID_HANDLE_VALUE;
	}
};

int main()
{
	_set_fmode(_O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
	_setmode(_fileno(stderr), _O_BINARY);
	_setmode(_fileno(stdin), _O_BINARY);

	if (GetACP() != 65001) {
		fmt::print(stderr, "The Active Code Page (ACP) for this process is not UTF-8 (65001).\n"
			"Command line parsing is not supported.\n"
			"Your version of Windows might be to old, so that the manifest embedded in the executable is not read. "
			"The manifest specifies, that this executable wants UTF-8 as ACP.\n"
			"As a workaround you can activate \"Beta: Use Unicode UTF-8 for worldwide language support\":\n"
			"  - Press Win+R\n"
			"  - Type \"intl.cpl\"\n"
			"  - Goto Tab \"Administrative\"\n"
			"  - Click on \"Change system locale\"\n"
			"  - Set Checkbox \"Beta: Use Unicode UTF-8 for worldwide language support\"\n"
			"\n");
		//PrintUsage(stderr);
		return 1;
	}

	handle_wrapper hIn{ GetStdHandle(STD_INPUT_HANDLE) };
	handle_wrapper hOut{ GetStdHandle(STD_OUTPUT_HANDLE) };

	if (hIn.IsNullOrInvalid() || hOut.IsNullOrInvalid())
	{
		fmt::print(stderr, "couldn't get standard input/output handles.\n");
		return 1;
	}
	DWORD dummy{};
	if (not GetConsoleMode(hIn.handle, &dummy)) {
		fmt::print(stderr, "stdin is not a console.\n");
		return 1;
	}



	static constexpr const size_t BUFF_SIZE{ 512 };
	wchar_t buffer[BUFF_SIZE]{};

	do {
		DWORD dwWideCharactersRead{};
		bool bRead{ false };
		bRead = ReadConsoleW(hIn.handle,buffer, BUFF_SIZE, &dwWideCharactersRead, nullptr);
		if (!bRead) {
			fmt::print(stderr, "ReadConsoleW() failed.\n");
			return 1;
		}
		if (dwWideCharactersRead == 0)
			break;
		if (dwWideCharactersRead > BUFF_SIZE) {
			fmt::print(stderr, "Unexpected error when using ReadConsoleW().\n");
			return 1;
		}

		if (GetConsoleMode(hOut.handle, &dummy)) {
			// StdOut is a console
			bool success{ false };
			DWORD absolute_number_of_wide_characters_written{ 0 };
			DWORD number_of_wchars_written{ 0 };
			while 
			(
				absolute_number_of_wide_characters_written < dwWideCharactersRead
				&&
				(
					success = WriteConsoleW(
						hOut.handle,
						&buffer[absolute_number_of_wide_characters_written],
						dwWideCharactersRead - absolute_number_of_wide_characters_written,
						&number_of_wchars_written,
						nullptr)
				)
				&&
				number_of_wchars_written > 0
			) {
				absolute_number_of_wide_characters_written += number_of_wchars_written;
				if (absolute_number_of_wide_characters_written < number_of_wchars_written) {
					fmt::print(stderr, "Overflow in DWORD.\n");
					return 1;
				}
			}
			if
			(
				not success
				||
				absolute_number_of_wide_characters_written != dwWideCharactersRead
			) {
				fmt::print(stderr, "Could not write everything to console.\n");
				return 1;
			}
		}
		else {
			// stdout is not a console
			std::wstring_view sv(buffer, dwWideCharactersRead);
			auto str = nowide::narrow(sv);
			fmt::print(stdout, "{}", str);
		}

	} while (true);
}
