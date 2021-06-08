#pragma once


#include <memory>
#include <thread>


/*
	weak_anchor is used to produce weak_ptr to a non-heap object.
		A shared_ptr derived from these will block the anchor's destruction.
	
	This class was designed for use with asynchronous I/O delegates.
		It is particularly lightweight, using a timed backoff spinlock for synchronization.
		Consequently, the user should be careful not to retain references too long.
*/


namespace telling
{
	template<typename T>
	class weak_anchor
	{
	private:
		struct noop_deleter {void operator()(T *ptr) const noexcept {}};


	public:
		weak_anchor(T *ptr)        : expire(0), hold(std::unique_ptr<T,noop_deleter>(ptr, {})) {}
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
				std::weak_ptr<T> weak = hold;
				hold.reset();

				// Spin 65,536 times
				for (size_t i = 0; i < 0xFFFF; ++i)
					if (weak.expired()) return;

				// Exponential backoff from .000001 to ~.26 seconds
				for (size_t i = 0; !weak.expired(); ++i)
					std::this_thread::sleep_for(std::chrono::microseconds(1 << std::min<size_t>(i, 18)));
			}
		}

		
	protected:
		std::shared_ptr<T> hold;
	};
}