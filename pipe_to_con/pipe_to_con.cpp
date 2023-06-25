#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <optional>

#include <fmt/core.h>


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


bool ReadPipeWriteConsole(HANDLE hPipe)
{
	constexpr const DWORD BUFF_SIZE{ 512 };
	static_assert(BUFF_SIZE % sizeof(wchar_t) == 0);
	alignas(wchar_t) char szBuffer[BUFF_SIZE]{};
	constexpr const auto element_size{ sizeof(szBuffer[0]) };
	static_assert(element_size == 1);
	bool success_ret = true;


	HANDLE hStdOut = nullptr;
	hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdOut == INVALID_HANDLE_VALUE || hStdOut == nullptr)
	{
		fmt::print(stderr, "GetStdHandle(STD_OUTPUT_HANDLE) failed with {:#x}\n", GetLastError());
		hStdOut = nullptr;
		success_ret = false;
		goto cleanup;
	}

	{
		DWORD dummy;
		if (!GetConsoleMode(hStdOut, &dummy)) {
			fmt::print(stderr, "stdout is not a console\n");
			success_ret = false;
			goto cleanup;
		}
	}

	{
		char* const read_end_ptr = (&szBuffer[0]) + BUFF_SIZE;
		static_assert(sizeof(wchar_t) == 2);

		// read_start_ptr is either:
		// - odd: `&szBuffer[1]`
		// - even: `&szBuffer[2]`
		char* read_start_ptr = &szBuffer[sizeof(wchar_t)]; // we begin with even.
		// If read_start_ptr is odd, than we have one byte more from the last round in `szBuffer[0]`.
		// So it is reasonable to begin with even, because in the beginning we don't have a byte from the last round.

		do {
			DWORD dwBytesRead{};
			bool bRead = ReadFile(hPipe, read_start_ptr, (read_end_ptr - read_start_ptr), &dwBytesRead, nullptr);
			if (!bRead)
				break;
			if (dwBytesRead == 0)
				break;
			if (dwBytesRead > (read_end_ptr - read_start_ptr)) {
				fmt::print(stderr, "Unexpected error when using ReadFile(): More bytes read than requested.\n");
				success_ret = false;
				goto cleanup;
			}

			// Do have an extra byte from the last round? (Yes, if odd).
			const bool read_start_ptr_odd = (std::bit_cast<uintptr_t>(read_start_ptr) % 2) != 0; 

			// Add +1, if we have an additonal byte from the last round
			const DWORD number_of_bytes_available = dwBytesRead + (read_start_ptr_odd ? 1 : 0);
			const bool available_odd = (number_of_bytes_available % 2) != 0;

			
			const DWORD absolute_number_of_wchars_to_write = number_of_bytes_available / sizeof(wchar_t); // round down
			const char* write_start_ptr = read_start_ptr_odd ? (read_start_ptr-1) : read_start_ptr; // must be even. So subtract 1, to make it even.
			DWORD absolute_number_of_wchars_written = 0;
			DWORD number_of_wchars_written = 0;
			bool success_write = true;
			if (absolute_number_of_wchars_to_write > 0)
			{
				while
					(
						absolute_number_of_wchars_written < absolute_number_of_wchars_to_write
						&&
						(
							success_write = ::WriteConsoleW(
								/* _In_             HANDLE  hConsoleOutput,         */ hStdOut,
								/* _In_       const VOID * lpBuffer,                */ write_start_ptr + (absolute_number_of_wchars_written * sizeof(wchar_t)),
								/* _In_             DWORD   nNumberOfCharsToWrite,  */ absolute_number_of_wchars_to_write - absolute_number_of_wchars_written,
								/* _Out_opt_        LPDWORD lpNumberOfCharsWritten, */ &number_of_wchars_written,
								/* _Reserved_       LPVOID  lpReserved              */ nullptr
							)
							)
						&&
						number_of_wchars_written > 0
						) {
					absolute_number_of_wchars_written += number_of_wchars_written;
				}
			}

			if
				(
					!success_write
					||
					absolute_number_of_wchars_written != absolute_number_of_wchars_to_write
				) {
				success_ret = false;
				goto cleanup;
			}
			
			// If the number of availble bytes is odd, than one byte couldn't be written, 
			// because we can only write an even number of bytes, because the size of wchar_t is 2 bytes.
			if (available_odd) {
				szBuffer[0] = *(read_start_ptr + dwBytesRead - 1);
				read_start_ptr = &szBuffer[1]; // make the read_start_ptr odd, to signal that we have one byte more
			}
			else {
				read_start_ptr = &szBuffer[sizeof(wchar_t)];
			}

		} while (true);
	}
cleanup:
	// hStdOut does not have to be closed

	return success_ret;
}
bool ReadConsoleWritePipe(HANDLE hPipe)
{
	constexpr const DWORD BUFF_SIZE{ 512 };
	static_assert(BUFF_SIZE % sizeof(wchar_t) == 0);
	alignas(wchar_t) char szBuffer[BUFF_SIZE]{};
	constexpr const auto element_size{ sizeof(szBuffer[0]) };
	static_assert(element_size == 1);


	HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
	if (hStdIn == INVALID_HANDLE_VALUE || hStdIn == nullptr)
	{
		fmt::print(stderr, "GetStdHandle(STD_INPUT_HANDLE) failed with {:#x}\n", GetLastError());
		return false;
	}

	{
		DWORD dummy;
		if (!GetConsoleMode(hStdIn, &dummy)) {
			fmt::print(stderr, "stdin is not a console\n");
			return false;
		}
	}

	DWORD dwWideCharsRead{};
	size_t bytesWritten{};
	bool bRead{ false };

	do {
		bRead = ReadConsoleW(hStdIn, szBuffer, BUFF_SIZE, &dwWideCharsRead, nullptr);
		if (!bRead)
			break;
		if (dwWideCharsRead == 0)
			break;
		if (dwWideCharsRead*sizeof(wchar_t) > BUFF_SIZE) {
			fmt::print(stderr, "Unexpected error when using ReadConsoleW().\n");
			goto cleanup;
		}

		{
			const DWORD absolute_number_of_bytes_to_write = dwWideCharsRead * sizeof(wchar_t);
			DWORD absolute_number_of_bytes_written = 0;
			DWORD number_of_bytes_written = 0;
			bool success = true;
			while
			(
				absolute_number_of_bytes_written < absolute_number_of_bytes_to_write
				&&
				(
					success = WriteFile(
						/*[in]                HANDLE       hFile,                  */ hPipe,
						/*[in]                LPCVOID      lpBuffer,               */ &szBuffer[absolute_number_of_bytes_written],
						/*[in]                DWORD        nNumberOfBytesToWrite,  */ absolute_number_of_bytes_to_write - absolute_number_of_bytes_written,
						/*[out, optional]     LPDWORD      lpNumberOfBytesWritten, */ &number_of_bytes_written,
						/*[in, out, optional] LPOVERLAPPED lpOverlapped            */ nullptr
					)
				)
				&&
				number_of_bytes_written > 0
			){
				absolute_number_of_bytes_written += number_of_bytes_written;
			}
			
			if
			(
				!success
				||
				absolute_number_of_bytes_written != absolute_number_of_bytes_to_write
			){
				return false;
			}
		}

	} while (true);

	return true;
}


bool AttachToConsole(uint32_t pid, HANDLE in_or_out, bool handle_is_in);
bool SpawnSelf(uint32_t pid, bool to_secondary)
{

	bool ret_value = false;
	DWORD exitCode{};
	std::string prog_path{};
	STARTUPINFOA startupinfo{};
	PROCESS_INFORMATION procinfo{};
	std::string cmd_line{};
	std::unique_ptr<char[]> mutable_cmd_line_buf{};

	SECURITY_ATTRIBUTES sa{ .nLength{sizeof(sa)}, .lpSecurityDescriptor{nullptr}, .bInheritHandle{true} };

	HANDLE h_read{ nullptr };
	HANDLE h_write{ nullptr };
	if (!CreatePipe(&h_read, &h_write, &sa, 0)) {
		fmt::print(stderr, "Failed to create pipe\n");
		goto cleanup;
	}
	{
		auto prog_path_opt = GetProgPath(fErr);
		if (!prog_path_opt.has_value()) {
			goto cleanup;
		}
		prog_path = *prog_path_opt;
	}

	HANDLE& handle_for_secondary = to_secondary ? h_read  : h_write;
	HANDLE& handle_for_us        = to_secondary ? h_write : h_read;

	startupinfo.cb = sizeof(startupinfo);

	//startupinfo.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
	//startupinfo.hStdOutput = INVALID_HANDLE_VALUE;
	//startupinfo.hStdInput  = INVALID_HANDLE_VALUE;
	//startupinfo.dwFlags   |= STARTF_USESTDHANDLES;
	cmd_line = fmt::format("\"{}\" --pid {} --handle {} --secondary --{}-secondary ",
		prog_path, pid, std::bit_cast<uintptr_t>(handle_for_secondary),
		(to_secondary ? "to" : "from")
		);

	mutable_cmd_line_buf = std::make_unique<char[]>(cmd_line.length() + 1);
	std::memcpy(mutable_cmd_line_buf.get(), cmd_line.c_str(), cmd_line.length() * sizeof(char));
	mutable_cmd_line_buf.get()[cmd_line.length()] = '\0';


	if (!CreateProcessA(prog_path.c_str(), mutable_cmd_line_buf.get(), nullptr, nullptr, true, 0, nullptr, nullptr, &startupinfo, &procinfo)) {
		auto error = GetLastError();
		fmt::print(stderr, "Couldn't create child process - {:#x} {}\n", error, get_error_message(error).value_or(""));
		goto cleanup;
	}
	CloseHandle(procinfo.hThread);
	procinfo.hThread = nullptr;

	CloseHandle(handle_for_secondary);
	handle_for_secondary = nullptr;

	{
		// TODO: read or write
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
}

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

	std::optional<uint32_t> PID{ std::nullopt };
	std::optional<intptr_t> handle_in_or_out{ std::nullopt };
	bool to_secondary { false };
	bool from_secondary { false };
	bool secondary{ false };


	// TODO: parse options

	if (!PID.has_value()) {
		fmt::print(stderr,
				"Error: Must specify a process identifier with the option \"--pid\"\n");
		return 1;
	}

	if (to_secondary == from_secondary) {
		if (to_secondary) {
			fmt::print(stderr,
					"Error: You are not allowed to set both options "
					"\"--to-secondary\" and \"from-secondary\"\n");
		}else{
			fmt::print(stderr,
					"Error: You must specifiy one of these options: "
					"\"--to-secondary\", \"from-secondary\"\n");
		}
		return 1;
	}

	if (secondary) {
		if(!handle_in_or_out.has_value()) {
			fmt::print(stderr,
					"Error: You must specify a handle value, for the secondary process.\n");
			return 1;
		}
		const intptr_t handle_intptr = handle_in_or_out.value();
		const HANDLE handle = std::bit_cast<HANDLE>(handle_intptr);
		const bool is_handle_input = to_secondary;
		if(!AttachToConsole(PID.value(), handle, is_handle_input)) {
			return 1;
		}
		return 0;
	}else{
		if(!handle_in_or_out.has_value()) {
			fmt::print(stderr,
					"Error: You must not specify a handle value, for the primary process.\n");
			return 1;
		}
		if (!SpawnSelf(PID.value(), to_secondary)){
			return 1;
		}
		return 0;
	}
	

	return 0;
}
