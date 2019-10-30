#include "test.hpp"

TEST_CASE("rev::expression_composition::expression_composition(expression_composition const&)")
{
    rev::expression_composition a;
    a["R_ZF"];

    auto const b = a;

    CHECK(b == a);
}

TEST_CASE("rev::expression_composition::update(rev::expression_composition)")
{
    rev::expression_composition base_block;
    rev::expression_composition new_block;

    rev::expression_composition result_block;

    SECTION("A")
    {
        SECTION("A1")
        {
        }
        SECTION("A2")
        {
            new_block["EAX"] = rev::expression::value(1);

            result_block["EAX"] = rev::expression::value(1);
        }
    }
    SECTION("B")
    {
        base_block["EAX"] = rev::expression::value(1);

        SECTION("B1")
        {
            result_block["EAX"] = rev::expression::value(1);
        }
        SECTION("B2")
        {
            new_block["EAX"] = rev::expression::value(2);

            result_block["EAX"] = rev::expression::value(2);
        }
        SECTION("B3")
        {
            new_block["EAX"] = rev::expression::unknown("EAX") + rev::expression::value(2);

            result_block["EAX"] = rev::expression::value(3);
        }
        SECTION("B4")
        {
            new_block["EBX"] = rev::expression::value(2);

            result_block["EAX"] = rev::expression::value(1);
            result_block["EBX"] = rev::expression::value(2);
        }
        SECTION("B5")
        {
            new_block["EBX"] = rev::expression::unknown("EAX") + rev::expression::value(2);

            result_block["EAX"] = rev::expression::value(1);
            result_block["EBX"] = rev::expression::value(3);
        }
        SECTION("B6")
        {
            new_block["EAX"] = rev::expression::unknown("EBX") + rev::expression::value(3);
            new_block["EBX"] = rev::expression::unknown("EAX") + rev::expression::value(2);

            result_block["EAX"] = rev::expression::unknown("EBX") + rev::expression::value(3);
            result_block["EBX"] = rev::expression::value(3);
        }
    }
    SECTION("C")
    {
        base_block["EBX"] = rev::expression::value(1);

        SECTION("C1")
        {
            new_block["EAX"] = rev::expression::unknown("EBX") + rev::expression::value(2);
            new_block["EBX"] = rev::expression::unknown("EAX") + rev::expression::value(3);

            result_block["EAX"] = rev::expression::value(3);
            result_block["EBX"] = rev::expression::unknown("EAX") + rev::expression::value(3);
        }
    }

    base_block.update(new_block);

    CHECK(base_block == result_block);
}

TEST_CASE("rev::expression_composition::operator==(rev::expression_composition) const")
{
    rev::expression_composition a;
    rev::expression_composition b;

    SECTION("A")
    {
        a["THIS"] = rev::expression::value(0);

        SECTION("A1")
        {
            b["THIS"] = rev::expression::value(0);
        }
        SECTION("A2")
        {
            b["THIS"] = rev::expression::value(0);
            b["OTHER"];
        }
        SECTION("A3")
        {
            b["THIS"] = rev::expression::value(0);
            b["OTHER"] = rev::expression::unknown("OTHER");
        }
    }
    SECTION("B")
    {
        a["THIS"] = rev::expression::value(0);
        a["OTHER"];

        SECTION("B1")
        {
            b["THIS"] = rev::expression::value(0);
        }
    }
    SECTION("C")
    {
        a["THIS"] = rev::expression::value(0);
        a["OTHER"] = rev::expression::unknown("OTHER");

        SECTION("C1")
        {
            b["THIS"] = rev::expression::value(0);
        }
    }
    SECTION("D")
    {
        a["THIS"] = rev::expression::value(0);
        a["OTHER"] = rev::expression::value(1);

        SECTION("D1: Same order")
        {
            b["THIS"] = rev::expression::value(0);
            b["OTHER"] = rev::expression::value(1);
        }
        SECTION("D2: Reversed order")
        {
            b["OTHER"] = rev::expression::value(1);
            b["THIS"] = rev::expression::value(0);
        }
    }

    CHECK(!(a != b));
    REQUIRE(a == b);
}
TEST_CASE("rev::expression_composition::operator!=(rev::expression_composition) const")
{
    rev::expression_composition a;
    rev::expression_composition b;

    SECTION("A")
    {
        a["THIS"] = rev::expression::value(0);

        SECTION("A1")
        {
            b["OTHER"] = rev::expression::value(0);
        }
        SECTION("A2")
        {
            b["THIS"] = rev::expression::value(1);
        }
    }

    CHECK(!(a == b));
    REQUIRE(a != b);
}