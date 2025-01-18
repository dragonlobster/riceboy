#include "ppu.h"

// ppu constructor
ppu::ppu(mmu &mmu) {
    // pass by reference
    this->gb_mmu = &mmu; // & refers to actual address to assign to the pointer

    this->LCDC = this->gb_mmu->get_pointer_from_address(0xff40);
    this->STAT = this->gb_mmu->get_pointer_from_address(0xff41);
    this->SCY = this->gb_mmu->get_pointer_from_address(0xff42);
    this->SCX = this->gb_mmu->get_pointer_from_address(0xff43);
    this->LY = this->gb_mmu->get_pointer_from_address(0xff44);
    this->LYC = this->gb_mmu->get_pointer_from_address(0xff45);
    this->DMA = this->gb_mmu->get_pointer_from_address(0xff46);
    this->BGP = this->gb_mmu->get_pointer_from_address(0xff47);
    this->OBP0 = this->gb_mmu->get_pointer_from_address(0xff48);
    this->OBP1 = this->gb_mmu->get_pointer_from_address(0xff49);
    this->WX = this->gb_mmu->get_pointer_from_address(0xff4b);
    this->WY = this->gb_mmu->get_pointer_from_address(0xff4a);
}

// determine if sprite should be added to sprite buffer
// TODO: need to change this to get all 4 bytes of the OAM sprite metadata
void ppu::add_to_sprite_buffer(uint8_t byte0, uint8_t byte1) {
    // byte 0 = y position, byte 1 = x position
    if (byte1 == 0) {
        return;
    }
    if (*(this->LY) + 16 < byte0) {
        return;
    }
    // get the tall mode LCDC bit 2
    if (const uint8_t sprite_height = *(this->LCDC) >> 2 == 1 ? 16 : 8;
        *(this->LY) + 16 >= byte0 + sprite_height) {
        return;
    }
    if (this->sprite_buffer.size() >= 10) {
        return;
    }

}


// 1 tick = 1 T-Cycle
void ppu::tick() {
    this->ticks++;

    switch (this->current_mode) {
    case mode::OAM_Scan: {
        if (this->ticks < 2) {
            return;
        } // oam scan every 2 ticks


        // TODO: fetch OAM every 2 ticks

        break;
    }
    }

    // this->ticks++; no need, tick is incremented in CPU - we will use that
}

void ppu::pixel_fetcher_tick() {
    if (this->ticks < 2) {
        return;
    }
}
