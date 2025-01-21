#include "Gameboy.h";

void Gameboy::tick() {
	this->gb_cpu.tick(); 
	this->gb_ppu.tick(); 
}