#include "TestHarness.hpp"
#include "search/ResultSet.hpp"

namespace sse::test {

void runResultSetTests() {
    std::cout << "[RUNNING] ResultSet Tests..." << std::endl;

    using namespace sse::search;

    // 1. Basic Monotonic Appending & Invariants
    {
        ResultSet set;
        TEST_ASSERT(set.empty());
        TEST_ASSERT_EQ(set.size(), 0);

        set.add(10);
        set.add(20);
        set.add(30);

        TEST_ASSERT(!set.empty());
        TEST_ASSERT_EQ(set.size(), 3);
        TEST_ASSERT_EQ(set[0], 10);
        TEST_ASSERT_EQ(set[1], 20);
        TEST_ASSERT_EQ(set[2], 30);
    }

    // 2. Out-of-Order Insertion with Sorted Invariant
    {
        ResultSet set;
        set.add(30);
        set.add(10);
        set.add(20);

        TEST_ASSERT_EQ(set.size(), 3);
        TEST_ASSERT_EQ(set[0], 10);
        TEST_ASSERT_EQ(set[1], 20);
        TEST_ASSERT_EQ(set[2], 30);
    }

    // 3. Duplicate Elimination
    {
        ResultSet set;
        set.add(10);
        set.add(20);
        set.add(10);
        set.add(20);
        set.add(30);
        set.add(30);

        TEST_ASSERT_EQ(set.size(), 3);
        TEST_ASSERT_EQ(set[0], 10);
        TEST_ASSERT_EQ(set[1], 20);
        TEST_ASSERT_EQ(set[2], 30);
    }

    // 4. Binary Search Membership (contains)
    {
        ResultSet set;
        set.add(5);
        set.add(15);
        set.add(25);
        set.add(35);

        TEST_ASSERT(set.contains(5));
        TEST_ASSERT(set.contains(15));
        TEST_ASSERT(set.contains(25));
        TEST_ASSERT(set.contains(35));
        TEST_ASSERT(!set.contains(1));
        TEST_ASSERT(!set.contains(10));
        TEST_ASSERT(!set.contains(40));
        TEST_ASSERT(!set.contains(INVALID_DOC_ID));
    }

    // 5. Two-Pointer Intersection (AND)
    {
        ResultSet a;
        a.add(1); a.add(2); a.add(4); a.add(6); a.add(8);

        ResultSet b;
        b.add(2); b.add(3); b.add(4); b.add(7); b.add(8);

        ResultSet inter = ResultSet::intersect(a, b);
        TEST_ASSERT_EQ(inter.size(), 3);
        TEST_ASSERT_EQ(inter[0], 2);
        TEST_ASSERT_EQ(inter[1], 4);
        TEST_ASSERT_EQ(inter[2], 8);

        // Disjoint intersection
        ResultSet c;
        c.add(100); c.add(200);
        ResultSet emptyInter = ResultSet::intersect(a, c);
        TEST_ASSERT(emptyInter.empty());

        // Empty intersection
        ResultSet emptySet;
        TEST_ASSERT(ResultSet::intersect(a, emptySet).empty());
        TEST_ASSERT(ResultSet::intersect(emptySet, b).empty());
    }

    // 6. Two-Pointer Union (OR)
    {
        ResultSet a;
        a.add(1); a.add(3); a.add(5);

        ResultSet b;
        b.add(2); b.add(3); b.add(4); b.add(6);

        ResultSet u = ResultSet::unionSets(a, b);
        TEST_ASSERT_EQ(u.size(), 6);
        TEST_ASSERT_EQ(u[0], 1);
        TEST_ASSERT_EQ(u[1], 2);
        TEST_ASSERT_EQ(u[2], 3);
        TEST_ASSERT_EQ(u[3], 4);
        TEST_ASSERT_EQ(u[4], 5);
        TEST_ASSERT_EQ(u[5], 6);

        // Union with empty
        ResultSet emptySet;
        ResultSet uEmpty = ResultSet::unionSets(a, emptySet);
        TEST_ASSERT_EQ(uEmpty.size(), 3);
        TEST_ASSERT_EQ(uEmpty[0], 1);
        TEST_ASSERT_EQ(uEmpty[1], 3);
        TEST_ASSERT_EQ(uEmpty[2], 5);
    }

    // 7. Two-Pointer Difference (NOT: Universe \ Exclude)
    {
        ResultSet universe;
        for (sse::DocID id = 1; id <= 10; ++id) {
            universe.add(id);
        }

        ResultSet exclude;
        exclude.add(2); exclude.add(4); exclude.add(6); exclude.add(8); exclude.add(10);

        ResultSet diff = ResultSet::difference(universe, exclude);
        TEST_ASSERT_EQ(diff.size(), 5);
        TEST_ASSERT_EQ(diff[0], 1);
        TEST_ASSERT_EQ(diff[1], 3);
        TEST_ASSERT_EQ(diff[2], 5);
        TEST_ASSERT_EQ(diff[3], 7);
        TEST_ASSERT_EQ(diff[4], 9);

        // Difference with empty exclude
        ResultSet emptySet;
        ResultSet diffEmpty = ResultSet::difference(universe, emptySet);
        TEST_ASSERT_EQ(diffEmpty.size(), 10);

        // Difference with universe as subset
        ResultSet diffAll = ResultSet::difference(exclude, universe);
        TEST_ASSERT(diffAll.empty());
    }
}

} // namespace sse::test
