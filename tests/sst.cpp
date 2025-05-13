#include "../src/cpu.h"
#include "../src/mmu.h"
#include "../src/timer.h"
#include "../src/interrupt.h"
#include "../src/ppu.h"
#include "opcodes.h" // import all opcodes
#include <array>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

class sst_mmu : public mmu {
  public:
    sst_mmu(timer &gb_timer, interrupt &gb_interrupt, ppu &gb_ppu) : mmu(gb_timer, gb_interrupt, gb_ppu) {};

    uint8_t memory[0xffff]{};

    uint8_t bus_read_memory(uint16_t address) override {
        return memory[address];
    };

    void bus_write_memory(uint16_t address, uint8_t value) override {
        memory[address] = value;
    };
};


timer test_timer{};
interrupt test_interrupt{};

sf::RenderWindow &window{};
ppu test_ppu{test_interrupt, window};

sst_mmu test_mmu{test_timer, test_interrupt, test_ppu};

cpu test_cpu = cpu(test_mmu, test_timer, test_interrupt);

namespace sst {
struct cpu_state {
    uint16_t pc{};
    uint16_t sp{};
    uint8_t a{};
    uint8_t b{};
    uint8_t c{};
    uint8_t d{};
    uint8_t e{};
    uint8_t f{};
    uint8_t h{};
    uint8_t l{};
    int ime{};
    // int ie; // final doesn't have ie, do i even test this?
    std::vector<std::array<int, 2>> ram;
};

struct cycles {
    int address_pins{};
    int data_pins{};
    std::string memory_request_pins;
};

struct sst {
    std::string name;
    cpu_state initial;
    cpu_state final;
    std::vector<cycles> cycle;
};

void from_json(const json &j, cycles &cycle) {
    j[0].get_to(cycle.address_pins);
    j[1].get_to(cycle.data_pins);
    j[2].get_to(cycle.memory_request_pins);
}

void from_json(const json &j, cpu_state &cpu_state) {
    j.at("pc").get_to(cpu_state.pc);
    j.at("sp").get_to(cpu_state.sp);
    j.at("a").get_to(cpu_state.a);
    j.at("b").get_to(cpu_state.b);
    j.at("c").get_to(cpu_state.c);
    j.at("d").get_to(cpu_state.d);
    j.at("e").get_to(cpu_state.e);
    j.at("f").get_to(cpu_state.f);
    j.at("h").get_to(cpu_state.h);
    j.at("l").get_to(cpu_state.l);
    j.at("ime").get_to(cpu_state.ime);
    // j.at("ie").get_to(cpu_state.ie);
    j.at("ram").get_to(cpu_state.ram);
}

void from_json(const json &j, sst &sst) {
    j.at("name").get_to(sst.name);
    j.at("initial").get_to(sst.initial);
    j.at("final").get_to(sst.final);
    j.at("cycles").get_to(sst.cycle);
}

} // namespace sst

void test_setup(cpu &test_cpu, sst::cpu_state &initial) {
    test_cpu.PC = initial.pc;
    test_cpu.SP = initial.sp;
    test_cpu.A = initial.a;
    test_cpu.B = initial.b;
    test_cpu.C = initial.c;
    test_cpu.D = initial.d;
    test_cpu.E = initial.e;
    test_interrupt.ime = initial.ime;

    std::array<bool, 4> znhc{}; // flags array
    for (unsigned int i = 0; i < znhc.size(); ++i) {
        znhc[i] = (initial.f >> (7 - i)) & 1;
    }
    test_cpu.Zf = znhc[0] == 1;
    test_cpu.Nf = znhc[1] == 1;
    test_cpu.Hf = znhc[2] == 1;
    test_cpu.Cf = znhc[3] == 1;

    test_cpu.H = initial.h;
    test_cpu.L = initial.l;

    for (const std::array<int, 2> &i : initial.ram) {
        test_cpu._set(i[0], i[1]);
    }
    // TODO: what is ime?
}

void compare_final(cpu &test_cpu, sst::cpu_state &final) {
    EXPECT_EQ(test_cpu.PC, final.pc);
    EXPECT_EQ(test_cpu.SP, final.sp);
    EXPECT_EQ(test_cpu.A, final.a);
    EXPECT_EQ(test_cpu.B, final.b);
    EXPECT_EQ(test_cpu.C, final.c);
    EXPECT_EQ(test_cpu.D, final.d);
    EXPECT_EQ(test_cpu.E, final.e);
    EXPECT_EQ(test_interrupt.ime, final.ime);

    std::array<bool, 4> znhc{}; // flags array
    for (unsigned int i = 0; i < znhc.size(); ++i) {
        znhc[i] = (final.f >> (7 - i)) & 1;
    }
    EXPECT_EQ(znhc[0], test_cpu.Zf);
    EXPECT_EQ(znhc[1], test_cpu.Nf);
    EXPECT_EQ(znhc[2], test_cpu.Hf);
    EXPECT_EQ(znhc[3], test_cpu.Cf);

    EXPECT_EQ(test_cpu.H, final.h);
    EXPECT_EQ(test_cpu.L, final.l);

    for (const std::array<int, 2> &i : final.ram) {
        EXPECT_EQ(test_cpu._get(i[0]), i[1]);
    }
}

class OpcodeTest : public testing::TestWithParam<uint8_t> {
  public:
    uint8_t value{};
    json data{};

    void SetUp() override {
        std::stringstream ss;
        uint8_t value = GetParam();
        ss << std::hex << std::setfill('0') << std::setw(2)
           << static_cast<int>(value);
        std::string filename = "sm83/v1/" + ss.str() + ".json";

        std::cout << filename << '\n';
        std::cout << std::filesystem::current_path() << '\n';

        std::ifstream f(filename);

        this->value = value;
        this->data = json::parse(f);
    }
};

class CBOpcodeTest : public OpcodeTest {
  public:
    void SetUp() override {
        std::stringstream ss;
        uint8_t value = GetParam();
        ss << std::hex << std::setfill('0') << std::setw(2)
           << static_cast<int>(value);
        std::string filename = "sm83/v1/cb " + ss.str() + ".json";

        std::cout << filename << '\n';
        std::cout << std::filesystem::current_path() << '\n';

        std::ifstream f(filename);

        this->value = value;
        this->data = json::parse(f);
    }
};

TEST_P(OpcodeTest, opcode) {
    for (const json &test_set : this->data) {
        sst::sst test = test_set.template get<sst::sst>();
        // initialize cpu values
        test_setup(test_cpu, test.initial);
        // fetch the opcode
        test_cpu.identify_opcode(test_cpu._get(test_cpu.PC));
        // execute the operations
        while (!test_cpu.M_operations.empty()) {
            test_cpu.execute_M_operations();
        }
        // see the final state
        compare_final(test_cpu, test.final);
    }
}

TEST_P(CBOpcodeTest, opcode_cb) {
    for (const json &test_set : this->data) {
        sst::sst test = test_set.template get<sst::sst>();
        // initialize cpu values
        test_setup(test_cpu, test.initial);
        // fetch the opcode
        test_cpu.identify_opcode(test_cpu._get(test_cpu.PC));
        // execute the operations
        while (!test_cpu.M_operations.empty()) {
            test_cpu.execute_M_operations();
        }
        // see the final state
        compare_final(test_cpu, test.final);
    }
}

std::string opcode_param_to_string(
    const testing::TestParamInfo<OpcodeTest::ParamType> &info) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(2)
       << static_cast<int>(info.param);
    return ss.str();
}

INSTANTIATE_TEST_SUITE_P(cpu, OpcodeTest, testing::ValuesIn(opcodes),
                         opcode_param_to_string);

INSTANTIATE_TEST_SUITE_P(cpu, CBOpcodeTest, testing::ValuesIn(cb_opcodes),
                         opcode_param_to_string);
