#include <stdio.h>
#include <stdbool.h>



const int MINIMUM  = 5;
const int MAXIMUM  = 31;


extern void* myMemmory;
extern unsigned long size;

typedef struct buddy {
	struct buddy *next;
	unsigned long address;
}buddy;


unsigned getPowerOf2(unsigned vel) {
	unsigned cnt = 0;
	while (vel > 0) {
		vel >>= 1;
		cnt++;
	}
	return cnt;
} 

unsigned long toPowerOf2(unsigned vel) {
	unsigned long ret = 1;
	while (vel > 0) {
		ret <<= 1;
		vel--;
	}
	return ret;
}

int getEntryLarger(unsigned vel) {
	unsigned powOf2 = getPowerOf2(vel); 

	int check = powOf2 - MAXIMUM;
	if (check > 0) return -1;   

	check = powOf2 - MINIMUM;
	if (check < 0) return 0; 

	return check;
}

int getEntry(unsigned vel) {
	unsigned powOf2 = getPowerOf2(vel)-1; 

	int check = powOf2 - MAXIMUM;
	if (check > 0) return -1;

	check = powOf2 - MINIMUM;
	if (check < 0) return 0;

	return check;
} 

unsigned long getSizeFromEntry(unsigned entry) {
	entry += MINIMUM;
	return toPowerOf2(entry);

}

void insertBuddy(int entry , void* pok) {
	buddy **lista = (buddy**)((unsigned long)myMemmory + sizeof(buddy*));

	buddy* pokazivac = (buddy*)pok;
	pokazivac->next = 0;
	pokazivac->address = (unsigned long)pok;

	buddy* tek = 0, *pret = 0;
	for (tek = lista[entry], pret = 0; tek && tek->address > pokazivac->address ; pret = tek, tek = tek->next);

	if (!pret) {
		pokazivac->next = lista[entry];
		lista[entry] = pokazivac;
	}
	else {
		pokazivac->next = pret->next;
		pret->next = pokazivac;
	}


}

buddy* removeBuddy(int entry) {
	buddy **lista = (buddy**)((unsigned long)myMemmory + sizeof(buddy*));

	buddy* ret = lista[entry];
	lista[entry] = lista[entry]->next;
	ret->next = 0;
	return ret;
}

void ispisiBuddy() {
	buddy** lista = (buddy**)((unsigned long)myMemmory + sizeof(buddy*) );
	int vel = MAXIMUM - MINIMUM + 1;
	for (int i = 0; i < vel; i++) {
		buddy* tek = lista[i];
		printf("Entry%d(%lu): ", i , (getSizeFromEntry(i)));
		while (tek != 0) {
			printf("%lu ", tek->address);
			tek = tek->next;
		}
		printf("\n");
	}
	printf("\n");
}

void buddy_init(void* startAdress, unsigned siz) {

	if (startAdress == 0) return;

	myMemmory = startAdress;
	size = siz;


	*((buddy**)myMemmory) = 0; // ovo se ostavlja za cache ( pokazivac na prvi element u listi )

	int vel = MAXIMUM - MINIMUM + 1;
	for (int i = 0; i < vel; i++) {		
		*(((buddy**)myMemmory)+i+1) = 0; // postavi ulaze sve na 0
	}

	startAdress = (void*)((unsigned long)startAdress + (vel + 1) * sizeof(buddy*));

	int sizeZaKoriscenje = siz - (vel + 1) * sizeof(buddy*);

	int minimumSize = getSizeFromEntry(0);

	while (sizeZaKoriscenje > minimumSize) {

		int entry = getEntry(sizeZaKoriscenje);

		if (entry == -1) entry = vel - 1;

		insertBuddy(entry, startAdress);

		unsigned long velicinaCelogDela = getSizeFromEntry(entry);
		
		sizeZaKoriscenje -= velicinaCelogDela;
		startAdress = (void*)((unsigned long)startAdress + velicinaCelogDela);

	}

//	ispisiBuddy();
}

bool checkIfPowerOf2(unsigned long val) {
	bool found = false;
	while (val > 0) {
		int bit = val & 1;
		if (bit) {
			if (found) return false;
			found = true;
		}
		val >>= 1;
	}
	return true;
}

void* buddy_alloc(unsigned siz){
	buddy** lista = (buddy**)((unsigned long)myMemmory + sizeof(buddy*));
	int vel = MAXIMUM - MINIMUM + 1;

	int start = getEntryLarger(siz);
	if (checkIfPowerOf2(siz)) start--;

	for (int i = start; i < vel; i++) {
		if (lista[i] == 0) continue;

		buddy* mem = removeBuddy(i);
		while (--i >= start) {
			unsigned long velicinaEntry = getSizeFromEntry(i);
			buddy* zaUbacivanje = (buddy*) ((unsigned long)mem + velicinaEntry);
			insertBuddy(i, zaUbacivanje);
		}

//		ispisiBuddy();

		return mem;
	}

	return 0;
}

void buddy_dealloc(void* startAdress, unsigned siz) {

	if ((unsigned long)startAdress > ((unsigned long) myMemmory + size)) return; 
	if ((unsigned long)startAdress < ((unsigned long)myMemmory + (MAXIMUM - MINIMUM + 2)*sizeof(buddy*))) return;

	int entry = getEntryLarger(siz) , vel = MAXIMUM - MINIMUM + 1;
	if (checkIfPowerOf2(siz)) entry--;

	buddy** lista = (buddy**)((unsigned long)myMemmory + sizeof(buddy*));
	siz = getSizeFromEntry(getEntryLarger(siz));

	int i = 0;
	for (i = entry; i < vel; i++) {
		unsigned long buddyNumber = ((unsigned long)startAdress - ((unsigned long)myMemmory + (MAXIMUM - MINIMUM + 2) * sizeof(buddy*))) / siz;
		unsigned long buddyAdress = (unsigned long)startAdress + siz;
		if (buddyNumber % 2 == 1) buddyAdress -= 2 * siz;

		bool found = false;
		for (buddy * tek = lista[i], *pret = 0; tek && tek->address >= buddyAdress; pret = tek , tek = tek->next) {
			if (tek->address != buddyAdress) continue;

			found = true;
			if (!pret) lista[i] = tek->next;
			else pret->next = tek->next;

			if ((unsigned long)startAdress > tek->address) startAdress = tek;
			break;

		}

		if (!found) break;
		siz <<= 1;

	}

	insertBuddy(i , startAdress);
//	ispisiBuddy();
}