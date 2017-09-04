/// @file
/// Реализация работы наблюдателя за системой для Windows.

#include "systemobserver.h"

#define KBYTESDIV 1024

namespace testtools
{

using std::string;
using std::unordered_map;

SystemObserver::SystemObserver()
    : AbstractObserver(System, "System")
    , processCount_(nullptr)
    , threadCount_(nullptr)
    , processorUsage_(nullptr)
    , diskUsage_(nullptr)
{
    static const char* const kProcessCount = "\\System\\Processes";
    static const char* const kThreadCount = "\\System\\Threads";
    static const char* const kProcessorUsage = "\\Processor(_Total)\\% Processor Time";
    static const char* const kDiskUsage = "\\PhysicalDisk(_Total)\\% Disk Time";

    processCount_.reset(AddPdhCounter(query_.get(), kProcessCount));
    threadCount_.reset(AddPdhCounter(query_.get(), kThreadCount));
    processorUsage_.reset(AddPdhCounter(query_.get(), kProcessorUsage));
    diskUsage_.reset(AddPdhCounter(query_.get(), kDiskUsage));

    const pdh::Result result = ::PdhCollectQueryData(query_.get());
    if (ERROR_SUCCESS != result) throw SystemError(static_cast<errno_t>(result));
}

SystemObserver::Result SystemObserver::Poll(Mask mask) const
{
    const pdh::Result result = ::PdhCollectQueryData(query_.get());
    if (ERROR_SUCCESS != result) throw SystemError(static_cast<errno_t>(result));

    Result presult = {};
    if (ProcessCount & mask)
        presult.emplace(std::make_pair("processes", GetPdhValue(processCount_.get())));
    if (ThreadCount & mask)
        presult.emplace(std::make_pair("threads", GetPdhValue(threadCount_.get())));
    if (ProcessorUsage & mask)
        presult.emplace(std::make_pair("procusage", GetPdhValue(processorUsage_.get())));
    if (PhysicalMemoryUsage & mask)
        presult.emplace(std::make_pair("pmemusage", GetPhysicalMemoryUsage(false)));
    if (PhysicalMemoryUsageKBytes & mask)
        presult.emplace(std::make_pair("pmemusagekb", GetPhysicalMemoryUsage(true)));
    if (VirtualMemoryUsage & mask)
        presult.emplace(std::make_pair("vmemusage", GetVirtualMemoryUsage(false)));
    if (VirtualMemoryUsageKBytes & mask)
        presult.emplace(std::make_pair("vmemusagekb", GetVirtualMemoryUsage(true)));
    if (DiskUsage & mask)
        presult.emplace(std::make_pair("diskusage", GetPdhValue(diskUsage_.get())));

    return presult;
}

double SystemObserver::GetPhysicalMemoryUsage(bool kbytes)
{
    MEMORYSTATUSEX msx = { 0 };
    msx.dwLength = sizeof(msx);
    if (!::GlobalMemoryStatusEx(&msx))
        throw SystemError(static_cast<errno_t>(::GetLastError()));

    const double total = static_cast<double>(msx.ullTotalPhys);
    const double avail = static_cast<double>(msx.ullAvailPhys);

    return kbytes ? ((total - avail) / KBYTESDIV) : std::floor(((total - avail) * 100) / total);
}

double SystemObserver::GetVirtualMemoryUsage(bool kbytes)
{
    MEMORYSTATUSEX msx = { 0 };
    msx.dwLength = sizeof(msx);
    if (!::GlobalMemoryStatusEx(&msx))
        throw SystemError(static_cast<errno_t>(::GetLastError()));

    const double total = static_cast<double>(msx.ullTotalVirtual);
    const double avail = static_cast<double>(msx.ullAvailVirtual);

    return kbytes ? ((total - avail) / KBYTESDIV) : std::floor(((total - avail) * 100) / total);
}

} // namespace testtools
