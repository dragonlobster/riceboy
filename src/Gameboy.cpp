#include "Gameboy.h";

void Gameboy::tick() {
	this->gb_cpu.tick(); 

	this->gb_cpu.timer_tick();

	this->gb_cpu.interrupt_tick();

	this->gb_ppu.tick(); 
}