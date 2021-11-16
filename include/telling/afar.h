#pragma once


//#define AFAR_USE_SHARED_MUTEX 0 // Autodetected if not defined
//#define AFAR_USE_STRING_VIEW  0 // Autodetected if not defined

#if !defined(AFAR_USE_STRING_VIEW)
	#if __cplusplus >= 201703L || _MSVC_LANG >= 201703L
		#define AFAR_USE_STRING_VIEW 1
	#else
		#define AFAR_USE_STRING_VIEW 0
	#endif
#endif

#if AFAR_USE_STRING_VIEW
	#include <string_view>
#endif

#include <string>
#include <chrono>
#include <functional>
#include <any>


/*
	
*/


namespace afar
{
#if AFAR_USE_STRING_VIEW
	using string_view = std::string_view;
#else
	using string_view = const std::string &;
#endif

	/*
		Call a functor from afar.
			If the functor does not exist, throws CallPathException.
			The functor may also throw exceptions.

		While the Call is running, it:
			Blocks other threads from making the same Call.
			Blocks destruction of the Callee.
	*/
	template<typename Result = void, typename Argument = void>
	static Result Call(string_view name, Argument args);


	/*
		Check if a Callee exists, without making a call.
			This is for monitoring purposes and does not reserve the Callee.
			The Callee can be destroyed at any time.
	*/
	bool CalleeExists(string_view path) noexcept;


	/*
		Register a functor for as long as this Callee object exists.
	*/
	class Callee
	{
	public:
		std::string path;

	public:
		/*
			Construction of the Callee registers a functor.
				Throws CallPathException if the name is taken.
		*/
		template<typename Result, typename Argument>
		Callee(std::string path, std::function<Result(Argument)> value);

		/*
			The functor is unregistered when the Callee object is destroyed.
				This may block until outstanding Calls have completed.
		*/
		~Callee() noexcept;
	};


	/*
		Exception thrown when path is missing or taken
	*/
	class CallPathException : public std::runtime_error
	{
	public:
		CallPathException(const std::string &what_arg)    : runtime_error(what_arg) {}
		CallPathException(const char        *what_arg)    : runtime_error(what_arg) {}
	};

	/*
		Exception thrown when attempting to call function with wrong types
	*/
	class CallTypeException : public std::logic_error
	{
	public:
		CallTypeException(const std::string &what_arg)    : logic_error(what_arg) {}
		CallTypeException(const char        *what_arg)    : logic_error(what_arg) {}
	};
}


/*
	Implementation
*/

#if !defined(AFAR_USE_SHARED_MUTEX)
	#if _MSC_VER // Not supported under Windows XP toolset
		#include <shared_mutex>
		#define AFAR_USE_SHARED_MUTEX _HAS_SHARED_MUTEX
	#elif __cplusplus >= 201703L
		#define AFAR_USE_SHARED_MUTEX 1
	#else
		#define AFAR_USE_SHARED_MUTEX 0
	#endif
#endif

#if AFAR_USE_SHARED_MUTEX
	#include <shared_mutex>
#else
	#include <mutex>
#endif
#include <condition_variable>
#include <unordered_map>

namespace afar
{
	class detail
	{
		class NamedFunctors
		{
		public:
#if AFAR_USE_SHARED_MUTEX
			using Mutex = std::shared_mutex;
			using ReadLock = std::shared_lock<Mutex>;
#else
			using Mutex = std::mutex;
			using ReadLock = std::unique_lock<Mutex>;
#endif
			using WriteLock = std::unique_lock<Mutex>;

			struct Functor
			{
				std::any    functor;
				Mutex       mtx;

				Functor(const Functor&) = delete;
				void operator=(const Functor&) = delete;
			};

			using Functors = std::unordered_map<string_view, Functor*>;
			std::condition_variable cond;
			Functors                functors;
			Mutex                   mtx;

		public:
			NamedFunctors()  {}
			~NamedFunctors() {}

			void add(std::string path, std::any functor)
			{
				WriteLock lock_table(mtx);

				// Already exists?
				auto p = functors.find(path);
				if (functors.count(path))
					throw CallPathException("afar: path is taken");

				// Add to table
				functors.emplace(path, new Functor{std::move(functor)});
			}
			void remove(string_view path)
			{
				WriteLock lock_table(mtx);
				auto p = functors.find(path);
				if (p == functors.end()) return;
				
				// Erase from table
				auto func = p->second;
				functors.erase(p);

				{
					// Handoff from table lock to functor lock
					WriteLock lock_functor(func->mtx);
					lock_table.unlock();

					// Destroy functor
					func->functor.reset();
				}
				delete func;
			}

			// Find an entry in the table, writing it to entry and returning a lock on the entry.
			ReadLock find(string_view path, Functor* &entry)
			{
				ReadLock lock_table(mtx);

				entry = nullptr;
				auto p = functors.find(path);
				if (p == functors.end()) return ReadLock();
				entry = p->second;

				// Transition from table lock to functor lock
				ReadLock lock_functor(entry->mtx);
				lock_table.unlock();

				return lock_functor;
			}

			// Call a functor, which will be locked for the duration of the call.
			template<typename Result, typename Argument>
			Result call(string_view path, Argument argument)
			{
				// Locate and lock the functor
				Functor *entry = nullptr;
				auto lock_functor = find(path, entry);
				if (!entry) throw CallPathException("afar: no functor with this path");

				// Cast the function
				auto *func = std::any_cast<std::function<Result(Argument)>>(&entry->functor);
				if (!func) throw CallTypeException("afar: functor called with wrong types");

				// Call the function
				return (*func)(argument);
			}

			// Thread-safe C++ initialization
			static NamedFunctors &Get() {static NamedFunctors manager; return manager;}
		};
	};

	template<typename Result, typename Argument>
	inline Callee::Callee(std::string path, std::function<Result(Argument)> value)
		{detail::NamedFunctors::Get().add(path, value);}
	inline Callee::~Callee() noexcept
		{detail::NamedFunctors::Get().remove(path);}

	template<typename Result, typename Argument>
	Result Call(string_view path, Argument argument)
	{
		return detail::NamedFunctors::Get().call<Result>(
			path, std::forward<Argument>(argument));
	}

	bool CalleeExists(string_view path) noexcept
	{
		detail::NamedFunctors::Functor *ptr;
		auto lock = detail::NamedFunctors::Get().find(path, ptr);
		return (ptr != nullptr);
	}
}