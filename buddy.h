#pragma once
#include <stdbool.h>

void* myMemmory;
unsigned long size;

void buddy_init(void* startAdress, unsigned siz); // size - velicina u bajtovima
void* buddy_alloc(unsigned siz); // size - velicina u bajtovima , vraca pokazivac
void buddy_dealloc(void* startAdress, unsigned siz); // size - velicina u bajtovima datog prostora

void ispisiBuddy();
bool checkIfPowerOf2(unsigned long val);


