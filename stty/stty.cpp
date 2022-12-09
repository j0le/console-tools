
#include <Windows.h>

#include <string>
#include <optional>
#include <cstdint>
#include <thread>
#include <sstream>
#include <memory>
#include <io.h>
#include <fcntl.h>

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

constexpr const std::string_view UTF_8_thumbs_up_with_skin_tone = "\xf0\x9f\x91\x8d\xf0\x9f\x8f\xbb";

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

static bool is_handle_invalid(HANDLE h, bool strict = false) {
	if (h == INVALID_HANDLE_VALUE || h == nullptr)
		return true;
	DWORD dummy;
	return !GetHandleInformation(h, &dummy);
}

enum class console_in_or_out :uint32_t {
	undefined,
	in,
	out
};
struct handle_with_name {
	HANDLE handle{ nullptr };
	std::string_view name{};
	console_in_or_out type{ console_in_or_out::undefined };
};


void PrintConsoleMode(FILE* stream, std::string_view indent, DWORD mode, console_in_or_out type) {
	constexpr std::size_t bits = sizeof(mode) * 8;

	fmt::print(stream, "{0}console mode: {1:#0{2}b}  {1:#0{3}x}\n", indent, mode, sizeof(mode) * 8 + 2, sizeof(mode) * 2 + 2);
	auto lambda = [&]<size_t size>(std::string_view const (&array)[size]) ->void {
		static_assert(size <= bits);
		for (int i = 0; i < size && i < bits; ++i) {
			DWORD mask = DWORD(1u) << i;
			bool set = mode & mask;
			auto pre_space  = bits - 1 - i;
			auto post_space = i;
			fmt::print(stream, "{0}                {1}{2}{3}  {4:#0{5}x} {6}\n",
				indent,
				std::string(pre_space, ' '),
				set ? '1' : '.',
				std::string(post_space, ' '),
				mask,
				sizeof(mode) * 2 + 2,
				array[i]);
		}
	};

	constexpr const std::string_view input_flags[]{
		"ENABLE_PROCESSED_INPUT",              // 0x0001
		"ENABLE_LINE_INPUT",                   // 0x0002
		"ENABLE_ECHO_INPUT",                   // 0x0004
		"ENABLE_WINDOW_INPUT",                 // 0x0008
		"ENABLE_MOUSE_INPUT",                  // 0x0010
		"ENABLE_INSERT_MODE",                  // 0x0020
		"ENABLE_QUICK_EDIT_MODE",              // 0x0040
		"ENABLE_EXTENDED_FLAGS",               // 0x0080
		"ENABLE_AUTO_POSITION",                // 0x0100
		"ENABLE_VIRTUAL_TERMINAL_INPUT",       // 0x0200
	};
	constexpr const std::string_view output_flags[]{
		"ENABLE_PROCESSED_OUTPUT",             //0x0001
		"ENABLE_WRAP_AT_EOL_OUTPUT",           //0x0002
		"ENABLE_VIRTUAL_TERMINAL_PROCESSING",  //0x0004
		"DISABLE_NEWLINE_AUTO_RETURN",         //0x0008
		"ENABLE_LVB_GRID_WORLDWIDE",           //0x0010
	};

	if (type == console_in_or_out::in || type == console_in_or_out::undefined)
		return lambda(input_flags);

	if (type == console_in_or_out::out || type == console_in_or_out::undefined)
		return lambda(output_flags);
}

bool PrintMode(FILE* stream, handle_with_name h, set_and_reset<DWORD> s_n_r = set_and_reset<DWORD>{}) {
	bool ret{ true };
	if (is_handle_invalid(h.handle, true)) {
		fmt::print("{}: {:#x} invalid\n", h.name, std::bit_cast<uintptr_t>(h.handle));
	}
	else {
		static_assert(sizeof(HANDLE) == sizeof(uintptr_t));

		fmt::print(stream, "{}: {:#x}\n", h.name, std::bit_cast<uintptr_t>(h.handle));
		DWORD console_mode{};
		if (GetConsoleMode(h.handle, &console_mode)) {
			PrintConsoleMode(stream, "  ", console_mode, h.type);
			auto new_mode = s_n_r.change(console_mode);
			if (new_mode != console_mode) {
				fmt::print(stream, "  new mode {:#x}\n", new_mode);
				if (SetConsoleMode(h.handle, new_mode)) {
					PrintConsoleMode(stream, "  ", new_mode, h.type);
				}
				else {
					auto error = GetLastError();
					auto message = get_error_message(error);
					fmt::print(stream, "  SetConsoleMode(): error {:#x} - {}", error, indent_message("    ", message.value_or("")));
					ret = false;
				}
			}
		}
		else {
			auto error = GetLastError();
			auto message = get_error_message(error);
			fmt::print(stream, "  console mode: error {:#x} - {}", error, indent_message("    ", message.value_or("")));
			//ret = false;
		}

		PrintFileType(stream, "  ", h.handle);
	}
	return ret;
}

void PrintComparison(FILE* stream, handle_with_name hn1, handle_with_name hn2) {
	if (!is_handle_invalid(hn1.handle, true) && !is_handle_invalid(hn2.handle, true)) {
		if (hn1.handle == hn2.handle)
			fmt::print(stream, "The handles for {} and {} are equal.\n", hn1.name, hn2.name);
		if (CompareObjectHandles(hn1.handle, hn2.handle))
			fmt::print(stream, "The handles for {} and {} point to the same kernel object.\n",hn1.name,hn2.name);
		else
			fmt::print(stream, "The handles for {} and {} do not point to the same kernel object.{}\n",hn1.name, hn2.name,
				hn1.handle == hn2.handle ? " It seems like, the object they are pointing to is not a kernel object, but some other kind of object." : "");
	}
}

void CompareEveryThing(FILE* stream, std::vector<handle_with_name> handles) {

	for (int i = 0; i < handles.size(); ++i) {
		for (int j = i+1; j < handles.size(); ++j) {
			PrintComparison(stream, handles[i], handles[j]);
		}
	}
}

void TestWriteConsole(FILE*stream, HANDLE conout) {
	constexpr const std::wstring_view wsv = L"Moin Moin\n";
	DWORD written;
	bool success = WriteConsoleW(conout, wsv.data(), wsv.size(), &written, nullptr);
	fmt::print(stream, "  WriteConsoleW result: {}\n", success?"success":"fail");
}


struct change_con_mode{
	set_and_reset<DWORD> conin{};
	set_and_reset<DWORD> conout{};
};

bool PrintInfo(FILE*stream, change_con_mode change_mode, bool more_info = false) {
	bool ret{ true };
	fmt::print(stream, "DEBUG: äöü {}{}{}\n", quote_open, UTF_8_thumbs_up_with_skin_tone, quote_close);

	UINT acp = GetACP();
	UINT oem_cp = GetACP();
	UINT console_input_cp = GetConsoleCP();
	UINT console_output_cp = GetConsoleOutputCP();
	fmt::print(stream, "ACP:               {}\n", acp);
	fmt::print(stream, "OEM CP:            {}\n", oem_cp);
	fmt::print(stream, "Console Input CP:  {}\n", console_input_cp);
	fmt::print(stream, "Console Output CP: {}\n", console_output_cp);
	fmt::print(stream, "\n");

	// -----------------------
	if (more_info)
		fmt::print(stream, "\nIN:\n\n");

	handle_with_name hC_stdin{
		.handle{reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdin)))},
		.name{"stdin"},
		.type{console_in_or_out::in},
	};

	if (more_info)
		ret = PrintMode(stream, hC_stdin) && ret;

	handle_with_name hStdIn{ .handle{ GetStdHandle(STD_INPUT_HANDLE)}, .name{"STD_INPUT_HANDLE"}, .type{console_in_or_out::in}, };

	if (more_info)
		ret = PrintMode(stream, hStdIn) && ret;

	handle_with_name hConIn{ .handle{nullptr}, .name{"CONIN$"}, .type{console_in_or_out::in}, };
	{
		SECURITY_ATTRIBUTES sa{ .nLength{sizeof(sa)}, .lpSecurityDescriptor{nullptr}, .bInheritHandle{false} };
		hConIn.handle = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, &sa, OPEN_EXISTING, 0, nullptr);
		ret = PrintMode(stream, hConIn, change_mode.conin) && ret;
	}


	if (more_info) {
		fmt::print(stream, "\n");
		CompareEveryThing(stream, std::vector{ hC_stdin, hStdIn, hConIn });
	}

	fmt::print(stream, "\n");
	// -----------------------
	if (more_info)
		fmt::print(stream, "\nOUT:\n\n");

	handle_with_name hC_stdout{ 
		.handle{reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout)))}, 
		.name{"stdout"},
		.type{console_in_or_out::out},
	};
	if (more_info)
		ret = PrintMode(stream, hC_stdout) && ret;

	handle_with_name hC_stderr{
		.handle{reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stderr)))},
		.name{"stderr"},
		.type{console_in_or_out::out},
	};
	if (more_info)
		ret = PrintMode(stream, hC_stderr) && ret;

	handle_with_name hStdOut{ 
		.handle{ GetStdHandle(STD_OUTPUT_HANDLE)}, 
		.name{"STD_OUTPUT_HANDLE"},
		.type{console_in_or_out::out},
	};
	if (more_info)
		ret = PrintMode(stream, hStdOut) && ret;

	handle_with_name hStdErr{
		.handle {GetStdHandle(STD_ERROR_HANDLE)},
		.name{"STD_ERROR_HANDLE"},
		.type{console_in_or_out::out},
	};
	if (more_info)
		ret = PrintMode(stream,  hStdErr) && ret;

	handle_with_name hConOut{ .handle{nullptr}, .name{"CONOUT$"}, .type{console_in_or_out::out}, };
	{
		SECURITY_ATTRIBUTES sa{ .nLength{sizeof(sa)}, .lpSecurityDescriptor{nullptr}, .bInheritHandle{false} };
		hConOut.handle = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
		ret = PrintMode(stream, hConOut, change_mode.conout) && ret;
		//TestWriteConsole(stream, hConOut.handle);
	}

	if (more_info) {
		fmt::print(stream, "\n");
		CompareEveryThing(stream, std::vector{ hC_stdout, hC_stderr, hStdOut, hStdErr, hConOut });
	}

	if (!is_handle_invalid(hConOut.handle)) {
		CloseHandle(hConOut.handle);
		hConOut.handle = nullptr;
	}

	if (!is_handle_invalid(hConIn.handle)) {
		CloseHandle(hConIn.handle);
		hConIn.handle = nullptr;
	}
	return ret;
}

void PrintUsage(FILE*stream) {
	fmt::print(stream,
		"Usage:\n"
		"\n"
		"  stty.exe [--pid <PID>] [--handle-out <handle-out>] [--no-self-spawn] [--set-in-mode <mode>] [--set-out-mode <mode>]\n"
	);
}

bool AttachToConsoleAndPrintInfo(FILE*stream, uint32_t PID, change_con_mode change_mode) {
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
		fmt::print(stream, "AttachConsole({}) failed with error {} - {}", PID, error, indent_message("  ", message.value_or("")));
		return false;
	}
	fmt::print(stream, "Attached to console of process {}\n", PID);
	//HWND hwnd = GetConsoleWindow();
	//if (hwnd != NULL) {

	//	if (!ShowWindow(hwnd, SW_SHOWNORMAL)) {
	//		//auto error = GetLastError();
	//		//auto message = get_error_message(error);
	//		//fmt::print(stream, "ShowWindow() failed with error {} - {}", error, indent_message("  ", message.value_or("")));
	//		//return false;
	//	}
	//}
	////else {
	////	fmt::print(stream, "GetConsoleWindow() returned NULL.\n");
	////	return false;
	////}

	return PrintInfo(stream, change_mode);
}

std::optional<std::string> GetProgPath(FILE*streamErr) {
	std::string ret{};
	for (DWORD buffer_size = 0x100u; buffer_size < 0x10000u && buffer_size >= 0x100u; buffer_size *= 2u) {
		auto prog_name = std::make_unique<CHAR[]>(buffer_size);
		DWORD result = GetModuleFileNameA(nullptr, prog_name.get(), buffer_size);

		if (result == 0) {
			auto error = GetLastError();
			fmt::print(streamErr, "Couldn't get name of the program - {}\n", get_error_message(error).value_or(""));
			return std::nullopt;
		}

		if (result < buffer_size) {
			return std::string(prog_name.get());
		}

		auto error = GetLastError();
		if (error != ERROR_INSUFFICIENT_BUFFER)
			continue;

		fmt::print(streamErr, "Unexpected error: {}\n", get_error_message(error).value_or(""));
		return std::nullopt;
	}
	
	fmt::print(streamErr, "The path to this program is to long.\n");
	return std::nullopt;
}

bool SpawnSelf(FILE* fOut, FILE* fErr, DWORD pid, change_con_mode change_mode) {

	//HANDLE hOut = std::bit_cast<HANDLE>(_get_osfhandle(_fileno(fOut)));
	//HANDLE hErr = std::bit_cast<HANDLE>(_get_osfhandle(_fileno(fErr)));
	bool ret_value = false;
	DWORD exitCode{};
	std::string prog_path{};
	STARTUPINFOA startupinfo{};
	PROCESS_INFORMATION procinfo{};
	std::string cmd_line{};
	std::unique_ptr<char[]> mutable_cmd_line_buf{};

	SECURITY_ATTRIBUTES sa{ .nLength{sizeof(sa)}, .lpSecurityDescriptor{nullptr}, .bInheritHandle{true} };

	HANDLE hChildStdOut_read{ nullptr };
	HANDLE hChildStdOut_write{ nullptr };
	if (!CreatePipe(&hChildStdOut_read, &hChildStdOut_write, &sa, 0)) {
		fmt::print(fErr, "Failed to create pipe\n");
		goto cleanup;
	}
	{
		auto prog_path_opt = GetProgPath(fErr);
		if (!prog_path_opt.has_value()) {
			goto cleanup;
		}
		prog_path = *prog_path_opt;
	}

	startupinfo.cb = sizeof(startupinfo);

	// startupinfo.hStdError  = g_hChildStd_OUT_Wr;
	// startupinfo.hStdOutput = g_hChildStd_OUT_Wr;
	// startupinfo.hStdInput  = g_hChildStd_IN_Rd;
	// startupinfo.dwFlags   |= STARTF_USESTDHANDLES;
	{
		auto set_conin_str = change_mode.conin.to_string();
		auto set_conout_str = change_mode.conout.to_string();
		if (!set_conin_str ||!set_conout_str) {
			fmt::print(fErr, "internal error");
			goto cleanup;
		}


		cmd_line = fmt::format("\"{0}\" --pid {1} --handle-out {2} --handle-err {2} --no-self-spawn --set-in-mode \"{3}\" --set-out-mode \"{4}\"",
			prog_path, pid, reinterpret_cast<uintptr_t>(hChildStdOut_write),
			*set_conin_str, *set_conout_str
			);
	}

	mutable_cmd_line_buf = std::make_unique<char[]>(cmd_line.length() + 1);
	std::memcpy(mutable_cmd_line_buf.get(), cmd_line.c_str(), cmd_line.length() * sizeof(char));
	mutable_cmd_line_buf.get()[cmd_line.length()] = '\0';


	if (!CreateProcessA(prog_path.c_str(), mutable_cmd_line_buf.get(), nullptr, nullptr, true, 0, nullptr, nullptr, &startupinfo, &procinfo)) {
		auto error = GetLastError();
		fmt::print(fErr, "Couldn't create child process - {:#x} {}\n", error, get_error_message(error).value_or(""));
		goto cleanup;
	}
	CloseHandle(procinfo.hThread);
	procinfo.hThread = nullptr;

	CloseHandle(hChildStdOut_write);
	hChildStdOut_write = nullptr;

	{
		constexpr const DWORD BUFF_SIZE{ 512 };
		char szBuffer[BUFF_SIZE]{};
		constexpr const auto element_size{ sizeof(szBuffer[0]) };
		static_assert(element_size == 1);

		DWORD dwBytesRead{};
		size_t bytesWritten{};
		bool bRead{ false };

		do {
			bRead = ReadFile(hChildStdOut_read, szBuffer, BUFF_SIZE, &dwBytesRead, nullptr);
			if (!bRead)
				break;
			if (dwBytesRead == 0)
				break;
			if (dwBytesRead > BUFF_SIZE) {
				fmt::print(fErr, "Unexpected error when using ReadFile().\n");
				goto cleanup;
			}

			std::string_view sv(szBuffer, dwBytesRead);
			fmt::print(fOut, "{}", sv); // this can also print zero bytes ('\0' ASCII NUL)
		} while (true);
	}


	WaitForSingleObject(procinfo.hProcess, INFINITE);

	if (0 == GetExitCodeProcess(procinfo.hProcess, &exitCode)) {
		fmt::print(fErr, "Failed to get Exit Code of Process.\n");
		goto cleanup;
	}

	if (exitCode != 0) {
		fmt::print(fErr, "Child process failed with exited code: {}", exitCode);
		goto cleanup;
	}

	//hPipeListenerThread = reinterpret_cast<HANDLE>(_beginthread(PipeListener, 0, hPipeIn));
	ret_value = true;
cleanup:
	if (procinfo.hProcess != nullptr) {
		CloseHandle(procinfo.hProcess);
		procinfo.hProcess = nullptr;
	}
	if (procinfo.hThread != nullptr) {
		CloseHandle(procinfo.hThread);
		procinfo.hThread = nullptr;
	}

	if (hChildStdOut_read) {
		CloseHandle(hChildStdOut_read);
		hChildStdOut_read = nullptr;
	}
	if (hChildStdOut_write) {
		CloseHandle(hChildStdOut_write);
		hChildStdOut_write = nullptr;
	}
		
	return ret_value;
}

int main(int argc, const char **argv) {
	_set_fmode(_O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
	_setmode(_fileno(stdin), _O_BINARY);

	if (argc <= 1) {
		if (!PrintInfo(stdout, change_con_mode{}))
			return 1;
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
	change_con_mode change_mode{};

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
		else if (current_arg == "--set-in-mode") {
			if (!next_arg) {
				fmt::print(stderr, "Missing value for option \"--set-in-mode\"\n");
				PrintUsage(stderr);
				return 1;
			}
			i += 1;
			auto opt_in_mode = parse_set_and_reset_string<DWORD>(*next_arg);
			if (!opt_in_mode) {
				fmt::print(stderr, "value for option \"--set-in-mode\" is in the wrong format.\n");
				PrintUsage(stderr);
				return 1;
			}
			change_mode.conin = *opt_in_mode;
		}
		else if (current_arg == "--set-out-mode") {
			if (!next_arg) {
				fmt::print(stderr, "Missing value for option \"--set-out-mode\"\n");
				PrintUsage(stderr);
				return 1;
			}
			i += 1;
			auto opt_out_mode = parse_set_and_reset_string<DWORD>(*next_arg);
			if (!opt_out_mode) {
				fmt::print(stderr, "value for option \"--set-in-mode\" is in the wrong format.\n");
				PrintUsage(stderr);
				return 1;
			}
			change_mode.conout = *opt_out_mode;
		}
		else {
			fmt::print(stderr, "Argument {}{}{} could not be interpreted\n", quote_open, current_arg, quote_close);
			PrintUsage(stderr);
			return 1;
		}
	}

	int fdOut{};
	FILE* fOut = stdout;
	FILE* fErr = stderr;

	if (handle_out) {
		fdOut = _open_osfhandle(handle_out.value(), 0);
		if (fdOut == -1)
			return 1;
		fOut = _fdopen(fdOut, "w");
		if (fOut == nullptr)
			return 1;
	}

	if (handle_err) {
		int fdErr{};
		if (handle_out && *handle_out == *handle_err) {
			fdErr = _dup(fdOut);
			if (fdErr == -1)
				return 1;
		}
		else {
			fdErr = _open_osfhandle(handle_err.value(), 0);
			if (fdErr == -1)
				return 1;
		}
		fErr = _fdopen(fdErr, "w");
		if (fOut == nullptr)
			return 1;
		
	}


	if (PID.has_value()) {
		if (no_self_spawn) {
			if (!AttachToConsoleAndPrintInfo(fOut, *PID, change_mode))
				return 1;
		}
		else {
			if (!SpawnSelf(fOut,fErr,*PID, change_mode))
				return 1;
		}
	}
	else {
		if(!PrintInfo(fOut, change_mode))
			return 1;
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