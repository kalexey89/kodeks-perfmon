/// @file
/// Реализация работы наблюдателя за процессами для Windows.

#include "processobserver.h"

/// Делитель для перевода байт в килобайты
#define KBYTESDIV 1024

namespace testtools
{

using std::string;
using std::list;
using std::unordered_map;

/// Функции-помощники ProcessObserver
namespace
{

/// Возвращает имя процесса по его индентификатору
/// @throw AbstractObserver#SystemError
/// @param[in] pid Идентификатор процесса
/// @return Имя процесса в случае успеха или пустая строка, если процесс не найден
std::string GetProcessNameByPid(uint32_t pid)
{
    HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == snapshot)
        throw ProcessObserver::SystemError(static_cast<errno_t>(::GetLastError()));

    PROCESSENTRY32 entry = { 0 };
    entry.dwSize = sizeof(entry);
    if (!::Process32First(snapshot, &entry)) {
        ::CloseHandle(snapshot);
        throw ProcessObserver::SystemError(static_cast<errno_t>(::GetLastError()));
    }

    std::string name(MAX_PATH, '\0');
    do if (entry.th32ProcessID == static_cast<DWORD>(pid)) {
        name = entry.szExeFile;
        break;
    }
    while (::Process32Next(snapshot, &entry));

    ::CloseHandle(snapshot);

    return name;
}

/// Возвращает индекс экземпляра процесса по его имени и идентификатору
/// @throw AbstractObserver#SystemError
/// @param[in] name Имя процесса
/// @param[in] pid Идентификатор процесса
/// @return Индекс экземпляра в случае успеха или @b -1, если экземпляр не найден
int16_t GetProcessInstanceIndex(const char* name, uint32_t pid)
{
    HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == snapshot)
        throw ProcessObserver::SystemError(static_cast<errno_t>(::GetLastError()));

    PROCESSENTRY32 entry = { 0 };
    entry.dwSize = sizeof(entry);
    if (!::Process32First(snapshot, &entry)) {
        ::CloseHandle(snapshot);
        throw ProcessObserver::SystemError(static_cast<errno_t>(::GetLastError()));
    }

    int16_t index = -1;
    do if (0 == std::strcmp(entry.szExeFile, name)) {
        index++;
        if (entry.th32ProcessID == static_cast<DWORD>(pid)) break;
    }
    while (::Process32Next(snapshot, &entry));

    ::CloseHandle(snapshot);

    return index;
}

} // namespace

double ProcessObserver::Instance::totalPhysicalMemory_ = -1.;
double ProcessObserver::Instance::totalVirtualMemory_ = -1.;

ProcessObserver::ProcessNotFound::ProcessNotFound(uint32_t pid)
    : Exception(("Process with id " + std::to_string(pid) + " not found.")) {}

ProcessObserver::Instance::Instance(pdh::Query query, const string& name, int16_t index)
    : processId_(nullptr)
    , handleCount_(nullptr)
    , threadCount_(nullptr)
    , processorUsage_(nullptr)
    , physicalMemoryUsage_(nullptr)
    , virtualMemoryUsage_(nullptr)
{
    /// @warning По умолчанию и в 99.9% случаев путь к счетчикам процесса формируется в формате:
    /// "\Процесс(имя процесса#индекс экземпляра)\Счетчик" (имя процесса указывается без расширения).
    /// Существует вероятность, что это правило формирования будет изменено с помощью специальной
    /// <a href="https://goo.gl/wf2wXF">настройки</a> в системном реестре и тогда мы станем получать 
    /// ошибку, что путь к счетчику сформирован неверно. Правда, вероятность этого крайне мала, т.к. 
    /// сам Microsoft не рекомендует эту настройку менять. 
    /// Тем не менее, если все-таки такие ошибки стали возникать - нужно проверить и изменить значение 
    /// ключа @e ProcessNameFormat на @b 1 в ветке "HKLM\SYSTEM\CurrentControlSet\Services\PerfProc\Performance".

    static const char* const kProcessId = "\\Process(%s)\\ID Process";
    static const char* const kHandleCount = "\\Process(%s)\\Handle Count";
    static const char* const kThreadCount = "\\Process(%s)\\Thread Count";
    static const char* const kProcessorUsage = "\\Process(%s)\\%% Processor Time";
    static const char* const kPhysicalMemoryUsage = "\\Process(%s)\\Working Set";
    static const char* const kVirtualMemoryUsage = "\\Process(%s)\\Private Bytes";

    // Поскольку в пути к счетчику используется имя процесса без расширения - удаляем его, если требуется.
    const size_t extpos = name.find_last_of('.');
    string instance = string::npos == extpos ? name : name.substr(0, extpos);
    if (0 != index) instance += ('#' + std::to_string(index));

    string path(PDH_MAX_COUNTER_PATH, '\0');
    std::sprintf(&path[0], kProcessId, instance.data());
    processId_.reset(AddPdhCounter(query, path));
    
    std::fill(path.begin(), path.end(), '\0');
    std::sprintf(&path[0], kHandleCount, instance.data());
    handleCount_.reset(AddPdhCounter(query, path));

    std::fill(path.begin(), path.end(), '\0');
    std::sprintf(&path[0], kThreadCount, instance.data());
    threadCount_.reset(AddPdhCounter(query, path));

    std::fill(path.begin(), path.end(), '\0');
    std::sprintf(&path[0], kProcessorUsage, instance.data());
    processorUsage_.reset(AddPdhCounter(query, path));

    std::fill(path.begin(), path.end(), '\0');
    std::sprintf(&path[0], kPhysicalMemoryUsage, instance.data());
    physicalMemoryUsage_.reset(AddPdhCounter(query, path));

    std::fill(path.begin(), path.end(), '\0');
    std::sprintf(&path[0], kVirtualMemoryUsage, instance.data());
    virtualMemoryUsage_.reset(AddPdhCounter(query, path));
    
    const pdh::Result result = ::PdhCollectQueryData(query);
    if (ERROR_SUCCESS != result) throw SystemError(static_cast<errno_t>(result));
}

ProcessObserver::Instance::Result ProcessNameObserver::Instance::Poll(Mask mask) const
{
    Instance::Result presult = {};
    presult.emplace(std::make_pair("pid", GetPdhValue(processId_.get())));
    if (HandleCount & mask)
        presult.emplace(std::make_pair("handles", GetPdhValue(handleCount_.get())));
    if (ThreadCount & mask)
        presult.emplace(std::make_pair("threads", GetPdhValue(threadCount_.get())));
    if (ProcessorUsage & mask)
        presult.emplace(std::make_pair("procusage", GetPdhValue(processorUsage_.get())));
    if (PhysicalMemoryUsage & mask)
        presult.emplace(std::make_pair("pmemusage", GetPhysicalMemoryUsage(physicalMemoryUsage_.get(), false)));
    if (PhysicalMemoryUsageKBytes & mask)
        presult.emplace(std::make_pair("pmemusagekb", GetPhysicalMemoryUsage(physicalMemoryUsage_.get(), true)));
    if (VirtualMemoryUsage & mask)
        presult.emplace(std::make_pair("vmemusage", GetVirtualMemoryUsage(virtualMemoryUsage_.get(), false)));
    if (VirtualMemoryUsageKBytes & mask)
        presult.emplace(std::make_pair("vmemusagekb", GetVirtualMemoryUsage(virtualMemoryUsage_.get(), true)));

    return presult;
}

double ProcessObserver::Instance::GetPhysicalMemoryUsage(pdh::Counter handle, bool kbytes)
{
    const double used = GetPdhValue(handle);

    return kbytes ? (used / KBYTESDIV) : std::floor((used * 100) / totalPhysicalMemory_);
}

double ProcessObserver::Instance::GetVirtualMemoryUsage(pdh::Counter handle, bool kbytes)
{
    const double used = GetPdhValue(handle);

    return kbytes ? (used / KBYTESDIV) : std::floor((used * 100) / totalVirtualMemory_);
}

ProcessObserver::ProcessObserver(uint32_t pid)
    : AbstractObserver(ProcessId, GetProcessNameByPid(pid))
{
    MEMORYSTATUSEX msx = { 0 };
    msx.dwLength = sizeof(msx);
    if (!::GlobalMemoryStatusEx(&msx))
        throw SystemError(static_cast<errno_t>(::GetLastError()));

    Instance::totalPhysicalMemory_ = static_cast<double>(msx.ullTotalPhys);
    Instance::totalVirtualMemory_ = static_cast<double>(msx.ullTotalVirtual);
}

ProcessObserver::ProcessObserver(const string& name)
    : AbstractObserver(ProcessName, name)
{
    MEMORYSTATUSEX msx = { 0 };
    msx.dwLength = sizeof(msx);
    if (!::GlobalMemoryStatusEx(&msx))
        throw SystemError(static_cast<errno_t>(::GetLastError()));

    Instance::totalPhysicalMemory_ = static_cast<double>(msx.ullTotalPhys);
    Instance::totalVirtualMemory_ = static_cast<double>(msx.ullTotalVirtual);
}

ProcessIdObserver::ProcessIdObserver(uint32_t pid)
    : ProcessObserver(pid)
    , pid_(pid)
    , index_(-1)
    , instance_(nullptr)
{
    // Ищем индекс соответствующий переданному pid и создаем экземпляр объекта-наблюдателя.
    const string name = GetObject();
    index_ = GetProcessInstanceIndex(name.data(), pid);
    if (-1 == index_) throw ProcessNotFound(pid);
    else instance_.reset(new Instance(query_.get(), name, index_));
}

ProcessIdObserver::Result ProcessIdObserver::Poll(Mask mask)
{
    const string name = GetObject();
    // Получаем текущий индекс экземпляра и сравниваем с сохраненным. Если значения 
    // не равны - переинициализируем объект наблюдателя с нужным индексом.
    const int16_t index = GetProcessInstanceIndex(name.data(), pid_);
    if (index_ != index) {
        instance_.reset(new Instance(query_.get(), name, index));
        index_ = index;
    }

    const pdh::Result result = ::PdhCollectQueryData(query_.get());
    if (ERROR_SUCCESS != result) throw SystemError(static_cast<errno_t>(result));

    return instance_->Poll(mask);
}

ProcessNameObserver::ProcessNameObserver(const std::string& name)
    : ProcessObserver(name)
{
    HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == snapshot)
        throw SystemError(static_cast<errno_t>(::GetLastError()));

    PROCESSENTRY32 entry = { 0 };
    entry.dwSize = sizeof(entry);
    if (!::Process32First(snapshot, &entry)) {
        ::CloseHandle(snapshot);
        throw SystemError(static_cast<errno_t>(::GetLastError()));
    }

    // Ищем все процессы с указанным именем и инициализируем 
    // экземпляры объектов-наблюдателей для каждого из них.
    int16_t index = -1;
    do if (0 == std::strcmp(entry.szExeFile, name.data())) {
            index++;
            instances_.push_back(std::make_unique<Instance>(query_.get(), name, index));
    }
    while (::Process32Next(snapshot, &entry));

    ::CloseHandle(snapshot);
}

ProcessNameObserver::Result ProcessNameObserver::Poll(Mask mask)
{
    HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == snapshot)
        throw SystemError(static_cast<errno_t>(::GetLastError()));

    const string name = GetObject();

    PROCESSENTRY32 entry = { 0 };
    entry.dwSize = sizeof(entry);
    if (!::Process32First(snapshot, &entry)) {
        ::CloseHandle(snapshot);
        throw SystemError(static_cast<errno_t>(::GetLastError()));
    }

    // Получаем текущее количество процессов с указанным именем и соответствующим образом
    // изменяем количество нужных экземпляров наблюдателей (объекты класса Instance).
    // Поскольку в классе Instance присутствует счетчик, который возвращает pid процесса по
    // переданному ему пути, то при изменении количества процессов мы все равно однозначно можем 
    // сопоставить значение данных остальных счетчиков с pid процесса.
    //
    // Пример: запущено 3 процесса с именем "notepad.exe", тогда получаются такие пути к их счетчикам:
    // - "\Process(notepad)\Счетчик";
    // - "\Process(notepad#1)\Счетчик";
    // - "\Process(notepad#2)\Счетчик".
    // Имеет соответствующее кол-во объектов класса Instance, в каждом из них счетчик processId_ указывает
    // на соответвующий пути pid, тогда выходит, что через значение этого счетчика мы можем однозначно сопоставить
    // для какого pid приходят данные от остальных счетчиков объекта.
    // Если кол-во процессов в какой то момент измениться, то мы просто оставляем/создаем нужное кол-во
    // объектов класса Instance, а счетчик processId_ автоматически будет указывать на "свой" pid. Т.о. остается 
    // только поддерживать в актульном состоянии число объектов класса Instance.

    size_t index = 0;
    do if (0 == std::strcmp(entry.szExeFile, name.data())) index++;
    while (::Process32Next(snapshot, &entry));

    ::CloseHandle(snapshot);

    if (index < instances_.size())
        instances_.resize(index);

    if (index > instances_.size())
        for (size_t i = instances_.size(); i < index; ++i)
            instances_.push_back(std::make_unique<Instance>(query_.get(), name, static_cast<int16_t>(i)));

    const pdh::Result result = ::PdhCollectQueryData(query_.get());
    if (ERROR_SUCCESS != result) throw SystemError(static_cast<errno_t>(result));

    Result presult = {};
    for (const auto& instance : instances_)
        presult.push_front(instance->Poll(mask));

    return presult;
}

} // namespace testtools
