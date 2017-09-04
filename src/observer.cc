/// @file
/// Реализация работы экспортируемого в Node.JS класса наблюдателя.

#include "observer.h"

#include "systemobserver.h"
#include "processobserver.h"

/// Возвращает ссылку на реализацию наблюдателя
#define IPTR(obj) (obj)->impl_.get()

/// Создает новый объект класса v8::String
#define JSSTR(str) Nan::New<v8::String>((str)).ToLocalChecked()

/// Создает новый объект класса v8::Number
#define JSNUM(num) Nan::New<v8::Number>((num))
/// Ковертирует объект класса v8::Number в uint32_t
#define JSNUM2UINT32(var) Nan::To<uint32_t>((var)).FromJust()

namespace testtools
{

using v8::Local;
using v8::Value;
using v8::Object;
using v8::Function;
using v8::FunctionTemplate;
using v8::Array;
using v8::Number;
using v8::String;

using Nan::ObjectWrap;
using Nan::Callback;
using Nan::AsyncWorker;


/// Классы реализующие асинхронную работу метода @e poll для каждого типа наблюдателя.
namespace
{

/// Базовый класс реализации асинхронной работы метода @e poll.
class ObserverWorker : public AsyncWorker
{
protected:
    /// @param[in] callback Указатель на callback
    /// @param[in] observer Указатель на реализацию наблюдателя
    /// @param[in] mask Маска опрашиваемых счетчиков
    ObserverWorker(Callback* callback, AbstractObserver* observer, AbstractObserver::Mask mask)
        : AsyncWorker(callback)
        , observer_(observer)
        , mask_(mask) {}
    virtual ~ObserverWorker() = default;

private:
    /// Вызывается в случае возникновения ошибки.
    /// Устанавливает значение параметра @e error callback-функции.
    inline void HandleErrorCallback() override final {
        const int argc = 2;
        Local<Value> argv[argc] = { Nan::Error(ErrorMessage()), Nan::Null() };
        callback->Call(argc, argv);
    }

protected:
    /// Указатель на реализацию наблюдателя
    AbstractObserver* observer_;
    /// Маска опрашиваемых счетчиков
    AbstractObserver::Mask mask_;
}; // class ObserverWorker

/// Реализует асинхронную работу метода @e poll для наблюдателя за системой.
class SystemObserverWorker final : public ObserverWorker
{
public:
    /// @param[in] callback Указатель на callback
    /// @param[in] observer Указатель на реализацию наблюдателя
    /// @param[in] mask Маска опрашиваемых счетчиков
    SystemObserverWorker(Callback* callback, AbstractObserver* observer, AbstractObserver::Mask mask)
        : ObserverWorker(callback, observer, mask)
        , result_({}) {}
    ~SystemObserverWorker() = default;

public:
    /// Запускает асинхронное выполнение метода @e poll
    inline void Execute() override {
        try {
            result_ = reinterpret_cast<SystemObserver*>(observer_)->Poll(mask_);
        }
        catch (const SystemObserver::Exception& error) {
            SetErrorMessage(error.what());
        }
    }

    /// Вызывается в случае успешного выполнения метода.
    /// Устанавливает значение параметра @e result callback-функции.
    inline void HandleOKCallback() override {
        auto jsresult = Nan::New<Object>();
        Nan::Set(jsresult, JSSTR("pid"), Nan::Null());
        for (const auto& item : result_) {
            Nan::Set(jsresult, JSSTR(item.first), JSNUM(item.second));
        }

        const int argc = 2;
        Local<Value> argv[argc] = { Nan::Null(), jsresult };
        callback->Call(argc, argv);
    }

private:
    /// Результат выполнения опроса счетчиков
    SystemObserver::Result result_;
}; // class SystemObserverWorker

/// Реализует асинхронную работу метода @e poll для наблюдателя за процессом с конкретным @e pid
class ProcessIdObserverWorker final : public ObserverWorker
{
public:
    /// @param[in] callback Указатель на callback
    /// @param[in] observer Указатель на реализацию наблюдателя
    /// @param[in] mask Маска опрашиваемых счетчиков
    ProcessIdObserverWorker(Callback* callback, AbstractObserver* observer, AbstractObserver::Mask mask)
        : ObserverWorker(callback, observer, mask)
        , result_({}) {}
    ~ProcessIdObserverWorker() = default;

public:
    /// Запускает асинхронное выполнение метода @e poll
    inline void Execute() override {
        try {
            result_ = reinterpret_cast<ProcessIdObserver*>(observer_)->Poll(mask_);
        }
        catch (const ProcessIdObserver::Exception& error) {
            SetErrorMessage(error.what());
        }
    }

    /// Вызывается в случае успешного выполнения метода.
    /// Устанавливает значение параметра @e result callback-функции.
    inline void HandleOKCallback() override {
        auto jsresult = Nan::New<Object>();
        for (const auto& item : result_) {
            Nan::Set(jsresult, JSSTR(item.first), JSNUM(item.second));
        }

        const int argc = 2;
        Local<Value> argv[argc] = { Nan::Null(), jsresult };
        callback->Call(argc, argv);
    }

private:
    /// Результат выполнения опроса счетчиков
    ProcessIdObserver::Result result_;
}; // class ProcessIdObserverWorker

/// Реализует асинхронную работу метода @e poll для наблюдателя за списком процессов по имени
class ProcessNameObserverWorker final : public ObserverWorker
{
public:
    /// @param[in] callback Указатель на callback
    /// @param[in] observer Указатель на реализацию наблюдателя
    /// @param[in] mask Маска опрашиваемых счетчиков
    ProcessNameObserverWorker(Callback* callback, AbstractObserver* observer, AbstractObserver::Mask mask)
        : ObserverWorker(callback, observer, mask)
        , result_({}) {}
    ~ProcessNameObserverWorker() = default;

public:
    /// Запускает асинхронное выполнение метода @e poll
    inline void Execute() override {
        try {
            result_ = reinterpret_cast<ProcessNameObserver*>(observer_)->Poll(mask_);
        }
        catch (const ProcessNameObserver::Exception& error) {
            SetErrorMessage(error.what());
        }
    }

    /// Вызывается в случае успешного выполнения метода.
    /// Устанавливает значение параметра @e result callback-функции.
    inline void HandleOKCallback() override {
        auto jsresult = Nan::New<Array>();
        uint32_t index = 0;
        for (const auto& ritem : result_) {
            auto jsitem = Nan::New<Object>();
            for (const auto& pitem : ritem) {
                Nan::Set(jsitem, JSSTR(pitem.first), JSNUM(pitem.second));
            }

            Nan::Set(jsresult, index, jsitem);
            index++;
        }

        const int argc = 2;
        Local<Value> argv[argc] = { Nan::Null(), jsresult };
        callback->Call(argc, argv);
    }

private:
    /// Результат выполнения опроса счетчиков
    ProcessNameObserver::Result result_;
}; // class PRocessNameObserverWorker

   /// Реализует асинхронную работу статического метода @e processes
class ProcessesWorker final : public AsyncWorker
{
public:
    /// @param[in] callback Указатель на callback
    ProcessesWorker(Callback* callback)
        : AsyncWorker(callback) {}
    ~ProcessesWorker() = default;

public:
    /// Запускает асинхронное выполнение метода @e processes
    inline void Execute() override {
        try {
            processes_ = AbstractObserver::GetProcessList();
        }
        catch (const AbstractObserver::SystemError& error) {
            SetErrorMessage(error.what());
        }
    }

    /// Вызывается в случае успешного выполнения метода.
    /// Устанавливает значение параметра @e result callback-функции.
    inline void HandleOKCallback() override {
        auto jsProcesses = Nan::New<Array>(processes_.size());
        uint32_t index = 0;
        for (const auto& process : processes_) {
            auto jsProcess = Nan::New<Object>();
            Nan::Set(jsProcess, JSSTR("pid"), JSNUM(process.pid));
            Nan::Set(jsProcess, JSSTR("ppid"), JSNUM(process.ppid));
            Nan::Set(jsProcess, JSSTR("name"), JSSTR(process.name.c_str()));
            Nan::Set(jsProcess, JSSTR("path"), JSSTR(process.path.c_str()));
            Nan::Set(jsProcess, JSSTR("owner"), JSSTR(process.owner.c_str()));
            Nan::Set(jsProcess, JSSTR("priority"), JSNUM(process.priority));
            Nan::Set(jsProcess, JSSTR("status"), JSNUM(process.status));
            Nan::Set(jsProcess, JSSTR("threads"), JSNUM(process.threads));
            Nan::Set(jsProcess, JSSTR("handles"), JSNUM(process.handles));
            Nan::Set(jsProcess, JSSTR("ktime"), JSNUM(process.ktime));
            Nan::Set(jsProcess, JSSTR("utime"), JSNUM(process.utime));
            Nan::Set(jsProcess, JSSTR("start"), JSNUM(process.start));
            Nan::Set(jsProcess, JSSTR("pmemory"), JSNUM(process.pmemory));
            Nan::Set(jsProcess, JSSTR("vmemory"), JSNUM(process.vmemory));

            Nan::Set(jsProcesses, index, jsProcess);
            index++;
        }

        const int argc = 2;
        Local<Value> argv[argc] = { Nan::Null(), jsProcesses };
        callback->Call(argc, argv);
    }

    /// Вызывается в случае возникновения ошибки.
    /// Устанавливает значение параметра @e error callback-функции.
    inline void HandleErrorCallback() override {
        const int argc = 2;
        Local<Value> argv[argc] = { Nan::Error(ErrorMessage()), Nan::Null() };
        callback->Call(argc, argv);
    }

private:
    /// Список процессов
    std::list<AbstractObserver::Process> processes_;
};

} // namespace

Observer::Observer()
    : impl_(std::make_unique<SystemObserver>()) {}

Observer::Observer(uint32_t pid)
    : impl_(std::make_unique<ProcessIdObserver>(pid)) {}

Observer::Observer(const std::string& name)
    : impl_(std::make_unique<ProcessNameObserver>(name)) {}

NODE_MODULE_INIT(Observer::Initialize)
{
    auto tpl = Nan::New<FunctionTemplate>(New);
    tpl->SetClassName(JSSTR("Observer"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    Nan::SetPrototypeMethod(tpl, "_type", GetType);
    Nan::SetPrototypeMethod(tpl, "_object", GetObject);
    Nan::SetPrototypeMethod(tpl, "_poll", Poll);

    Nan::SetMethod(tpl, "_processes", Processes);

    GetConstructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
    Nan::Set(module, JSSTR("exports"), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(Observer::New)
{
    if (!info.IsConstructCall())
        return;

    Observer* self = nullptr;
    try {
        if (0 == info.Length()) {
                self = new Observer();
        }
        else if (info[0]->IsUint32()) {
                const uint32_t pid = JSNUM2UINT32(info[0]);
                self = new Observer(pid);
        }
        else if (info[0]->IsString()) {
                const String::Utf8Value process(info[0]);
                self = new Observer(*process);
        }
        else {
            return Nan::ThrowError("Observer#Constructor - invalid arguments");
        }
    }
    catch (const AbstractObserver::Exception& error) {
        return Nan::ThrowError(error.what());
    }

    self->Wrap(info.This());
}

NAN_METHOD(Observer::GetType)
{
    Observer* self = Unwrap<Observer>(info.Holder());
    info.GetReturnValue().Set(JSNUM(IPTR(self)->GetType()));
}

NAN_METHOD(Observer::GetObject)
{
    Observer* self = Unwrap<Observer>(info.Holder());
    info.GetReturnValue().Set(JSSTR(IPTR(self)->GetObject()));
}

NAN_METHOD(Observer::Poll)
{
    if (2 != info.Length() || !info[0]->IsUint32() || !info[1]->IsFunction())
        return Nan::ThrowError("Observer#_poll() - invalid arguments");

    Callback* callback = new Callback(Local<Function>::Cast(info[1]));
    Observer* self = Unwrap<Observer>(info.Holder());
    const uint32_t mask = JSNUM2UINT32(info[0]);
    switch (IPTR(self)->GetType()) {
        case AbstractObserver::System:
            return Nan::AsyncQueueWorker(new SystemObserverWorker(callback, IPTR(self), mask));
        case AbstractObserver::ProcessId:
            return Nan::AsyncQueueWorker(new ProcessIdObserverWorker(callback, IPTR(self), mask));
        case AbstractObserver::ProcessName:
            return Nan::AsyncQueueWorker(new ProcessNameObserverWorker(callback, IPTR(self), mask));
        default: 
            break;
    }
}

NAN_METHOD(Observer::Processes)
{
    if (1 != info.Length() || !info[0]->IsFunction())
        return Nan::ThrowError("Observer#_processes - invalid arguments");

    auto callback = new Callback(Local<Function>::Cast(info[0]));
    Nan::AsyncQueueWorker(new ProcessesWorker(callback));
}

NODE_MODULE(observer, Observer::Initialize);

} // namespace testtools
