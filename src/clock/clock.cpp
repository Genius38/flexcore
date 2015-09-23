/*
 * clock.hpp
 *
 *  Created on: Sep 21, 2015
 *      Author: ckielwein
 */

#include "clock.hpp"

namespace fc
{

namespace chrono
{

using namespace std::chrono;

virtual_clock::system::time_point virtual_clock::system::now() noexcept
{
	return current_time.load();
}

std::time_t virtual_clock::system::to_time_t(const time_point& t)
{
	const auto duration = t.time_since_epoch();
	const auto seconds_ = duration_cast<seconds>(duration);
	return seconds_.count();
}

//virtual_clock::system::time_point virtual_clock::system::from_time_t(std::time_t t)
//{
//	//ToDo
//}

void virtual_clock::system::advance() noexcept
{
	const auto tmp = current_time.load();
	current_time.store(tmp + tmp.min().time_since_epoch());
}

void virtual_clock::system::set_time(time_point r) noexcept
{
	current_time.store(r);
}

virtual_clock::steady::time_point virtual_clock::steady::now() noexcept
{
	return current_time.load();
}

void virtual_clock::steady::advance() noexcept
{
	const auto tmp = current_time.load();
	current_time.store(tmp + tmp.min().time_since_epoch());
}

} //namespace chrono
}  //namespace fc
