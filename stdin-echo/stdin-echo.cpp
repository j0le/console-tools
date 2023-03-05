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


bool g_ctrl_event_handled{ false };

BOOL WINAPI HandleCtrlEvent(
	_In_ DWORD dwCtrlType
) {
	switch (dwCtrlType) {
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
		g_ctrl_event_handled = true;
		return TRUE;
	default:
		return FALSE;
	}
}

enum class e_replace_CR_with:uint32_t {
	CR, // no replacment, everything stays as is
	CRLF,
	LF
};

constexpr const e_replace_CR_with replace_CR_with{ e_replace_CR_with::CR };

int main()
{
	while (not IsDebuggerPresent());
	DebugBreak();
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

	HANDLE hIn{ GetStdHandle(STD_INPUT_HANDLE) };
	HANDLE hOut{ GetStdHandle(STD_OUTPUT_HANDLE) };

	if (hIn == nullptr || hIn == INVALID_HANDLE_VALUE || hOut == nullptr || hOut == INVALID_HANDLE_VALUE)
	{
		fmt::print(stderr, "couldn't get standard input/output handles.\n");
		return 1;
	}

	DWORD console_in_mode{};

	if (not GetConsoleMode(hIn, &console_in_mode)) {
		fmt::print(stderr, "stdin is not a console.\n");
		return 1;
	}

	DWORD dummy{};
	bool is_stdout_console = GetConsoleMode(hOut, &dummy);

	bool handler_set{ false };
	if (!(handler_set = SetConsoleCtrlHandler(&HandleCtrlEvent, TRUE)))
	{
		fmt::print(stderr, "Error: SetConsoleCtrlHandler() failed.\n");
		return 1;
	}

	struct restore_console_mode {
		DWORD orig_console_mode{};
		HANDLE console_handle{};
		~restore_console_mode() {
			SetConsoleMode(console_handle, orig_console_mode);
		}
	} restore_console_mode_instance{ console_in_mode, hIn };

	if (not SetConsoleMode(hIn, ENABLE_VIRTUAL_TERMINAL_INPUT))
	{
		fmt::print(stderr, "SetConsoleMode() failed for stdin.\n");
		return 1;
	}

	static constexpr const size_t BUFF_SIZE{ 512 };

	auto print_out = [&](std::wstring_view sv) -> bool {
		if (sv.size() == 0)
			return true;
		if (is_stdout_console) {
			// StdOut is a console
			bool success{ false };
			DWORD absolute_number_of_wide_characters_written{ 0 };
			DWORD number_of_wchars_written{ 0 };
			while
				(
					absolute_number_of_wide_characters_written < sv.size()
					&&
					(
						success = WriteConsoleW(
							hOut,
							&sv[absolute_number_of_wide_characters_written],
							sv.size() - absolute_number_of_wide_characters_written,
							&number_of_wchars_written,
							nullptr)
						)
					&&
					number_of_wchars_written > 0
					) {
				absolute_number_of_wide_characters_written += number_of_wchars_written;
				if (absolute_number_of_wide_characters_written < number_of_wchars_written) {
					fmt::print(stderr, "Overflow in DWORD.\n");
					return false;
				}
			}
			if
				(
					not success
					||
					absolute_number_of_wide_characters_written != sv.size()
					) {
				fmt::print(stderr, "Could not write everything to console.\n");
				return false;
			}
		}
		else {
			// stdout is not a console
			const auto str = nowide::narrow(sv);
			if (not WriteFile(hOut, str.data(), str.size(), &dummy, nullptr))
			{
				fmt::print(stderr, "WriteFile() failed.\n");
				return false;
			}
			//fmt::print(stdout, "{}", str);
			//std::fflush(stdout);
		}
		return true;
	};

	wchar_t buffer[BUFF_SIZE]{};
	do {
		if (g_ctrl_event_handled) {
			fmt::print(stderr, "Control-C\n");
			return 1;
		}
		DWORD dwWideCharactersRead{};
		bool bRead{ false };
		bRead = ReadConsoleW(hIn, buffer, BUFF_SIZE, &dwWideCharactersRead, nullptr);
		if (!bRead) {
			fmt::print(stderr, "ReadConsoleW() failed.\n");
			return 1;
		}
		if (dwWideCharactersRead == 0) {
			fmt::print(stderr, "nothing to read anymore.\n");
			break;
		}
		if (dwWideCharactersRead > BUFF_SIZE) {
			fmt::print(stderr, "Unexpected error when using ReadConsoleW().\n");
			return 1;
		}
		if (g_ctrl_event_handled) {
			fmt::print(stderr, "Control-C\n");
			return 1;
		}

		std::wstring_view print_later(buffer, dwWideCharactersRead);
		for (size_t i = 0; i < print_later.size();) {
			switch(print_later[i])
			{
				case wchar_t(0x3):
				{
					if (not print_out(print_later.substr(0, i))) { return 1; }
					fmt::print(stderr, "Control-C");
					return 1;
				}
				case wchar_t(0x4):
				{
					if (not print_out(print_later.substr(0, i))) { return 1; }
					fmt::print(stderr, "Control-D");
					return 0;
				}

				case wchar_t(0xd): // ^M, Carriage Return, \r
				{
					if (replace_CR_with != e_replace_CR_with::CR) {
						size_t count = i; // CR not included
						if (replace_CR_with == e_replace_CR_with::CRLF)
							count += 1; // include CR
						if (not print_out(print_later.substr(0, count)))
							return 1;
						if (not print_out(std::wstring_view(L"\n")))
							return 1;
						print_later = print_later.substr(i + 1);
						i = 0;
						continue;
					}
					break;
				}
			}

			++i;
		}

		if (not print_out(print_later))
			return 1;


	} while (true);
	return 0;
}
