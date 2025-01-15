#include <gtest/gtest.h>
#include "../src/cpu.h"
#include "../src/mmu.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

mmu test_mmu{};
cpu test_cpu = cpu(test_mmu);

TEST(ExamplesTest, Test1) {
    EXPECT_EQ(2, 1);
}

TEST(ExamplesTest, Test2) {
    EXPECT_EQ(2, 2);
}

TEST(ExamplesTest, Test3) {
    EXPECT_EQ(3, 5);
}
