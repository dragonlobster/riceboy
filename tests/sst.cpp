#include "../src/cpu.h"
#include "../src/mmu.h"
#include <array>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;


class sst_mmu : public mmu {
  public:
    uint8_t memory[0xffff]{};

    uint8_t get_value_from_address(uint16_t address) const override {
        return memory[address];
    };

    void write_value_to_address(uint16_t address, uint8_t value) override {
        memory[address] = value;
    };

};

sst_mmu test_mmu{};

cpu test_cpu = cpu(test_mmu);

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
    //int ie; // final doesn't have ie, do i even test this?
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
    //j.at("ie").get_to(cpu_state.ie);
    j.at("ram").get_to(cpu_state.ram);
}

void from_json(const json &j, sst &sst) {
    j.at("name").get_to(sst.name);
    j.at("initial").get_to(sst.initial);
    j.at("final").get_to(sst.final);
    j.at("cycles").get_to(sst.cycle);
}

} // namespace sst

void test_setup(cpu& test_cpu, sst::cpu_state& initial) {
    test_cpu.PC = initial.pc;
    test_cpu.SP = initial.sp;
    test_cpu.A = initial.a;
    test_cpu.B = initial.b;
    test_cpu.C = initial.c;
    test_cpu.D = initial.d;
    test_cpu.E = initial.e;

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
        test_cpu._write_memory(i[0], i[1]);
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
        EXPECT_EQ(test_cpu._read_memory(i[0]), i[1]);
    }

}

TEST(CPUOpcodeTest, 0x31) {
    std::ifstream f("sm83/v1/31.json");
    json data = json::parse(f);

    for (const json& test_set : data) {
        sst::sst test = test_set.template get<sst::sst>();
        // initialize cpu values
        test_setup(test_cpu, test.initial);
        // fetch the opcode
        test_cpu.identify_opcode(0x31);
        // execute the operations
        while (!test_cpu.M_operations.empty()) {
            test_cpu.execute_M_operations();
        }
        // see the final state
        compare_final(test_cpu, test.final);
    }
}

TEST(CPUOpcodeTest, 0xaf) {
    std::ifstream f("sm83/v1/af.json");
    json data = json::parse(f);

    for (const json& test_set : data) {
        sst::sst test = test_set.template get<sst::sst>();
        // initialize cpu values
        test_setup(test_cpu, test.initial);
        // fetch the opcode
        test_cpu.identify_opcode(0xaf);
        // execute the operations
        while (!test_cpu.M_operations.empty()) {
            test_cpu.execute_M_operations();
        }
        // see the final state
        compare_final(test_cpu, test.final);
    }
}

TEST(CPUOpcodeTest, 0x21) {
    std::ifstream f("sm83/v1/21.json");
    json data = json::parse(f);

    for (const json& test_set : data) {
        sst::sst test = test_set.template get<sst::sst>();
        // initialize cpu values
        test_setup(test_cpu, test.initial);
        // fetch the opcode
        test_cpu.identify_opcode(0x21);
        // execute the operations
        while (!test_cpu.M_operations.empty()) {
            test_cpu.execute_M_operations();
        }
        // see the final state
        compare_final(test_cpu, test.final);
    }
}

TEST(CPUOpcodeTest, 0x32) {
    std::ifstream f("sm83/v1/32.json");
    json data = json::parse(f);

    for (const json& test_set : data) {
        sst::sst test = test_set.template get<sst::sst>();
        // initialize cpu values
        test_setup(test_cpu, test.initial);
        // fetch the opcode
        test_cpu.identify_opcode(0x32);
        // execute the operations
        while (!test_cpu.M_operations.empty()) {
            test_cpu.execute_M_operations();
        }
        // see the final state
        compare_final(test_cpu, test.final);
    }
}

TEST(CPUOpcodeTest, 0x20) {
    std::ifstream f("sm83/v1/20.json");
    json data = json::parse(f);

    for (const json& test_set : data) {
        sst::sst test = test_set.template get<sst::sst>();
        // initialize cpu values
        test_setup(test_cpu, test.initial);
        // fetch the opcode
        test_cpu.identify_opcode(0x20);
        // execute the operations
        while (!test_cpu.M_operations.empty()) {
            test_cpu.execute_M_operations();
        }
        // see the final state
        compare_final(test_cpu, test.final);
    }
}

TEST(CPUOpcodeTest, 0x0e) {
    std::ifstream f("sm83/v1/0e.json");
    json data = json::parse(f);

    for (const json& test_set : data) {
        sst::sst test = test_set.template get<sst::sst>();
        // initialize cpu values
        test_setup(test_cpu, test.initial);
        // fetch the opcode
        test_cpu.identify_opcode(0x0e);
        // execute the operations
        while (!test_cpu.M_operations.empty()) {
            test_cpu.execute_M_operations();
        }
        // see the final state
        compare_final(test_cpu, test.final);
    }
}

TEST(CPUOpcodeTest, 0x28) {
    std::ifstream f("sm83/v1/28.json");
    json data = json::parse(f);

    for (const json& test_set : data) {
        sst::sst test = test_set.template get<sst::sst>();
        // initialize cpu values
        test_setup(test_cpu, test.initial);
        // fetch the opcode
        test_cpu.identify_opcode(0x28);
        // execute the operations
        while (!test_cpu.M_operations.empty()) {
            test_cpu.execute_M_operations();
        }
        // see the final state
        compare_final(test_cpu, test.final);
    }
}


