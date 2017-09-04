/// @file 
/// Реализация класса абстрактного наблюдателя для Windows.

#include "abstractobserver.h"

#include <array>
#include <codecvt>

// Константа и макрос для перевода секунд в милисекунды
#define SEC_TO_MS 10000
#define TO_MS(var) static_cast<double>((var) / SEC_TO_MS)

// Делитель для перевода байт в килобайты
#define KBYTE 1024

namespace testtools
{

using std::list;
using std::string;

/// Функции-помощники AbstractObserver
namespace
{

/// Возвращает тект сообщения об ошибке по ее коду
/// @param[in] code Код ошибки
/// @return Сообщение соответствующее переданному коду
std::string GetErrorMessage(errno_t code)
{
    // Получаем дескриптор PDH библиотеки для корректного отображения сообщений
    // об ошибках связанных с ней.
    const HMODULE source = ::LoadLibraryA("pdh.dll");
    if (nullptr == source) {}

    LPSTR buffer = nullptr;
    DWORD length = ::FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_FROM_HMODULE |
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        static_cast<LPCVOID>(source),
        static_cast<DWORD>(code),
        static_cast<DWORD>(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)),
        reinterpret_cast<LPSTR>(&buffer), 0, nullptr
    );

    std::string message(static_cast<size_t>(length), '\0');
    if (nullptr != buffer) {
        message.assign(buffer);
        ::HeapFree(::GetProcessHeap(), 0, buffer);
    }

    return message;
}

/// Включает Debug привилегию
void EnableDebugPrivelege()
{
    HANDLE token = nullptr;
    LUID seDebugNameValue = { 0 };
    TOKEN_PRIVILEGES privileges = { 0 };
    if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        if (::LookupPrivilegeValueA(nullptr, SE_DEBUG_NAME, &seDebugNameValue)) {
            privileges.PrivilegeCount = 1;
            privileges.Privileges[0].Luid = seDebugNameValue;
            privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            if (::AdjustTokenPrivileges(token, FALSE, &privileges, sizeof(privileges), nullptr, nullptr)) {
                ::CloseHandle(token);
            }
            else {
                ::CloseHandle(token);
                throw AbstractObserver::SystemError(static_cast<errno_t>(::GetLastError()));
            }
        }
        else {
            ::CloseHandle(token);
            throw AbstractObserver::SystemError(static_cast<errno_t>(::GetLastError()));
        }
    }
    else {
        throw AbstractObserver::SystemError(static_cast<errno_t>(::GetLastError()));
    }
}

/// Заполняет информацию о расположении exe файла процесса
/// @throw AbstractObserver#SystemError
/// @param[in] handle Дескриптор процесса
/// @param[out] process Стуктура для хранения информации о процессе
void FillProcessPath(const HANDLE handle, AbstractObserver::Process& process)
{
    std::array<::CHAR, MAX_PATH> path = {'\0'};
    if (!::GetModuleFileNameExA(handle, nullptr, path.data(), path.size())) {
        if (ERROR_PARTIAL_COPY == ::GetLastError()) {}
        else throw AbstractObserver::SystemError(static_cast<errno_t>(::GetLastError()));
    }

    ::PathRemoveFileSpecA(path.data());
    process.path = path.data();
}

/// Заполняет информацию о владельце процесса
/// @throw AbstractObserver#SystemError
/// @param[in] handle Дескриптор процесса
/// @param[out] process Стуктура для хранения информации о процессе
void FillProcessOwner(const HANDLE handle, AbstractObserver::Process& process)
{
    HANDLE token = nullptr;
    if (!::OpenProcessToken(handle, TOKEN_QUERY, &token))
        throw AbstractObserver::SystemError(static_cast<errno_t>(::GetLastError()));

    PTOKEN_USER ptu = nullptr;
    DWORD length = 0;
    if (!::GetTokenInformation(token, TokenUser, nullptr, 0, &length))
        if (ERROR_INSUFFICIENT_BUFFER != ::GetLastError())
            throw AbstractObserver::SystemError(static_cast<errno_t>(::GetLastError()));

    ptu = reinterpret_cast<PTOKEN_USER>(::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, length));
    if (nullptr == ptu)
        throw AbstractObserver::SystemError(static_cast<errno_t>(::GetLastError()));

    if (!::GetTokenInformation(token, TokenUser, reinterpret_cast<LPVOID>(ptu), length, &length))
        throw AbstractObserver::SystemError(static_cast<errno_t>(::GetLastError()));

    char user[MAX_PATH] = { 0 };
    char domain[MAX_PATH] = { 0 };
    SID_NAME_USE use;
    if (!::LookupAccountSidA(nullptr, ptu->User.Sid, &user[0], &length, &domain[0], &length, &use)) {
        const DWORD result = ::GetLastError();
        if (ERROR_NONE_MAPPED == result) {
            std::strcpy(&user[0], "Unknown");
            std::strcpy(&domain[0], "Unknown");
        }
        else {
            throw AbstractObserver::SystemError(static_cast<errno_t>(::GetLastError()));
        }
    }

    if (nullptr != ptu)
        ::HeapFree(GetProcessHeap(), 0, ptu);

    ::CloseHandle(token);

    std::string owner(MAX_PATH, '\0');
    std::snprintf(&owner[0], owner.size(), "%s\\%s", domain, user);
    process.owner = owner;
}

/// Заполняет информацию о времени старта процесса, а также о загрузке процессора
/// @throw AbstractObserver#SystemError
/// @param[in] handle Дескриптор процесса
/// @param[out] process Стуктура для хранения информации о процессе
void FillProcessTimes(const HANDLE handle, AbstractObserver::Process& process)
{
    FILETIME start = { 0 };
    FILETIME finish = { 0 };
    FILETIME kernel = { 0 };
    FILETIME user = { 0 };
    if (!::GetProcessTimes(handle, &start, &finish, &kernel, &user))
        throw AbstractObserver::SystemError(static_cast<errno_t>(::GetLastError()));

    ULARGE_INTEGER liCreated = { 0 };
    liCreated.LowPart = start.dwLowDateTime;
    liCreated.HighPart = start.dwHighDateTime;

    process.start = static_cast<double>(TO_MS(liCreated.QuadPart));

    ULARGE_INTEGER liKernel = { 0 };
    liKernel.LowPart = kernel.dwLowDateTime;
    liKernel.HighPart = kernel.dwHighDateTime;

    process.ktime = static_cast<double>(TO_MS(liKernel.QuadPart));

    ULARGE_INTEGER liUser = { 0 };
    liUser.LowPart = user.dwLowDateTime;
    liUser.HighPart = user.dwHighDateTime;

    process.utime = static_cast<double>(TO_MS(liUser.QuadPart));
}

/// Заполняет информацию о потребляемой процессом памяти
/// @param[in] handle Дескриптор процесса
/// @param[out] process Стуктура для хранения информации о процессе
void FillProcessMemoryInfo(const HANDLE handle, AbstractObserver::Process& process)
{
    PROCESS_MEMORY_COUNTERS_EX pmcx = { 0 };
    pmcx.cb = sizeof(pmcx);
    ::GetProcessMemoryInfo(handle, reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&pmcx), sizeof(pmcx));

    process.pmemory = static_cast<double>(pmcx.WorkingSetSize / KBYTE);
    process.vmemory = static_cast<double>(pmcx.PrivateUsage / KBYTE);
}

/// Заполняет информацию о пколичестве открытых процессом дескрипторах
/// @throw AbstractObserver#SystemError
/// @param[in] handle Дескриптор процесса
/// @param[out] process Стуктура для хранения информации о процессе
void FillProcessHandleCount(const HANDLE handle, AbstractObserver::Process& process)
{
    DWORD count = 0;
    if (!::GetProcessHandleCount(handle, &count))
        throw AbstractObserver::SystemError(static_cast<errno_t>(::GetLastError()));

    process.handles = static_cast<uint32_t>(count);
}

/// Заполняет информацию о статусе процесса
/// @throw AbstractObserver#SystemError
/// @param[in] handle Дескриптор процесса
/// @param[out] process Стуктура для хранения информации о процессе
void FillProcessStatus(const HANDLE handle, AbstractObserver::Process& process)
{
    DWORD ec = 0;
    if (!::GetExitCodeProcess(handle, &ec))
        throw AbstractObserver::SystemError(static_cast<errno_t>(::GetLastError()));

    process.status = static_cast<uint32_t>(ec);
}

} // namespace

AbstractObserver::SystemError::SystemError(errno_t code)
    : Exception(GetErrorMessage(code)) {}

AbstractObserver::AbstractObserver(uint8_t type, const string& object)
    : type_(type)
    , object_(object)
    , query_(nullptr, &::PdhCloseQuery)
{
    pdh::Query query = nullptr;
    const pdh::Result result = ::PdhOpenQueryA(nullptr, 0, &query);
    if (ERROR_SUCCESS == result) query_.reset(query);
    else throw SystemError(static_cast<errno_t>(result));
}

pdh::Counter AbstractObserver::AddPdhCounter(pdh::Query query, const string& path)
{
    static const bool isWindowsVistaOrGreater = static_cast<bool>(IsWindowsVistaOrGreater());

    pdh::Counter counter = nullptr;
    pdh::Result result = ERROR_SUCCESS;
    
    /// @warning На Windows XP и Windows Server 2003 имеем проблему: путь к счетчику указывается
    /// только на языке установленном в системе. Поэтому если язык системы отличен от английского,
    /// при добавлении счетчика "схватим" ошибку, т.к. у нас в константах путь указан как раз
    /// на английском. В системах выше XP и Server 2003 таких проблем нет - там мы можем использовать
    /// функцию <a href="https://goo.gl/ZZdvWo">PdhAddEnglishCounter</a>, 
    /// которая добавляет счетчик независимо от языка системы.
    /// @todo Реализовать добавление счетчиков в Windows XP и Windows Server 2003 независимо от языка системы - по их индексам.
    if (isWindowsVistaOrGreater)
        result = ::PdhAddEnglishCounterA(query, path.data(), 0, &counter);
    else 
        result = ::PdhAddCounterA(query, path.data(), 0, &counter);

    if (ERROR_SUCCESS != result) throw SystemError(static_cast<errno_t>(result));

    return counter;
}

double AbstractObserver::GetPdhValue(pdh::Counter handle)
{
    pdh::Value value = { 0 };
    const pdh::Result result = 
        ::PdhGetFormattedCounterValue(handle, PDH_FMT_DOUBLE, nullptr, &value);
    if (ERROR_SUCCESS == result) return value.doubleValue;
    /// @note Игнорируем все ошибки связанные с вычислением значения счетчика
    /// и возвращаем в этом случае @b 0. Такие ошибки, по наблюдениям, возникают 
    /// при большой частоте опроса счетчиков, которые вычисляются на основе 
    /// диапазона значений. Например - счетчик загрузки процессора.
    /// Проблема вероятно вызвана ошибками синхронизации между данными счетчиков,
    /// о чем можно прочитать, например, на <a href="https://goo.gl/PE7ANW">официальном форуме Microsoft</a>.
    else if (PDH_CALC_NEGATIVE_DENOMINATOR == result) {}
    else if (PDH_CALC_NEGATIVE_TIMEBASE == result) {}
    else if (PDH_CALC_NEGATIVE_VALUE == result) {}
    else throw SystemError(static_cast<errno_t>(result));

    return 0.;
}

list<AbstractObserver::Process> AbstractObserver::GetProcessList()
{
    EnableDebugPrivelege();

    HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == snapshot) 
        throw SystemError(static_cast<errno_t>(::GetLastError()));

    PROCESSENTRY32 entry = { 0 };
    entry.dwSize = sizeof(entry);
    if (!::Process32First(snapshot, &entry)) {
        ::CloseHandle(snapshot);
        throw SystemError(static_cast<errno_t>(::GetLastError()));
    }

    list<Process> plist = {};
    do {
        if (0 == entry.th32ProcessID || 4 == entry.th32ProcessID) continue;



        Process proc = { 0 };
        proc.pid = static_cast<uint32_t>(entry.th32ProcessID);
        proc.ppid = static_cast<uint32_t>(entry.th32ParentProcessID);
        proc.name = entry.szExeFile;
        proc.priority = static_cast<uint32_t>(entry.pcPriClassBase);
        proc.threads = static_cast<uint32_t>(entry.cntThreads);

        HANDLE handle = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID);
        if (nullptr != handle) {
            FillProcessPath(handle, proc);
            FillProcessOwner(handle, proc);
            FillProcessTimes(handle, proc);
            FillProcessMemoryInfo(handle, proc);
            FillProcessHandleCount(handle, proc);
            FillProcessStatus(handle, proc);
        }
        plist.push_back(proc);

        ::CloseHandle(handle);
    }
    while (::Process32Next(snapshot, &entry));

    ::CloseHandle(snapshot);

    return plist;
}

} // namespace testtools
