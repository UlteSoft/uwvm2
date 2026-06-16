#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include "../../win32_family.h"
#include "../../../fast_io_core_impl/socket/posix_sockaddr.h"

namespace fast_io::win32
{

/**
 * @brief fast_io will never define these types which defined in WDK
 */
// using NTSTATUS = ::std::int_least32_t;
// using LUID = ::std::int_least64_t;

struct overlapped
{
	::std::conditional_t<(sizeof(::std::size_t) > 4), ::std::uint_least64_t, ::std::uint_least32_t> Internal,
		InternalHigh;
	union dummy_union_name_t
	{
		struct dummy_struct_name_t
		{
			::std::uint_least32_t Offset;
			::std::uint_least32_t OffsetHigh;
		} dummy_struct_name;
		void *Pointer;
	} dummy_union_name;
	void *hEvent;
};

struct security_attributes
{
	::std::uint_least32_t nLength;
	void *lpSecurityDescriptor;
	int bInheritHandle;
};

struct startupinfoa
{
	::std::uint_least32_t cb;
	char *lpReserved;
	char *lpDesktop;
	char *lpTitle;
	::std::uint_least32_t dwX;
	::std::uint_least32_t dwY;
	::std::uint_least32_t dwXSize;
	::std::uint_least32_t dwYSize;
	::std::uint_least32_t dwXCountChars;
	::std::uint_least32_t dwYCountChars;
	::std::uint_least32_t dwFillAttribute;
	::std::uint_least32_t dwFlags;
	::std::uint_least16_t wShowWindow;
	::std::uint_least16_t cbReserved2;
	::std::uint_least8_t *lpReserved2;
	void *hStdInput;
	void *hStdOutput;
	void *hStdError;
};

struct startupinfow
{
	::std::uint_least32_t cb;
	char16_t *lpReserved;
	char16_t *lpDesktop;
	char16_t *lpTitle;
	::std::uint_least32_t dwX;
	::std::uint_least32_t dwY;
	::std::uint_least32_t dwXSize;
	::std::uint_least32_t dwYSize;
	::std::uint_least32_t dwXCountChars;
	::std::uint_least32_t dwYCountChars;
	::std::uint_least32_t dwFillAttribute;
	::std::uint_least32_t dwFlags;
	::std::uint_least16_t wShowWindow;
	::std::uint_least16_t cbReserved2;
	::std::uint_least8_t *lpReserved2;
	void *hStdInput;
	void *hStdOutput;
	void *hStdError;
};

struct process_information
{
	void *hProcess;
	void *hThread;
	::std::uint_least32_t dwProcessId;
	::std::uint_least32_t dwThreadId;
};

/*
https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getfileinformationbyhandleex
*/

struct file_standard_info
{
	::std::int_least64_t AllocationSize;
	::std::int_least64_t EndOfFile;
	::std::uint_least32_t NumberOfLinks;
	int DeletePending;
	int Directory;
};

struct file_attribute_tag_info
{
	::std::uint_least32_t FileAttributes;
	::std::uint_least32_t ReparseTag;
};

enum class file_info_by_handle_class
{
	FileBasicInfo,
	FileStandardInfo,
	FileNameInfo,
	FileRenameInfo,
	FileDispositionInfo,
	FileAllocationInfo,
	FileEndOfFileInfo,
	FileStreamInfo,
	FileCompressionInfo,
	FileAttributeTagInfo,
	FileIdBothDirectoryInfo,
	FileIdBothDirectoryRestartInfo,
	FileIoPriorityHintInfo,
	FileRemoteProtocolInfo,
	FileFullDirectoryInfo,
	FileFullDirectoryRestartInfo,
	FileStorageInfo,
	FileAlignmentInfo,
	FileIdInfo,
	FileIdExtdDirectoryInfo,
	FileIdExtdDirectoryRestartInfo,
	FileDispositionInfoEx,
	FileRenameInfoEx,
	FileCaseSensitiveInfo,
	FileNormalizedNameInfo,
	MaximumFileInfoByHandleClass
};

struct filetime
{
	::std::uint_least32_t dwLowDateTime, dwHighDateTime;
};

struct by_handle_file_information
{
	::std::uint_least32_t dwFileAttributes;
	filetime ftCreationTime;
	filetime ftLastAccessTime;
	filetime ftLastWriteTime;
	::std::uint_least32_t dwVolumeSerialNumber;
	::std::uint_least32_t nFileSizeHigh;
	::std::uint_least32_t nFileSizeLow;
	::std::uint_least32_t nNumberOfLinks;
	::std::uint_least32_t nFileIndexHigh;
	::std::uint_least32_t nFileIndexLow;
};

struct coord
{
	::std::int_least16_t X, Y;
};

struct small_rect
{
	::std::int_least16_t Left, Top, Right, Bottom;
};

struct char_info
{
	char16_t character;
	::std::uint_least16_t Attrib;
};

struct console_screen_buffer_info
{
	coord Size, CursorPosition;
	::std::uint_least16_t Attrib;
	small_rect Window;
	coord MaxWindowSize;
};

struct guid
{
	unsigned long Data1;
	unsigned short Data2;
	unsigned short Data3;
	unsigned char Data4[8];
};

inline constexpr ::std::size_t wsaprotocol_len{255};

struct wsaprotocolchain
{
	int ChainLen;
	::std::uint_least32_t ChainEntries[7];
};

struct wsaprotocol_infow
{
	::std::uint_least32_t dwServiceFlags1;
	::std::uint_least32_t dwServiceFlags2;
	::std::uint_least32_t dwServiceFlags3;
	::std::uint_least32_t dwServiceFlags4;
	::std::uint_least32_t dwProviderFlags;
	guid ProviderId;
	::std::uint_least32_t dwCatalogEntryId;
	wsaprotocolchain ProtocolChain;
	int iVersion;
	int iAddressFamily;
	int iMaxSockAddr;
	int iMinSockAddr;
	int iSocketType;
	int iProtocol;
	int iProtocolMaxOffset;
	int iNetworkByteOrder;
	int iSecurityScheme;
	::std::uint_least32_t dwMessageSize;
	::std::uint_least32_t dwProviderReserved;
	char16_t szProtocol[wsaprotocol_len + 1];
};

struct wsaprotocol_infoa
{
	::std::uint_least32_t dwServiceFlags1;
	::std::uint_least32_t dwServiceFlags2;
	::std::uint_least32_t dwServiceFlags3;
	::std::uint_least32_t dwServiceFlags4;
	::std::uint_least32_t dwProviderFlags;
	guid ProviderId;
	::std::uint_least32_t dwCatalogEntryId;
	wsaprotocolchain ProtocolChain;
	int iVersion;
	int iAddressFamily;
	int iMaxSockAddr;
	int iMinSockAddr;
	int iSocketType;
	int iProtocol;
	int iProtocolMaxOffset;
	int iNetworkByteOrder;
	int iSecurityScheme;
	::std::uint_least32_t dwMessageSize;
	::std::uint_least32_t dwProviderReserved;
	char szProtocol[wsaprotocol_len + 1];
};

inline constexpr ::std::size_t wsadescription_len{256};
inline constexpr ::std::size_t wsasys_status_len{128};

struct wsadata
{
	::std::uint_least16_t wVersion;
	::std::uint_least16_t wHighVersion;
#ifdef _WIN64
	unsigned short iMaxSockets;
	unsigned short iMaxUdpDg;
	char *lpVendorInfo;
	char szDescription[wsadescription_len + 1];
	char szSystemStatus[wsasys_status_len + 1];
#else
	char szDescription[wsadescription_len + 1];
	char szSystemStatus[wsasys_status_len + 1];
	unsigned short iMaxSockets;
	unsigned short iMaxUdpDg;
	char *lpVendorInfo;
#endif
};

struct wsabuf
{
	::std::uint_least32_t len;
	char *buf;
};

struct wsamsg
{
	void *name;
	int namelen;
	wsabuf *lpBuffers;
	::std::uint_least32_t dwBufferCount;
	wsabuf Control;
	::std::uint_least32_t dwflags;
};

inline constexpr ::std::size_t fd_max_events{10u};

struct wsanetworkevents
{
	::std::int_least32_t lNetworkEvents;
	int iErrorCode[fd_max_events];
};

using ptimerapcroutine = void(
#if defined(_MSC_VER) && (!__has_cpp_attribute(__gnu__::__stdcall__) && !defined(__WINE__))
	__stdcall
#elif (__has_cpp_attribute(__gnu__::__stdcall__) && !defined(__WINE__))
	__attribute__((__stdcall__))
#endif
		*)(void *, ::std::uint_least32_t, ::std::uint_least32_t) noexcept;

using lpwsaoverlapped_completion_routine = void(
#if defined(_MSC_VER) && (!__has_cpp_attribute(__gnu__::__stdcall__) && !defined(__WINE__))
	__stdcall
#elif (__has_cpp_attribute(__gnu__::__stdcall__) && !defined(__WINE__))
	__attribute__((__stdcall__))
#endif
		*)(::std::uint_least32_t dwError, ::std::uint_least32_t cbTransferred, overlapped *lpOverlapped,
		   ::std::uint_least32_t dwFlags) noexcept;

struct flowspec
{
	::std::uint_least32_t TokenRate;
	::std::uint_least32_t TokenBucketSize;
	::std::uint_least32_t PeakBandwidth;
	::std::uint_least32_t Latency;
	::std::uint_least32_t DelayVariation;
	::std::uint_least32_t ServiceType;
	::std::uint_least32_t MaxSduSize;
	::std::uint_least32_t MinimumPolicedSize;
};

struct qualityofservice
{
	flowspec SendingFlowspec;
	flowspec ReceivingFlowspec;
	wsabuf ProviderSpecific;
};

using lpconditionproc = void(
#if defined(_MSC_VER) && (!__has_cpp_attribute(__gnu__::__stdcall__) && !defined(__WINE__))
	__stdcall
#elif (__has_cpp_attribute(__gnu__::__stdcall__) && !defined(__WINE__))
	__attribute__((__stdcall__))
#endif
		*)(wsabuf *, wsabuf *, qualityofservice *, qualityofservice *, wsabuf *, wsabuf *, ::std::uint_least32_t *,
		   ::std::size_t) noexcept;

template <win32_family fam>
	requires(fam == win32_family::ansi_9x || fam == win32_family::wide_nt)
struct
#if __has_cpp_attribute(__gnu__::__may_alias__)
	[[__gnu__::__may_alias__]]
#endif
	win32_family_addrinfo
{
	int ai_flags{};
	int ai_family{};
	int ai_socktype{};
	int ai_protocol{};
	::std::size_t ai_addrlen{};
	::std::conditional_t<fam == win32_family::ansi_9x, char, char16_t> *ai_canonname{};
	posix_sockaddr *ai_addr{};
	win32_family_addrinfo<fam> *ai_next{};
};

using win32_addrinfo_9xa = win32_family_addrinfo<win32_family::ansi_9x>;
using win32_addrinfo_ntw = win32_family_addrinfo<win32_family::wide_nt>;

struct systemtime
{
	::std::uint_least16_t wYear;
	::std::uint_least16_t wMonth;
	::std::uint_least16_t wDayOfWeek;
	::std::uint_least16_t wDay;
	::std::uint_least16_t wHour;
	::std::uint_least16_t wMinute;
	::std::uint_least16_t wSecond;
	::std::uint_least16_t wMilliseconds;
};

struct time_zone_information
{
	::std::int_least32_t Bias;
	char16_t StandardName[32];
	systemtime StandardDate;
	::std::int_least32_t StandardBias;
	char16_t DaylightName[32];
	systemtime DaylightDate;
	::std::int_least32_t DaylightBias;
};

struct system_info
{
	union DUMMYUNIONNAMEU
	{
		::std::uint_least32_t dwOemId; // Obsolete field...do not use

		struct DUMMYSTRUCTNAMET
		{
			::std::uint_least16_t wProcessorArchitecture;
			::std::uint_least16_t wReserved;
		} DUMMYSTRUCTNAME;
	} DUMMYUNIONNAME;

	::std::uint_least32_t dwPageSize;
	void *lpMinimumApplicationAddress;
	void *lpMaximumApplicationAddress;
	::std::size_t dwActiveProcessorMask;
	::std::uint_least32_t dwNumberOfProcessors;
	::std::uint_least32_t dwProcessorType;
	::std::uint_least32_t dwAllocationGranularity;
	::std::uint_least16_t wProcessorLevel;
	::std::uint_least16_t wProcessorRevision;
};

struct file_basic_info
{
	::std::int_least64_t CreationTime;
	::std::int_least64_t LastAccessTime;
	::std::int_least64_t LastWriteTime;
	::std::int_least64_t ChangeTime;
	::std::uint_least32_t FileAttributes;
};

struct win32_find_dataa
{
	::std::uint_least32_t dwFileAttributes;
	filetime ftCreationTime;
	filetime ftLastAccessTime;
	filetime ftLastWriteTime;
	::std::uint_least32_t nFileSizeHigh;
	::std::uint_least32_t nFileSizeLow;
	::std::uint_least32_t dwReserved0;
	::std::uint_least32_t dwReserved1;
	char cFileName[260];
	char cAlternateFileName[14];
#ifdef _MAC
	::std::uint_least32_t dwFileType;
	::std::uint_least32_t dwCreatorType;
	::std::uint_least16_t wFinderFlags;
#endif
};

struct win32_find_dataw
{
	::std::uint_least32_t dwFileAttributes;
	filetime ftCreationTime;
	filetime ftLastAccessTime;
	filetime ftLastWriteTime;
	::std::uint_least32_t nFileSizeHigh;
	::std::uint_least32_t nFileSizeLow;
	::std::uint_least32_t dwReserved0;
	::std::uint_least32_t dwReserved1;
	char16_t cFileName[260];
	char16_t cAlternateFileName[14];
#if defined(_68K_) || defined(_MPPC_)
	::std::uint_least32_t dwFileType;
	::std::uint_least32_t dwCreatorType;
	::std::uint_least16_t wFinderFlags;
#endif
};

using farproc = ::std::ptrdiff_t(FAST_IO_STDCALL *)() noexcept;

struct win32_memory_range_entry
{
	void *VirtualAddress;
	::std::size_t NumberOfBytes;
};

inline constexpr ::std::size_t exception_maximum_parameters{15u};

struct exception_record
{
	::std::uint_least32_t ExceptionCode;
	::std::uint_least32_t ExceptionFlags;
	exception_record *ExceptionRecord;
	void *ExceptionAddress;
	::std::uint_least32_t NumberParameters;
	::std::size_t ExceptionInformation[exception_maximum_parameters];
};

struct exception_record32
{
	::std::uint_least32_t ExceptionCode;
	::std::uint_least32_t ExceptionFlags;
	::std::uint_least32_t ExceptionRecord;
	::std::uint_least32_t ExceptionAddress;
	::std::uint_least32_t NumberParameters;
	::std::uint_least32_t ExceptionInformation[exception_maximum_parameters];
};

struct exception_record64
{
	::std::uint_least32_t ExceptionCode;
	::std::uint_least32_t ExceptionFlags;
	::std::uint_least64_t ExceptionRecord;
	::std::uint_least64_t ExceptionAddress;
	::std::uint_least32_t NumberParameters;
	::std::uint_least32_t UnusedAlignment;
	::std::uint_least64_t ExceptionInformation[exception_maximum_parameters];
};

struct exception_pointers
{
	exception_record *ExceptionRecord;
	void *ContextRecord;
};

using pvectored_exception_handler = ::std::int_least32_t(FAST_IO_WINSTDCALL *)(exception_pointers *) noexcept;

#if defined(_WIN32) && !defined(__CYGWIN__)

# if defined(__i386__) || defined(_M_IX86)
struct win_i386_floating_save_area
{
	::std::uint32_t ControlWord;
	::std::uint32_t StatusWord;
	::std::uint32_t TagWord;
	::std::uint32_t ErrorOffset;
	::std::uint32_t ErrorSelector;
	::std::uint32_t DataOffset;
	::std::uint32_t DataSelector;
	::std::uint8_t RegisterArea[80];
	::std::uint32_t Spare0;
};

struct win_i386_context
{
	::std::uint32_t ContextFlags;
	::std::uint32_t Dr0;
	::std::uint32_t Dr1;
	::std::uint32_t Dr2;
	::std::uint32_t Dr3;
	::std::uint32_t Dr6;
	::std::uint32_t Dr7;
	win_i386_floating_save_area FloatSave;
	::std::uint32_t SegGs;
	::std::uint32_t SegFs;
	::std::uint32_t SegEs;
	::std::uint32_t SegDs;
	::std::uint32_t Edi;
	::std::uint32_t Esi;
	::std::uint32_t Ebx;
	::std::uint32_t Edx;
	::std::uint32_t Ecx;
	::std::uint32_t Eax;
	::std::uint32_t Ebp;
	::std::uint32_t Eip;
	::std::uint32_t SegCs;
	::std::uint32_t EFlags;
	::std::uint32_t Esp;
	::std::uint32_t SegSs;
	::std::uint8_t ExtendedRegisters[512];
};
# endif

# if defined(__arm64ec__) || defined(_M_ARM64EC) || (defined(_WIN64) && (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)))
struct alignas(16) win64_m128a
{
	::std::uint64_t Low;
	::std::int64_t High;
};

struct win64_xmm_save_area32
{
	::std::uint16_t ControlWord;
	::std::uint16_t StatusWord;
	::std::uint8_t TagWord;
	::std::uint8_t Reserved1;
	::std::uint16_t ErrorOpcode;
	::std::uint32_t ErrorOffset;
	::std::uint16_t ErrorSelector;
	::std::uint16_t Reserved2;
	::std::uint32_t DataOffset;
	::std::uint16_t DataSelector;
	::std::uint16_t Reserved3;
	::std::uint32_t MxCsr;
	::std::uint32_t MxCsr_Mask;
	win64_m128a FloatRegisters[8];
	win64_m128a XmmRegisters[16];
	::std::uint8_t Reserved4[96];
};

struct win64_xmm_registers
{
	win64_m128a Header[2];
	win64_m128a Legacy[8];
	win64_m128a Xmm0;
	win64_m128a Xmm1;
	win64_m128a Xmm2;
	win64_m128a Xmm3;
	win64_m128a Xmm4;
	win64_m128a Xmm5;
	win64_m128a Xmm6;
	win64_m128a Xmm7;
	win64_m128a Xmm8;
	win64_m128a Xmm9;
	win64_m128a Xmm10;
	win64_m128a Xmm11;
	win64_m128a Xmm12;
	win64_m128a Xmm13;
	win64_m128a Xmm14;
	win64_m128a Xmm15;
};

struct alignas(16) win64_context
{
	::std::uint64_t P1Home;
	::std::uint64_t P2Home;
	::std::uint64_t P3Home;
	::std::uint64_t P4Home;
	::std::uint64_t P5Home;
	::std::uint64_t P6Home;
	::std::uint32_t ContextFlags;
	::std::uint32_t MxCsr;
	::std::uint16_t SegCs;
	::std::uint16_t SegDs;
	::std::uint16_t SegEs;
	::std::uint16_t SegFs;
	::std::uint16_t SegGs;
	::std::uint16_t SegSs;
	::std::uint32_t EFlags;
	::std::uint64_t Dr0;
	::std::uint64_t Dr1;
	::std::uint64_t Dr2;
	::std::uint64_t Dr3;
	::std::uint64_t Dr6;
	::std::uint64_t Dr7;
	::std::uint64_t Rax;
	::std::uint64_t Rcx;
	::std::uint64_t Rdx;
	::std::uint64_t Rbx;
	::std::uint64_t Rsp;
	::std::uint64_t Rbp;
	::std::uint64_t Rsi;
	::std::uint64_t Rdi;
	::std::uint64_t R8;
	::std::uint64_t R9;
	::std::uint64_t R10;
	::std::uint64_t R11;
	::std::uint64_t R12;
	::std::uint64_t R13;
	::std::uint64_t R14;
	::std::uint64_t R15;
	::std::uint64_t Rip;
	union
	{
		win64_xmm_save_area32 FltSave;
		win64_xmm_save_area32 FloatSave;
		win64_xmm_registers XmmRegisters;
	};
	win64_m128a VectorRegister[26];
	::std::uint64_t VectorControl;
	::std::uint64_t DebugControl;
	::std::uint64_t LastBranchToRip;
	::std::uint64_t LastBranchFromRip;
	::std::uint64_t LastExceptionToRip;
	::std::uint64_t LastExceptionFromRip;
};

struct win64_runtime_function
{
	::std::uint32_t BeginAddress;
	::std::uint32_t EndAddress;
	::std::uint32_t UnwindData;
};
# endif

# if defined(__arm__) || defined(_M_ARM)
struct win_arm_neon128
{
	::std::uint64_t Low;
	::std::int64_t High;
};

struct alignas(8) win_arm_context
{
	::std::uint32_t ContextFlags;
	::std::uint32_t R0;
	::std::uint32_t R1;
	::std::uint32_t R2;
	::std::uint32_t R3;
	::std::uint32_t R4;
	::std::uint32_t R5;
	::std::uint32_t R6;
	::std::uint32_t R7;
	::std::uint32_t R8;
	::std::uint32_t R9;
	::std::uint32_t R10;
	::std::uint32_t R11;
	::std::uint32_t R12;
	::std::uint32_t Sp;
	::std::uint32_t Lr;
	::std::uint32_t Pc;
	::std::uint32_t Cpsr;
	::std::uint32_t Fpscr;
	::std::uint32_t Padding;
	union
	{
		win_arm_neon128 Q[16];
		::std::uint64_t D[32];
		::std::uint32_t S[32];
	} Neon;
	::std::uint32_t Bvr[8];
	::std::uint32_t Bcr[8];
	::std::uint32_t Wvr[1];
	::std::uint32_t Wcr[1];
	::std::uint32_t Padding2[2];
};

struct win_arm_runtime_function
{
	::std::uint32_t BeginAddress;
	::std::uint32_t UnwindData;
};
# endif

# if defined(_WIN64) && (defined(__aarch64__) || defined(_M_ARM64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
union win_arm64_neon128
{
	struct
	{
		::std::uint64_t Low;
		::std::int64_t High;
	} Integer;
	double D[2];
	float S[4];
	::std::uint16_t H[8];
	::std::uint8_t B[16];
};

struct alignas(16) win_arm64_context
{
	::std::uint32_t ContextFlags;
	::std::uint32_t Cpsr;
	::std::uint64_t X[31];
	::std::uint64_t Sp;
	::std::uint64_t Pc;
	win_arm64_neon128 V[32];
	::std::uint32_t Fpcr;
	::std::uint32_t Fpsr;
	::std::uint32_t Bcr[8];
	::std::uint64_t Bvr[8];
	::std::uint32_t Wcr[2];
	::std::uint64_t Wvr[2];
};

struct win_arm64_runtime_function
{
	::std::uint32_t BeginAddress;
	::std::uint32_t UnwindData;
};
# endif

# if defined(__arm64ec__) || defined(_M_ARM64EC)
using win_arm64ec_context = win64_context;
# endif

inline constexpr ::std::uint32_t win_unwind_flag_nhandler{};
inline constexpr ::std::uint32_t win64_unwind_flag_nhandler{};

# if defined(__i386__) || defined(_M_IX86)
static_assert(sizeof(win_i386_floating_save_area) == 112);
static_assert(sizeof(win_i386_context) == 716);
# endif
# if defined(__arm64ec__) || defined(_M_ARM64EC) || (defined(_WIN64) && (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)))
static_assert(sizeof(win64_m128a) == 16);
static_assert(alignof(win64_m128a) == 16);
static_assert(sizeof(win64_xmm_save_area32) == 512);
static_assert(sizeof(win64_xmm_registers) == 416);
static_assert(sizeof(win64_context) == 1232);
static_assert(alignof(win64_context) == 16);
static_assert(sizeof(win64_runtime_function) == 12);
# endif
# if defined(__arm__) || defined(_M_ARM)
static_assert(sizeof(win_arm_neon128) == 16);
static_assert(sizeof(win_arm_context) == 416);
static_assert(alignof(win_arm_context) == 8);
static_assert(sizeof(win_arm_runtime_function) == 8);
# endif
# if defined(_WIN64) && (defined(__aarch64__) || defined(_M_ARM64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
static_assert(sizeof(win_arm64_neon128) == 16);
static_assert(sizeof(win_arm64_context) == 912);
static_assert(alignof(win_arm64_context) == 16);
static_assert(sizeof(win_arm64_runtime_function) == 8);
# endif

# if defined(__arm64ec__) || defined(_M_ARM64EC)
struct win64_nonvolatile_context_pointers;
using win_current_context = win_arm64ec_context;
using win_current_runtime_function = win64_runtime_function;
using win_current_nonvolatile_context_pointers = win64_nonvolatile_context_pointers;
using win_current_unwind_address = ::std::uint64_t;
# elif defined(_WIN64) && (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64))
struct win64_nonvolatile_context_pointers;
using win_current_context = win64_context;
using win_current_runtime_function = win64_runtime_function;
using win_current_nonvolatile_context_pointers = win64_nonvolatile_context_pointers;
using win_current_unwind_address = ::std::uint64_t;
# elif defined(_WIN64) && (defined(__aarch64__) || defined(_M_ARM64))
struct win_arm64_nonvolatile_context_pointers;
using win_current_context = win_arm64_context;
using win_current_runtime_function = win_arm64_runtime_function;
using win_current_nonvolatile_context_pointers = win_arm64_nonvolatile_context_pointers;
using win_current_unwind_address = ::std::uintptr_t;
# elif defined(__arm__) || defined(_M_ARM)
struct win_arm_nonvolatile_context_pointers;
using win_current_context = win_arm_context;
using win_current_runtime_function = win_arm_runtime_function;
using win_current_nonvolatile_context_pointers = win_arm_nonvolatile_context_pointers;
using win_current_unwind_address = ::std::uint32_t;
# elif defined(__i386__) || defined(_M_IX86)
using win_current_context = win_i386_context;
# endif

#endif

using address_family = ::std::uint_least16_t;

inline constexpr ::std::size_t ss_maxsize{128u};
inline constexpr ::std::size_t ss_alignsize{sizeof(::std::int_least64_t)};
inline constexpr ::std::size_t ss_pad1size{ss_alignsize - sizeof(::std::uint_least16_t)};
inline constexpr ::std::size_t ss_pad2size{ss_maxsize - (sizeof(::std::uint_least16_t) + ss_pad1size + ss_alignsize)};

//
// Definitions used for sockaddr_storage structure paddings design.
//

struct sockaddr_storage
{
	address_family ss_family;        // address family
	char ss_pad1[ss_pad1size];       // 6 byte pad, this is to make
									 //   implementation specific pad up to
									 //   alignment field that follows explicit
									 //   in the data structure
	::std::int_least64_t __ss_align; // Field to force desired structure
	char ss_pad2[ss_pad2size];       // 112 byte pad to achieve desired size;
									 //   _SS_MAXSIZE value minus size of
									 //   ss_family, __ss_pad1, and
									 //   __ss_align fields is 112
};

struct in_addr
{
	union S_un_u
	{
		struct S_un_b_t
		{
			::std::uint_least8_t s_b1, s_b2, s_b3, s_b4;
		} S_un_b;
		struct S_un_w_t
		{
			::std::uint_least16_t s_w1, s_w2;
		} S_un_w;
		::std::uint_least32_t S_addr;
	} S_un;
};

struct sockaddr_in
{
	address_family sin_family;
	::std::uint_least16_t sin_port;
	in_addr sin_addr;
	char sin_zero[8];
};

struct in6_addr
{
	union u_u
	{
		::std::uint_least8_t Byte[16];
		::std::uint_least16_t Word[8];
	} u;
};

struct scope_id
{
	union DUMMYUNIONNAME_U
	{
		struct DUMMYSTRUCTNAME_T
		{
			::std::uint_least32_t Zone : 28;
			::std::uint_least32_t Level : 4;
		} DUMMYSTRUCTNAME;
		::std::uint_least32_t Value;
	} DUMMYUNIONNAME;
};

struct sockaddr_in6
{
	address_family sin6_family;          // AF_INET6.
	::std::uint_least16_t sin6_port;     // Transport level port number.
	::std::uint_least32_t sin6_flowinfo; // IPv6 flow information.
	in6_addr sin6_addr;                  // IPv6 address.
	union sin6_scope_u
	{
		::std::uint_least32_t sin6_scope_id; // Set of interfaces for a scope.
		scope_id sin6_scope_struct;
	} sin6_scope;
};

} // namespace fast_io::win32
