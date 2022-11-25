
#include <Windows.h>

#include <string>
#include <optional>
#include <cstdint>
#include <thread>
#include <io.h>

#include "helper.h"

#include <fmt/core.h>
#include <nowide/args.hpp>


//void print_bits(DWORD dw) {
//	constexpr const int number_of_bits = sizeof(dw) * 8;
//	for (int i = number_of_bits - 1; i >= 0; --i) {
//		bool one = 0 != (dw & (1 << i));
//
//		fmt::print("{}", one ? "1" : ".");
//
//		// space separators
//		if (i != 0 && (i % 4) == 0)
//			fmt::print(" ");
//	}
//}

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

bool get_error_message(DWORD error_code, ::std::string& message) {

	LPWSTR wstr_buffer = nullptr;
	bool return_value{ true };
	DWORD number_of_wchars_without_terminating_nul = ::FormatMessageW(
		FORMAT_MESSAGE_IGNORE_INSERTS   // ignore paremeter "Arguments"
		| FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_ALLOCATE_BUFFER, // function allocates buffer using
		                                // LocalAlloc. That means we have to
		                                // use local free
		nullptr,                        // lpSource
		error_code,                     // dwMessageId
		0, // dwLanguageId. Zero means: Try different languages until message is
		   // found.
		reinterpret_cast<LPWSTR>(
			&wstr_buffer), // We have to cast, because the argument is interpreted
		                   // differently based on flags

		0,                 // nSize. We don't know the size yet
		nullptr);
	if (number_of_wchars_without_terminating_nul == 0) {
		return_value = false;
	}
	else {
		message = ::nowide::narrow(wstr_buffer,
			number_of_wchars_without_terminating_nul);
	}

	if (wstr_buffer != nullptr) {
		if (LocalFree(wstr_buffer) != nullptr) {
			return_value = false;
		}
	}
	return return_value;
}

void PrintFileType(FILE*stream, std::string_view leading, HANDLE handle) {
	auto file_type = GetFileType(handle);
	switch (file_type) {
	case FILE_TYPE_CHAR:
		fmt::print(stream, "{}Filetype: FILE_TYPE_CHAR\n", leading);
		break;
	case FILE_TYPE_DISK:
		fmt::print(stream, "{}Filetype: FILE_TYPE_DISK\n", leading);
		break;
	case FILE_TYPE_PIPE:
		fmt::print(stream, "{}Filetype: FILE_TYPE_PIPE\n", leading);
		break;
	case FILE_TYPE_REMOTE:
		fmt::print(stream, "{}Filetype: FILE_TYPE_REMOTE\n", leading);
		break;
	case FILE_TYPE_UNKNOWN: {
		auto error = GetLastError();
		if (error == NO_ERROR) {
			fmt::print(stream, "{}Filetype: FILE_TYPE_UNKNOWN\n",leading);
		}
		else {
			std::string error_message{};
			bool got_msg = get_error_message(error, error_message);
			fmt::print(stream, "{0}Filetype: FILE_TYPE_UNKNOWN, error: {1:#x} {1:d} \"{2}\"\n",leading, error, got_msg?error_message:"");
		}
		break;
	}
	default:
		fmt::print(stream, "{}Filetype: {:#0x} - this is unexpected, undocumented and an error", leading, file_type);
		break;
	}
}

void PrintMode(FILE*stream, std::string_view name, HANDLE hStd) {
	if (hStd == INVALID_HANDLE_VALUE || hStd == nullptr) {
		fmt::print("{}: invalid\n", name);
	}
	else {
		static_assert(sizeof(HANDLE) == sizeof(uintptr_t));

		fmt::print(stream, "{}: {:#x}\n", name, std::bit_cast<uintptr_t>(hStd));
		DWORD console_output_mode{};
		if (GetConsoleMode(hStd, &console_output_mode)) {
			fmt::print(stream, "  console mode: {:#0{}b}\n", console_output_mode, sizeof(console_output_mode) * 8 + 2);
		}
		else {
			auto error = GetLastError();
			std::string message{};
			get_error_message(error, message);
			fmt::print(stream, "  console mode: error {:#x} \"{}\"", error, message);
		}

		PrintFileType(stream, "  ", hStd);
	}
}

void PrintInfo(FILE*stream) {
	UINT acp = GetACP();
	UINT oem_cp = GetACP();
	UINT console_input_cp = GetConsoleCP();
	UINT console_output_cp = GetConsoleOutputCP();
	fmt::print("ACP:               {}\n", acp);
	fmt::print("OEM CP:            {}\n", oem_cp);
	fmt::print("Console Input CP:  {}\n", console_input_cp);
	fmt::print("Console Output CP: {}\n", console_output_cp);

	HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
	PrintMode(stream, "stdin", hStdIn);

	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	PrintMode(stream, "stdout", hStdOut);

	HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);
	PrintMode(stream, "stderr", hStdErr);

	if (hStdOut != INVALID_HANDLE_VALUE && hStdErr != INVALID_HANDLE_VALUE) {
		if (hStdOut == hStdErr)
			fmt::print(stream, "The handles for stdout and stderr are equal.\n");
		if (CompareObjectHandles(hStdOut, hStdErr))
			fmt::print(stream, "The handles for stdout and stderr point to the same kernel object.\n");
		else
			fmt::print(stream, "The handles for stdout and stderr do not point to the same kernel object.{}\n",
				hStdOut == hStdErr ? " It seems like, the object they are pointing to is not a kernel object, but some other kind of object." : "");
	}
}

void PrintUsage(FILE*stream) {
	fmt::print(stream,
		"Usage:\""
		"\n"
		"  stty.exe [--pid <PID>] [--handle-out <handle-out>] [--no-self-spawn] \n"
	);
}

bool AttachToConsole(uint32_t PID) {
	// free console
	// attach console
	// print info
	return true;
}

int main_disabled(int argc, const char **argv) {

	if (argc <= 1) {
		PrintInfo(stdout);
		return 0;
	}
	if (GetACP() != 65001) {
		fmt::print(stderr, "The Active Code Page is not UTF-8. Commandline parsing not supported.\n");
		PrintUsage(stderr);
		return 1;
	}


	std::optional<uint32_t> PID{ std::nullopt };
	std::optional<intptr_t> handle_out{ std::nullopt };
	std::optional<intptr_t> handle_err{ std::nullopt };
	bool no_self_spawn{ false };

	int i{ 1 };
	std::string_view current_arg{ argv[i] };
	while(true) {
		std::optional<std::string_view> next_arg{ std::nullopt };

		{
			int next_index = i + 1;
			bool next_available = next_index < argc;
			if (next_available)
				next_arg = argv[next_index];
		}


		if (current_arg == "--pid") {
			if (!next_arg) {
				fmt::print(stderr, "No process identifier supplied for option \"--pid\".\n");
				PrintUsage(stderr);
				return 1;
			}

			PID = string_to_uint<uint32_t>(*next_arg);
			if (!PID) {
				fmt::print(stderr, "Process identifier supplied for option \"--pid\" is not a number in base ten.\n");
				PrintUsage(stderr);
				return 1;
			}
		}
		else if (current_arg == "--no-self-spawn") {
			no_self_spawn = true;
		}
		else if (current_arg == "--handle-out") {
			if (!next_arg) {
				fmt::print(stderr, "Missing value for option \"--handle-out\"");
				PrintUsage(stderr);
				return 1;
			}
			auto opt_uint = string_to_uint<uintptr_t>(*next_arg);
			if (!opt_uint) {
				fmt::print(stderr, "value for option \"--handle-out\" is not a number or not in range.");
				PrintUsage(stderr);
				return 1;
			}
			handle_out = std::bit_cast<intptr_t>(opt_uint.value());
		}
		else if (current_arg == "--handle-err") {
			if (!next_arg) {
				fmt::print(stderr, "Missing value for option \"--handle-err\"");
				PrintUsage(stderr);
				return 1;
			}
			auto opt_uint = string_to_uint<uintptr_t>(*next_arg);
			if (!opt_uint) {
				fmt::print(stderr, "value for option \"--handle-err\" is not a number or not in range.");
				PrintUsage(stderr);
				return 1;
			}
			handle_err = std::bit_cast<intptr_t>(opt_uint.value());
		}
		else {
			fmt::print(stderr, "Argument {}{}{} could not be interpreted", quote_open, current_arg, quote_close);
		}

		if (!next_arg)
			break;
		current_arg = *next_arg;
	}

	FILE* fOut = stdout;
	FILE* fErr = stderr;

	if (handle_out) {
		int fd = _open_osfhandle(handle_out.value(), 0);
		if (fd == -1)
			return 1;
		fOut = _fdopen(fd, "w");
		if (fOut == nullptr)
			return 1;
	}

	if (handle_err) {
		int fd = _open_osfhandle(handle_out.value(), 0);
		if (fd == -1)
			return 1;
		fErr = _fdopen(fd, "w");
		if (fOut == nullptr)
			return 1;
	}


	if (PID.has_value()) {
		if (no_self_spawn) {
			if (!AttachConsole(*PID))
				return 1;
		}
		else {
			// TODO: Self Spawn
		}
	}
	else {
		PrintInfo(fOut);
	}

	return 0;
}

int main_temp() {
	if (false) {
		fmt::print(stdout, "waiting for debugger.\n");
		using namespace std::chrono_literals;
		while (!IsDebuggerPresent()) std::this_thread::sleep_for(100ms);
		DebugBreak();
	}
	fmt::print(stdout, "Tach ");
	SECURITY_ATTRIBUTES sa{ .nLength{sizeof(sa)}, .lpSecurityDescriptor{nullptr}, .bInheritHandle{true} };
	HANDLE conout = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
	if (conout == nullptr || conout == INVALID_HANDLE_VALUE)
		return 1;

	SetStdHandle(STD_OUTPUT_HANDLE, conout);

	int fd = _open_osfhandle(reinterpret_cast<intptr_t>(conout),0);
	if (fd == -1)
		return 1;

	bool use_new_FILE = false;
	FILE* f_stream{ nullptr };
	if (use_new_FILE) {
		 f_stream = _fdopen(fd, "w+");
		 if (f_stream == nullptr)
			 return 1;
	}
	else {
		if (0 != _dup2(fd, _fileno(stdout)))
			return 1;
		f_stream = stdout;
	}
	fmt::print(f_stream, "Moin!\n");
	return 0;
}