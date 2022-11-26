
#include <Windows.h>

#include <string>
#include <optional>
#include <cstdint>
#include <thread>
#include <sstream>
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

std::optional<std::string> get_error_message(DWORD error_code) {

	LPWSTR wstr_buffer = nullptr;
	std::optional<std::string> message_opt{};
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
	if (number_of_wchars_without_terminating_nul != 0) {
		message_opt = ::nowide::narrow(wstr_buffer,
			number_of_wchars_without_terminating_nul);
	}

	if (wstr_buffer != nullptr) {
		if (LocalFree(wstr_buffer) != nullptr) {
			message_opt.reset();
		}
	}
	return message_opt;
}

std::string indent_message(const std::string_view& spaces, const std::string& str) {
	std::ostringstream oss{};
	size_t next_start_index_of_substr{ 0 };
	for (size_t i = 0; i < str.length(); ++i) {
		char c = str[i];
		if (c == '\n') {
			if (next_start_index_of_substr == 0 && i == str.length() - 1)
				return str;
			else {
				if (next_start_index_of_substr == 0 && i != 0)
					oss << '\n';
				oss << spaces << str.substr(next_start_index_of_substr, i - next_start_index_of_substr + 1);
				next_start_index_of_substr = i + 1;
			}
		}
	}

	if (str.length() == next_start_index_of_substr)
		return oss.str();

	oss << spaces << str.substr(next_start_index_of_substr, str.length() - next_start_index_of_substr) << '\n';
	return oss.str();
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
			auto error_message = get_error_message(error);
			fmt::print(stream, "{0}Filetype: FILE_TYPE_UNKNOWN, error: {1:#x} {1:d} - {2}",leading, error, indent_message(std::string{leading}+"  ", error_message.value_or("")));
		}
		break;
	}
	default:
		fmt::print(stream, "{}Filetype: {:#0x} - this is unexpected, undocumented and an error\n", leading, file_type);
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
			auto message = get_error_message(error);
			fmt::print(stream, "  console mode: error {:#x} - {}", error, indent_message("    ", message.value_or("")));
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

bool AttachToConsoleAndPrintInfo(FILE*stream, uint32_t PID) {
	fmt::print(stream, "DEBUG: out before free&attach: {:#x}\n", std::bit_cast<uintptr_t>(GetStdHandle(STD_OUTPUT_HANDLE)));

	if (!FreeConsole()) {
		auto error = GetLastError();
		auto message = get_error_message(error);
		fmt::print(stream, "FreeConsole() failed, but we are ignoring that. The error is {:#x} with message is {}{}{}\n",
			error, quote_open, message.value_or(""), quote_close);
	}
	static_assert(sizeof(PID) == sizeof(DWORD) && std::is_unsigned_v<decltype(PID)> == std::is_unsigned_v<DWORD>);
	if (!AttachConsole(PID)) {
		auto error = GetLastError();
		auto message = get_error_message(error);
		fmt::print(stream, "AttachConsole({}) failed with error {}{}{}\n", PID, error, quote_open, message.value_or(""), quote_close);
		return false;
	}
	fmt::print("Attached to console of process {}\n", PID);


	fmt::print(stream, "DEBUG: out after free&attach: {:#x}\n", std::bit_cast<uintptr_t>(GetStdHandle(STD_OUTPUT_HANDLE)));

	// TODO: open console handles, set std handle.  Skipping for now
	
	PrintInfo(stream);
	return true;
}

bool SpawnSelf(FILE* fOut, FILE *fErr) {
	HANDLE hOut = std::bit_cast<HANDLE>(_get_osfhandle(_fileno(fOut)));
	HANDLE hErr = std::bit_cast<HANDLE>(_get_osfhandle(_fileno(fErr)));


	fmt::print(fErr, "SpawnSelf() not implemented\n");
	return false;
}

int main(int argc, const char **argv) {

	if (argc <= 1) {
		PrintInfo(stdout);
		return 0;
	}
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
		PrintUsage(stderr);
		return 1;
	}


	std::optional<uint32_t> PID{ std::nullopt };
	std::optional<intptr_t> handle_out{ std::nullopt };
	std::optional<intptr_t> handle_err{ std::nullopt };
	bool no_self_spawn{ false };

	for (int i = 1; i < argc; ++i) {
		std::string_view current_arg{ argv[i] };
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
			i += 1;
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
				fmt::print(stderr, "Missing value for option \"--handle-out\"\n");
				PrintUsage(stderr);
				return 1;
			}
			i += 1;
			auto opt_uint = string_to_uint<uintptr_t>(*next_arg);
			if (!opt_uint) {
				fmt::print(stderr, "value for option \"--handle-out\" is not a number or not in range.\n");
				PrintUsage(stderr);
				return 1;
			}
			handle_out = std::bit_cast<intptr_t>(opt_uint.value());
		}
		else if (current_arg == "--handle-err") {
			if (!next_arg) {
				fmt::print(stderr, "Missing value for option \"--handle-err\"\n");
				PrintUsage(stderr);
				return 1;
			}
			i += 1;
			auto opt_uint = string_to_uint<uintptr_t>(*next_arg);
			if (!opt_uint) {
				fmt::print(stderr, "value for option \"--handle-err\" is not a number or not in range.\n");
				PrintUsage(stderr);
				return 1;
			}
			handle_err = std::bit_cast<intptr_t>(opt_uint.value());
		}
		else {
			fmt::print(stderr, "Argument {}{}{} could not be interpreted\n", quote_open, current_arg, quote_close);
			PrintUsage(stderr);
			return 1;
		}
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
		if (handle_out && *handle_out == *handle_err) {
			fErr = fOut;
		}
		else {
			int fd = _open_osfhandle(handle_err.value(), 0);
			if (fd == -1)
				return 1;
			fErr = _fdopen(fd, "w");
			if (fOut == nullptr)
				return 1;
		}
	}


	if (PID.has_value()) {
		if (no_self_spawn) {
			if (!AttachToConsoleAndPrintInfo(fOut, *PID))
				return 1;
		}
		else {
			if (!SpawnSelf(fOut,fErr))
				return 1;
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