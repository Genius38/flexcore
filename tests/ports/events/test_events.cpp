#include <boost/test/unit_test.hpp>

#include "ports/event_ports.hpp"
#include "event_sink_with_queue.hpp"
#include "core/connection.hpp"

using namespace fc;

template<class T>
struct event_sink
{
	void operator()(T in)
	{
		*storage = in;
	}
	std::shared_ptr<T> storage = std::make_shared<T>();
};

namespace fc
{

template<class T>
struct is_passive_sink<event_sink<T>> : public std::true_type
{};

} // namespace fc

template<class T>
struct event_vector_sink
{
	void operator()(T in)
	{
		storage->push_back(in);
	}
	std::shared_ptr<std::vector<T>> storage =
			std::make_shared<std::vector<T>>();
};

namespace fc
{

template<class T>
struct is_passive_sink<event_vector_sink<T>> : public std::true_type
{};

} // namespace fc

BOOST_AUTO_TEST_SUITE( events )

BOOST_AUTO_TEST_CASE( connections )
{
	static_assert(is_active<event_out_port<int>>::value,
			"event_out_port is active by definition");
	static_assert(is_passive<event_in_port<int>>::value,
			"event_in_port is passive by definition");
	static_assert(!is_active<event_in_port<int>>::value,
			"event_in_port is not active by definition");
	static_assert(!is_passive<event_out_port<int>>::value,
			"event_out_port is not passive by definition");

	event_out_port<int> test_event;
	event_sink<int> test_handler;


	connect(test_event, test_handler);
	test_event.fire(1);
	BOOST_CHECK_EQUAL(*(test_handler.storage), 1);


	auto tmp_connection = test_event >> [](int i){return ++i;};
	tmp_connection >> test_handler;

	test_event.fire(1);
	BOOST_CHECK_EQUAL(*(test_handler.storage), 2);

	auto incr = [](int i){return ++i;};
	test_event >> incr >> incr >> incr >> test_handler;
	test_event.fire(1);
	BOOST_CHECK_EQUAL(*(test_handler.storage), 4);
}

BOOST_AUTO_TEST_CASE( queue_sink )
{
	auto inc = [](int i) { return i + 1; };

	event_out_port<int> source;
	event_in_queue<int> sink;
	source >> inc >> sink;
	source.fire(4);
	BOOST_CHECK_EQUAL(sink.empty(), false);
	int received = sink.get();
	BOOST_CHECK_EQUAL(received, 5);
	BOOST_CHECK_EQUAL(sink.empty(), true);
}

BOOST_AUTO_TEST_CASE( merge_events )
{
	event_out_port<int> test_event;
	event_out_port<int> test_event_2;
	event_vector_sink<int> test_handler;

	test_event >> test_handler;
	test_event_2 >> test_handler;

	test_event.fire(0);
	BOOST_CHECK_EQUAL(test_handler.storage->size(), 1);
	BOOST_CHECK_EQUAL(test_handler.storage->back(), 0);

	test_event_2.fire(1);

	BOOST_CHECK_EQUAL(test_handler.storage->size(), 2);
	BOOST_CHECK_EQUAL(test_handler.storage->front(), 0);
	BOOST_CHECK_EQUAL(test_handler.storage->back(), 1);

}

BOOST_AUTO_TEST_CASE( split_events )
{
	event_out_port<int> test_event;
	event_sink<int> test_handler_1;
	event_sink<int> test_handler_2;

	test_event >> test_handler_1;
	test_event >> test_handler_2;

	test_event.fire(2);
	BOOST_CHECK_EQUAL(*(test_handler_1.storage), 2);
	BOOST_CHECK_EQUAL(*(test_handler_2.storage), 2);
}

BOOST_AUTO_TEST_CASE( in_port )
{
	int test_value = 0;

	auto test_writer = [&](int i) {test_value = i;};

	event_in_port<int> in_port(test_writer);
	event_out_port<int> test_event;

	test_event >> in_port;
	test_event.fire(1);
	BOOST_CHECK_EQUAL(test_value, 1);


	//test void event
	auto write_999 = [&]() {test_value = 999;};


	event_in_port<void> void_in(write_999);
	event_out_port<void> void_out;
	void_out >> void_in;
	void_out.fire();
	BOOST_CHECK_EQUAL(test_value, 999);
}

BOOST_AUTO_TEST_CASE( lambda )
{
	int test_value = 0;

	auto write_666 = [&]() {test_value = 666;};
	event_out_port<void> void_out_2;
	void_out_2 >> write_666;
	void_out_2.fire();
	BOOST_CHECK_EQUAL(test_value, 666);
}

/**
 * Confirm that connecting ports and connectables
 * does not depend on any particular order.
 */
BOOST_AUTO_TEST_CASE( associativity )
{
	int storage = 0;
	auto c = [&](int i) { storage = i; return i; };
	auto b = [](int i) { return i + 1; };

	{
	event_out_port<int> a;
	event_in_queue<int> d;
	a >> b >> c >> d;
	a.fire(2);
	BOOST_CHECK_EQUAL(storage, 3);
	BOOST_CHECK_EQUAL(d.get(), 3);
	}

	{
	event_out_port<int> a;
	event_in_queue<int> d;
	(a >> b) >> (c >> d);
	a.fire(2);
	BOOST_CHECK_EQUAL(storage, 3);
	BOOST_CHECK_EQUAL(d.get(), 3);
	}

	{
	event_out_port<int> a;
	event_in_queue<int> d;
	a >> ((b >> c) >> d);
	a.fire(2);
	BOOST_CHECK_EQUAL(storage, 3);
	BOOST_CHECK_EQUAL(d.get(), 3);
	}

	{
	event_out_port<int> a;
	event_in_queue<int> d;
	(a >> (b >> c)) >> d;
	a.fire(2);
	BOOST_CHECK_EQUAL(storage, 3);
	BOOST_CHECK_EQUAL(d.get(), 3);
	}
}

BOOST_AUTO_TEST_SUITE_END()

