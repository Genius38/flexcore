#include <boost/test/unit_test.hpp>

#include "ports/event_ports.hpp"
#include "event_sink_with_queue.hpp"
#include "core/connection.hpp"

using namespace fc;

namespace fc
{

template<class T>
struct event_sink
{
	void operator()(T in)
	{
		*storage = in;
	}
	std::shared_ptr<T> storage = std::make_shared<T>();
};

template<class T>
struct is_passive_sink<event_sink<T>> : public std::true_type
{};

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

template<class T>
struct is_passive_sink<event_vector_sink<T>> : public std::true_type
{};

/**
 * \brief Node for calculating the number of elements in a range
 */
struct range_size
{
public:
	range_size()
		: out()
	{}
	event_out_port<int> out;

	auto in()
	{
		return ::fc::make_event_in_port_tmpl( [this](auto event)
		{
			size_t elems = std::distance(std::begin(event), std::end(event));
			this->out.fire(static_cast<int>(elems));
		} );
	}
};

} // namespace fc

namespace
{

/**
 * Helper class for testing event_in_port_tmpl
 */
class generic_input_node
{
public:
	generic_input_node() : value() {}

	/*
	 * Define a getter for the port named "in" and
	 * Declare a member function to be called from the port.
	 * The token type is available as "event_t" and the token as "event".
	 */
	auto in()
	{
		return ::fc::make_event_in_port_tmpl( [this](auto event)
		{
			value = event;
		} );
	}

	int value;
};

} // unnamed namespace

BOOST_AUTO_TEST_SUITE(test_events)

BOOST_AUTO_TEST_CASE( test_event_in_port_tmpl )
{
	event_out_port<int> src_int;
	event_out_port<double> src_double;
	generic_input_node to;

	src_int >> to.in();
	src_double >> to.in();

	src_int.fire(2);
	BOOST_CHECK_EQUAL(to.value, 2);
	src_int.fire(4.1);
	BOOST_CHECK_EQUAL(to.value, 4);
}

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
	static_assert(is_instantiation_of<
			detail::active_connection_proxy, decltype(tmp_connection)>::value,
			"active port connected with standard connectable gets proxy");
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

BOOST_AUTO_TEST_CASE( test_event_out_port )
{
	range_size get_size;
	int storage = 0;
	get_size.out >> [&](int i) { storage = i; };

	get_size.in()(std::list<float>{1., 2., .3});
	BOOST_CHECK_EQUAL(storage, 3);

	get_size.in()(std::vector<int>{0, 1});
	BOOST_CHECK_EQUAL(storage, 2);
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

namespace
{
template<class T>
void test_connection(const T& connection)
{
	int storage = 0;
	event_out_port<int> a;
	event_in_queue<int> d;
	auto c = [&](int i) { storage = i; return i; };
	auto b = [](int i) { return i + 1; };

	connection(a,b,c,d);

	a.fire(2);
	BOOST_CHECK_EQUAL(storage, 3);
	BOOST_CHECK_EQUAL(d.get(), 3);
}
}

/**
 * Confirm that connecting ports and connectables
 * does not depend on any particular order.
 */
BOOST_AUTO_TEST_CASE( associativity )
{
	test_connection([](auto a, auto b, auto c, auto d)
	{
		a >> b >> c >> d;
	});

	test_connection([](auto a, auto b, auto c, auto d)
	{
		(a >> b) >> (c >> d);
	});

	test_connection([](auto a, auto b, auto c, auto d)
	{
		a >> ((b >> c) >> d);
	});

	test_connection([](auto a, auto b, auto c, auto d)
	{
		(a >> (b >> c)) >> d;
	});
}

template<class operation>
struct sink_t
{
	typedef void result_t;

	void operator()(auto && in)
	{
		op(in);
	}

	operation op;
};

template<class operation>
auto sink(const operation& op )
{
	return sink_t<operation>{op};
}

BOOST_AUTO_TEST_CASE( test_polymorphic_lambda )
{
	int test_value = 0;

	event_out_port<int> p;
	auto write = sink([&](auto in) {test_value = in;});

	static_assert(is_passive_sink<decltype(write)>::value, "");

	p >> write;
	BOOST_CHECK_EQUAL(test_value, 0);
	p.fire(4);
	BOOST_CHECK_EQUAL(test_value, 4);
}

BOOST_AUTO_TEST_CASE(test_sink_has_callback)
{
	static_assert(has_register_function<event_in_port<void>>(0),
				"type is defined with ability to register a callback");
}

template <class T>
struct disconnecting_event_sink : public event_in_port<T>
{
	disconnecting_event_sink() :
		event_in_port<T>(
			[&](T in){
				*storage = in;
				std::cout<<"hello from sink with number "<<in<<"\n";
			}
		)
	{

	}

	std::shared_ptr<T> storage = std::make_shared<T>();
};

BOOST_AUTO_TEST_CASE(test_sink_deleted_callback)
{
	std::cout<<"test_sink_deleted_callback\n";
	disconnecting_event_sink<int> test_sink1;

	{
		event_out_port<int> test_source;

		disconnecting_event_sink<int> test_sink4;
		test_source >> test_sink1;
		test_source.fire(5);
		BOOST_CHECK_EQUAL(*(test_sink1.storage), 5);

		{
			disconnecting_event_sink<int> test_sink2;
			disconnecting_event_sink<int> test_sink3;
			test_source >> test_sink2;
			test_source >> test_sink3;
			test_source.fire(6);
			BOOST_CHECK_EQUAL(*(test_sink2.storage), 6);
			BOOST_CHECK_EQUAL(*(test_sink3.storage), 6);

			test_source >> test_sink4;
			test_source.fire(7);
			BOOST_CHECK_EQUAL(*(test_sink4.storage), 7);
		}

		// this primarily checks, that no exception is thrown
		// since the connections from test_source to sink1-3 are deleted.
		test_source.fire(8);
		BOOST_CHECK_EQUAL(*(test_sink4.storage), 8);
	}

}

BOOST_AUTO_TEST_CASE(test_lambda_in_connection)
{
	std::cout<<"test_lambda_in_connection\n";
	event_sink<int> test_sink;
	event_sink<int> test_sink_2;

	event_out_port<int> test_source;

	(test_source >> [](int i){ return i+1; }) >> test_sink;

	test_source >> [](int i){ return i+1; }
		>> [](int i){ return i+1; }
		>> test_sink_2;

	test_source.fire(10);
	BOOST_CHECK_EQUAL(*(test_sink.storage), 11);
	BOOST_CHECK_EQUAL(*(test_sink_2.storage), 12);
}


BOOST_AUTO_TEST_SUITE_END()
