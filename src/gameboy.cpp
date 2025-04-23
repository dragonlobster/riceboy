#include "gameboy.h"

void gameboy::tick() {
	this->gb_cpu.tick(); 

	this->gb_cpu.timer_tick();

	this->gb_cpu.interrupt_tick();

	this->gb_ppu.tick(); 
}

void gameboy::skip_bootrom() { 
	this->gb_mmu.initialize_skip_bootrom_values(); 
	this->gb_cpu.initialize_skip_bootrom_values(); 
	this->gb_ppu.initialize_skip_bootrom_values(); 
}
