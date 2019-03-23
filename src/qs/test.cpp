#define BOOST_TEST_MODULE qs_test

#include <boost/test/included/unit_test.hpp>

#include "tuple_util.h"
#include "mult_div.h"

#define TPL(...) __VA_ARGS__
#define STR(X) #X

#define TEST_SAME(L, R) \
    BOOST_CHECK_MESSAGE( TPL(std::is_same<L, R>::value), STR(L) " == " STR(R) )

using namespace qs::detail;
using namespace qs;

BOOST_AUTO_TEST_SUITE( qs_tests )


BOOST_AUTO_TEST_CASE( test_drop_if_same_t )
{

    TEST_SAME( TPL( drop_if_same_t<int, int> ), std::tuple<> );
    TEST_SAME( TPL( drop_if_same_t<int, short> ), std::tuple<short> );
}


BOOST_AUTO_TEST_CASE( test_drop_first_t )
{
    TEST_SAME( TPL( drop_first_t<int, std::tuple<>> ), std::tuple<> );
    TEST_SAME( TPL( drop_first_t<int, std::tuple<int>> ), std::tuple<> );
    TEST_SAME( TPL( drop_first_t<int, std::tuple<int, int>> ), std::tuple<int> );
    TEST_SAME( TPL( drop_first_t<int, std::tuple<short>> ), std::tuple<short> );
    TEST_SAME( TPL( drop_first_t<int, std::tuple<short, int>> ), std::tuple<short> );
}


BOOST_AUTO_TEST_CASE( test_first_tuple_t )
{
    TEST_SAME( TPL( first_tuple_t<int, short, char> ), std::tuple<int> );
    TEST_SAME( TPL( first_tuple_t<short> ), std::tuple<short> );
    TEST_SAME( TPL( first_tuple_t<short, int> ), std::tuple<short> );
}


BOOST_AUTO_TEST_CASE( test_first_type_t )
{
    TEST_SAME( TPL( first_type_t<int, short, char> ), int );
    TEST_SAME( TPL( first_type_t<short> ), short );
    TEST_SAME( TPL( first_type_t<short, int> ), short );
}


BOOST_AUTO_TEST_CASE( test_except_first_tuple_t )
{
    TEST_SAME( TPL( except_first_tuple_t<std::tuple<int, short, char>> ), TPL( std::tuple<short, char> ) );
    TEST_SAME( except_first_tuple_t<std::tuple<short>>, std::tuple<> );
    TEST_SAME( TPL( except_first_tuple_t<std::tuple<short, int>> ), std::tuple<int> );
}


BOOST_AUTO_TEST_CASE( test_drop_all_once_t )
{
    TEST_SAME( TPL( drop_all_once_t<std::tuple<int, short>, std::tuple<char>> ), std::tuple<char> );
    TEST_SAME( TPL( drop_all_once_t<std::tuple<short>, std::tuple<short, int>> ), std::tuple<int> );
    TEST_SAME( TPL( drop_all_once_t<std::tuple<short>, std::tuple<short, short, int>> ),
               TPL( std::tuple<short, int> ) );
    TEST_SAME( TPL( drop_all_once_t<std::tuple<short>, std::tuple<>> ), std::tuple<> );
}


BOOST_AUTO_TEST_CASE( test_tuple_drop_common )
{
    TEST_SAME(
            TPL( typename tuple_drop_common<std::tuple<int, short>, std::tuple<char>>::reduced_l ),
            TPL( std::tuple<int, short> )
    );
    TEST_SAME(
            TPL( typename tuple_drop_common<std::tuple<int, short>, std::tuple<char>>::reduced_r ),
            std::tuple<char>
    );
    TEST_SAME(
            TPL( typename tuple_drop_common<std::tuple<short>, std::tuple<short, int>>::reduced_l ),
            std::tuple<>
    );
    TEST_SAME(
            TPL( typename tuple_drop_common<std::tuple<short>, std::tuple<short, int>>::reduced_r ),
            std::tuple<int>
    );
    TEST_SAME(
            TPL( typename tuple_drop_common<std::tuple<short, short, int>, std::tuple<short, int>>::reduced_l ),
            std::tuple<short>
    );
    TEST_SAME(
            TPL( typename tuple_drop_common<std::tuple<short, short, int>, std::tuple<short, int>>::reduced_r ),
            std::tuple<>
    );
    TEST_SAME(
            TPL( typename tuple_drop_common<std::tuple<short>, std::tuple<>>::reduced_l ),
            std::tuple<short>
    );
    TEST_SAME(
            TPL( typename tuple_drop_common<std::tuple<short>, std::tuple<>>::reduced_r ),
            std::tuple<>
    );
}


BOOST_AUTO_TEST_CASE( test_fraction_unit_t )
{
    TEST_SAME(
            TPL( fraction_unit_t<std::tuple<int, int, int, int>, std::tuple<short, int>> ),
            TPL( fraction_unit<std::tuple<int, int, int>, std::tuple<short>> )
    );

    TEST_SAME(
            TPL( fraction_unit_t<std::tuple<int, char>, std::tuple<char>> ),
            int
    );

    TEST_SAME(
            TPL( fraction_unit_t<std::tuple<int, int, int>, std::tuple<int>> ),
            TPL( product_unit<int, int> )
    );

    TEST_SAME(
            TPL( fraction_unit_t<std::tuple<int, int>, std::tuple<int>> ),
            int
    );
}

BOOST_AUTO_TEST_SUITE_END()
