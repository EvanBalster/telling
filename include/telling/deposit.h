#pragma once

#include <chrono>
#include <cstdint>
#include <any>


namespace telling
{
	using ClaimNumber = uint32_t;

	using DepositClock    = std::chrono::steady_clock;
	using DepositDuration = DepositClock::duration;

	/*
		Deposit an object.  (thread-safe)
			If it is not claimed within the duration it will expire.
			Duration must be positive or deposit will fail.
		
		A ClaimNumber of 0 is invalid and indicates failure.
	*/
	ClaimNumber Deposit(std::any&&, DepositDuration);

	/*
		Claim an object previously deposited.  (thread-safe)
			The item may only be Claimed once.
			any::has_value() will be false for claimed or expired items.
	*/
	std::any    Claim  (ClaimNumber);
}



/*
===========================
	IMPLEMENTATION
===========================
*/



#ifndef TELLING_DEPOSIT_IMPL
	#define TELLING_DEPOSIT_IMPL 1
#endif


#if TELLING_DEPOSIT_IMPL

#include <queue>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace telling
{
	/*
		Implementation follows
	*/
	namespace detail
	{
		class Depository
		{
		private:
			using ExpireTime = DepositClock::time_point;
			struct Expiration
			{
				ClaimNumber number;
				ExpireTime  expire;

				// Earlier expire times have greater priority in the queue
				bool operator<(const Expiration &o) const noexcept    {return expire > o.expire;}
			};

			using Lockers     = std::unordered_map<ClaimNumber, std::any>;
			using ExpireQueue = std::priority_queue<Expiration>;

			std::mutex              mtx;
			Lockers                 lockers;
			ExpireQueue             expireQueue;
			std::thread             expireThread;
			std::condition_variable expireCond;
			bool                    terminate = false;

			ClaimNumber             claimNumberGen = 0;
			

		public:
			Depository()
			{
				expireThread = std::thread(&run_expire_thread, this);
			}
			~Depository()
			{
				terminate = true;
				expireCond.notify_one();
				expireThread.join();
			}

			void run_expire_thread()
			{
				std::unique_lock lock(mtx);

				while (!terminate)
				{
					if (expireQueue.empty())
					{
						expireCond.wait(lock);
					}
					else
					{
						auto &top = expireQueue.top();
						
						if (top.expire < DepositClock::now())
						{
							// Destroy the item and proceed.
							lockers.erase(top.number);
							expireQueue.pop();
							continue;
						}
						else
						{
							// Wait until the next item expires or the queue is changed.
							expireCond.wait_until(lock, top.expire);
						}
					}
				}
			}

			ClaimNumber deposit(std::any &&object, DepositDuration duration)
			{
				if (duration <= DepositDuration::zero()) return 0;

				std::lock_guard g(mtx);

				// Generate an unused nonzero claim number
				ClaimNumber claimNumber;
				do
				{
					// TODO something cleverer?  More secure?
					claimNumber = ++claimNumberGen;
				}
				while (claimNumber != 0 && !lockers.count(claimNumber));

				// Configure expiration
				Expiration expiration = {claimNumber, DepositClock::now() + duration};
				bool wakeExpiration = (!expireQueue.size() || expiration < expireQueue.top());
				expireQueue.emplace(std::move(expiration));

				// Add to map
				lockers.emplace(claimNumber, std::move(object));

				// Wake expiration queue if this new item expires next
				if (wakeExpiration) expireCond.notify_one();
			}

			std::any claim(ClaimNumber number)
			{
				std::lock_guard g(mtx);

				// Locate the item...
				std::any result;
				auto pos = lockers.find(number);
				if (pos != lockers.end()) result = std::move(pos->second);
				return result;
			}

			// Thread-safe C++ initialization
			static Depository &Get() {static Depository manager; return manager;}
		};
	}

	inline ClaimNumber Deposit(std::any object, DepositDuration duration)
	{
		return detail::Depository::Get().deposit(std::move(object), duration);
	}

	inline std::any Claim(ClaimNumber number)
	{
		return detail::Depository::Get().claim(number);
	}
}

#endif