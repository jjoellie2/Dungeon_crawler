
/*
 * Dungeon Crawler (cp2)
 * 
 * Features:
 * - Graph-based dungeon generation with adjacency lists
 * - Rooms contain monsters, items, treasure, or are empty
 * - Player navigates rooms, fights monsters with bitwise combat
 * - Items apply effects (hp restore, damage boost)
 * - Save and load game to/from file
 * - Function pointers for room entry handling
 * - Dynamic memory allocation, proper cleanup
 * - Command-line args: <num_rooms> for new game or <filename> to load
 * 
 * Compile: gcc -o dungeon_crawler dungeon_crawler.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_NEIGHBORS 4
#define BIT_ROUND_BITS 16

// Forward declarations
struct Room;
struct Player;
struct Monster;
struct Item;

typedef enum { NONE, MONSTER, ITEM, TREASURE } ContentType;
typedef enum { GOBLIN, TROLL } MonsterType;
typedef enum { POTION, SWORD } ItemType;

typedef void (*EnterFunc)(struct Room*, struct Player*);

