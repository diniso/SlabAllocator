#include "slab.h"
#include "buddy.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <windows.h>



#define MAXIMUM_SIZE_BUFFER 17
#define MINIMUM_SIZE_BUFFER 5
#define SIZE_N 1 // broj u stepenu dvojke, govori koliko blokova se treba alocirati za jednu plocu , maksimalna vrednost 124

#define NULL_POINTER_ARGUMENT_OBJECT 4
#define BAD_POINTER_ARGUMENT_OBJECT 5
#define NOT_ENOUGHT_MEMORY 6


HANDLE Semaphore;

extern void* myMemmory;
extern unsigned long size;

typedef void(*fun) (void*);

typedef struct object_s {
	struct object_s * next; // ostavi se prostor za objekat
//	bool isFree; // ostavi se jos jedan bajt za proveru dal je slobodno


}object;

typedef struct slab_s {
	struct slab_s* next;
	object*free;
	unsigned startAdress;
	unsigned objectAllocated;
	bool dirty;

}slab;

typedef struct kmem_cache_s {
	struct kmem_cache_s* next;
	const char* name;
	fun constructor;
	fun destructor;
	slab* full, *empty, *mix; // empty* kod buffera je pokazivac na buffer
	unsigned slotSize; // kod buffera sadrzi koliko ima size, 
	unsigned NumberOfAllAllocatedSlabs;
	unsigned cacheSize;
	unsigned objectPerSlab; // kod buffera govori dal li je sve u jednom objektu
	unsigned memoryAllocated;
	unsigned numberOfAllocatedObjests;
	bool isBuffer;
	unsigned char error;

}kmem_cache_t;

void wait() {
	WaitForSingleObject(Semaphore, INFINITE);
}
void signal() {
	ReleaseSemaphore(Semaphore, 1, NULL);
}

unsigned getLargerNumberPowerOf2(unsigned num) {

	if (checkIfPowerOf2(num)) return num;

	int ret = 1;
	while (num > 0) {
		ret <<= 1;
		num >>= 1;
	}
	return ret;
}

void kmem_init(void *space, int block_num) {
	buddy_init(space, block_num*BLOCK_SIZE);
	Semaphore = CreateSemaphore(NULL,1,  100,  NULL);

}

bool checkIfCacheExists(kmem_cache_t *cachep) {

	if (cachep == 0) { 
		return false;
	}
	for (kmem_cache_t* tek = (*((kmem_cache_t**)myMemmory)); tek; tek = tek->next)
		if (tek == cachep) return true;

	return false;
}

void kmem_cache_info(kmem_cache_t *cachep) {
	wait();
	if (!checkIfCacheExists(cachep)) { 	
		signal();
		return;
	}

	int objectSize = cachep->slotSize + sizeof(bool);
	if (objectSize < (sizeof(bool) + sizeof(object*))) objectSize = (sizeof(bool) + sizeof(object*));

	int slabSize = getLargerNumberPowerOf2(objectSize * cachep->objectPerSlab + sizeof(slab));

	printf("\nIme kesa: %s\n", cachep->name);
	printf("Velicina podatka u bajtovima: %d\n", cachep->slotSize);
	printf("Velicina celog kesa u blokovima(%d): %d\n", BLOCK_SIZE, cachep->memoryAllocated / BLOCK_SIZE + (cachep->memoryAllocated % (BLOCK_SIZE) == 0 ? 0 : 1 ));
	printf("Broj ploca: %d\n", cachep->memoryAllocated / slabSize);
	printf("Broj objekata po ploci: %d\n", cachep->objectPerSlab);
	printf("Procentualna popunjenost kesa(zauzet prostor): %f\n", (cachep->cacheSize * 100.0 / cachep->memoryAllocated));
	
	int brojObjekataUkupno = (cachep->memoryAllocated / slabSize)*cachep->objectPerSlab;
	double popunjenost = 0;
	if (brojObjekataUkupno > 0) popunjenost = cachep->numberOfAllocatedObjests  * 100.0 / brojObjekataUkupno;
	printf("Procentualna popunjenost kesa(zauzeti objekti): %f\n", popunjenost );
	printf("Broj alociranih objekata: %d\n\n", cachep->numberOfAllocatedObjests);

	signal();
}

kmem_cache_t *kmem_cache_create(const char *name, size_t size, void(*ctor)(void*), void(*dtor)(void*)) {
	wait();

	kmem_cache_t *pok = (kmem_cache_t *)buddy_alloc(sizeof(kmem_cache_t));
	
	if (pok == 0) {
		signal();
		return 0;
	}

	pok->name = name;
	pok->next = 0;
	pok->constructor = ctor;
	pok->destructor = dtor;
	pok->full = pok->empty = pok->mix = 0;
	pok->slotSize = size;
	pok->cacheSize = sizeof(kmem_cache_t);
	pok->memoryAllocated = getLargerNumberPowerOf2(pok->cacheSize);
	pok->error = 0;
	pok->NumberOfAllAllocatedSlabs = 0;
	pok->isBuffer = false;
	pok->numberOfAllocatedObjests = 0;

	size += sizeof(bool);
	if (size < (sizeof(bool) + sizeof(object*))) size = (sizeof(bool) + sizeof(object*)); // ako hocemo dinamicki promeniti samo da se promeni velicina slaba, tj. SIZE_N * BLOCK_SIZE
	pok->objectPerSlab = (SIZE_N * BLOCK_SIZE - sizeof(slab)) / size;



	kmem_cache_t* head = (*((kmem_cache_t**)myMemmory));
	
	pok->next = head;
	(*((kmem_cache_t**)myMemmory)) = pok;

	signal();
	return pok;
}

int kmem_cache_shrink(kmem_cache_t *cachep) {
	wait();

	if (!checkIfCacheExists(cachep)) {
		signal();
		return 0;
	}
	int ret = 0;

	for (slab* pret = 0, * tek = cachep->empty; tek; ) {
		if (tek->dirty) {
			tek->dirty = false;
			pret = tek;
			tek = tek->next;
			continue;
		}
		int objectSize = cachep->slotSize + sizeof(bool);
		if (objectSize < (sizeof(bool) + sizeof(object*))) objectSize = (sizeof(bool) + sizeof(object*));

		int slabSize = objectSize * cachep->objectPerSlab + sizeof(slab);

		ret += getLargerNumberPowerOf2(slabSize) / BLOCK_SIZE;
		if (!pret) 	cachep->empty = tek->next;
		else pret->next = tek->next;

		slab* zaBrisanje = tek;
		tek = tek->next;
		zaBrisanje->next = 0;
		
		cachep->cacheSize -= slabSize;
		cachep->memoryAllocated -= getLargerNumberPowerOf2(slabSize);
		buddy_dealloc(zaBrisanje, slabSize);

	}

	signal();
	return ret;
}

void kmem_cache_destroy(kmem_cache_t *cachep) { 
	wait();
	if (!checkIfCacheExists(cachep)) {
		signal();
		return;
	}

	int objectSize = cachep->slotSize + sizeof(bool);
	if (objectSize < (sizeof(bool) + sizeof(object*))) objectSize = (sizeof(bool) + sizeof(object*));

	int slabSize = objectSize * cachep->objectPerSlab + sizeof(slab);

	for (slab* tek = cachep->empty; tek; ) {
		slab* ZaBrisanje = tek;
		tek = tek->next;
		ZaBrisanje->next = 0;
		cachep->cacheSize -= slabSize;
		cachep->memoryAllocated -= getLargerNumberPowerOf2(slabSize);
		buddy_dealloc(ZaBrisanje, slabSize);
	}

	for (slab* tek = cachep->mix; tek; ) {
		slab* ZaBrisanje = tek;
		tek = tek->next;
		ZaBrisanje->next = 0;
		cachep->cacheSize -= slabSize;
		cachep->memoryAllocated -= getLargerNumberPowerOf2(slabSize);
		buddy_dealloc(ZaBrisanje, slabSize);
	}

	for (slab* tek = cachep->full; tek; ) {
		slab* ZaBrisanje = tek;
		tek = tek->next;
		ZaBrisanje->next = 0;
		cachep->cacheSize -= slabSize;
		cachep->memoryAllocated -= getLargerNumberPowerOf2(slabSize);
		buddy_dealloc(ZaBrisanje, slabSize);
	}

	kmem_cache_t* head = (*((kmem_cache_t**)myMemmory));

	for (kmem_cache_t*pret = 0, *tek = head; tek; pret = tek, tek = tek->next) {
		if (tek != cachep) continue;

		if (!pret) (*((kmem_cache_t**)myMemmory)) = tek->next;
		else pret->next = tek->next;

		break;
	}
	buddy_dealloc(cachep, cachep->cacheSize);

	signal();
}

slab* napraviNoviSlab(unsigned objectPerSlab, unsigned objectSize, unsigned CacheOffset) {

	objectSize += sizeof(bool);
	if (objectSize < (sizeof(bool) + sizeof(object*))) objectSize = (sizeof(bool) + sizeof(object*));

	int slabSize = objectSize * objectPerSlab + sizeof(slab);

	slab* novi = buddy_alloc(slabSize);

	if (novi == 0) return 0; // nema memorije
//	ispisiBuddy();

	novi->dirty = false;
	novi->free = 0;
	novi->next = 0;
	novi->objectAllocated = 0;

	void* pok = (void*)((unsigned long)novi + sizeof(slab) + CacheOffset);

	novi->startAdress = (unsigned)pok;

	object*pret = 0;
	for (unsigned i = 0; i < objectPerSlab; i++) {
		if (!pret) novi->free = pok;
		else pret->next = pok;

		object* obj = (object*)pok;
		obj->next = 0;
		*((bool*)((unsigned long)pok + objectSize - 1)) = true;

		pret = (object*)pok;
		pok = (void*)((unsigned long)pok + objectSize);
		
	}

	return novi;
}

void *kmem_cache_alloc(kmem_cache_t *cachep) {
	wait();

	if (!checkIfCacheExists(cachep)) {
		signal();
		return 0;
	}

	object* ret = 0;
	int objectSize = cachep->slotSize + sizeof(bool);
	if (objectSize < (sizeof(bool) + sizeof(object*))) objectSize = (sizeof(bool) + sizeof(object*));

	if (cachep->mix != 0) {
		ret = cachep->mix->free;
		cachep->mix->free = cachep->mix->free->next;
		cachep->mix->objectAllocated++;

		if (cachep->mix->free == 0) {
			slab* premestiUFull = cachep->mix;
			cachep->mix = cachep->mix->next;

			premestiUFull->next = cachep->full;
			cachep->full = premestiUFull;
		}
	}

	if (ret != 0) {
		if (cachep->constructor != 0) cachep->constructor((void*)ret);
		*((bool*)((unsigned long)ret + objectSize - 1)) = false;
		cachep->numberOfAllocatedObjests++;
		signal();
		return (void*)((void*)ret);
	}



	if (cachep->empty == 0) {
		int objectSize = cachep->slotSize + sizeof(bool);
		if (objectSize < (sizeof(bool) + sizeof(object*))) objectSize = (sizeof(bool) + sizeof(object*));
		int slabSize = objectSize * cachep->objectPerSlab + sizeof(slab);

		int internalFragmentation = getLargerNumberPowerOf2(slabSize) - slabSize;
		int SlabOffset = 0;
		if (internalFragmentation >= CACHE_L1_LINE_SIZE) {
			SlabOffset = cachep->NumberOfAllAllocatedSlabs % (internalFragmentation / CACHE_L1_LINE_SIZE);
		}

		slab* novi = napraviNoviSlab(cachep->objectPerSlab, cachep->slotSize , SlabOffset * CACHE_L1_LINE_SIZE);
		if (novi == 0) {
			cachep->error = NOT_ENOUGHT_MEMORY;
			signal();
			return 0;
		}



		cachep->NumberOfAllAllocatedSlabs += 1;
		cachep->cacheSize += slabSize;
		cachep->memoryAllocated += getLargerNumberPowerOf2(slabSize);
		novi->next = cachep->empty;
		cachep->empty = novi;
	}


		ret = cachep->empty->free;
		cachep->empty->free = cachep->empty->free->next;
		cachep->empty->objectAllocated++;

		if (cachep->empty->free == 0) {
			slab* premestiUFull = cachep->empty;
			premestiUFull->dirty = true;
			cachep->empty = cachep->empty->next;

			premestiUFull->next = cachep->full;
			cachep->full = premestiUFull;
		}
		else {
			slab *premestiUMix = cachep->empty;
			premestiUMix->dirty = true;
			cachep->empty = cachep->empty->next;

			premestiUMix->next = cachep->mix;
			cachep->mix = premestiUMix;
		}


		if (cachep->constructor != 0) cachep->constructor((void*)ret);
		*((bool*)((unsigned long)ret + objectSize - 1)) = false;
		cachep->numberOfAllocatedObjests++;
		signal();
		return (void*)((void*)ret);


}

void kmem_cache_free(kmem_cache_t *cachep, void *objp) {
	wait();

	if (!checkIfCacheExists(cachep) || objp == 0) {
		signal();
		return;
	}

	int objectSize = cachep->slotSize + sizeof(bool);
	if (objectSize < (sizeof(bool) + sizeof(object*))) objectSize = (sizeof(bool) + sizeof(object*));

	int slabSize = objectSize * cachep->objectPerSlab + sizeof(slab);

	for (slab* pret = 0, *tek = cachep->mix; tek; pret = tek, tek = tek->next) {

		if (tek->startAdress > ((unsigned long)objp) || (tek->startAdress + slabSize) <((unsigned long)objp +objectSize )) continue;

		if ((((unsigned long)objp) - tek->startAdress) % objectSize != 0 || (*((bool*)((unsigned long)objp + objectSize - 1))) == true) {
			cachep->error = BAD_POINTER_ARGUMENT_OBJECT;
			signal();
			return;
		}

		if (cachep->destructor != 0) cachep->destructor(objp);
		(*((bool*)((unsigned long)objp + objectSize - 1))) = true;
		tek->objectAllocated--;
		cachep->numberOfAllocatedObjests--;

		object* obj = (object*)objp;
		obj->next = tek->free;
		tek->free = obj;

		if (tek->objectAllocated == 0) {
			if (!pret) cachep->mix = tek->next;
			else pret->next = tek->next;

			tek->next = cachep->empty;
			cachep->empty = tek;
		}

		signal();
		return;

	}

	for (slab* pret = 0, *tek = cachep->full; tek; pret = tek, tek = tek->next) {
		if (tek->startAdress > ((unsigned long)objp) || (tek->startAdress + slabSize) < ((unsigned long)objp + objectSize)) continue;

		if ((((unsigned long)objp) - tek->startAdress) % objectSize != 0 || (*((bool*)((unsigned long)objp + objectSize - 1))) == true) {
			cachep->error = BAD_POINTER_ARGUMENT_OBJECT;
			signal();
			return;
		}

		if (cachep->destructor != 0) cachep->destructor(objp);
		(*((bool*)((unsigned long)objp + objectSize - 1))) = true;
		tek->objectAllocated--;
		cachep->numberOfAllocatedObjests--;

		object* obj = (object*)objp;
		obj->next = tek->free;
		tek->free = obj;

		if (!pret) cachep->full = tek->next;
		else pret->next = tek->next;

		if (tek->objectAllocated == 0) {		

			tek->next = cachep->empty;
			cachep->empty = tek;
		}
		else {
			tek->next = cachep->mix;
			cachep->mix = tek;
		}

		signal();
		return;

	}

	cachep->error = BAD_POINTER_ARGUMENT_OBJECT;
	signal();
}

unsigned log2(unsigned long val) { // vraca gornji deo log2
	unsigned ret = 0;
	while (val > 0) {
		ret++;
		val >>= 1;
	}
	return ret;
}

void *kmalloc(size_t size) {
	wait();

	unsigned sizeNBuffer = log2(size);
	if (sizeNBuffer < MINIMUM_SIZE_BUFFER || sizeNBuffer > MAXIMUM_SIZE_BUFFER) {
		signal();
		return 0;
	}

	int fragmentation = getLargerNumberPowerOf2(size) - size;
	kmem_cache_t* kes = 0;
	if (fragmentation > sizeof(kmem_cache_t)) { // mozemo da smestimo u alocirani objekat

		size += sizeof(kmem_cache_t);
		kes = (kmem_cache_t*)buddy_alloc(size);

		if (kes == 0) {
			signal();
			return 0;
		}


		kes->cacheSize = size;
		kes->memoryAllocated = getLargerNumberPowerOf2(size);
		kes->next = 0;
		kes->full = kes->mix = 0;
		kes->constructor = kes->destructor = 0;
		kes->slotSize = size - sizeof(kmem_cache_t);
		kes->error = 0;
		kes->isBuffer = true;
		kes->name = 0;
		kes->objectPerSlab = 1;  // smestio sam sve u jedan blok podataka
		kes->numberOfAllocatedObjests = 0;
		kes->NumberOfAllAllocatedSlabs = 0;

		kes->empty = (slab*)(((unsigned long)kes) + sizeof(kmem_cache_t));


	}
	else {

		kes = (kmem_cache_t *)buddy_alloc(sizeof(kmem_cache_t));

		if (kes == 0) {
			signal();
			return 0;
		}


		kes->empty = (slab*)buddy_alloc(size); // napraviBuffer

		if (kes->empty == 0) {
			buddy_dealloc(kes, sizeof(kmem_cache_t));
			signal();
			return 0;
		}

		kes->cacheSize = sizeof(kmem_cache_t) + size;
		kes->memoryAllocated = getLargerNumberPowerOf2(sizeof(kmem_cache_t) + size);
		kes->next = 0;
		kes->full = kes->mix = 0;
		kes->constructor = kes->destructor = 0;
		kes->slotSize = size;
		kes->error = 0;
		kes->isBuffer = true;
		kes->name = 0;
		kes->objectPerSlab = 0; // govori dal je sve odjednom
		kes->numberOfAllocatedObjests = 0;
		kes->NumberOfAllAllocatedSlabs = 0;
	}
	

	kmem_cache_t* head = (*((kmem_cache_t**)myMemmory));

	kes->next = head;
	(*((kmem_cache_t**)myMemmory)) = kes;

	signal();
	return (void*)(kes->empty);
}

void kfree(const void *objp) {
	wait();

	if (objp == 0) {
		signal();
		return;
	}


	kmem_cache_t *head = (*((kmem_cache_t**)myMemmory));

	for (kmem_cache_t *pret = 0, *tek = head; tek; pret = tek, tek = tek->next) {
		if (!tek->isBuffer || tek->empty != objp) continue;

		if (!pret) (*((kmem_cache_t**)myMemmory)) = tek->next;
		else pret->next = tek->next;

		if (tek->objectPerSlab > 0) {
			buddy_dealloc(tek, tek->cacheSize);
			break;
		}

		buddy_dealloc(tek->empty, tek->slotSize);
		tek->cacheSize -= tek->slotSize;
		tek->memoryAllocated -= getLargerNumberPowerOf2(tek->slotSize);
		buddy_dealloc(tek, tek->cacheSize);
		break;
	}

	signal();
}

int kmem_cache_error(kmem_cache_t *cachep) {
	wait();
	if (!checkIfCacheExists(cachep)) {
		printf("\nKes ne postoji.\n");
		signal();
		return 2;
	}
	printf("\nU kesu '%s': ", cachep->name);

	switch (cachep->error) {
	case 0:
		printf("Nije bilo greske\n");
		break;
	case 4:
		printf("Objekat je bio NULL\n");
		break;
	case 5:
		printf("Objekat nije imao validnu vrednost\n");
		break;
	case 6:
		printf("Nije bilo dovoljno memorije da se alocira objeakt\n");
		break;
	}

	signal();
	return cachep->error;
}