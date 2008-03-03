#ifndef MEMMANAGER_H_
#define MEMMANAGER_H_

typedef struct {
        char* data;
        int size;
}MemData;

typedef struct
{
		int key;
		char* description;
		int size;
		char* BaseAdd;
}MemKey;


MemKey* createMemKeyFromChar( char* key );

MemKey* initSpace( MemKey *key );

int fetchData( MemKey *key, MemData *data );

int putData( MemKey *key, MemData *data );

#endif /*MEMMANAGER_H_*/
