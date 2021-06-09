#pragma once


#include <memory>
#include <thread>
#include <atomic>


/*
	weak_anchor is used to produce weak pointers to an object that isn't necessarily
		allocated with new or make_shared.  Shared pointers created from these
		weak pointers will block the object's destruction until they are released.

	The user should take care to lock these pointers only briefly lest the program deadlock.
	
	This class was designed for use with asynchronous I/O delegates, allowing observers
		to be destroyed at will *unless* a callback happens to be occurring.
		The one-time lock mechanism used here is much more lightweight than a mutex.
*/


namespace telling
{
	template<typename T>
	class weak_anchor
	{
	private:
		struct deleter
		{
			std::atomic<std::atomic_flag*> keep_alive;

			deleter() : keep_alive(nullptr) {}
			void operator()(T *ptr) const noexcept    {keep_alive.load()->clear();}
		};


	public:
		weak_anchor(T *ptr)        : expire(0), hold(std::unique_ptr<T,deleter>(ptr, {})) {}
		~weak_anchor() noexcept    {destroy();}

		/*
			Get a weak_ptr to the object.
		*/
		std::weak_ptr<T>      get() const noexcept    {return hold;}
		operator std::weak_ptr<T>() const noexcept    {return hold;}

		/*
			Destroy the reference, waiting for any shared_ptr to expire.
		*/
		void destroy() noexcept
		{
			if (hold)
			{
				// Create a "keep alive" flag; when this is unset, destruction can complete
				std::atomic_flag keep_alive;
				keep_alive.test_and_set(std::memory_order_release);
				std::get_deleter<deleter>(hold)->expire.store(&expire);
				hold.reset();

				// Spin 16,192 times
				for (size_t i = 0; i < 0x4000; ++i)
					if (!keep_alive.test_and_set(std::memory_order_acquire)) return;

				// Exponential backoff from .000001 to ~.26 seconds
				for (size_t i = 0; keep_alive.test_and_set(std::memory_order_acquire); ++i)
					std::this_thread::sleep_for(std::chrono::microseconds(1 << std::min<size_t>(i, 18)));
			}
		}

		
	protected:
		std::shared_ptr<T> hold;
	};
}